// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

RenderFrameHostManager::PendingNavigationParams::PendingNavigationParams(
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    Referrer referrer,
    PageTransition page_transition,
    int render_frame_id,
    bool should_replace_current_entry)
    : global_request_id(global_request_id),
      cross_site_transferring_request(cross_site_transferring_request.Pass()),
      transfer_url_chain(transfer_url_chain),
      referrer(referrer),
      page_transition(page_transition),
      render_frame_id(render_frame_id),
      should_replace_current_entry(should_replace_current_entry) {
}

RenderFrameHostManager::PendingNavigationParams::~PendingNavigationParams() {}

bool RenderFrameHostManager::ClearRFHsPendingShutdown(FrameTreeNode* node) {
  node->render_manager()->pending_delete_hosts_.clear();
  return true;
}

RenderFrameHostManager::RenderFrameHostManager(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    Delegate* delegate)
    : frame_tree_node_(frame_tree_node),
      delegate_(delegate),
      cross_navigation_pending_(false),
      render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      interstitial_page_(NULL),
      weak_factory_(this) {
  DCHECK(frame_tree_node_);
}

RenderFrameHostManager::~RenderFrameHostManager() {
  if (pending_render_frame_host_)
    CancelPending();

  // We should always have a current RenderFrameHost except in some tests.
  SetRenderFrameHost(scoped_ptr<RenderFrameHostImpl>());

  // Delete any swapped out RenderFrameHosts.
  STLDeleteValues(&proxy_hosts_);
}

void RenderFrameHostManager::Init(BrowserContext* browser_context,
                                  SiteInstance* site_instance,
                                  int view_routing_id,
                                  int frame_routing_id) {
  // Create a RenderViewHost and RenderFrameHost, once we have an instance.  It
  // is important to immediately give this SiteInstance to a RenderViewHost so
  // that the SiteInstance is ref counted.
  if (!site_instance)
    site_instance = SiteInstance::Create(browser_context);

  SetRenderFrameHost(CreateRenderFrameHost(site_instance,
                                           view_routing_id,
                                           frame_routing_id,
                                           false,
                                           delegate_->IsHidden()));

  // Keep track of renderer processes as they start to shut down or are
  // crashed/killed.
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSING,
                 NotificationService::AllSources());
}

RenderViewHostImpl* RenderFrameHostManager::current_host() const {
  if (!render_frame_host_)
    return NULL;
  return render_frame_host_->render_view_host();
}

RenderViewHostImpl* RenderFrameHostManager::pending_render_view_host() const {
  if (!pending_render_frame_host_)
    return NULL;
  return pending_render_frame_host_->render_view_host();
}

RenderWidgetHostView* RenderFrameHostManager::GetRenderWidgetHostView() const {
  if (interstitial_page_)
    return interstitial_page_->GetView();
  if (!render_frame_host_)
    return NULL;
  return render_frame_host_->render_view_host()->GetView();
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToParent() {
  if (frame_tree_node_->IsMainFrame())
    return NULL;

  RenderFrameProxyHostMap::iterator iter =
      proxy_hosts_.find(frame_tree_node_->parent()
                            ->render_manager()
                            ->current_frame_host()
                            ->GetSiteInstance()
                            ->GetId());
  if (iter == proxy_hosts_.end())
    return NULL;

  return iter->second;
}

void RenderFrameHostManager::SetPendingWebUI(const NavigationEntryImpl& entry) {
  pending_web_ui_.reset(
      delegate_->CreateWebUIForRenderManager(entry.GetURL()));
  pending_and_current_web_ui_.reset();

  // If we have assigned (zero or more) bindings to this NavigationEntry in the
  // past, make sure we're not granting it different bindings than it had
  // before.  If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (pending_web_ui_.get() &&
      entry.bindings() != NavigationEntryImpl::kInvalidBindings &&
      pending_web_ui_->GetBindings() != entry.bindings()) {
    RecordAction(
        base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    pending_web_ui_.reset();
  }
}

RenderFrameHostImpl* RenderFrameHostManager::Navigate(
    const NavigationEntryImpl& entry) {
  TRACE_EVENT0("browser", "RenderFrameHostManager:Navigate");
  // Create a pending RenderFrameHost to use for the navigation.
  RenderFrameHostImpl* dest_render_frame_host = UpdateStateForNavigate(entry);
  if (!dest_render_frame_host)
    return NULL;  // We weren't able to create a pending render frame host.

  // If the current render_frame_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_frame_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_frame_host != render_frame_host_ &&
      !render_frame_host_->render_view_host()->IsRenderViewLive()) {
    // Note: we don't call InitRenderView here because we are navigating away
    // soon anyway, and we don't have the NavigationEntry for this host.
    delegate_->CreateRenderViewForRenderManager(
        render_frame_host_->render_view_host(), MSG_ROUTING_NONE,
        MSG_ROUTING_NONE, frame_tree_node_->IsMainFrame());
  }

  // If the renderer crashed, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_frame_host->render_view_host()->IsRenderViewLive()) {
    // Recreate the opener chain.
    int opener_route_id = delegate_->CreateOpenerRenderViewsForRenderManager(
        dest_render_frame_host->GetSiteInstance());
    if (!InitRenderView(dest_render_frame_host->render_view_host(),
                        opener_route_id,
                        MSG_ROUTING_NONE,
                        frame_tree_node_->IsMainFrame()))
      return NULL;

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_frame_host != render_frame_host_ &&
        dest_render_frame_host->render_view_host()->GetView()) {
      dest_render_frame_host->render_view_host()->GetView()->Hide();
    } else if (frame_tree_node_->IsMainFrame()) {
      // This is our primary renderer, notify here as we won't be calling
      // CommitPending (which does the notify).  We only do this for top-level
      // frames.
      delegate_->NotifySwappedFromRenderManager(
          NULL, render_frame_host_->render_view_host());
    }
  }

  // If entry includes the request ID of a request that is being transferred,
  // the destination render frame will take ownership, so release ownership of
  // the request.
  if (pending_nav_params_ &&
      pending_nav_params_->global_request_id ==
          entry.transferred_global_request_id()) {
    pending_nav_params_->cross_site_transferring_request->ReleaseRequest();
  }

  return dest_render_frame_host;
}

void RenderFrameHostManager::Stop() {
  render_frame_host_->render_view_host()->Stop();

  // If we are cross-navigating, we should stop the pending renderers.  This
  // will lead to a DidFailProvisionalLoad, which will properly destroy them.
  if (cross_navigation_pending_) {
    pending_render_frame_host_->render_view_host()->Send(new ViewMsg_Stop(
        pending_render_frame_host_->render_view_host()->GetRoutingID()));
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_frame_host_->render_view_host()->SetIsLoading(is_loading);
  if (pending_render_frame_host_)
    pending_render_frame_host_->render_view_host()->SetIsLoading(is_loading);
}

bool RenderFrameHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  if (!cross_navigation_pending_)
    return true;

  // We should always have a pending RFH when there's a cross-process navigation
  // in progress.  Sanity check this for http://crbug.com/276333.
  CHECK(pending_render_frame_host_);

  // If the tab becomes unresponsive during {before}unload while doing a
  // cross-site navigation, proceed with the navigation.  (This assumes that
  // the pending RenderFrameHost is still responsive.)
  if (render_frame_host_->render_view_host()->IsWaitingForUnloadACK()) {
    // The request has been started and paused while we're waiting for the
    // unload handler to finish.  We'll pretend that it did.  The pending
    // renderer will then be swapped in as part of the usual DidNavigate logic.
    // (If the unload handler later finishes, this call will be ignored because
    // the pending_nav_params_ state will already be cleaned up.)
    current_host()->OnSwappedOut(true);
  } else if (render_frame_host_->render_view_host()->
                 is_waiting_for_beforeunload_ack()) {
    // Haven't gotten around to starting the request, because we're still
    // waiting for the beforeunload handler to finish.  We'll pretend that it
    // did finish, to let the navigation proceed.  Note that there's a danger
    // that the beforeunload handler will later finish and possibly return
    // false (meaning the navigation should not proceed), but we'll ignore it
    // in this case because it took too long.
    if (pending_render_frame_host_->render_view_host()->
            are_navigations_suspended()) {
      pending_render_frame_host_->render_view_host()->SetNavigationsSuspended(
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
    // Ignore if we're not in a cross-site navigation.
    if (!cross_navigation_pending_)
      return;

    if (proceed) {
      // Ok to unload the current page, so proceed with the cross-site
      // navigation.  Note that if navigations are not currently suspended, it
      // might be because the renderer was deemed unresponsive and this call was
      // already made by ShouldCloseTabOnUnresponsiveRenderer.  In that case, it
      // is ok to do nothing here.
      if (pending_render_frame_host_ &&
          pending_render_frame_host_->render_view_host()->
              are_navigations_suspended()) {
        pending_render_frame_host_->render_view_host()->
            SetNavigationsSuspended(false, proceed_time);
      }
    } else {
      // Current page says to cancel.
      CancelPending();
      cross_navigation_pending_ = false;
    }
  } else {
    // Non-cross site transition means closing the entire tab.
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed, proceed_time,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // If we're about to close the tab and there's a pending RFH, cancel it.
      // Otherwise, if the navigation in the pending RFH completes before the
      // close in the current RFH, we'll lose the tab close.
      if (pending_render_frame_host_) {
        CancelPending();
        cross_navigation_pending_ = false;
      }

      // This is not a cross-site navigation, the tab is being closed.
      render_frame_host_->render_view_host()->ClosePage();
    }
  }
}

void RenderFrameHostManager::OnCrossSiteResponse(
    RenderFrameHostImpl* pending_render_frame_host,
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    PageTransition page_transition,
    bool should_replace_current_entry) {
  // This should be called either when the pending RFH is ready to commit or
  // when we realize that the current RFH's request requires a transfer.
  DCHECK(pending_render_frame_host == pending_render_frame_host_ ||
         pending_render_frame_host == render_frame_host_);

  // TODO(creis): Eventually we will want to check all navigation responses
  // here, but currently we pass information for a transfer if
  // ShouldSwapProcessesForRedirect returned true in the network stack.
  // In that case, we should set up a transfer after the unload handler runs.
  // If |cross_site_transferring_request| is NULL, we will just run the unload
  // handler and resume.
  pending_nav_params_.reset(new PendingNavigationParams(
      global_request_id, cross_site_transferring_request.Pass(),
      transfer_url_chain, referrer, page_transition,
      pending_render_frame_host->GetRoutingID(),
      should_replace_current_entry));

  // Run the unload handler of the current page.
  SwapOutOldPage();
}

void RenderFrameHostManager::SwappedOut(
    RenderFrameHostImpl* render_frame_host) {
  // Make sure this is from our current RFH, and that we have a pending
  // navigation from OnCrossSiteResponse.  (There may be no pending navigation
  // for data URLs that don't make network requests, for example.)   If not,
  // just return early and ignore.
  if (render_frame_host != render_frame_host_ || !pending_nav_params_.get()) {
    pending_nav_params_.reset();
    return;
  }

  // Now that the unload handler has run, we need to either initiate the
  // pending transfer (if there is one) or resume the paused response (if not).
  // TODO(creis): The blank swapped out page is visible during this time, but
  // we can shorten this by delivering the response directly, rather than
  // forcing an identical request to be made.
  if (pending_nav_params_->cross_site_transferring_request) {
    // Sanity check that the params are for the correct frame and process.
    // These should match the RenderFrameHost that made the request.
    // If it started as a cross-process navigation via OpenURL, this is the
    // pending one.  If it wasn't cross-process until the transfer, this is the
    // current one.
    int render_frame_id = pending_render_frame_host_ ?
        pending_render_frame_host_->GetRoutingID() :
        render_frame_host_->GetRoutingID();
    DCHECK_EQ(render_frame_id, pending_nav_params_->render_frame_id);
    int process_id = pending_render_frame_host_ ?
        pending_render_frame_host_->GetProcess()->GetID() :
        render_frame_host_->GetProcess()->GetID();
    DCHECK_EQ(process_id, pending_nav_params_->global_request_id.child_id);

    // Treat the last URL in the chain as the destination and the remainder as
    // the redirect chain.
    CHECK(pending_nav_params_->transfer_url_chain.size());
    GURL transfer_url = pending_nav_params_->transfer_url_chain.back();
    pending_nav_params_->transfer_url_chain.pop_back();

    // We don't know whether the original request had |user_action| set to true.
    // However, since we force the navigation to be in the current tab, it
    // doesn't matter.
    render_frame_host->frame_tree_node()->navigator()->RequestTransferURL(
        render_frame_host,
        transfer_url,
        pending_nav_params_->transfer_url_chain,
        pending_nav_params_->referrer,
        pending_nav_params_->page_transition,
        CURRENT_TAB,
        pending_nav_params_->global_request_id,
        pending_nav_params_->should_replace_current_entry,
        true);
  } else if (pending_render_frame_host_) {
    RenderProcessHostImpl* pending_process =
        static_cast<RenderProcessHostImpl*>(
            pending_render_frame_host_->GetProcess());
    pending_process->ResumeDeferredNavigation(
        pending_nav_params_->global_request_id);
  }
  pending_nav_params_.reset();
}

void RenderFrameHostManager::DidNavigateFrame(
    RenderFrameHostImpl* render_frame_host) {
  if (!cross_navigation_pending_) {
    DCHECK(!pending_render_frame_host_);

    // We should only hear this from our current renderer.
    DCHECK_EQ(render_frame_host_, render_frame_host);

    // Even when there is no pending RVH, there may be a pending Web UI.
    if (pending_web_ui())
      CommitPending();
    return;
  }

  if (render_frame_host == pending_render_frame_host_) {
    // The pending cross-site navigation completed, so show the renderer.
    // If it committed without sending network requests (e.g., data URLs),
    // then we still need to swap out the old RFH first and run its unload
    // handler, only if it hasn't happened yet.  OK for that to happen in the
    // background.
    if (pending_render_frame_host_->render_view_host()->
            HasPendingCrossSiteRequest() &&
        pending_render_frame_host_->render_view_host()->rvh_state() ==
            RenderViewHostImpl::STATE_DEFAULT) {
      SwapOutOldPage();
    }

    CommitPending();
    cross_navigation_pending_ = false;
  } else if (render_frame_host == render_frame_host_) {
    // A navigation in the original page has taken place.  Cancel the pending
    // one.
    CancelPending();
    cross_navigation_pending_ = false;
  } else {
    // No one else should be sending us DidNavigate in this state.
    DCHECK(false);
  }
}

// TODO(creis): Take in RenderFrameHost instead, since frames can have openers.
void RenderFrameHostManager::DidDisownOpener(RenderViewHost* render_view_host) {
  // Notify all swapped out hosts, including the pending RVH.
  for (RenderFrameProxyHostMap::iterator iter = proxy_hosts_.begin();
       iter != proxy_hosts_.end();
       ++iter) {
    DCHECK_NE(iter->second->GetSiteInstance(),
              current_frame_host()->GetSiteInstance());
    iter->second->GetRenderViewHost()->DisownOpener();
  }
}

void RenderFrameHostManager::RendererProcessClosing(
    RenderProcessHost* render_process_host) {
  // Remove any swapped out RVHs from this process, so that we don't try to
  // swap them back in while the process is exiting.  Start by finding them,
  // since there could be more than one.
  std::list<int> ids_to_remove;
  for (RenderFrameProxyHostMap::iterator iter = proxy_hosts_.begin();
       iter != proxy_hosts_.end();
       ++iter) {
    if (iter->second->GetProcess() == render_process_host)
      ids_to_remove.push_back(iter->first);
  }

  // Now delete them.
  while (!ids_to_remove.empty()) {
    delete proxy_hosts_[ids_to_remove.back()];
    proxy_hosts_.erase(ids_to_remove.back());
    ids_to_remove.pop_back();
  }
}

void RenderFrameHostManager::SwapOutOldPage() {
  // Should only see this while we have a pending renderer or transfer.
  CHECK(cross_navigation_pending_ || pending_nav_params_.get());

  // Tell the renderer to suppress any further modal dialogs so that we can swap
  // it out.  This must be done before canceling any current dialog, in case
  // there is a loop creating additional dialogs.
  // TODO(creis): Handle modal dialogs in subframe processes.
  render_frame_host_->render_view_host()->SuppressDialogsUntilSwapOut();

  // Now close any modal dialogs that would prevent us from swapping out.  This
  // must be done separately from SwapOut, so that the PageGroupLoadDeferrer is
  // no longer on the stack when we send the SwapOut message.
  delegate_->CancelModalDialogsForRenderManager();

  // Create the RenderFrameProxyHost that will replace the
  // RenderFrameHost which is swapping out. If one exists, ensure it is deleted
  // from the map and not leaked.
  RenderFrameProxyHostMap::iterator iter = proxy_hosts_.find(
      render_frame_host_->GetSiteInstance()->GetId());
  if (iter != proxy_hosts_.end()) {
    delete iter->second;
    proxy_hosts_.erase(iter);
  }

  RenderFrameProxyHost* proxy = new RenderFrameProxyHost(
      render_frame_host_->GetSiteInstance(), frame_tree_node_);
  proxy_hosts_[render_frame_host_->GetSiteInstance()->GetId()] = proxy;

  // Tell the old frame it is being swapped out.  This will fire the unload
  // handler in the background (without firing the beforeunload handler a second
  // time).  When the navigation completes, we will send a message to the
  // ResourceDispatcherHost, allowing the pending RVH's response to resume.
  render_frame_host_->SwapOut(proxy);

  // ResourceDispatcherHost has told us to run the onunload handler, which
  // means it is not a download or unsafe page, and we are going to perform the
  // navigation.  Thus, we no longer need to remember that the RenderFrameHost
  // is part of a pending cross-site request.
  if (pending_render_frame_host_) {
    pending_render_frame_host_->render_view_host()->
        SetHasPendingCrossSiteRequest(false);
  }
}

void RenderFrameHostManager::ClearPendingShutdownRFHForSiteInstance(
    int32 site_instance_id,
    RenderFrameHostImpl* rfh) {
  RFHPendingDeleteMap::iterator iter =
      pending_delete_hosts_.find(site_instance_id);
  if (iter != pending_delete_hosts_.end() && iter->second.get() == rfh)
    pending_delete_hosts_.erase(site_instance_id);
}

void RenderFrameHostManager::ResetProxyHosts() {
  STLDeleteValues(&proxy_hosts_);
}

void RenderFrameHostManager::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case NOTIFICATION_RENDERER_PROCESS_CLOSING:
      RendererProcessClosing(
          Source<RenderProcessHost>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

bool RenderFrameHostManager::ClearProxiesInSiteInstance(
    int32 site_instance_id,
    FrameTreeNode* node) {
  RenderFrameProxyHostMap::iterator iter =
      node->render_manager()->proxy_hosts_.find(site_instance_id);
  if (iter != node->render_manager()->proxy_hosts_.end()) {
    RenderFrameProxyHost* proxy = iter->second;
    // If the RVH is pending swap out, it needs to switch state to
    // pending shutdown. Otherwise it is deleted.
    if (proxy->GetRenderViewHost()->rvh_state() ==
        RenderViewHostImpl::STATE_PENDING_SWAP_OUT) {
      scoped_ptr<RenderFrameHostImpl> swapped_out_rfh =
          proxy->PassFrameHostOwnership();

      swapped_out_rfh->SetPendingShutdown(base::Bind(
          &RenderFrameHostManager::ClearPendingShutdownRFHForSiteInstance,
          node->render_manager()->weak_factory_.GetWeakPtr(),
          site_instance_id,
          swapped_out_rfh.get()));
      RFHPendingDeleteMap::iterator pending_delete_iter =
          node->render_manager()->pending_delete_hosts_.find(site_instance_id);
      if (pending_delete_iter ==
              node->render_manager()->pending_delete_hosts_.end() ||
          pending_delete_iter->second.get() != swapped_out_rfh) {
        node->render_manager()->pending_delete_hosts_[site_instance_id] =
            linked_ptr<RenderFrameHostImpl>(swapped_out_rfh.release());
      }
    }
    delete proxy;
    node->render_manager()->proxy_hosts_.erase(site_instance_id);
  }

  return true;
}

bool RenderFrameHostManager::ShouldTransitionCrossSite() {
  // False in the single-process mode, as it makes RVHs to accumulate
  // in swapped_out_hosts_.
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  return
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab);
}

bool RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const NavigationEntry* current_entry,
    const NavigationEntryImpl* new_entry) const {
  DCHECK(new_entry);

  // If new_entry already has a SiteInstance, assume it is correct.  We only
  // need to force a swap if it is in a different BrowsingInstance.
  if (new_entry->site_instance()) {
    return !new_entry->site_instance()->IsRelatedSiteInstance(
        render_frame_host_->GetSiteInstance());
  }

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new_entry will be rendered in a new SiteInstance AND BrowsingInstance.

  // We use the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameWebSite.  If there is no
  // current_entry, check the current SiteInstance's site, which might already
  // be committed to a Web UI URL (such as the NTP).
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();
  const GURL& current_url = (current_entry) ?
      SiteInstanceImpl::GetEffectiveURL(browser_context,
                                        current_entry->GetURL()) :
      render_frame_host_->GetSiteInstance()->GetSiteURL();
  const GURL& new_url = SiteInstanceImpl::GetEffectiveURL(browser_context,
                                                          new_entry->GetURL());

  // Don't force a new BrowsingInstance for debug URLs that are handled in the
  // renderer process, like javascript: or chrome://crash.
  if (IsRendererDebugURL(new_url))
    return false;

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't.
  if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, current_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, new_url)) {
      return true;
    }
  } else {
    // Force a swap if it's a Web UI URL.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, new_url)) {
      return true;
    }
  }

  // Check with the content client as well.  Important to pass current_url here,
  // which uses the SiteInstance's site if there is no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_frame_host_->GetSiteInstance(),
          current_url, new_url)) {
    return true;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't treat
  // it as a new navigation). So require a BrowsingInstance switch.
  if (current_entry &&
      current_entry->IsViewSourceMode() != new_entry->IsViewSourceMode())
    return true;

  return false;
}

bool RenderFrameHostManager::ShouldReuseWebUI(
    const NavigationEntry* current_entry,
    const NavigationEntryImpl* new_entry) const {
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  return current_entry && web_ui_.get() &&
      (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), current_entry->GetURL()) ==
       WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), new_entry->GetURL()));
}

SiteInstance* RenderFrameHostManager::GetSiteInstanceForEntry(
    const NavigationEntryImpl& entry,
    SiteInstance* current_instance,
    bool force_browsing_instance_swap) {
  // Determine which SiteInstance to use for navigating to |entry|.
  const GURL& dest_url = entry.GetURL();
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  BrowserContext* browser_context = controller.GetBrowserContext();

  // If the entry has an instance already we should use it.
  if (entry.site_instance()) {
    // If we are forcing a swap, this should be in a different BrowsingInstance.
    if (force_browsing_instance_swap) {
      CHECK(!entry.site_instance()->IsRelatedSiteInstance(
                render_frame_host_->GetSiteInstance()));
    }
    return entry.site_instance();
  }

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (force_browsing_instance_swap)
    return SiteInstance::CreateForURL(browser_context, dest_url);

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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerSite) &&
      PageTransitionCoreTypeIs(entry.GetTransitionType(),
                               PAGE_TRANSITION_GENERATED)) {
    return current_instance;
  }

  SiteInstanceImpl* current_site_instance =
      static_cast<SiteInstanceImpl*>(current_instance);

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!current_site_instance->HasSite()) {
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
    if (current_site_instance->HasRelatedSiteInstance(dest_url) ||
        use_process_per_site) {
      return current_site_instance->GetRelatedSiteInstance(dest_url);
    }

    // For extensions, Web UI URLs (such as the new tab page), and apps we do
    // not want to use the current_instance if it has no site, since it will
    // have a RenderProcessHost of PRIV_NORMAL.  Create a new SiteInstance for
    // this URL instead (with the correct process type).
    if (current_site_instance->HasWrongProcessForURL(dest_url))
      return current_site_instance->GetRelatedSiteInstance(dest_url);

    // View-source URLs must use a new SiteInstance and BrowsingInstance.
    // TODO(nasko): This is the same condition as later in the function. This
    // should be taken into account when refactoring this method as part of
    // http://crbug.com/123007.
    if (entry.IsViewSourceMode())
      return SiteInstance::CreateForURL(browser_context, dest_url);

    // If we are navigating from a blank SiteInstance to a WebUI, make sure we
    // create a new SiteInstance.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, dest_url)) {
        return SiteInstance::CreateForURL(browser_context, dest_url);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    if (entry.restore_type() != NavigationEntryImpl::RESTORE_NONE)
      current_site_instance->SetSite(dest_url);

    return current_site_instance;
  }

  // Otherwise, only create a new SiteInstance for a cross-site navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = current_instance->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* current_entry = controller.GetLastCommittedEntry();
  if (interstitial_page_) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    current_entry = controller.GetEntryAtOffset(-1);
  }
  // If there is no last non-interstitial entry (and current_instance already
  // has a site), then we must have been opened from another tab.  We want
  // to compare against the URL of the page that opened us, but we can't
  // get to it directly.  The best we can do is check against the site of
  // the SiteInstance.  This will be correct when we intercept links and
  // script-based navigations, but for now, it could place some pages in a
  // new process unnecessarily.  We should only hit this case if a page tries
  // to open a new tab to an interstitial-inducing URL, and then navigates
  // the page to a different same-site URL.  (This seems very unlikely in
  // practice.)
  const GURL& current_url = (current_entry) ? current_entry->GetURL() :
      current_instance->GetSiteURL();

  // View-source URLs must use a new SiteInstance and BrowsingInstance.
  // We don't need a swap when going from view-source to a debug URL like
  // chrome://crash, however.
  // TODO(creis): Refactor this method so this duplicated code isn't needed.
  // See http://crbug.com/123007.
  if (current_entry &&
      current_entry->IsViewSourceMode() != entry.IsViewSourceMode() &&
      !IsRendererDebugURL(dest_url)) {
    return SiteInstance::CreateForURL(browser_context, dest_url);
  }

  // Use the current SiteInstance for same site navigations, as long as the
  // process type is correct.  (The URL may have been installed as an app since
  // the last time we visited it.)
  if (SiteInstance::IsSameWebSite(browser_context, current_url, dest_url) &&
      !current_site_instance->HasWrongProcessForURL(dest_url)) {
    return current_instance;
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance.  It is important to immediately give this new
  // SiteInstance to a RenderViewHost (if it is different than our current
  // SiteInstance), so that it is ref counted.  This will happen in
  // CreateRenderView.
  return current_instance->GetRelatedSiteInstance(dest_url);
}

scoped_ptr<RenderFrameHostImpl> RenderFrameHostManager::CreateRenderFrameHost(
    SiteInstance* site_instance,
    int view_routing_id,
    int frame_routing_id,
    bool swapped_out,
    bool hidden) {
  if (frame_routing_id == MSG_ROUTING_NONE)
    frame_routing_id = site_instance->GetProcess()->GetNextRoutingID();

  // Create a RVH for main frames, or find the existing one for subframes.
  FrameTree* frame_tree = frame_tree_node_->frame_tree();
  RenderViewHostImpl* render_view_host = NULL;
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host = frame_tree->CreateRenderViewHostForMainFrame(
        site_instance, view_routing_id, frame_routing_id, swapped_out, hidden);
  } else {
    render_view_host = frame_tree->GetRenderViewHostForSubFrame(site_instance);

    // If we haven't found a RVH for a subframe RFH, it's because we currently
    // do not create top-level RFHs for pending subframe navigations.  Create
    // the RVH here for now.
    // TODO(creis): Mirror the frame tree so this check isn't necessary.
    if (!render_view_host) {
      render_view_host = frame_tree->CreateRenderViewHostForMainFrame(
          site_instance, view_routing_id, frame_routing_id, swapped_out,
          hidden);
    }
  }

  // TODO(creis): Pass hidden to RFH.
  scoped_ptr<RenderFrameHostImpl> render_frame_host =
      make_scoped_ptr(RenderFrameHostFactory::Create(render_view_host,
                                                     render_frame_delegate_,
                                                     frame_tree,
                                                     frame_tree_node_,
                                                     frame_routing_id,
                                                     swapped_out).release());
  return render_frame_host.Pass();
}

int RenderFrameHostManager::CreateRenderFrame(
    SiteInstance* instance,
    int opener_route_id,
    bool swapped_out,
    bool hidden) {
  CHECK(instance);
  DCHECK(!swapped_out || hidden); // Swapped out views should always be hidden.

  scoped_ptr<RenderFrameHostImpl> new_render_frame_host;
  RenderFrameHostImpl* frame_to_announce = NULL;
  int routing_id = MSG_ROUTING_NONE;

  // We are creating a pending or swapped out RFH here.  We should never create
  // it in the same SiteInstance as our current RFH.
  CHECK_NE(render_frame_host_->GetSiteInstance(), instance);

  // Check if we've already created an RFH for this SiteInstance.  If so, try
  // to re-use the existing one, which has already been initialized.  We'll
  // remove it from the list of swapped out hosts if it commits.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);

  if (proxy) {
    routing_id = proxy->GetRenderViewHost()->GetRoutingID();
    // Delete the existing RenderFrameProxyHost, but reuse the RenderFrameHost.
    // Prevent the process from exiting while we're trying to use it.
    if (!swapped_out) {
      new_render_frame_host = proxy->PassFrameHostOwnership();
      new_render_frame_host->GetProcess()->AddPendingView();

      proxy_hosts_.erase(instance->GetId());
      delete proxy;

      // When a new render view is created by the renderer, the new WebContents
      // gets a RenderViewHost in the SiteInstance of its opener WebContents.
      // If not used in the first navigation, this RVH is swapped out and is not
      // granted bindings, so we may need to grant them when swapping it in.
      if (pending_web_ui() &&
          !new_render_frame_host->GetProcess()->IsIsolatedGuest()) {
        int required_bindings = pending_web_ui()->GetBindings();
        RenderViewHost* rvh = new_render_frame_host->render_view_host();
        if ((rvh->GetEnabledBindings() & required_bindings) !=
                required_bindings) {
          rvh->AllowBindings(required_bindings);
        }
      }
    }
  } else {
    // Create a new RenderFrameHost if we don't find an existing one.
    new_render_frame_host = CreateRenderFrameHost(
        instance, MSG_ROUTING_NONE, MSG_ROUTING_NONE, swapped_out, hidden);
    RenderViewHostImpl* render_view_host =
        new_render_frame_host->render_view_host();
    int proxy_routing_id = MSG_ROUTING_NONE;

    // Prevent the process from exiting while we're trying to navigate in it.
    // Otherwise, if the new RFH is swapped out already, store it.
    if (!swapped_out) {
      new_render_frame_host->GetProcess()->AddPendingView();
    } else {
      proxy = new RenderFrameProxyHost(
          new_render_frame_host->GetSiteInstance(), frame_tree_node_);
      proxy_hosts_[instance->GetId()] = proxy;
      proxy->TakeFrameHostOwnership(new_render_frame_host.Pass());
      proxy_routing_id = proxy->GetRoutingID();
    }

    bool success = InitRenderView(
        render_view_host, opener_route_id, proxy_routing_id,
        frame_tree_node_->IsMainFrame());
    if (success && frame_tree_node_->IsMainFrame()) {
      // Don't show the main frame's view until we get a DidNavigate from it.
      render_view_host->GetView()->Hide();
    } else if (!swapped_out && pending_render_frame_host_) {
      CancelPending();
    }
    routing_id = render_view_host->GetRoutingID();
    frame_to_announce = new_render_frame_host.get();
  }

  // Use this as our new pending RFH if it isn't swapped out.
  if (!swapped_out)
    pending_render_frame_host_ = new_render_frame_host.Pass();

  // If a brand new RFH was created, announce it to observers.
  if (frame_to_announce)
    render_frame_delegate_->RenderFrameCreated(frame_to_announce);

  return routing_id;
}

bool RenderFrameHostManager::InitRenderView(RenderViewHost* render_view_host,
                                            int opener_route_id,
                                            int proxy_routing_id,
                                            bool for_main_frame) {
  // We may have initialized this RenderViewHost for another RenderFrameHost.
  if (render_view_host->IsRenderViewLive())
    return true;

  // If the pending navigation is to a WebUI and the RenderView is not in a
  // guest process, tell the RenderViewHost about any bindings it will need
  // enabled.
  if (pending_web_ui() && !render_view_host->GetProcess()->IsIsolatedGuest()) {
    render_view_host->AllowBindings(pending_web_ui()->GetBindings());
  } else {
    // Ensure that we don't create an unprivileged RenderView in a WebUI-enabled
    // process unless it's swapped out.
    RenderViewHostImpl* rvh_impl =
        static_cast<RenderViewHostImpl*>(render_view_host);
    if (!rvh_impl->IsSwappedOut()) {
      CHECK(!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                render_view_host->GetProcess()->GetID()));
    }
  }

  return delegate_->CreateRenderViewForRenderManager(
      render_view_host, opener_route_id, proxy_routing_id, for_main_frame);
}

void RenderFrameHostManager::CommitPending() {
  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared |pending_web_ui_| the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  // We expect SwapOutOldPage to have canceled any modal dialogs and told the
  // renderer to suppress any further dialogs until it is swapped out.  However,
  // crash reports indicate that it's still possible for modal dialogs to exist
  // at this point, which poses a risk if we delete their RenderViewHost below.
  // Cancel them again to be safe.  http://crbug.com/324320.
  delegate_->CancelModalDialogsForRenderManager();

  // Next commit the Web UI, if any. Either replace |web_ui_| with
  // |pending_web_ui_|, or clear |web_ui_| if there is no pending WebUI, or
  // leave |web_ui_| as is if reusing it.
  DCHECK(!(pending_web_ui_.get() && pending_and_current_web_ui_.get()));
  if (pending_web_ui_) {
    web_ui_.reset(pending_web_ui_.release());
  } else if (!pending_and_current_web_ui_.get()) {
    web_ui_.reset();
  } else {
    DCHECK_EQ(pending_and_current_web_ui_.get(), web_ui_.get());
    pending_and_current_web_ui_.reset();
  }

  // It's possible for the pending_render_frame_host_ to be NULL when we aren't
  // crossing process boundaries. If so, we just needed to handle the Web UI
  // committing above and we're done.
  if (!pending_render_frame_host_) {
    if (will_focus_location_bar)
      delegate_->SetFocusToLocationBar(false);
    return;
  }

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
      render_frame_host_->render_view_host()->GetView() &&
      render_frame_host_->render_view_host()->GetView()->HasFocus();

  // TODO(creis): As long as show/hide are on RVH, we don't want to do them for
  // subframe navigations or they'll interfere with the top-level page.
  bool is_main_frame = frame_tree_node_->IsMainFrame();

  // Swap in the pending frame and make it active. Also ensure the FrameTree
  // stays in sync.
  scoped_ptr<RenderFrameHostImpl> old_render_frame_host =
      SetRenderFrameHost(pending_render_frame_host_.Pass());
  if (is_main_frame)
    render_frame_host_->render_view_host()->AttachToFrameTree();

  // The process will no longer try to exit, so we can decrement the count.
  render_frame_host_->GetProcess()->RemovePendingView();

  // If the view is gone, then this RenderViewHost died while it was hidden.
  // We ignored the RenderProcessGone call at the time, so we should send it now
  // to make sure the sad tab shows up, etc.
  if (!render_frame_host_->render_view_host()->GetView()) {
    delegate_->RenderProcessGoneFromRenderManager(
        render_frame_host_->render_view_host());
  } else if (!delegate_->IsHidden()) {
    render_frame_host_->render_view_host()->GetView()->Show();
  }

  // If the old view is live and top-level, hide it now that the new one is
  // visible.
  int32 old_site_instance_id =
      old_render_frame_host->GetSiteInstance()->GetId();
  if (old_render_frame_host->render_view_host()->GetView()) {
    if (is_main_frame) {
      old_render_frame_host->render_view_host()->GetView()->Hide();
      old_render_frame_host->render_view_host()->WasSwappedOut(base::Bind(
          &RenderFrameHostManager::ClearPendingShutdownRFHForSiteInstance,
          weak_factory_.GetWeakPtr(),
          old_site_instance_id,
          old_render_frame_host.get()));
    } else {
      // TODO(creis): We'll need to set this back to false if we navigate back.
      old_render_frame_host->set_swapped_out(true);
    }
  }

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (will_focus_location_bar) {
    delegate_->SetFocusToLocationBar(false);
  } else if (focus_render_view &&
             render_frame_host_->render_view_host()->GetView()) {
    render_frame_host_->render_view_host()->GetView()->Focus();
  }

  // Notify that we've swapped RenderFrameHosts. We do this before shutting down
  // the RFH so that we can clean up RendererResources related to the RFH first.
  // TODO(creis): Only do this on top-level RFHs for now, and later update it to
  // pass the RFHs.
  if (is_main_frame) {
    delegate_->NotifySwappedFromRenderManager(
        old_render_frame_host->render_view_host(),
        render_frame_host_->render_view_host());
  }

  // If the old RFH is not live, just return as there is no work to do.
  if (!old_render_frame_host->render_view_host()->IsRenderViewLive()) {
    return;
  }

  // If the old RFH is live, we are swapping it out and should keep track of
  // it in case we navigate back to it, or it is waiting for the unload event
  // to execute in the background.
  // TODO(creis): Swap out the subframe in --site-per-process.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess))
    DCHECK(old_render_frame_host->is_swapped_out() ||
           !RenderViewHostImpl::IsRVHStateActive(
               old_render_frame_host->render_view_host()->rvh_state()));

  // If the RenderViewHost backing the RenderFrameHost is pending shutdown,
  // the RenderFrameHost should be put in the map of RenderFrameHosts pending
  // shutdown. Otherwise, it is stored in the map of proxy hosts.
  if (old_render_frame_host->render_view_host()->rvh_state() ==
          RenderViewHostImpl::STATE_PENDING_SHUTDOWN) {
    // The proxy for this RenderFrameHost is created when sending the
    // SwapOut message, so check if it already exists and delete it.
    RenderFrameProxyHostMap::iterator iter =
        proxy_hosts_.find(old_site_instance_id);
    if (iter != proxy_hosts_.end()) {
      delete iter->second;
      proxy_hosts_.erase(iter);
    }
    RFHPendingDeleteMap::iterator pending_delete_iter =
        pending_delete_hosts_.find(old_site_instance_id);
    if (pending_delete_iter == pending_delete_hosts_.end() ||
        pending_delete_iter->second.get() != old_render_frame_host) {
      pending_delete_hosts_[old_site_instance_id] =
          linked_ptr<RenderFrameHostImpl>(old_render_frame_host.release());
    }
  } else {
    // Capture the active view count on the old RFH SiteInstance, since the
    // ownership will be passed into the proxy and the pointer will be invalid.
    int active_view_count =
        static_cast<SiteInstanceImpl*>(old_render_frame_host->GetSiteInstance())
            ->active_view_count();

    RenderFrameProxyHostMap::iterator iter =
        proxy_hosts_.find(old_site_instance_id);
    CHECK(iter != proxy_hosts_.end());
    iter->second->TakeFrameHostOwnership(old_render_frame_host.Pass());

    // If there are no active views in this SiteInstance, it means that
    // this RFH was the last active one in the SiteInstance. Now that we
    // know that all RFHs are swapped out, we can delete all the RFHs and RVHs
    // in this SiteInstance.
    if (!active_view_count) {
      ShutdownRenderFrameHostsInSiteInstance(old_site_instance_id);
    } else {
      // If this is a subframe, it should have a CrossProcessFrameConnector
      // created already and we just need to link it to the proper view in the
      // new process.
      if (!is_main_frame) {
        RenderFrameProxyHost* proxy = GetProxyToParent();
        if (proxy) {
          proxy->SetChildRWHView(
              render_frame_host_->render_view_host()->GetView());
        }
      }
    }
  }
}

void RenderFrameHostManager::ShutdownRenderFrameHostsInSiteInstance(
    int32 site_instance_id) {
  // First remove any swapped out RFH for this SiteInstance from our own list.
  ClearProxiesInSiteInstance(site_instance_id, frame_tree_node_);

  // Use the safe RenderWidgetHost iterator for now to find all RenderViewHosts
  // in the SiteInstance, then tell their respective FrameTrees to remove all
  // RenderFrameProxyHosts corresponding to them.
  // TODO(creis): Replace this with a RenderFrameHostIterator that protects
  // against use-after-frees if a later element is deleted before getting to it.
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (!widget->IsRenderView())
      continue;
    RenderViewHostImpl* rvh =
        static_cast<RenderViewHostImpl*>(RenderViewHost::From(widget));
    if (site_instance_id == rvh->GetSiteInstance()->GetId()) {
      // This deletes all RenderFrameHosts using the |rvh|, which then causes
      // |rvh| to Shutdown.
      FrameTree* tree = rvh->GetDelegate()->GetFrameTree();
      tree->ForEach(base::Bind(
          &RenderFrameHostManager::ClearProxiesInSiteInstance,
          site_instance_id));
    }
  }
}

RenderFrameHostImpl* RenderFrameHostManager::UpdateStateForNavigate(
    const NavigationEntryImpl& entry) {
  // If we are currently navigating cross-process, we want to get back to normal
  // and then navigate as usual.
  if (cross_navigation_pending_) {
    if (pending_render_frame_host_)
      CancelPending();
    cross_navigation_pending_ = false;
  }

  // render_frame_host_'s SiteInstance and new_instance will not be deleted
  // before the end of this method, so we don't have to worry about their ref
  // counts dropping to zero.
  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();
  SiteInstance* new_instance = current_instance;

  // We do not currently swap processes for navigations in webview tag guests.
  bool is_guest_scheme = current_instance->GetSiteURL().SchemeIs(kGuestScheme);

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrowsingInstance will be unable to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.
  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();
  bool force_swap = !is_guest_scheme &&
      ShouldSwapBrowsingInstancesForNavigation(current_entry, &entry);
  if (!is_guest_scheme && (ShouldTransitionCrossSite() || force_swap))
    new_instance = GetSiteInstanceForEntry(entry, current_instance, force_swap);

  // If force_swap is true, we must use a different SiteInstance.  If we didn't,
  // we would have two RenderFrameHosts in the same SiteInstance and the same
  // frame, resulting in page_id conflicts for their NavigationEntries.
  if (force_swap)
    CHECK_NE(new_instance, current_instance);

  if (new_instance != current_instance) {
    // New SiteInstance: create a pending RFH to navigate.
    DCHECK(!cross_navigation_pending_);

    // This will possibly create (set to NULL) a Web UI object for the pending
    // page. We'll use this later to give the page special access. This must
    // happen before the new renderer is created below so it will get bindings.
    // It must also happen after the above conditional call to CancelPending(),
    // otherwise CancelPending may clear the pending_web_ui_ and the page will
    // not have its bindings set appropriately.
    SetPendingWebUI(entry);

    // Ensure that we have created RFHs for the new RFH's opener chain if
    // we are staying in the same BrowsingInstance. This allows the pending RFH
    // to send cross-process script calls to its opener(s).
    int opener_route_id = MSG_ROUTING_NONE;
    if (new_instance->IsRelatedSiteInstance(current_instance)) {
      opener_route_id =
          delegate_->CreateOpenerRenderViewsForRenderManager(new_instance);
    }

    // Create a non-swapped-out pending RFH with the given opener and navigate
    // it.
    int route_id = CreateRenderFrame(new_instance, opener_route_id, false,
                                     delegate_->IsHidden());
    if (route_id == MSG_ROUTING_NONE)
      return NULL;

    // Check if our current RFH is live before we set up a transition.
    if (!render_frame_host_->render_view_host()->IsRenderViewLive()) {
      if (!cross_navigation_pending_) {
        // The current RFH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RFH while we wait for the pending RFH to
        // navigate.  Just switch to the pending RFH now and go back to non
        // cross-navigating (Note that we don't care about on{before}unload
        // handlers if the current RFH isn't live.)
        CommitPending();
        return render_frame_host_.get();
      } else {
        NOTREACHED();
        return render_frame_host_.get();
      }
    }
    // Otherwise, it's safe to treat this as a pending cross-site transition.

    // We need to wait until the beforeunload handler has run, unless we are
    // transferring an existing request (in which case it has already run).
    // Suspend the new render view (i.e., don't let it send the cross-site
    // Navigate message) until we hear back from the old renderer's
    // beforeunload handler.  If the handler returns false, we'll have to
    // cancel the request.
    DCHECK(!pending_render_frame_host_->render_view_host()->
               are_navigations_suspended());
    bool is_transfer =
        entry.transferred_global_request_id() != GlobalRequestID();
    if (is_transfer) {
      // We don't need to stop the old renderer or run beforeunload/unload
      // handlers, because those have already been done.
      DCHECK(pending_nav_params_->global_request_id ==
                entry.transferred_global_request_id());
    } else {
      // Also make sure the old render view stops, in case a load is in
      // progress.  (We don't want to do this for transfers, since it will
      // interrupt the transfer with an unexpected DidStopLoading.)
      render_frame_host_->render_view_host()->Send(new ViewMsg_Stop(
          render_frame_host_->render_view_host()->GetRoutingID()));

      pending_render_frame_host_->render_view_host()->SetNavigationsSuspended(
          true, base::TimeTicks());

      // Tell the CrossSiteRequestManager that this RVH has a pending cross-site
      // request, so that ResourceDispatcherHost will know to tell us to run the
      // old page's unload handler before it sends the response.
      // TODO(creis): This needs to be on the RFH.
      pending_render_frame_host_->render_view_host()->
          SetHasPendingCrossSiteRequest(true);
    }

    // We now have a pending RFH.
    DCHECK(!cross_navigation_pending_);
    cross_navigation_pending_ = true;

    // Unless we are transferring an existing request, we should now
    // tell the old render view to run its beforeunload handler, since it
    // doesn't otherwise know that the cross-site request is happening.  This
    // will trigger a call to OnBeforeUnloadACK with the reply.
    if (!is_transfer)
      render_frame_host_->DispatchBeforeUnload(true);

    return pending_render_frame_host_.get();
  }

  // Otherwise the same SiteInstance can be used.  Navigate render_frame_host_.
  DCHECK(!cross_navigation_pending_);
  if (ShouldReuseWebUI(current_entry, &entry)) {
    pending_web_ui_.reset();
    pending_and_current_web_ui_ = web_ui_->AsWeakPtr();
  } else {
    SetPendingWebUI(entry);

    // Make sure the new RenderViewHost has the right bindings.
    if (pending_web_ui() &&
        !render_frame_host_->GetProcess()->IsIsolatedGuest()) {
      render_frame_host_->render_view_host()->AllowBindings(
          pending_web_ui()->GetBindings());
    }
  }

  if (pending_web_ui() &&
      render_frame_host_->render_view_host()->IsRenderViewLive()) {
    pending_web_ui()->GetController()->RenderViewReused(
        render_frame_host_->render_view_host());
  }

  // The renderer can exit view source mode when any error or cancellation
  // happen. We must overwrite to recover the mode.
  if (entry.IsViewSourceMode()) {
    render_frame_host_->render_view_host()->Send(
        new ViewMsg_EnableViewSourceMode(
            render_frame_host_->render_view_host()->GetRoutingID()));
  }

  return render_frame_host_.get();
}

void RenderFrameHostManager::CancelPending() {
  scoped_ptr<RenderFrameHostImpl> pending_render_frame_host =
      pending_render_frame_host_.Pass();

  RenderViewDevToolsAgentHost::OnCancelPendingNavigation(
      pending_render_frame_host->render_view_host(),
      render_frame_host_->render_view_host());

  // We no longer need to prevent the process from exiting.
  pending_render_frame_host->GetProcess()->RemovePendingView();

  // If the SiteInstance for the pending RFH is being used by others, don't
  // delete the RFH, just swap it out and it can be reused at a later point.
  SiteInstanceImpl* site_instance = static_cast<SiteInstanceImpl*>(
      pending_render_frame_host->GetSiteInstance());
  if (site_instance->active_view_count() > 1) {
    // Any currently suspended navigations are no longer needed.
    pending_render_frame_host->render_view_host()->CancelSuspendedNavigations();

    RenderFrameProxyHost* proxy =
        new RenderFrameProxyHost(site_instance, frame_tree_node_);
    proxy_hosts_[site_instance->GetId()] = proxy;
    pending_render_frame_host->SwapOut(proxy);
    proxy->TakeFrameHostOwnership(pending_render_frame_host.Pass());
  } else {
    // We won't be coming back, so delete this one.
    pending_render_frame_host.reset();
  }

  pending_web_ui_.reset();
  pending_and_current_web_ui_.reset();
}

scoped_ptr<RenderFrameHostImpl> RenderFrameHostManager::SetRenderFrameHost(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  // Swap the two.
  scoped_ptr<RenderFrameHostImpl> old_render_frame_host =
      render_frame_host_.Pass();
  render_frame_host_ = render_frame_host.Pass();

  if (frame_tree_node_->IsMainFrame()) {
    // Update the count of top-level frames using this SiteInstance.  All
    // subframes are in the same BrowsingInstance as the main frame, so we only
    // count top-level ones.  This makes the value easier for consumers to
    // interpret.
    if (render_frame_host_) {
      static_cast<SiteInstanceImpl*>(render_frame_host_->GetSiteInstance())->
          IncrementRelatedActiveContentsCount();
    }
    if (old_render_frame_host) {
      static_cast<SiteInstanceImpl*>(old_render_frame_host->GetSiteInstance())->
          DecrementRelatedActiveContentsCount();
    }
  }

  return old_render_frame_host.Pass();
}

bool RenderFrameHostManager::IsRVHOnSwappedOutList(
    RenderViewHostImpl* rvh) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(
      rvh->GetSiteInstance());
  if (!proxy)
    return false;
  return IsOnSwappedOutList(proxy->render_frame_host());
}

bool RenderFrameHostManager::IsOnSwappedOutList(
    RenderFrameHostImpl* rfh) const {
  if (!rfh->GetSiteInstance())
    return false;

  RenderFrameProxyHostMap::const_iterator iter = proxy_hosts_.find(
      rfh->GetSiteInstance()->GetId());
  if (iter == proxy_hosts_.end())
    return false;

  return iter->second->render_frame_host() == rfh;
}

RenderViewHostImpl* RenderFrameHostManager::GetSwappedOutRenderViewHost(
   SiteInstance* instance) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy)
    return proxy->GetRenderViewHost();
  return NULL;
}

RenderFrameProxyHost* RenderFrameHostManager::GetRenderFrameProxyHost(
    SiteInstance* instance) const {
  RenderFrameProxyHostMap::const_iterator iter =
      proxy_hosts_.find(instance->GetId());
  if (iter != proxy_hosts_.end())
    return iter->second;

  return NULL;
}

}  // namespace content
