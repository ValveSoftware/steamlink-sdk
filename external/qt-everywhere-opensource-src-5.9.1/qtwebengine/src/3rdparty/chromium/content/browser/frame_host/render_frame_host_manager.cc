// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"

namespace content {

RenderFrameHostManager::RenderFrameHostManager(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    Delegate* delegate)
    : frame_tree_node_(frame_tree_node),
      delegate_(delegate),
      render_frame_delegate_(render_frame_delegate),
      render_widget_delegate_(render_widget_delegate),
      interstitial_page_(nullptr),
      weak_factory_(this) {
  DCHECK(frame_tree_node_);
}

RenderFrameHostManager::~RenderFrameHostManager() {
  if (pending_render_frame_host_)
    UnsetPendingRenderFrameHost();

  if (speculative_render_frame_host_)
    UnsetSpeculativeRenderFrameHost();

  // Delete any RenderFrameProxyHosts and swapped out RenderFrameHosts.
  // It is important to delete those prior to deleting the current
  // RenderFrameHost, since the CrossProcessFrameConnector (owned by
  // RenderFrameProxyHost) points to the RenderWidgetHostView associated with
  // the current RenderFrameHost and uses it during its destructor.
  ResetProxyHosts();

  // We should always have a current RenderFrameHost except in some tests.
  SetRenderFrameHost(std::unique_ptr<RenderFrameHostImpl>());
}

void RenderFrameHostManager::Init(SiteInstance* site_instance,
                                  int32_t view_routing_id,
                                  int32_t frame_routing_id,
                                  int32_t widget_routing_id,
                                  bool renderer_initiated_creation) {
  DCHECK(site_instance);
  // TODO(avi): While RenderViewHostImpl is-a RenderWidgetHostImpl, this must
  // hold true to avoid having two RenderWidgetHosts for the top-level frame.
  // https://crbug.com/545684
  DCHECK(!frame_tree_node_->IsMainFrame() ||
         view_routing_id == widget_routing_id);
  SetRenderFrameHost(CreateRenderFrameHost(site_instance, view_routing_id,
                                           frame_routing_id, widget_routing_id,
                                           delegate_->IsHidden(),
                                           renderer_initiated_creation));

  // Notify the delegate of the creation of the current RenderFrameHost.
  // Do this only for subframes, as the main frame case is taken care of by
  // WebContentsImpl::Init.
  if (!frame_tree_node_->IsMainFrame()) {
    delegate_->NotifySwappedFromRenderManager(
        nullptr, render_frame_host_.get(), false);
  }
}

RenderViewHostImpl* RenderFrameHostManager::current_host() const {
  if (!render_frame_host_)
    return nullptr;
  return render_frame_host_->render_view_host();
}

RenderViewHostImpl* RenderFrameHostManager::pending_render_view_host() const {
  if (!pending_render_frame_host_)
    return nullptr;
  return pending_render_frame_host_->render_view_host();
}

WebUIImpl* RenderFrameHostManager::GetNavigatingWebUI() const {
  if (IsBrowserSideNavigationEnabled()) {
    if (speculative_render_frame_host_)
      return speculative_render_frame_host_->web_ui();
  } else {
    if (pending_render_frame_host_)
      return pending_render_frame_host_->web_ui();
  }
  return render_frame_host_->pending_web_ui();
}

RenderWidgetHostView* RenderFrameHostManager::GetRenderWidgetHostView() const {
  if (interstitial_page_)
    return interstitial_page_->GetView();
  if (render_frame_host_)
    return render_frame_host_->GetView();
  return nullptr;
}

bool RenderFrameHostManager::ForInnerDelegate() {
  return delegate_->GetOuterDelegateFrameTreeNodeId() !=
      FrameTreeNode::kFrameTreeNodeInvalidId;
}

RenderWidgetHostImpl*
RenderFrameHostManager::GetOuterRenderWidgetHostForKeyboardInput() {
  if (!ForInnerDelegate() || !frame_tree_node_->IsMainFrame())
    return nullptr;

  FrameTreeNode* outer_contents_frame_tree_node =
      FrameTreeNode::GloballyFindByID(
          delegate_->GetOuterDelegateFrameTreeNodeId());
  return outer_contents_frame_tree_node->parent()
      ->current_frame_host()
      ->render_view_host()
      ->GetWidget();
}

FrameTreeNode* RenderFrameHostManager::GetOuterDelegateNode() {
  int outer_contents_frame_tree_node_id =
      delegate_->GetOuterDelegateFrameTreeNodeId();
  return FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id);
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToParent() {
  if (frame_tree_node_->IsMainFrame())
    return nullptr;

  return GetRenderFrameProxyHost(frame_tree_node_->parent()
                                     ->render_manager()
                                     ->current_frame_host()
                                     ->GetSiteInstance());
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToOuterDelegate() {
  int outer_contents_frame_tree_node_id =
      delegate_->GetOuterDelegateFrameTreeNodeId();
  FrameTreeNode* outer_contents_frame_tree_node =
      FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id);
  if (!outer_contents_frame_tree_node ||
      !outer_contents_frame_tree_node->parent()) {
    return nullptr;
  }

  return GetRenderFrameProxyHost(outer_contents_frame_tree_node->parent()
                                     ->current_frame_host()
                                     ->GetSiteInstance());
}

void RenderFrameHostManager::RemoveOuterDelegateFrame() {
  FrameTreeNode* outer_delegate_frame_tree_node =
      FrameTreeNode::GloballyFindByID(
          delegate_->GetOuterDelegateFrameTreeNodeId());
  DCHECK(outer_delegate_frame_tree_node->parent());
  outer_delegate_frame_tree_node->frame_tree()->RemoveFrame(
      outer_delegate_frame_tree_node);
}

RenderFrameHostImpl* RenderFrameHostManager::Navigate(
    const GURL& dest_url,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    bool is_reload) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager:Navigate",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  // Create a pending RenderFrameHost to use for the navigation.
  RenderFrameHostImpl* dest_render_frame_host = UpdateStateForNavigate(
      dest_url, frame_entry.source_site_instance(), frame_entry.site_instance(),
      entry.GetTransitionType(), entry.restore_type() != RestoreType::NONE,
      entry.IsViewSourceMode(), entry.transferred_global_request_id(),
      entry.bindings(), is_reload);
  if (!dest_render_frame_host)
    return nullptr;  // We weren't able to create a pending render frame host.

  // If the current render_frame_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_frame_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_frame_host != render_frame_host_.get() &&
      !render_frame_host_->IsRenderFrameLive()) {
    // Note: we don't call InitRenderView here because we are navigating away
    // soon anyway, and we don't have the NavigationEntry for this host.
    delegate_->CreateRenderViewForRenderManager(
        render_frame_host_->render_view_host(), MSG_ROUTING_NONE,
        MSG_ROUTING_NONE, frame_tree_node_->current_replication_state());
  }

  // If the renderer isn't live, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_frame_host->IsRenderFrameLive()) {
    // Instruct the destination render frame host to set up a Mojo connection
    // with the new render frame if necessary.  Note that this call needs to
    // occur before initializing the RenderView; the flow of creating the
    // RenderView can cause browser-side code to execute that expects the this
    // RFH's service_manager::InterfaceRegistry to be initialized (e.g., if the
    // site is a
    // WebUI site that is handled via Mojo, then Mojo WebUI code in //chrome
    // will add an interface to this RFH's InterfaceRegistry).
    dest_render_frame_host->SetUpMojoIfNeeded();

    if (!ReinitializeRenderFrame(dest_render_frame_host))
      return nullptr;

    if (GetNavigatingWebUI()) {
      // A new RenderFrame was created and there is a navigating WebUI which
      // never interacted with it. So notify the WebUI using
      // RenderFrameCreated.
      GetNavigatingWebUI()->RenderFrameCreated(dest_render_frame_host);
    }

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_frame_host != render_frame_host_.get()) {
      if (dest_render_frame_host->GetView())
        dest_render_frame_host->GetView()->Hide();
    } else {
      // After a renderer crash we'd have marked the host as invisible, so we
      // need to set the visibility of the new View to the correct value here
      // after reload.
      if (dest_render_frame_host->GetView() &&
          dest_render_frame_host->render_view_host()
                  ->GetWidget()
                  ->is_hidden() != delegate_->IsHidden()) {
        if (delegate_->IsHidden()) {
          dest_render_frame_host->GetView()->Hide();
        } else {
          dest_render_frame_host->GetView()->Show();
        }
      }

      // TODO(nasko): This is a very ugly hack. The Chrome extensions process
      // manager still uses NotificationService and expects to see a
      // RenderViewHost changed notification after WebContents and
      // RenderFrameHostManager are completely initialized. This should be
      // removed once the process manager moves away from NotificationService.
      // See https://crbug.com/462682.
      delegate_->NotifyMainFrameSwappedFromRenderManager(
          nullptr, render_frame_host_->render_view_host());
    }
  }

  // If entry includes the request ID of a request that is being transferred,
  // the destination render frame will take ownership, so release ownership of
  // the transferring NavigationHandle.
  if (transfer_navigation_handle_.get() &&
      transfer_navigation_handle_->GetGlobalRequestID() ==
          entry.transferred_global_request_id()) {
    // The navigating RenderFrameHost should take ownership of the
    // NavigationHandle that came from the transferring RenderFrameHost.
    dest_render_frame_host->SetNavigationHandle(
        std::move(transfer_navigation_handle_));

    dest_render_frame_host->navigation_handle()->set_render_frame_host(
        dest_render_frame_host);
  }

  return dest_render_frame_host;
}

void RenderFrameHostManager::Stop() {
  render_frame_host_->Stop();

  // If a cross-process navigation is happening, the pending RenderFrameHost
  // should stop. This will lead to a DidFailProvisionalLoad, which will
  // properly destroy it.
  if (pending_render_frame_host_) {
    pending_render_frame_host_->Send(new FrameMsg_Stop(
        pending_render_frame_host_->GetRoutingID()));
  }

  // PlzNavigate: a loading speculative RenderFrameHost should also stop.
  if (IsBrowserSideNavigationEnabled()) {
    if (speculative_render_frame_host_ &&
        speculative_render_frame_host_->is_loading()) {
      speculative_render_frame_host_->Send(
          new FrameMsg_Stop(speculative_render_frame_host_->GetRoutingID()));
    }
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_frame_host_->render_view_host()->GetWidget()->SetIsLoading(is_loading);
  if (pending_render_frame_host_) {
    pending_render_frame_host_->render_view_host()->GetWidget()->SetIsLoading(
        is_loading);
  }
}

bool RenderFrameHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  // If we're waiting for a close ACK, then the tab should close whether there's
  // a navigation in progress or not.  Unfortunately, we also need to check for
  // cases that we arrive here with no navigation in progress, since there are
  // some tab closure paths that don't set is_waiting_for_close_ack to true.
  // TODO(creis): Clean this up in http://crbug.com/418266.
  if (!pending_render_frame_host_ ||
      render_frame_host_->render_view_host()->is_waiting_for_close_ack())
    return true;

  // We should always have a pending RFH when there's a cross-process navigation
  // in progress.  Sanity check this for http://crbug.com/276333.
  CHECK(pending_render_frame_host_);

  // Unload handlers run in the background, so we should never get an
  // unresponsiveness warning for them.
  CHECK(!render_frame_host_->IsWaitingForUnloadACK());

  // If the tab becomes unresponsive during beforeunload while doing a
  // cross-process navigation, proceed with the navigation.  (This assumes that
  // the pending RenderFrameHost is still responsive.)
  if (render_frame_host_->is_waiting_for_beforeunload_ack()) {
    // Haven't gotten around to starting the request, because we're still
    // waiting for the beforeunload handler to finish.  We'll pretend that it
    // did finish, to let the navigation proceed.  Note that there's a danger
    // that the beforeunload handler will later finish and possibly return
    // false (meaning the navigation should not proceed), but we'll ignore it
    // in this case because it took too long.
    if (pending_render_frame_host_->are_navigations_suspended()) {
      pending_render_frame_host_->SetNavigationsSuspended(
          false, base::TimeTicks::Now());
    }
  }
  return false;
}

void RenderFrameHostManager::OnBeforeUnloadACK(
    bool for_cross_site_transition,
    bool proceed,
    const base::TimeTicks& proceed_time) {
  if (for_cross_site_transition) {
    DCHECK(!IsBrowserSideNavigationEnabled());
    // Ignore if we're not in a cross-process navigation.
    if (!pending_render_frame_host_)
      return;

    if (proceed) {
      // Ok to unload the current page, so proceed with the cross-process
      // navigation.  Note that if navigations are not currently suspended, it
      // might be because the renderer was deemed unresponsive and this call was
      // already made by ShouldCloseTabOnUnresponsiveRenderer.  In that case, it
      // is ok to do nothing here.
      if (pending_render_frame_host_ &&
          pending_render_frame_host_->are_navigations_suspended()) {
        pending_render_frame_host_->SetNavigationsSuspended(false,
                                                            proceed_time);
      }
    } else {
      // Current page says to cancel.
      CancelPending();
    }
  } else {
    // Non-cross-process transition means closing the entire tab.
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed, proceed_time,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // If we're about to close the tab and there's a pending RFH, cancel it.
      // Otherwise, if the navigation in the pending RFH completes before the
      // close in the current RFH, we'll lose the tab close.
      if (pending_render_frame_host_) {
        CancelPending();
      }

      // PlzNavigate: clean up the speculative RenderFrameHost if there is one.
      if (IsBrowserSideNavigationEnabled() && speculative_render_frame_host_)
        CleanUpNavigation();

      // This is not a cross-process navigation; the tab is being closed.
      render_frame_host_->render_view_host()->ClosePage();
    }
  }
}

void RenderFrameHostManager::OnCrossSiteResponse(
    RenderFrameHostImpl* transferring_render_frame_host,
    const GlobalRequestID& global_request_id,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry) {
  // A transfer should only have come from our pending or current RFH.  If it
  // started as a cross-process navigation via OpenURL, this is the pending
  // one.  If it wasn't cross-process until the transfer, this is the current
  // one.
  //
  // Note that having a pending RFH does not imply that it was the one that
  // made the request.  Suppose that during a pending cross-site navigation,
  // the frame performs a different same-site navigation which redirects
  // cross-site.  In this case, there will be a pending RFH, but this request
  // is made by the current RFH. Later, this will create a new pending RFH and
  // clean up the old one.
  //
  // TODO(creis): We need to handle the case that the pending RFH has changed
  // in the mean time, while this was being posted from the IO thread.  We
  // should probably cancel the request in that case.
  DCHECK(transferring_render_frame_host == pending_render_frame_host_.get() ||
         transferring_render_frame_host == render_frame_host_.get());

  // Check if the FrameTreeNode is loading. This will be used later to notify
  // the FrameTreeNode that the load stop if the transfer fails.
  bool frame_tree_node_was_loading = frame_tree_node_->IsLoading();

  // Store the NavigationHandle to give it to the appropriate RenderFrameHost
  // after it started navigating.
  transfer_navigation_handle_ =
      transferring_render_frame_host->PassNavigationHandleOwnership();
  CHECK(transfer_navigation_handle_);

  // Set the transferring RenderFrameHost as not loading, so that it does not
  // emit a DidStopLoading notification if it is destroyed when creating the
  // new navigating RenderFrameHost.
  transferring_render_frame_host->set_is_loading(false);

  // Treat the last URL in the chain as the destination and the remainder as
  // the redirect chain.
  CHECK(transfer_url_chain.size());
  GURL transfer_url = transfer_url_chain.back();
  std::vector<GURL> rest_of_chain = transfer_url_chain;
  rest_of_chain.pop_back();

  // |extra_headers| passed to RequestTransferURL below are always empty for
  // now, because there are no known scenarios where headers (from POST request
  // made from one renderer) need to be forwarded into the renderer where that
  // request ends up being transfered to.  In particular, XSSAuditor doesn't
  // look at the headers (e.g. the Content-Type header) when analyzing the body
  // of the POST request.
  std::string extra_headers;

  transferring_render_frame_host->frame_tree_node()
      ->navigator()
      ->RequestTransferURL(
          transferring_render_frame_host, transfer_url, nullptr, rest_of_chain,
          referrer, page_transition, global_request_id,
          should_replace_current_entry,
          transfer_navigation_handle_->IsPost() ? "POST" : "GET",
          transfer_navigation_handle_->resource_request_body(), extra_headers);

  // If the navigation continued, the NavigationHandle should have been
  // transfered to a RenderFrameHost. In the other cases, it should be cleared.
  // If the NavigationHandle wasn't claimed, this will lead to the cancelation
  // of the request in the network stack.
  if (transfer_navigation_handle_) {
    transfer_navigation_handle_->set_is_transferring(false);
    transfer_navigation_handle_.reset();
  }

  // If the navigation in the new renderer did not start, inform the
  // FrameTreeNode that it stopped loading.
  if (!frame_tree_node_->IsLoading() && frame_tree_node_was_loading)
    frame_tree_node_->DidStopLoading();
}

void RenderFrameHostManager::DidNavigateFrame(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture) {
  CommitPendingIfNecessary(render_frame_host, was_caused_by_user_gesture);

  // Make sure any dynamic changes to this frame's sandbox flags that were made
  // prior to navigation take effect.
  CommitPendingSandboxFlags();
}

void RenderFrameHostManager::CommitPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture) {
  if (!pending_render_frame_host_ && !speculative_render_frame_host_) {
    // There's no pending/speculative RenderFrameHost so it must be that the
    // current renderer process completed a navigation.

    // We should only hear this from our current renderer.
    DCHECK_EQ(render_frame_host_.get(), render_frame_host);

    // If the current RenderFrameHost has a pending WebUI it must be committed.
    // Note: When one tries to move same-site commit logic into RenderFrameHost
    // itself, mind that the focus setting logic inside CommitPending also needs
    // to be moved there.
    if (render_frame_host_->pending_web_ui())
      CommitPendingWebUI();
    return;
  }

  if (render_frame_host == pending_render_frame_host_.get() ||
      render_frame_host == speculative_render_frame_host_.get()) {
    // A cross-process navigation completed, so show the new renderer. If a
    // same-process navigation is also ongoing, it will be canceled when the
    // pending/speculative RenderFrameHost replaces the current one in the
    // commit call below.
    CommitPending();
    if (IsBrowserSideNavigationEnabled())
      frame_tree_node_->ResetNavigationRequest(false);
  } else if (render_frame_host == render_frame_host_.get()) {
    // A same-process navigation committed while a simultaneous cross-process
    // navigation is still ongoing.

    // If the current RenderFrameHost has a pending WebUI it must be committed.
    if (render_frame_host_->pending_web_ui())
      CommitPendingWebUI();

    // A navigation in the original page has taken place.  Cancel the pending
    // one. Only do it for user gesture originated navigations to prevent page
    // doing any shenanigans to prevent user from navigating.  See
    // https://code.google.com/p/chromium/issues/detail?id=75195
    if (was_caused_by_user_gesture) {
      if (IsBrowserSideNavigationEnabled()) {
        CleanUpNavigation();
        frame_tree_node_->ResetNavigationRequest(false);
      } else {
        CancelPending();
      }
    }
  } else {
    // No one else should be sending us DidNavigate in this state.
    NOTREACHED();
  }
}

void RenderFrameHostManager::DidChangeOpener(
    int opener_routing_id,
    SiteInstance* source_site_instance) {
  FrameTreeNode* opener = nullptr;
  if (opener_routing_id != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* opener_rfhi = RenderFrameHostImpl::FromID(
        source_site_instance->GetProcess()->GetID(), opener_routing_id);
    // If |opener_rfhi| is null, the opener RFH has already disappeared.  In
    // this case, clear the opener rather than keeping the old opener around.
    if (opener_rfhi)
      opener = opener_rfhi->frame_tree_node();
  }

  if (frame_tree_node_->opener() == opener)
    return;

  frame_tree_node_->SetOpener(opener);

  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() == source_site_instance)
      continue;
    pair.second->UpdateOpener();
  }

  if (render_frame_host_->GetSiteInstance() != source_site_instance)
    render_frame_host_->UpdateOpener();

  // Notify the pending and speculative RenderFrameHosts as well.  This is
  // necessary in case a process swap has started while the message was in
  // flight.
  if (pending_render_frame_host_ &&
      pending_render_frame_host_->GetSiteInstance() != source_site_instance) {
    pending_render_frame_host_->UpdateOpener();
  }

  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->GetSiteInstance() !=
          source_site_instance) {
    speculative_render_frame_host_->UpdateOpener();
  }
}

void RenderFrameHostManager::CommitPendingSandboxFlags() {
  // Return early if there were no pending sandbox flags updates.
  if (!frame_tree_node_->CommitPendingSandboxFlags())
    return;

  // Sandbox flags updates can only happen when the frame has a parent.
  CHECK(frame_tree_node_->parent());

  // Notify all of the frame's proxies about updated sandbox flags, excluding
  // the parent process since it already knows the latest flags.
  SiteInstance* parent_site_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_site_instance) {
      pair.second->Send(new FrameMsg_DidUpdateSandboxFlags(
          pair.second->GetRoutingID(),
          frame_tree_node_->current_replication_state().sandbox_flags));
    }
  }
}

void RenderFrameHostManager::SwapOutOldFrame(
    std::unique_ptr<RenderFrameHostImpl> old_render_frame_host) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::SwapOutOldFrame",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());

  // Tell the renderer to suppress any further modal dialogs so that we can swap
  // it out.  This must be done before canceling any current dialog, in case
  // there is a loop creating additional dialogs.
  old_render_frame_host->SuppressFurtherDialogs();

  // Now close any modal dialogs that would prevent us from swapping out.  This
  // must be done separately from SwapOut, so that the ScopedPageLoadDeferrer is
  // no longer on the stack when we send the SwapOut message.
  delegate_->CancelModalDialogsForRenderManager();

  // If the old RFH is not live, just return as there is no further work to do.
  // It will be deleted and there will be no proxy created.
  if (!old_render_frame_host->IsRenderFrameLive())
    return;

  // Create a replacement proxy for the old RenderFrameHost. (There should not
  // be one yet.)  This is done even if there are no active frames besides this
  // one to simplify cleanup logic on the renderer side (see
  // https://crbug.com/568836 for motivation).
  RenderFrameProxyHost* proxy =
      CreateRenderFrameProxyHost(old_render_frame_host->GetSiteInstance(),
                                 old_render_frame_host->render_view_host());

  // Reset any NavigationHandle in the RenderFrameHost. This will prevent any
  // ongoing navigation from attempting to transfer.
  old_render_frame_host->SetNavigationHandle(nullptr);

  // Tell the old RenderFrameHost to swap out and be replaced by the proxy.
  old_render_frame_host->SwapOut(proxy, true);

  // SwapOut creates a RenderFrameProxy, so set the proxy to be initialized.
  proxy->set_render_frame_proxy_created(true);

  // |old_render_frame_host| will be deleted when its SwapOut ACK is received,
  // or when the timer times out, or when the RFHM itself is deleted (whichever
  // comes first).
  pending_delete_hosts_.push_back(std::move(old_render_frame_host));
}

void RenderFrameHostManager::DiscardUnusedFrame(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // TODO(carlosk): this code is very similar to what can be found in
  // SwapOutOldFrame and we should see that these are unified at some point.

  // If the SiteInstance for the pending RFH is being used by others, ensure
  // that it is replaced by a RenderFrameProxyHost to allow other frames to
  // communicate to this frame.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  RenderViewHostImpl* rvh = render_frame_host->render_view_host();
  RenderFrameProxyHost* proxy = nullptr;
  if (site_instance->HasSite() && site_instance->active_frame_count() > 1) {
    // Any currently suspended navigations are no longer needed.
    render_frame_host->CancelSuspendedNavigations();

    // If a proxy already exists for the |site_instance|, just reuse it instead
    // of creating a new one. There is no need to call SwapOut on the
    // |render_frame_host|, as this method is only called to discard a pending
    // or speculative RenderFrameHost, i.e. one that has never hosted an actual
    // document.
    proxy = GetRenderFrameProxyHost(site_instance);
    if (!proxy)
      proxy = CreateRenderFrameProxyHost(site_instance, rvh);
  }

  // Doing this is important in the case where the replacement proxy is created
  // above, as the RenderViewHost will continue to exist and should be
  // considered swapped out if it is ever reused.  When there's no replacement
  // proxy, this doesn't really matter, as the RenderViewHost will be destroyed
  // shortly, since |render_frame_host| is its last active frame and will be
  // deleted below.  See https://crbug.com/627400.
  if (frame_tree_node_->IsMainFrame()) {
    rvh->set_main_frame_routing_id(MSG_ROUTING_NONE);
    rvh->set_is_active(false);
    rvh->set_is_swapped_out(true);
  }

  render_frame_host.reset();

  // If a new RenderFrameProxyHost was created above, or if the old proxy isn't
  // live, create the RenderFrameProxy in the renderer, so that other frames
  // can still communicate with this frame.  See https://crbug.com/653746.
  if (proxy && !proxy->is_render_frame_proxy_live())
    proxy->InitRenderFrameProxy();
}

bool RenderFrameHostManager::DeleteFromPendingList(
    RenderFrameHostImpl* render_frame_host) {
  for (RFHPendingDeleteList::iterator iter = pending_delete_hosts_.begin();
       iter != pending_delete_hosts_.end();
       iter++) {
    if (iter->get() == render_frame_host) {
      pending_delete_hosts_.erase(iter);
      return true;
    }
  }
  return false;
}

void RenderFrameHostManager::ResetProxyHosts() {
  for (const auto& pair : proxy_hosts_) {
    static_cast<SiteInstanceImpl*>(pair.second->GetSiteInstance())
        ->RemoveObserver(this);
  }
  proxy_hosts_.clear();
}

void RenderFrameHostManager::ClearRFHsPendingShutdown() {
  pending_delete_hosts_.clear();
}

void RenderFrameHostManager::ClearWebUIInstances() {
  current_frame_host()->ClearAllWebUI();
  if (pending_render_frame_host_)
    pending_render_frame_host_->ClearAllWebUI();
  // PlzNavigate
  if (speculative_render_frame_host_)
    speculative_render_frame_host_->ClearAllWebUI();
}

// PlzNavigate
void RenderFrameHostManager::DidCreateNavigationRequest(
    NavigationRequest* request) {
  CHECK(IsBrowserSideNavigationEnabled());
  RenderFrameHostImpl* dest_rfh = GetFrameHostForNavigation(*request);
  DCHECK(dest_rfh);
  request->set_associated_site_instance_type(
      dest_rfh == render_frame_host_.get()
          ? NavigationRequest::AssociatedSiteInstanceType::CURRENT
          : NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
}

// PlzNavigate
RenderFrameHostImpl* RenderFrameHostManager::GetFrameHostForNavigation(
    const NavigationRequest& request) {
  CHECK(IsBrowserSideNavigationEnabled());

  SiteInstance* current_site_instance = render_frame_host_->GetSiteInstance();

  SiteInstance* candidate_site_instance =
      speculative_render_frame_host_
          ? speculative_render_frame_host_->GetSiteInstance()
          : nullptr;

  scoped_refptr<SiteInstance> dest_site_instance = GetSiteInstanceForNavigation(
      request.common_params().url, request.source_site_instance(),
      request.dest_site_instance(), candidate_site_instance,
      request.common_params().transition,
      request.restore_type() != RestoreType::NONE, request.is_view_source());

  // The appropriate RenderFrameHost to commit the navigation.
  RenderFrameHostImpl* navigation_rfh = nullptr;

  bool notify_webui_of_rf_creation = false;

  // Reuse the current RenderFrameHost if its SiteInstance matches the
  // navigation's.
  bool no_renderer_swap = current_site_instance == dest_site_instance.get();

  if (frame_tree_node_->IsMainFrame()) {
    // Renderer-initiated main frame navigations that may require a
    // SiteInstance swap are sent to the browser via the OpenURL IPC and are
    // afterwards treated as browser-initiated navigations. NavigationRequests
    // marked as renderer-initiated are created by receiving a BeginNavigation
    // IPC, and will then proceed in the same renderer. In site-per-process
    // mode, it is possible for renderer-intiated navigations to be allowed to
    // go cross-process. Check it first.
    bool can_renderer_initiate_transfer =
        render_frame_host_->IsRenderFrameLive() &&
        ShouldMakeNetworkRequestForURL(request.common_params().url) &&
        IsRendererTransferNeededForNavigation(render_frame_host_.get(),
                                              request.common_params().url);

    no_renderer_swap |=
        !request.browser_initiated() && !can_renderer_initiate_transfer;
  } else {
    // Subframe navigations will use the current renderer, unless specifically
    // allowed to swap processes.
    no_renderer_swap |= !CanSubframeSwapProcess(request.common_params().url,
                                                request.source_site_instance(),
                                                request.dest_site_instance());
  }

  if (no_renderer_swap) {
    // GetFrameHostForNavigation will be called more than once during a
    // navigation (currently twice, on request and when it's about to commit in
    // the renderer). In the follow up calls an existing pending WebUI should
    // not be recreated if the URL didn't change. So instead of calling
    // CleanUpNavigation just discard the speculative RenderFrameHost if one
    // exists.
    if (speculative_render_frame_host_)
      DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost());

    UpdatePendingWebUIOnCurrentFrameHost(request.common_params().url,
                                         request.bindings());

    navigation_rfh = render_frame_host_.get();

    DCHECK(!speculative_render_frame_host_);
  } else {
    // If the current RenderFrameHost cannot be used a speculative one is
    // created with the SiteInstance for the current URL. If a speculative
    // RenderFrameHost already exists we try as much as possible to reuse it and
    // its associated WebUI.

    // Check if an existing speculative RenderFrameHost can be reused.
    if (!speculative_render_frame_host_ ||
        speculative_render_frame_host_->GetSiteInstance() !=
            dest_site_instance.get()) {
      // If a previous speculative RenderFrameHost didn't exist or if its
      // SiteInstance differs from the one for the current URL, a new one needs
      // to be created.
      CleanUpNavigation();
      bool success = CreateSpeculativeRenderFrameHost(current_site_instance,
                                                      dest_site_instance.get());
      DCHECK(success);
    }
    DCHECK(speculative_render_frame_host_);

    bool changed_web_ui = speculative_render_frame_host_->UpdatePendingWebUI(
        request.common_params().url, request.bindings());
    speculative_render_frame_host_->CommitPendingWebUI();
    DCHECK_EQ(GetNavigatingWebUI(), speculative_render_frame_host_->web_ui());
    notify_webui_of_rf_creation =
        changed_web_ui && speculative_render_frame_host_->web_ui();

    navigation_rfh = speculative_render_frame_host_.get();

    // Check if our current RFH is live.
    if (!render_frame_host_->IsRenderFrameLive()) {
      // The current RFH is not live.  There's no reason to sit around with a
      // sad tab or a newly created RFH while we wait for the navigation to
      // complete. Just switch to the speculative RFH now and go back to normal.
      // (Note that we don't care about on{before}unload handlers if the current
      // RFH isn't live.)
      CommitPending();

      // Notify the WebUI about the new RenderFrame if needed (the newly
      // created WebUI has just been committed by CommitPending, so
      // GetNavigatingWebUI() below will return false).
      if (notify_webui_of_rf_creation && render_frame_host_->web_ui()) {
        render_frame_host_->web_ui()->RenderFrameCreated(
            render_frame_host_.get());
        notify_webui_of_rf_creation = false;
      }
    }
  }
  DCHECK(navigation_rfh &&
         (navigation_rfh == render_frame_host_.get() ||
          navigation_rfh == speculative_render_frame_host_.get()));

  // If the RenderFrame that needs to navigate is not live (its process was just
  // created or has crashed), initialize it.
  if (!navigation_rfh->IsRenderFrameLive()) {
    if (!ReinitializeRenderFrame(navigation_rfh))
      return nullptr;

    notify_webui_of_rf_creation = true;

    if (navigation_rfh == render_frame_host_.get()) {
      // TODO(nasko): This is a very ugly hack. The Chrome extensions process
      // manager still uses NotificationService and expects to see a
      // RenderViewHost changed notification after WebContents and
      // RenderFrameHostManager are completely initialized. This should be
      // removed once the process manager moves away from NotificationService.
      // See https://crbug.com/462682.
      delegate_->NotifyMainFrameSwappedFromRenderManager(
          nullptr, render_frame_host_->render_view_host());
    }
  }

  // If a WebUI was created in a speculative RenderFrameHost or a new
  // RenderFrame was created then the WebUI never interacted with the
  // RenderFrame or its RenderView. Notify using RenderFrameCreated.
  if (notify_webui_of_rf_creation && GetNavigatingWebUI())
    GetNavigatingWebUI()->RenderFrameCreated(navigation_rfh);

  return navigation_rfh;
}

// PlzNavigate
void RenderFrameHostManager::CleanUpNavigation() {
  CHECK(IsBrowserSideNavigationEnabled());
  if (speculative_render_frame_host_) {
    bool was_loading = speculative_render_frame_host_->is_loading();
    DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost());
    if (was_loading)
      frame_tree_node_->DidStopLoading();
  }
}

// PlzNavigate
std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetSpeculativeRenderFrameHost() {
  CHECK(IsBrowserSideNavigationEnabled());
  speculative_render_frame_host_->GetProcess()->RemovePendingView();
  return std::move(speculative_render_frame_host_);
}

void RenderFrameHostManager::OnDidStartLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidStartLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidStopLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidStopLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidUpdateName(const std::string& name,
                                             const std::string& unique_name) {
  // The window.name message may be sent outside of --site-per-process when
  // report_frame_name_changes renderer preference is set (used by
  // WebView).  Don't send the update to proxies in those cases.
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidUpdateName(pair.second->GetRoutingID(),
                                                 name, unique_name));
  }
}

void RenderFrameHostManager::OnDidAddContentSecurityPolicy(
    const ContentSecurityPolicyHeader& header) {
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_AddContentSecurityPolicy(
        pair.second->GetRoutingID(), header));
  }
}

void RenderFrameHostManager::OnDidResetContentSecurityPolicy() {
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_ResetContentSecurityPolicy(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_EnforceInsecureRequestPolicy(
        pair.second->GetRoutingID(), policy));
  }
}

void RenderFrameHostManager::OnDidUpdateFrameOwnerProperties(
    const FrameOwnerProperties& properties) {
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  // FrameOwnerProperties exist only for frames that have a parent.
  CHECK(frame_tree_node_->parent());
  SiteInstance* parent_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();

  // Notify the RenderFrame if it lives in a different process from its parent.
  if (render_frame_host_->GetSiteInstance() != parent_instance) {
    render_frame_host_->Send(new FrameMsg_SetFrameOwnerProperties(
        render_frame_host_->GetRoutingID(), properties));
  }

  // Notify this frame's proxies if they live in a different process from its
  // parent.  This is only currently needed for the allowFullscreen property,
  // since that can be queried on RemoteFrame ancestors.
  //
  // TODO(alexmos): It would be sufficient to only send this update to proxies
  // in the current FrameTree.
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_instance) {
      pair.second->Send(new FrameMsg_SetFrameOwnerProperties(
          pair.second->GetRoutingID(), properties));
    }
  }
}

void RenderFrameHostManager::OnDidUpdateOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidUpdateOrigin(pair.second->GetRoutingID(), origin,
                                     is_potentially_trustworthy_unique_origin));
  }
}

RenderFrameHostManager::SiteInstanceDescriptor::SiteInstanceDescriptor(
    BrowserContext* browser_context,
    GURL dest_url,
    SiteInstanceRelation relation_to_current)
    : existing_site_instance(nullptr), relation(relation_to_current) {
  new_site_url = SiteInstance::GetSiteForURL(browser_context, dest_url);
}

void RenderFrameHostManager::RenderProcessGone(SiteInstanceImpl* instance) {
  GetRenderFrameProxyHost(instance)->set_render_frame_proxy_created(false);
}

void RenderFrameHostManager::CancelPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host == pending_render_frame_host_.get())
    CancelPending();
  else if (render_frame_host == speculative_render_frame_host_.get()) {
    // TODO(nasko, clamy): This should just clean up the speculative RFH
    // without canceling the request.  See https://crbug.com/636119.
    frame_tree_node_->ResetNavigationRequest(false);
  }
}

void RenderFrameHostManager::ActiveFrameCountIsZero(
    SiteInstanceImpl* site_instance) {
  // |site_instance| no longer contains any active RenderFrameHosts, so we don't
  // need to maintain a proxy there anymore.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance);
  CHECK(proxy);

  DeleteRenderFrameProxyHost(site_instance);
}

RenderFrameProxyHost* RenderFrameHostManager::CreateRenderFrameProxyHost(
    SiteInstance* site_instance,
    RenderViewHostImpl* rvh) {
  int site_instance_id = site_instance->GetId();
  CHECK(proxy_hosts_.find(site_instance_id) == proxy_hosts_.end())
      << "A proxy already existed for this SiteInstance.";
  RenderFrameProxyHost* proxy_host =
      new RenderFrameProxyHost(site_instance, rvh, frame_tree_node_);
  proxy_hosts_[site_instance_id] = base::WrapUnique(proxy_host);
  static_cast<SiteInstanceImpl*>(site_instance)->AddObserver(this);
  return proxy_host;
}

void RenderFrameHostManager::DeleteRenderFrameProxyHost(
    SiteInstance* site_instance) {
  static_cast<SiteInstanceImpl*>(site_instance)->RemoveObserver(this);
  proxy_hosts_.erase(site_instance->GetId());
}

bool RenderFrameHostManager::ShouldTransitionCrossSite() {
  // The logic below is weaker than "are all sites isolated" -- it asks instead,
  // "is any site isolated". That's appropriate here since we're just trying to
  // figure out if we're in any kind of site isolated mode, and in which case,
  // we ignore the kSingleProcess and kProcessPerTab settings.
  //
  // TODO(nick): Move all handling of kSingleProcess/kProcessPerTab into
  // SiteIsolationPolicy so we have a consistent behavior around the interaction
  // of the process model flags.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return true;

  // False in the single-process mode, as it makes RVHs to accumulate
  // in swapped_out_hosts_.
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  // TODO(nick): Move handling of kSingleProcess and kProcessPerTab into
  // SiteIsolationPolicy.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kProcessPerTab);
}

bool RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const GURL& current_effective_url,
    bool current_is_view_source_mode,
    SiteInstance* new_site_instance,
    const GURL& new_effective_url,
    bool new_is_view_source_mode) const {
  // A subframe must stay in the same BrowsingInstance as its parent.
  // TODO(nasko): Ensure that SiteInstance swap is still triggered for subframes
  // in the cases covered by the rest of the checks in this method.
  if (!frame_tree_node_->IsMainFrame())
    return false;

  // If new_entry already has a SiteInstance, assume it is correct.  We only
  // need to force a swap if it is in a different BrowsingInstance.
  if (new_site_instance) {
    return !new_site_instance->IsRelatedSiteInstance(
        render_frame_host_->GetSiteInstance());
  }

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new_entry will be rendered in a new SiteInstance AND BrowsingInstance.
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();

  // Don't force a new BrowsingInstance for debug URLs that are handled in the
  // renderer process, like javascript: or chrome://crash.
  if (IsRendererDebugURL(new_effective_url))
    return false;

  // Transitions across BrowserContexts should always require a
  // BrowsingInstance swap. For example, this can happen if an extension in a
  // normal profile opens an incognito window with a web URL using
  // chrome.windows.create().
  //
  // TODO(alexmos): This check should've been enforced earlier in the
  // navigation, in chrome::Navigate().  Verify this, and then convert this to
  // a CHECK and remove the fallback.
  DCHECK_EQ(browser_context,
            render_frame_host_->GetSiteInstance()->GetBrowserContext());
  if (browser_context !=
      render_frame_host_->GetSiteInstance()->GetBrowserContext()) {
    return true;
  }

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't, or if the WebUI types differ.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          render_frame_host_->GetProcess()->GetID()) ||
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
          browser_context, current_effective_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, new_effective_url)) {
      return true;
    }

    // Force swap if the current WebUI type differs from the one for the
    // destination.
    if (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, current_effective_url) !=
        WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, new_effective_url)) {
      return true;
    }
  } else {
    // Force a swap if it's a Web UI URL.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
            browser_context, new_effective_url)) {
      return true;
    }
  }

  // Check with the content client as well.  Important to pass
  // current_effective_url here, which uses the SiteInstance's site if there is
  // no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_frame_host_->GetSiteInstance(),
          current_effective_url, new_effective_url)) {
    return true;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't treat
  // it as a new navigation). So require a BrowsingInstance switch.
  if (current_is_view_source_mode != new_is_view_source_mode)
    return true;

  return false;
}

scoped_refptr<SiteInstance>
RenderFrameHostManager::GetSiteInstanceForNavigation(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance,
    SiteInstance* candidate_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode) {
  // On renderer-initiated navigations, when the frame initiating the navigation
  // and the frame being navigated differ, |source_instance| is set to the
  // SiteInstance of the initiating frame. |dest_instance| is present on session
  // history navigations. The two cannot be set simultaneously.
  DCHECK(!source_instance || !dest_instance);

  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();

  // We do not currently swap processes for navigations in webview tag guests.
  if (current_instance->GetSiteURL().SchemeIs(kGuestScheme))
    return current_instance;

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrowsingInstance will be unable to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.
  // TODO(clamy): Remove the dependency on the current entry.
  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();
  const GURL& current_effective_url = current_entry ?
      SiteInstanceImpl::GetEffectiveURL(browser_context,
                                        current_entry->GetURL()) :
      render_frame_host_->GetSiteInstance()->GetSiteURL();
  bool current_is_view_source_mode = current_entry ?
      current_entry->IsViewSourceMode() : dest_is_view_source_mode;
  bool force_swap = ShouldSwapBrowsingInstancesForNavigation(
      current_effective_url,
      current_is_view_source_mode,
      dest_instance,
      SiteInstanceImpl::GetEffectiveURL(browser_context, dest_url),
      dest_is_view_source_mode);
  SiteInstanceDescriptor new_instance_descriptor =
      SiteInstanceDescriptor(current_instance);
  if (ShouldTransitionCrossSite() || force_swap) {
    new_instance_descriptor = DetermineSiteInstanceForURL(
        dest_url, source_instance, current_instance, dest_instance, transition,
        dest_is_restore, dest_is_view_source_mode, force_swap);
  }

  scoped_refptr<SiteInstance> new_instance =
      ConvertToSiteInstance(new_instance_descriptor, candidate_instance);
  // If |force_swap| is true, we must use a different SiteInstance than the
  // current one. If we didn't, we would have two RenderFrameHosts in the same
  // SiteInstance and the same frame, breaking lookup of RenderFrameHosts by
  // SiteInstance.
  if (force_swap)
    CHECK_NE(new_instance, current_instance);

  // Double-check that the new SiteInstance is associated with the right
  // BrowserContext.
  DCHECK_EQ(new_instance->GetBrowserContext(), browser_context);

  return new_instance;
}

RenderFrameHostManager::SiteInstanceDescriptor
RenderFrameHostManager::DetermineSiteInstanceForURL(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* current_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    bool force_browsing_instance_swap) {
  SiteInstanceImpl* current_instance_impl =
      static_cast<SiteInstanceImpl*>(current_instance);
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  BrowserContext* browser_context = controller.GetBrowserContext();

  // If the entry has an instance already we should use it.
  if (dest_instance) {
    // If we are forcing a swap, this should be in a different BrowsingInstance.
    if (force_browsing_instance_swap) {
      CHECK(!dest_instance->IsRelatedSiteInstance(
                render_frame_host_->GetSiteInstance()));
    }
    return SiteInstanceDescriptor(dest_instance);
  }

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (force_browsing_instance_swap)
    return SiteInstanceDescriptor(browser_context, dest_url,
                                  SiteInstanceRelation::UNRELATED);

  // (UGLY) HEURISTIC, process-per-site only:
  //
  // If this navigation is generated, then it probably corresponds to a search
  // query.  Given that search results typically lead to users navigating to
  // other sites, we don't really want to use the search engine hostname to
  // determine the site instance for this navigation.
  //
  // NOTE: This can be removed once we have a way to transition between
  //       RenderViews in response to a link click.
  //
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessPerSite) &&
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    return SiteInstanceDescriptor(current_instance_impl);
  }

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!current_instance_impl->HasSite()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the current_instance has a site, because for now, we
    // want to compare against the current URL and not the SiteInstance's site.
    // In this case, there is no current URL, so comparing against the site is
    // ok.  See additional comments below.)
    //
    // Also, if the URL should use process-per-site mode and there is an
    // existing process for the site, we should use it.  We can call
    // GetRelatedSiteInstance() for this, which will eagerly set the site and
    // thus use the correct process.
    bool use_process_per_site =
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, dest_url) &&
        RenderProcessHostImpl::GetProcessHostForSite(browser_context, dest_url);
    if (current_instance_impl->HasRelatedSiteInstance(dest_url) ||
        use_process_per_site) {
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::RELATED);
    }

    // For extensions, Web UI URLs (such as the new tab page), and apps we do
    // not want to use the |current_instance_impl| if it has no site, since it
    // will have a RenderProcessHost of PRIV_NORMAL. Create a new SiteInstance
    // for this URL instead (with the correct process type).
    if (current_instance_impl->HasWrongProcessForURL(dest_url))
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::RELATED);

    // View-source URLs must use a new SiteInstance and BrowsingInstance.
    // TODO(nasko): This is the same condition as later in the function. This
    // should be taken into account when refactoring this method as part of
    // http://crbug.com/123007.
    if (dest_is_view_source_mode)
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::UNRELATED);

    // If we are navigating from a blank SiteInstance to a WebUI, make sure we
    // create a new SiteInstance.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, dest_url)) {
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::UNRELATED);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    //
    // The embedder can request some urls never to be assigned to SiteInstance
    // through the ShouldAssignSiteForURL() content client method, so that
    // renderers created for particular chrome urls (e.g. the chrome-native://
    // scheme) can be reused for subsequent navigations in the same WebContents.
    // See http://crbug.com/386542.
    if (dest_is_restore &&
        GetContentClient()->browser()->ShouldAssignSiteForURL(dest_url)) {
      current_instance_impl->SetSite(dest_url);
    }

    return SiteInstanceDescriptor(current_instance_impl);
  }

  // Otherwise, only create a new SiteInstance for a cross-process navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = current_instance_impl->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* current_entry = controller.GetLastCommittedEntry();
  if (interstitial_page_) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    current_entry = controller.GetEntryAtOffset(-1);
  }

  // View-source URLs must use a new SiteInstance and BrowsingInstance.
  // We don't need a swap when going from view-source to a debug URL like
  // chrome://crash, however.
  // TODO(creis): Refactor this method so this duplicated code isn't needed.
  // See http://crbug.com/123007.
  if (current_entry &&
      current_entry->IsViewSourceMode() != dest_is_view_source_mode &&
      !IsRendererDebugURL(dest_url)) {
    return SiteInstanceDescriptor(browser_context, dest_url,
                                  SiteInstanceRelation::UNRELATED);
  }

  // Use the source SiteInstance in case of data URLs or about:blank pages,
  // because the content is then controlled and/or scriptable by the source
  // SiteInstance.
  GURL about_blank(url::kAboutBlankURL);
  if (source_instance &&
      (dest_url == about_blank || dest_url.scheme() == url::kDataScheme)) {
    return SiteInstanceDescriptor(source_instance);
  }

  // Use the current SiteInstance for same site navigations.
  if (IsCurrentlySameSite(render_frame_host_.get(), dest_url))
    return SiteInstanceDescriptor(render_frame_host_->GetSiteInstance());

  if (SiteIsolationPolicy::IsTopDocumentIsolationEnabled()) {
    // TODO(nick): Looking at the main frame and openers is required for TDI
    // mode, but should be safe to enable unconditionally.
    if (!frame_tree_node_->IsMainFrame()) {
      RenderFrameHostImpl* main_frame =
          frame_tree_node_->frame_tree()->root()->current_frame_host();
      if (IsCurrentlySameSite(main_frame, dest_url))
        return SiteInstanceDescriptor(main_frame->GetSiteInstance());
    }

    if (frame_tree_node_->opener()) {
      RenderFrameHostImpl* opener_frame =
          frame_tree_node_->opener()->current_frame_host();
      if (IsCurrentlySameSite(opener_frame, dest_url))
        return SiteInstanceDescriptor(opener_frame->GetSiteInstance());
    }
  }

  if (!frame_tree_node_->IsMainFrame() &&
      SiteIsolationPolicy::IsTopDocumentIsolationEnabled() &&
      !SiteInstanceImpl::DoesSiteRequireDedicatedProcess(browser_context,
                                                         dest_url)) {
    if (GetContentClient()
            ->browser()
            ->ShouldFrameShareParentSiteInstanceDespiteTopDocumentIsolation(
                dest_url, current_instance)) {
      return SiteInstanceDescriptor(render_frame_host_->GetSiteInstance());
    }

    // This is a cross-site subframe of a non-isolated origin, so place this
    // frame in the default subframe site instance.
    return SiteInstanceDescriptor(
        browser_context, dest_url,
        SiteInstanceRelation::RELATED_DEFAULT_SUBFRAME);
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance.
  return SiteInstanceDescriptor(browser_context, dest_url,
                                SiteInstanceRelation::RELATED);
}

bool RenderFrameHostManager::IsRendererTransferNeededForNavigation(
    RenderFrameHostImpl* rfh,
    const GURL& dest_url) {
  // A transfer is not needed if the current SiteInstance doesn't yet have a
  // site.  This is the case for tests that use NavigateToURL.
  if (!rfh->GetSiteInstance()->HasSite())
    return false;

  // We do not currently swap processes for navigations in webview tag guests.
  if (rfh->GetSiteInstance()->GetSiteURL().SchemeIs(kGuestScheme))
    return false;

  // Don't swap processes for extensions embedded in DevTools. See
  // https://crbug.com/564216.
  if (rfh->GetSiteInstance()->GetSiteURL().SchemeIs(kChromeDevToolsScheme)) {
    // TODO(nick): https://crbug.com/570483 Check to see if |dest_url| is a
    // devtools extension, and swap processes if not.
    return false;
  }

  BrowserContext* context = rfh->GetSiteInstance()->GetBrowserContext();
  // TODO(nasko, nick): These following --site-per-process checks are
  // overly simplistic. Update them to match all the cases
  // considered by DetermineSiteInstanceForURL.
  if (IsCurrentlySameSite(rfh, dest_url)) {
    // The same site, no transition needed for security purposes, and we must
    // keep the same SiteInstance for correctness of synchronous scripting.
    return false;
  }

  // The sites differ. If either one requires a dedicated process,
  // then a transfer is needed.
  if (rfh->GetSiteInstance()->RequiresDedicatedProcess() ||
      SiteInstanceImpl::DoesSiteRequireDedicatedProcess(context,
                                                        dest_url)) {
    return true;
  }

  if (SiteIsolationPolicy::IsTopDocumentIsolationEnabled() &&
      (!frame_tree_node_->IsMainFrame() ||
       rfh->GetSiteInstance()->IsDefaultSubframeSiteInstance())) {
    // Always attempt a transfer in these cases.
    return true;
  }

  return false;
}

scoped_refptr<SiteInstance> RenderFrameHostManager::ConvertToSiteInstance(
    const SiteInstanceDescriptor& descriptor,
    SiteInstance* candidate_instance) {
  SiteInstanceImpl* current_instance = render_frame_host_->GetSiteInstance();

  // Note: If the |candidate_instance| matches the descriptor, it will already
  // be set to |descriptor.existing_site_instance|.
  if (descriptor.existing_site_instance)
    return descriptor.existing_site_instance;

  // Note: If the |candidate_instance| matches the descriptor,
  // GetRelatedSiteInstance will return it.
  if (descriptor.relation == SiteInstanceRelation::RELATED)
    return current_instance->GetRelatedSiteInstance(descriptor.new_site_url);

  if (descriptor.relation == SiteInstanceRelation::RELATED_DEFAULT_SUBFRAME)
    return current_instance->GetDefaultSubframeSiteInstance();

  // At this point we know an unrelated site instance must be returned. First
  // check if the candidate matches.
  if (candidate_instance &&
      !current_instance->IsRelatedSiteInstance(candidate_instance) &&
      candidate_instance->GetSiteURL() == descriptor.new_site_url) {
    return candidate_instance;
  }

  // Otherwise return a newly created one.
  return SiteInstance::CreateForURL(
      delegate_->GetControllerForRenderManager().GetBrowserContext(),
      descriptor.new_site_url);
}

bool RenderFrameHostManager::IsCurrentlySameSite(RenderFrameHostImpl* candidate,
                                                 const GURL& dest_url) {
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();

  // If the process type is incorrect, reject the candidate even if |dest_url|
  // is same-site.  (The URL may have been installed as an app since
  // the last time we visited it.)
  if (candidate->GetSiteInstance()->HasWrongProcessForURL(dest_url))
    return false;

  // If we don't have a last successful URL, we can't trust the origin or URL
  // stored on the frame, so we fall back to GetSiteURL(). This case occurs
  // after commits of net errors, since net errors do not currently swap
  // processes for transfer navigations. Note: browser-initiated net errors do
  // swap processes, but the frame's last successful URL will still be empty in
  // that case.
  if (candidate->last_successful_url().is_empty()) {
    // TODO(creis): GetSiteURL() is not 100% accurate. Eliminate this fallback.
    return SiteInstance::IsSameWebSite(
        browser_context, candidate->GetSiteInstance()->GetSiteURL(), dest_url);
  }

  // In the common case, we use the RenderFrameHost's last successful URL. Thus,
  // we compare against the last successful commit when deciding whether to swap
  // this time.
  if (SiteInstance::IsSameWebSite(browser_context,
                                  candidate->last_successful_url(), dest_url)) {
    return true;
  }

  // It is possible that last_successful_url() was a nonstandard scheme (for
  // example, "about:blank"). If so, examine the replicated origin to determine
  // the site.
  if (!candidate->GetLastCommittedOrigin().unique() &&
      SiteInstance::IsSameWebSite(
          browser_context,
          GURL(candidate->GetLastCommittedOrigin().Serialize()), dest_url)) {
    return true;
  }

  // Not same-site.
  return false;
}

void RenderFrameHostManager::CreatePendingRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance) {
  if (pending_render_frame_host_)
    CancelPending();

  // The process for the new SiteInstance may (if we're sharing a process with
  // another host that already initialized it) or may not (we have our own
  // process or the existing process crashed) have been initialized. Calling
  // Init multiple times will be ignored, so this is safe.
  if (!new_instance->GetProcess()->Init())
    return;

  CreateProxiesForNewRenderFrameHost(old_instance, new_instance);

  // Create a non-swapped-out RFH with the given opener.
  pending_render_frame_host_ =
      CreateRenderFrame(new_instance, delegate_->IsHidden(), nullptr);
}

void RenderFrameHostManager::CreateProxiesForNewRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance) {
  // Only create opener proxies if they are in the same BrowsingInstance.
  if (new_instance->IsRelatedSiteInstance(old_instance)) {
    CreateOpenerProxies(new_instance, frame_tree_node_);
  } else if (SiteIsolationPolicy::AreCrossProcessFramesPossible()) {
    // Ensure that the frame tree has RenderFrameProxyHosts for the
    // new SiteInstance in all nodes except the current one.  We do this for
    // all frames in the tree, whether they are in the same BrowsingInstance or
    // not.  If |new_instance| is in the same BrowsingInstance as
    // |old_instance|, this will be done as part of CreateOpenerProxies above;
    // otherwise, we do this here.  We will still check whether two frames are
    // in the same BrowsingInstance before we allow them to interact (e.g.,
    // postMessage).
    frame_tree_node_->frame_tree()->CreateProxiesForSiteInstance(
        frame_tree_node_, new_instance);
  }
}

void RenderFrameHostManager::CreateProxiesForNewNamedFrame() {
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  DCHECK(!frame_tree_node_->frame_name().empty());

  // If this is a top-level frame, create proxies for this node in the
  // SiteInstances of its opener's ancestors, which are allowed to discover
  // this frame by name (see https://crbug.com/511474 and part 4 of
  // https://html.spec.whatwg.org/#the-rules-for-choosing-a-browsing-context-
  // given-a-browsing-context-name).
  FrameTreeNode* opener = frame_tree_node_->opener();
  if (!opener || !frame_tree_node_->IsMainFrame())
    return;
  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();

  // Start from opener's parent.  There's no need to create a proxy in the
  // opener's SiteInstance, since new windows are always first opened in the
  // same SiteInstance as their opener, and if the new window navigates
  // cross-site, that proxy would be created as part of swapping out.
  for (FrameTreeNode* ancestor = opener->parent(); ancestor;
       ancestor = ancestor->parent()) {
    RenderFrameHostImpl* ancestor_rfh = ancestor->current_frame_host();
    if (ancestor_rfh->GetSiteInstance() != current_instance)
      CreateRenderFrameProxy(ancestor_rfh->GetSiteInstance());
  }
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::CreateRenderFrameHost(
    SiteInstance* site_instance,
    int32_t view_routing_id,
    int32_t frame_routing_id,
    int32_t widget_routing_id,
    bool hidden,
    bool renderer_initiated_creation) {
  if (frame_routing_id == MSG_ROUTING_NONE)
    frame_routing_id = site_instance->GetProcess()->GetNextRoutingID();

  // Create a RVH for main frames, or find the existing one for subframes.
  FrameTree* frame_tree = frame_tree_node_->frame_tree();
  RenderViewHostImpl* render_view_host = nullptr;
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host = frame_tree->CreateRenderViewHost(
        site_instance, view_routing_id, frame_routing_id, false, hidden);
    // TODO(avi): It's a bit bizarre that this logic lives here instead of in
    // CreateRenderFrame(). It turns out that FrameTree::CreateRenderViewHost
    // doesn't /always/ create a new RenderViewHost. It first tries to find an
    // already existing one to reuse by a SiteInstance lookup. If it finds one,
    // then the supplied routing IDs are completely ignored.
    // CreateRenderFrame() could do this lookup too, but it seems redundant to
    // do this lookup in two places. This is a good yak shave to clean up, or,
    // if just ignored, should be an easy cleanup once RenderViewHostImpl has-a
    // RenderWidgetHostImpl. https://crbug.com/545684
    if (view_routing_id == MSG_ROUTING_NONE) {
      widget_routing_id = render_view_host->GetRoutingID();
    } else {
      DCHECK_EQ(view_routing_id, render_view_host->GetRoutingID());
    }
  } else {
    render_view_host = frame_tree->GetRenderViewHost(site_instance);
    CHECK(render_view_host);
  }

  return RenderFrameHostFactory::Create(
      site_instance, render_view_host, render_frame_delegate_,
      render_widget_delegate_, frame_tree, frame_tree_node_, frame_routing_id,
      widget_routing_id, hidden, renderer_initiated_creation);
}

// PlzNavigate
bool RenderFrameHostManager::CreateSpeculativeRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance) {
  CHECK(new_instance);
  CHECK_NE(old_instance, new_instance);

  // The process for the new SiteInstance may (if we're sharing a process with
  // another host that already initialized it) or may not (we have our own
  // process or the existing process crashed) have been initialized. Calling
  // Init multiple times will be ignored, so this is safe.
  if (!new_instance->GetProcess()->Init())
    return false;

  CreateProxiesForNewRenderFrameHost(old_instance, new_instance);

  speculative_render_frame_host_ =
      CreateRenderFrame(new_instance, delegate_->IsHidden(), nullptr);

  return !!speculative_render_frame_host_;
}

std::unique_ptr<RenderFrameHostImpl> RenderFrameHostManager::CreateRenderFrame(
    SiteInstance* instance,
    bool hidden,
    int* view_routing_id_ptr) {
  int32_t widget_routing_id = MSG_ROUTING_NONE;
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);

  CHECK(instance);
  CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible() ||
        frame_tree_node_->IsMainFrame());

  std::unique_ptr<RenderFrameHostImpl> new_render_frame_host;
  bool success = true;
  if (view_routing_id_ptr)
    *view_routing_id_ptr = MSG_ROUTING_NONE;

  // We are creating a pending, speculative or swapped out RFH here. We should
  // never create it in the same SiteInstance as our current RFH.
  CHECK_NE(render_frame_host_->GetSiteInstance(), instance);

  // A RenderFrame in a different process from its parent RenderFrame
  // requires a RenderWidget for input/layout/painting.
  if (frame_tree_node_->parent() &&
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance() !=
          instance) {
    CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible());
    widget_routing_id = instance->GetProcess()->GetNextRoutingID();
  }

  new_render_frame_host = CreateRenderFrameHost(
      instance, MSG_ROUTING_NONE, MSG_ROUTING_NONE, widget_routing_id, hidden,
      false);
  RenderViewHostImpl* render_view_host =
      new_render_frame_host->render_view_host();

  // Prevent the process from exiting while we're trying to navigate in it.
  new_render_frame_host->GetProcess()->AddPendingView();

  if (frame_tree_node_->IsMainFrame()) {
    success = InitRenderView(render_view_host, proxy);

    // If we are reusing the RenderViewHost and it doesn't already have a
    // RenderWidgetHostView, we need to create one if this is the main frame.
    if (!render_view_host->GetWidget()->GetView())
      delegate_->CreateRenderWidgetHostViewForRenderManager(render_view_host);
  } else {
    DCHECK(render_view_host->IsRenderViewLive());
  }

  if (success) {
    if (frame_tree_node_->IsMainFrame()) {
      // Don't show the main frame's view until we get a DidNavigate from it.
      // Only the RenderViewHost for the top-level RenderFrameHost has a
      // RenderWidgetHostView; RenderWidgetHosts for out-of-process iframes
      // will be created later and hidden.
      if (render_view_host->GetWidget()->GetView())
        render_view_host->GetWidget()->GetView()->Hide();
    }
    // RenderViewHost for |instance| might exist prior to calling
    // CreateRenderFrame. In such a case, InitRenderView will not create the
    // RenderFrame in the renderer process and it needs to be done
    // explicitly.
    DCHECK(new_render_frame_host);
    success = InitRenderFrame(new_render_frame_host.get());
  }

  if (success) {
    if (view_routing_id_ptr)
      *view_routing_id_ptr = render_view_host->GetRoutingID();
  }

  // Return the new RenderFrameHost on successful creation.
  if (success) {
    DCHECK(new_render_frame_host->GetSiteInstance() == instance);
    return new_render_frame_host;
  }
  return nullptr;
}

int RenderFrameHostManager::CreateRenderFrameProxy(SiteInstance* instance) {
  // A RenderFrameProxyHost should never be created in the same SiteInstance as
  // the current RFH.
  CHECK(instance);
  CHECK_NE(instance, render_frame_host_->GetSiteInstance());

  RenderViewHostImpl* render_view_host = nullptr;

  // Ensure a RenderViewHost exists for |instance|, as it creates the page
  // level structure in Blink.
  render_view_host =
      frame_tree_node_->frame_tree()->GetRenderViewHost(instance);
  if (!render_view_host) {
    CHECK(frame_tree_node_->IsMainFrame());
    render_view_host = frame_tree_node_->frame_tree()->CreateRenderViewHost(
        instance, MSG_ROUTING_NONE, MSG_ROUTING_NONE, true,
        delegate_->IsHidden());
  }

  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy && proxy->is_render_frame_proxy_live())
    return proxy->GetRoutingID();

  if (!proxy)
    proxy = CreateRenderFrameProxyHost(instance, render_view_host);

  if (frame_tree_node_->IsMainFrame()) {
    InitRenderView(render_view_host, proxy);
  } else {
    proxy->InitRenderFrameProxy();
  }

  return proxy->GetRoutingID();
}

void RenderFrameHostManager::CreateProxiesForChildFrame(FrameTreeNode* child) {
  RenderFrameProxyHost* outer_delegate_proxy =
      ForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  for (const auto& pair : proxy_hosts_) {
    // Do not create proxies for subframes in the outer delegate's process,
    // since the outer delegate does not need to interact with them.
    if (pair.second.get() == outer_delegate_proxy)
      continue;

    child->render_manager()->CreateRenderFrameProxy(
        pair.second->GetSiteInstance());
  }
}

void RenderFrameHostManager::EnsureRenderViewInitialized(
    RenderViewHostImpl* render_view_host,
    SiteInstance* instance) {
  DCHECK(frame_tree_node_->IsMainFrame());

  if (render_view_host->IsRenderViewLive())
    return;

  // If the proxy in |instance| doesn't exist, this RenderView is not swapped
  // out and shouldn't be reinitialized here.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (!proxy)
    return;

  InitRenderView(render_view_host, proxy);
}

void RenderFrameHostManager::CreateOuterDelegateProxy(
    SiteInstance* outer_contents_site_instance,
    RenderFrameHostImpl* render_frame_host) {
  // We only get here when Delegate for this manager is an inner delegate and is
  // based on cross process frames.
  RenderFrameProxyHost* proxy =
      CreateRenderFrameProxyHost(outer_contents_site_instance, nullptr);

  // Swap the outer WebContents's frame with the proxy to inner WebContents.
  //
  // We are in the outer WebContents, and its FrameTree would never see
  // a load start for any of its inner WebContents. Eventually, that also makes
  // the FrameTree never see the matching load stop. Therefore, we always pass
  // false to |is_loading| below.
  // TODO(lazyboy): This |is_loading| behavior might not be what we want,
  // investigate and fix.
  render_frame_host->Send(new FrameMsg_SwapOut(
      render_frame_host->GetRoutingID(), proxy->GetRoutingID(),
      false /* is_loading */,
      render_frame_host->frame_tree_node()->current_replication_state()));
  proxy->set_render_frame_proxy_created(true);

  // There is no longer a RenderFrame associated with this RenderFrameHost.
  render_frame_host->SetRenderFrameCreated(false);
}

void RenderFrameHostManager::SetRWHViewForInnerContents(
    RenderWidgetHostView* child_rwhv) {
  DCHECK(ForInnerDelegate() && frame_tree_node_->IsMainFrame());
  GetProxyToOuterDelegate()->SetChildRWHView(child_rwhv);
}

bool RenderFrameHostManager::InitRenderView(
    RenderViewHostImpl* render_view_host,
    RenderFrameProxyHost* proxy) {
  // Ensure the renderer process is initialized before creating the
  // RenderView.
  if (!render_view_host->GetProcess()->Init())
    return false;

  // We may have initialized this RenderViewHost for another RenderFrameHost.
  if (render_view_host->IsRenderViewLive())
    return true;

  int opener_frame_routing_id =
      GetOpenerRoutingID(render_view_host->GetSiteInstance());

  bool created = delegate_->CreateRenderViewForRenderManager(
      render_view_host, opener_frame_routing_id,
      proxy ? proxy->GetRoutingID() : MSG_ROUTING_NONE,
      frame_tree_node_->current_replication_state());

  if (created && proxy)
    proxy->set_render_frame_proxy_created(true);

  return created;
}

bool RenderFrameHostManager::InitRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host->IsRenderFrameLive())
    return true;

  SiteInstance* site_instance = render_frame_host->GetSiteInstance();

  int opener_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->opener())
    opener_routing_id = GetOpenerRoutingID(site_instance);

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    parent_routing_id = frame_tree_node_->parent()
                            ->render_manager()
                            ->GetRoutingIdForSiteInstance(site_instance);
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  // At this point, all RenderFrameProxies for sibling frames have already been
  // created, including any proxies that come after this frame.  To preserve
  // correct order for indexed window access (e.g., window.frames[1]), pass the
  // previous sibling frame so that this frame is correctly inserted into the
  // frame tree on the renderer side.
  int previous_sibling_routing_id = MSG_ROUTING_NONE;
  FrameTreeNode* previous_sibling = frame_tree_node_->PreviousSibling();
  if (previous_sibling) {
    previous_sibling_routing_id =
        previous_sibling->render_manager()->GetRoutingIdForSiteInstance(
            site_instance);
    CHECK_NE(previous_sibling_routing_id, MSG_ROUTING_NONE);
  }

  // Check whether there is an existing proxy for this frame in this
  // SiteInstance. If there is, the new RenderFrame needs to be able to find
  // the proxy it is replacing, so that it can fully initialize itself.
  // NOTE: This is the only time that a RenderFrameProxyHost can be in the same
  // SiteInstance as its RenderFrameHost. This is only the case until the
  // RenderFrameHost commits, at which point it will replace and delete the
  // RenderFrameProxyHost.
  int proxy_routing_id = MSG_ROUTING_NONE;
  RenderFrameProxyHost* existing_proxy = GetRenderFrameProxyHost(site_instance);
  if (existing_proxy) {
    proxy_routing_id = existing_proxy->GetRoutingID();
    CHECK_NE(proxy_routing_id, MSG_ROUTING_NONE);
    if (!existing_proxy->is_render_frame_proxy_live()) {
      // Calling InitRenderFrameProxy on main frames seems to be causing
      // https://crbug.com/575245, so track down how this can happen.
      // TODO(creis): Remove this once we've found the cause.
      if (!frame_tree_node_->parent()) {
        base::debug::SetCrashKeyValue(
            "initrf_frame_id",
            base::IntToString(render_frame_host->GetRoutingID()));
        base::debug::SetCrashKeyValue("initrf_proxy_id",
                                      base::IntToString(proxy_routing_id));
        base::debug::SetCrashKeyValue(
            "initrf_view_id",
            base::IntToString(
                render_frame_host->render_view_host()->GetRoutingID()));
        base::debug::SetCrashKeyValue(
            "initrf_main_frame_id",
            base::IntToString(render_frame_host->render_view_host()
                                  ->main_frame_routing_id()));
        base::debug::SetCrashKeyValue(
            "initrf_view_is_live",
            render_frame_host->render_view_host()->IsRenderViewLive() ? "yes"
                                                                      : "no");
        base::debug::DumpWithoutCrashing();
      }

      existing_proxy->InitRenderFrameProxy();
    }
  }

  // TODO(alexmos): These crash keys are temporary to track down
  // https://crbug.com/591478. Verify that the parent routing ID
  // points to a live RenderFrameProxy when this is not a remote-to-local
  // navigation (i.e., when there's no |existing_proxy|).
  if (!existing_proxy && frame_tree_node_->parent()) {
    RenderFrameProxyHost* parent_proxy = RenderFrameProxyHost::FromID(
        render_frame_host->GetProcess()->GetID(), parent_routing_id);
    if (!parent_proxy || !parent_proxy->is_render_frame_proxy_live()) {
      base::debug::SetCrashKeyValue("initrf_parent_proxy_exists",
                                    parent_proxy ? "yes" : "no");

      SiteInstance* parent_instance =
          frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();
      base::debug::SetCrashKeyValue(
          "initrf_parent_is_in_same_site_instance",
          site_instance == parent_instance ? "yes" : "no");
      base::debug::SetCrashKeyValue("initrf_parent_process_is_live",
                                    frame_tree_node_->parent()
                                            ->current_frame_host()
                                            ->GetProcess()
                                            ->HasConnection()
                                        ? "yes"
                                        : "no");
      base::debug::SetCrashKeyValue(
          "initrf_render_view_is_live",
          render_frame_host->render_view_host()->IsRenderViewLive() ? "yes"
                                                                    : "no");

      // Collect some additional information for root's proxy if it's different
      // from the parent.
      FrameTreeNode* root = frame_tree_node_->frame_tree()->root();
      if (root != frame_tree_node_->parent()) {
        SiteInstance* root_instance =
            root->current_frame_host()->GetSiteInstance();
        base::debug::SetCrashKeyValue(
            "initrf_root_is_in_same_site_instance",
            site_instance == root_instance ? "yes" : "no");
        base::debug::SetCrashKeyValue(
            "initrf_root_is_in_same_site_instance_as_parent",
            parent_instance == root_instance ? "yes" : "no");
        base::debug::SetCrashKeyValue("initrf_root_process_is_live",
                                      frame_tree_node_->frame_tree()
                                              ->root()
                                              ->current_frame_host()
                                              ->GetProcess()
                                              ->HasConnection()
                                          ? "yes"
                                          : "no");

        RenderFrameProxyHost* top_proxy =
            root->render_manager()->GetRenderFrameProxyHost(site_instance);
        if (top_proxy) {
          base::debug::SetCrashKeyValue(
              "initrf_root_proxy_is_live",
              top_proxy->is_render_frame_proxy_live() ? "yes" : "no");
        }
      }

      base::debug::DumpWithoutCrashing();
    }
  }

  return delegate_->CreateRenderFrameForRenderManager(
      render_frame_host, proxy_routing_id, opener_routing_id, parent_routing_id,
      previous_sibling_routing_id);
}

bool RenderFrameHostManager::ReinitializeRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  // This should be used only when the RenderFrame is not live.
  DCHECK(!render_frame_host->IsRenderFrameLive());

  // Recreate the opener chain.
  CreateOpenerProxies(render_frame_host->GetSiteInstance(), frame_tree_node_);

  // Main frames need both the RenderView and RenderFrame reinitialized, so
  // use InitRenderView.  For cross-process subframes, InitRenderView won't
  // recreate the RenderFrame, so use InitRenderFrame instead.  Note that for
  // subframe RenderFrameHosts, the swapped out RenderView in their
  // SiteInstance will be recreated as part of CreateOpenerProxies above.
  if (!frame_tree_node_->parent()) {
    DCHECK(!GetRenderFrameProxyHost(render_frame_host->GetSiteInstance()));
    if (!InitRenderView(render_frame_host->render_view_host(), nullptr))
      return false;
  } else {
    if (!InitRenderFrame(render_frame_host))
      return false;

    // When a subframe renderer dies, its RenderWidgetHostView is cleared in
    // its CrossProcessFrameConnector, so we need to restore it now that it
    // is re-initialized.
    RenderFrameProxyHost* proxy_to_parent = GetProxyToParent();
    if (proxy_to_parent)
      GetProxyToParent()->SetChildRWHView(render_frame_host->GetView());
  }

  DCHECK(render_frame_host->IsRenderFrameLive());
  return true;
}

int RenderFrameHostManager::GetRoutingIdForSiteInstance(
    SiteInstance* site_instance) {
  if (render_frame_host_->GetSiteInstance() == site_instance)
    return render_frame_host_->GetRoutingID();

  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance);
  if (proxy)
    return proxy->GetRoutingID();

  return MSG_ROUTING_NONE;
}

void RenderFrameHostManager::CommitPendingWebUI() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPendingWebUI",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  DCHECK(render_frame_host_->pending_web_ui());

  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared the pending WebUI the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  render_frame_host_->CommitPendingWebUI();

  if (will_focus_location_bar)
    delegate_->SetFocusToLocationBar(false);
}

void RenderFrameHostManager::CommitPending() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  DCHECK(pending_render_frame_host_ || speculative_render_frame_host_);

  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared the pending WebUI the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
                           render_frame_host_->GetView() &&
                           render_frame_host_->GetView()->HasFocus();

  bool is_main_frame = frame_tree_node_->IsMainFrame();

  // While the old frame is still current, remove its children from the tree.
  frame_tree_node_->ResetForNewProcess();

  // Swap in the pending or speculative frame and make it active. Also ensure
  // the FrameTree stays in sync.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host;
  if (!IsBrowserSideNavigationEnabled()) {
    DCHECK(!speculative_render_frame_host_);
    old_render_frame_host =
        SetRenderFrameHost(std::move(pending_render_frame_host_));
  } else {
    // PlzNavigate
    DCHECK(speculative_render_frame_host_);
    old_render_frame_host =
        SetRenderFrameHost(std::move(speculative_render_frame_host_));
  }

  // The process will no longer try to exit, so we can decrement the count.
  render_frame_host_->GetProcess()->RemovePendingView();

  // Show the new view (or a sad tab) if necessary.
  bool new_rfh_has_view = !!render_frame_host_->GetView();
  if (!delegate_->IsHidden() && new_rfh_has_view) {
    // In most cases, we need to show the new view.
    render_frame_host_->GetView()->Show();
  }
  if (!new_rfh_has_view) {
    // If the view is gone, then this RenderViewHost died while it was hidden.
    // We ignored the RenderProcessGone call at the time, so we should send it
    // now to make sure the sad tab shows up, etc.
    DCHECK(!render_frame_host_->IsRenderFrameLive());
    DCHECK(!render_frame_host_->render_view_host()->IsRenderViewLive());
    render_frame_host_->ResetLoadingState();
    delegate_->RenderProcessGoneFromRenderManager(
        render_frame_host_->render_view_host());
  }

  // For top-level frames, also hide the old RenderViewHost's view.
  // TODO(creis): As long as show/hide are on RVH, we don't want to hide on
  // subframe navigations or we will interfere with the top-level frame.
  if (is_main_frame &&
      old_render_frame_host->render_view_host()->GetWidget()->GetView()) {
    old_render_frame_host->render_view_host()->GetWidget()->GetView()->Hide();
  }

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (will_focus_location_bar) {
    delegate_->SetFocusToLocationBar(false);
  } else if (focus_render_view && render_frame_host_->GetView()) {
    if (is_main_frame) {
      render_frame_host_->GetView()->Focus();
    } else {
      // The main frame's view is already focused, but we need to set
      // page-level focus in the subframe's renderer.
      frame_tree_node_->frame_tree()->SetPageFocus(
          render_frame_host_->GetSiteInstance(), true);
    }
  }

  // Notify that we've swapped RenderFrameHosts. We do this before shutting down
  // the RFH so that we can clean up RendererResources related to the RFH first.
  delegate_->NotifySwappedFromRenderManager(
      old_render_frame_host.get(), render_frame_host_.get(), is_main_frame);

  // The RenderViewHost keeps track of the main RenderFrameHost routing id.
  // If this is committing a main frame navigation, update it and set the
  // routing id in the RenderViewHost associated with the old RenderFrameHost
  // to MSG_ROUTING_NONE.
  if (is_main_frame) {
    RenderViewHostImpl* rvh = render_frame_host_->render_view_host();
    rvh->set_main_frame_routing_id(render_frame_host_->routing_id());

    // If the RenderViewHost is transitioning from swapped out to active state,
    // it was reused, so dispatch a RenderViewReady event.  For example, this
    // is necessary to hide the sad tab if one is currently displayed.  See
    // https://crbug.com/591984.
    //
    // TODO(alexmos):  Remove this and move RenderViewReady consumers to use
    // the main frame's RenderFrameCreated instead.
    if (!rvh->is_active())
      rvh->PostRenderViewReady();

    rvh->set_is_active(true);
    rvh->set_is_swapped_out(false);
    old_render_frame_host->render_view_host()->set_main_frame_routing_id(
        MSG_ROUTING_NONE);
  }

  // Swap out the old frame now that the new one is visible.
  // This will swap it out and schedule it for deletion when the swap out ack
  // arrives (or immediately if the process isn't live).
  SwapOutOldFrame(std::move(old_render_frame_host));

  // Since the new RenderFrameHost is now committed, there must be no proxies
  // for its SiteInstance. Delete any existing ones.
  DeleteRenderFrameProxyHost(render_frame_host_->GetSiteInstance());

  // If this is a subframe, it should have a CrossProcessFrameConnector
  // created already.  Use it to link the new RFH's view to the proxy that
  // belongs to the parent frame's SiteInstance. If this navigation causes
  // an out-of-process frame to return to the same process as its parent, the
  // proxy would have been removed from proxy_hosts_ above.
  // Note: We do this after swapping out the old RFH because that may create
  // the proxy we're looking for.
  RenderFrameProxyHost* proxy_to_parent = GetProxyToParent();
  if (proxy_to_parent) {
    CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible());
    proxy_to_parent->SetChildRWHView(render_frame_host_->GetView());
  }

  // After all is done, there must never be a proxy in the list which has the
  // same SiteInstance as the current RenderFrameHost.
  CHECK(!GetRenderFrameProxyHost(render_frame_host_->GetSiteInstance()));
}

RenderFrameHostImpl* RenderFrameHostManager::UpdateStateForNavigate(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    const GlobalRequestID& transferred_request_id,
    int bindings,
    bool is_reload) {
  if (!frame_tree_node_->IsMainFrame() &&
      !CanSubframeSwapProcess(dest_url, source_instance, dest_instance)) {
    // Note: Do not add code here to determine whether the subframe should swap
    // or not. Add it to CanSubframeSwapProcess instead.
    return render_frame_host_.get();
  }

  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();
  scoped_refptr<SiteInstance> new_instance = GetSiteInstanceForNavigation(
      dest_url, source_instance, dest_instance, nullptr, transition,
      dest_is_restore, dest_is_view_source_mode);

  // Inform the transferring NavigationHandle of a transfer to a different
  // SiteInstance.  It is important do so now, in order to mark the request as
  // transferring on the IO thread before attempting to destroy the pending RFH.
  // This ensures the network request will not be destroyed along the pending
  // RFH but will persist until it is picked up by the new RFH.
  if (transfer_navigation_handle_.get() &&
      transfer_navigation_handle_->GetGlobalRequestID() ==
          transferred_request_id &&
      new_instance.get() !=
          transfer_navigation_handle_->GetRenderFrameHost()
              ->GetSiteInstance()) {
    transfer_navigation_handle_->Transfer();
  }

  // If we are currently navigating cross-process to a pending RFH for a
  // different SiteInstance, we want to get back to normal and then navigate as
  // usual.  We will reuse the pending RFH below if it matches the destination
  // SiteInstance.
  if (pending_render_frame_host_) {
    if (pending_render_frame_host_->GetSiteInstance() != new_instance) {
      CancelPending();
    } else {
      // When a pending RFH is reused, it should always be live, since it is
      // cleared whenever a process dies.
      CHECK(pending_render_frame_host_->IsRenderFrameLive());
    }
  }

  if (new_instance.get() != current_instance) {
    TRACE_EVENT_INSTANT2(
        "navigation",
        "RenderFrameHostManager::UpdateStateForNavigate:New SiteInstance",
        TRACE_EVENT_SCOPE_THREAD,
        "current_instance id", current_instance->GetId(),
        "new_instance id", new_instance->GetId());

    // New SiteInstance: create a pending RFH to navigate.

    if (!pending_render_frame_host_)
      CreatePendingRenderFrameHost(current_instance, new_instance.get());
    DCHECK(pending_render_frame_host_);
    if (!pending_render_frame_host_)
      return nullptr;
    DCHECK_EQ(new_instance, pending_render_frame_host_->GetSiteInstance());

    pending_render_frame_host_->UpdatePendingWebUI(dest_url, bindings);
    pending_render_frame_host_->CommitPendingWebUI();
    DCHECK_EQ(GetNavigatingWebUI(), pending_render_frame_host_->web_ui());

    // If a WebUI exists in the pending RenderFrameHost it was just created, as
    // well as the RenderFrame, and they never interacted. So notify the WebUI
    // using RenderFrameCreated.
    if (pending_render_frame_host_->web_ui()) {
      pending_render_frame_host_->web_ui()->RenderFrameCreated(
          pending_render_frame_host_.get());
    }

    // Check if our current RFH is live before we set up a transition.
    if (!render_frame_host_->IsRenderFrameLive()) {
      // The current RFH is not live.  There's no reason to sit around with a
      // sad tab or a newly created RFH while we wait for the pending RFH to
      // navigate.  Just switch to the pending RFH now and go back to normal.
      // (Note that we don't care about on{before}unload handlers if the current
      // RFH isn't live.)
      CommitPending();
      return render_frame_host_.get();
    }
    // Otherwise, it's safe to treat this as a pending cross-process transition.

    bool is_transfer = transferred_request_id != GlobalRequestID();
    if (is_transfer) {
      // We don't need to stop the old renderer or run beforeunload/unload
      // handlers, because those have already been done.
      DCHECK(transfer_navigation_handle_ &&
             transfer_navigation_handle_->GetGlobalRequestID() ==
                 transferred_request_id);
    } else if (!pending_render_frame_host_->are_navigations_suspended()) {
      // If the pending RFH hasn't already been suspended from a previous
      // attempt to navigate it, then we need to wait for the beforeunload
      // handler to run.  Suspend navigations in the pending RFH until we hear
      // back from the old RFH's beforeunload handler (via OnBeforeUnloadACK or
      // a timeout).  If the handler returns false, we'll have to cancel the
      // request.
      //
      // Also make sure the old RenderFrame stops, in case a load is in
      // progress.  (We don't want to do this for transfers, since it will
      // interrupt the transfer with an unexpected DidStopLoading.)
      render_frame_host_->Send(new FrameMsg_Stop(
          render_frame_host_->GetRoutingID()));
      pending_render_frame_host_->SetNavigationsSuspended(true,
                                                          base::TimeTicks());
      render_frame_host_->DispatchBeforeUnload(true, is_reload);
    }

    return pending_render_frame_host_.get();
  }

  // Otherwise the same SiteInstance can be used.  Navigate render_frame_host_.

  // It's possible to swap out the current RFH and then decide to navigate in it
  // anyway (e.g., a cross-process navigation that redirects back to the
  // original site).  In that case, we have a proxy for the current RFH but
  // haven't deleted it yet.  The new navigation will swap it back in, so we can
  // delete the proxy.
  DeleteRenderFrameProxyHost(new_instance.get());

  UpdatePendingWebUIOnCurrentFrameHost(dest_url, bindings);

  // The renderer can exit view source mode when any error or cancellation
  // happen. We must overwrite to recover the mode.
  if (dest_is_view_source_mode) {
    DCHECK(!render_frame_host_->GetParent());
    render_frame_host_->Send(
        new FrameMsg_EnableViewSourceMode(render_frame_host_->GetRoutingID()));
  }

  return render_frame_host_.get();
}

void RenderFrameHostManager::UpdatePendingWebUIOnCurrentFrameHost(
    const GURL& dest_url,
    int entry_bindings) {
  bool pending_webui_changed =
      render_frame_host_->UpdatePendingWebUI(dest_url, entry_bindings);
  DCHECK_EQ(GetNavigatingWebUI(), render_frame_host_->pending_web_ui());

  if (render_frame_host_->pending_web_ui() && pending_webui_changed &&
      render_frame_host_->IsRenderFrameLive()) {
    // If a pending WebUI exists in the current RenderFrameHost and it has been
    // updated and the associated RenderFrame is alive, notify the WebUI about
    // the RenderFrame.
    // Note: If the RenderFrame is not alive at this point the notification
    // will happen later, when the RenderFrame is created.
    if (render_frame_host_->pending_web_ui() == render_frame_host_->web_ui()) {
      // If the active WebUI is being reused it has already interacting with
      // this RenderFrame and its RenderView in the past, so call
      // RenderFrameReused.
      render_frame_host_->pending_web_ui()->RenderFrameReused(
          render_frame_host_.get());
    } else {
      // If this is a new WebUI it has never interacted with the existing
      // RenderFrame so call RenderFrameCreated.
      render_frame_host_->pending_web_ui()->RenderFrameCreated(
          render_frame_host_.get());
    }
  }
}

void RenderFrameHostManager::CancelPending() {
  CHECK(pending_render_frame_host_);
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CancelPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  render_frame_host_->ClearPendingWebUI();

  bool pending_was_loading = pending_render_frame_host_->is_loading();
  DiscardUnusedFrame(UnsetPendingRenderFrameHost());
  if (pending_was_loading)
    frame_tree_node_->DidStopLoading();
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetPendingRenderFrameHost() {
  std::unique_ptr<RenderFrameHostImpl> pending_render_frame_host =
      std::move(pending_render_frame_host_);

  RenderFrameDevToolsAgentHost::OnCancelPendingNavigation(
      pending_render_frame_host.get(),
      render_frame_host_.get());

  // We no longer need to prevent the process from exiting.
  pending_render_frame_host->GetProcess()->RemovePendingView();

  return pending_render_frame_host;
}

std::unique_ptr<RenderFrameHostImpl> RenderFrameHostManager::SetRenderFrameHost(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // Swap the two.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host =
      std::move(render_frame_host_);
  render_frame_host_ = std::move(render_frame_host);

  if (frame_tree_node_->IsMainFrame()) {
    // Update the count of top-level frames using this SiteInstance.  All
    // subframes are in the same BrowsingInstance as the main frame, so we only
    // count top-level ones.  This makes the value easier for consumers to
    // interpret.
    if (render_frame_host_) {
      render_frame_host_->GetSiteInstance()->
          IncrementRelatedActiveContentsCount();
    }
    if (old_render_frame_host) {
      old_render_frame_host->GetSiteInstance()->
          DecrementRelatedActiveContentsCount();
    }
  }

  return old_render_frame_host;
}

RenderViewHostImpl* RenderFrameHostManager::GetSwappedOutRenderViewHost(
    SiteInstance* instance) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy)
    return proxy->GetRenderViewHost();
  return nullptr;
}

RenderFrameProxyHost* RenderFrameHostManager::GetRenderFrameProxyHost(
    SiteInstance* instance) const {
  auto it = proxy_hosts_.find(instance->GetId());
  if (it != proxy_hosts_.end())
    return it->second.get();
  return nullptr;
}

int RenderFrameHostManager::GetProxyCount() {
  return proxy_hosts_.size();
}

void RenderFrameHostManager::CollectOpenerFrameTrees(
    std::vector<FrameTree*>* opener_frame_trees,
    base::hash_set<FrameTreeNode*>* nodes_with_back_links) {
  CHECK(opener_frame_trees);
  opener_frame_trees->push_back(frame_tree_node_->frame_tree());

  // Add the FrameTree of the given node's opener to the list of
  // |opener_frame_trees| if it doesn't exist there already. |visited_index|
  // indicates which FrameTrees in |opener_frame_trees| have already been
  // visited (i.e., those at indices less than |visited_index|).
  // |nodes_with_back_links| collects FrameTreeNodes with openers in FrameTrees
  // that have already been visited (such as those with cycles).
  size_t visited_index = 0;
  while (visited_index < opener_frame_trees->size()) {
    FrameTree* frame_tree = (*opener_frame_trees)[visited_index];
    visited_index++;
    for (FrameTreeNode* node : frame_tree->Nodes()) {
      if (!node->opener())
        continue;

      FrameTree* opener_tree = node->opener()->frame_tree();
      const auto& existing_tree_it = std::find(
          opener_frame_trees->begin(), opener_frame_trees->end(), opener_tree);

      if (existing_tree_it == opener_frame_trees->end()) {
        // This is a new opener tree that we will need to process.
        opener_frame_trees->push_back(opener_tree);
      } else {
        // If this tree is already on our processing list *and* we have visited
        // it,
        // then this node's opener is a back link.  This means the node will
        // need
        // special treatment to process its opener.
        size_t position =
            std::distance(opener_frame_trees->begin(), existing_tree_it);
        if (position < visited_index)
          nodes_with_back_links->insert(node);
      }
    }
  }
}

void RenderFrameHostManager::CreateOpenerProxies(
    SiteInstance* instance,
    FrameTreeNode* skip_this_node) {
  std::vector<FrameTree*> opener_frame_trees;
  base::hash_set<FrameTreeNode*> nodes_with_back_links;

  CollectOpenerFrameTrees(&opener_frame_trees, &nodes_with_back_links);

  // Create opener proxies for frame trees, processing furthest openers from
  // this node first and this node last.  In the common case without cycles,
  // this will ensure that each tree's openers are created before the tree's
  // nodes need to reference them.
  for (int i = opener_frame_trees.size() - 1; i >= 0; i--) {
    opener_frame_trees[i]
        ->root()
        ->render_manager()
        ->CreateOpenerProxiesForFrameTree(instance, skip_this_node);
  }

  // Set openers for nodes in |nodes_with_back_links| in a second pass.
  // The proxies created at these FrameTreeNodes in
  // CreateOpenerProxiesForFrameTree won't have their opener routing ID
  // available when created due to cycles or back links in the opener chain.
  // They must have their openers updated as a separate step after proxy
  // creation.
  for (auto* node : nodes_with_back_links) {
    RenderFrameProxyHost* proxy =
        node->render_manager()->GetRenderFrameProxyHost(instance);
    // If there is no proxy, the cycle may involve nodes in the same process,
    // or, if this is a subframe, --site-per-process may be off.  Either way,
    // there's nothing more to do.
    if (!proxy)
      continue;

    int opener_routing_id =
        node->render_manager()->GetOpenerRoutingID(instance);
    DCHECK_NE(opener_routing_id, MSG_ROUTING_NONE);
    proxy->Send(new FrameMsg_UpdateOpener(proxy->GetRoutingID(),
                                          opener_routing_id));
  }
}

void RenderFrameHostManager::CreateOpenerProxiesForFrameTree(
    SiteInstance* instance,
    FrameTreeNode* skip_this_node) {
  // Currently, this function is only called on main frames.  It should
  // actually work correctly for subframes as well, so if that need ever
  // arises, it should be sufficient to remove this DCHECK.
  DCHECK(frame_tree_node_->IsMainFrame());

  if (frame_tree_node_ == skip_this_node)
    return;

  FrameTree* frame_tree = frame_tree_node_->frame_tree();
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible()) {
    // Ensure that all the nodes in the opener's FrameTree have
    // RenderFrameProxyHosts for the new SiteInstance.  Only pass the node to
    // be skipped if it's in the same FrameTree.
    if (skip_this_node && skip_this_node->frame_tree() != frame_tree)
      skip_this_node = nullptr;
    frame_tree->CreateProxiesForSiteInstance(skip_this_node, instance);
  } else {
    // If any of the RenderViewHosts (current, pending, or swapped out) for this
    // FrameTree has the same SiteInstance, then we can return early and reuse
    // them.  An exception is if we find a pending RenderViewHost: in this case,
    // we should still create a proxy, which will allow communicating with the
    // opener until the pending RenderView commits, or if the pending navigation
    // is canceled.
    // PlzNavigate: similarly, if a speculative RenderViewHost  is present, a
    // proxy should be created.
    RenderViewHostImpl* rvh = frame_tree->GetRenderViewHost(instance);
    bool need_proxy_for_pending_rvh = (rvh == pending_render_view_host());
    bool need_proxy_for_speculative_rvh =
        IsBrowserSideNavigationEnabled() && speculative_render_frame_host_ &&
        speculative_render_frame_host_->GetRenderViewHost() == rvh;
    if (rvh && rvh->IsRenderViewLive() && !need_proxy_for_pending_rvh &&
        !need_proxy_for_speculative_rvh) {
      return;
    }

    if (rvh && !rvh->IsRenderViewLive()) {
      EnsureRenderViewInitialized(rvh, instance);
    } else {
      // Create a RenderFrameProxyHost in the given SiteInstance if none
      // exists. Since an opener can point to a subframe, do this on the root
      // frame of the current opener's frame tree.
      frame_tree->root()->render_manager()->CreateRenderFrameProxy(instance);
    }
  }
}

int RenderFrameHostManager::GetOpenerRoutingID(SiteInstance* instance) {
  if (!frame_tree_node_->opener())
    return MSG_ROUTING_NONE;

  return frame_tree_node_->opener()
      ->render_manager()
      ->GetRoutingIdForSiteInstance(instance);
}

void RenderFrameHostManager::SendPageMessage(IPC::Message* msg,
                                             SiteInstance* instance_to_skip) {
  DCHECK(IPC_MESSAGE_CLASS(*msg) == PageMsgStart);

  // We should always deliver page messages through the main frame.
  DCHECK(!frame_tree_node_->parent());

  if ((IPC_MESSAGE_CLASS(*msg) != PageMsgStart) || frame_tree_node_->parent()) {
    delete msg;
    return;
  }

  auto send_msg = [instance_to_skip](IPC::Sender* sender,
                                     int routing_id,
                                     IPC::Message* msg,
                                     SiteInstance* sender_instance) {
    if (sender_instance == instance_to_skip)
      return;

    IPC::Message* copy = new IPC::Message(*msg);
    copy->set_routing_id(routing_id);
    sender->Send(copy);
  };

  // When sending a PageMessage for an inner WebContents, we don't want to also
  // send it to the outer WebContent's frame as well.
  RenderFrameProxyHost* outer_delegate_proxy =
      ForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  for (const auto& pair : proxy_hosts_) {
    if (outer_delegate_proxy != pair.second.get()) {
      send_msg(pair.second.get(), pair.second->GetRoutingID(), msg,
               pair.second->GetSiteInstance());
    }
  }

  if (speculative_render_frame_host_) {
    send_msg(speculative_render_frame_host_.get(),
             speculative_render_frame_host_->GetRoutingID(), msg,
             speculative_render_frame_host_->GetSiteInstance());
  } else if (pending_render_frame_host_) {
    send_msg(pending_render_frame_host_.get(),
             pending_render_frame_host_->GetRoutingID(), msg,
             pending_render_frame_host_->GetSiteInstance());
  }

  if (render_frame_host_->GetSiteInstance() != instance_to_skip) {
    // Send directly instead of using send_msg() so that |msg| doesn't leak.
    msg->set_routing_id(render_frame_host_->GetRoutingID());
    render_frame_host_->Send(msg);
  } else {
    delete msg;
  }
}

bool RenderFrameHostManager::CanSubframeSwapProcess(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance) {
  // On renderer-initiated navigations, when the frame initiating the navigation
  // and the frame being navigated differ, |source_instance| is set to the
  // SiteInstance of the initiating frame. |dest_instance| is present on session
  // history navigations. The two cannot be set simultaneously.
  DCHECK(!source_instance || !dest_instance);

  // Don't swap for subframes unless we are in an OOPIF-enabled mode.  We can
  // get here in tests for subframes (e.g., NavigateFrameToURL).
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return false;

  // If dest_url is a unique origin like about:blank, then the need for a swap
  // is determined by the source_instance or dest_instance.
  GURL resolved_url = dest_url;
  if (url::Origin(resolved_url).unique()) {
    if (source_instance) {
      resolved_url = source_instance->GetSiteURL();
    } else if (dest_instance) {
      resolved_url = dest_instance->GetSiteURL();
    } else {
      // If there is no SiteInstance this unique origin can be associated with,
      // then we should avoid a process swap.
      return false;
    }
  }

  // If we are in an OOPIF mode that only applies to some sites, only swap if
  // the policy determines that a transfer would have been needed.  We can get
  // here for session restore.
  if (!IsRendererTransferNeededForNavigation(render_frame_host_.get(),
                                             resolved_url)) {
    DCHECK(!dest_instance ||
           dest_instance == render_frame_host_->GetSiteInstance());
    return false;
  }

  return true;
}

}  // namespace content
