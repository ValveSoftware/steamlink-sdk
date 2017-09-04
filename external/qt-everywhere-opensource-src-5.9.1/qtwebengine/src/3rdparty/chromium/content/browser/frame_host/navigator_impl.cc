// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/page_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

FrameMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context,
    const NavigationEntryImpl& entry,
    ReloadType reload_type) {
  switch (reload_type) {
    case ReloadType::NORMAL:
      return FrameMsg_Navigate_Type::RELOAD;
    case ReloadType::MAIN_RESOURCE:
      return FrameMsg_Navigate_Type::RELOAD_MAIN_RESOURCE;
    case ReloadType::BYPASSING_CACHE:
    case ReloadType::DISABLE_LOFI_MODE:
      return FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE;
    case ReloadType::ORIGINAL_REQUEST_URL:
      return FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case ReloadType::NONE:
      break;  // Fall through to rest of function.
  }

  // |RenderViewImpl::PopulateStateFromPendingNavigationParams| differentiates
  // between |RESTORE_WITH_POST| and |RESTORE|.
  if (entry.restore_type() == RestoreType::LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return FrameMsg_Navigate_Type::RESTORE_WITH_POST;
    return FrameMsg_Navigate_Type::RESTORE;
  }

  return FrameMsg_Navigate_Type::NORMAL;
}

}  // namespace

struct NavigatorImpl::NavigationMetricsData {
  NavigationMetricsData(base::TimeTicks start_time,
                        GURL url,
                        RestoreType restore_type)
      : start_time_(start_time), url_(url) {
    is_restoring_from_last_session_ =
        (restore_type == RestoreType::LAST_SESSION_EXITED_CLEANLY ||
         restore_type == RestoreType::LAST_SESSION_CRASHED);
  }

  base::TimeTicks start_time_;
  GURL url_;
  bool is_restoring_from_last_session_;
  base::TimeTicks url_job_start_time_;
  base::TimeDelta before_unload_delay_;
};

NavigatorImpl::NavigatorImpl(
    NavigationControllerImpl* navigation_controller,
    NavigatorDelegate* delegate)
    : controller_(navigation_controller),
      delegate_(delegate) {
}

NavigatorImpl::~NavigatorImpl() {}

// static
void NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url) {
  int enabled_bindings =
      render_frame_host->render_view_host()->GetEnabledBindings();
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          render_frame_host->frame_tree_node()
              ->navigator()
              ->GetController()
              ->GetBrowserContext(),
          url);
  if ((enabled_bindings & BINDINGS_POLICY_WEB_UI) &&
      !is_allowed_in_web_ui_renderer) {
    // Log the URL to help us diagnose any future failures of this CHECK.
    GetContentClient()->SetActiveURL(url);
    CHECK(0);
  }
}

NavigatorDelegate* NavigatorImpl::GetDelegate() {
  return delegate_;
}

NavigationController* NavigatorImpl::GetController() {
  return controller_;
}

void NavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const base::TimeTicks& navigation_start) {
  bool is_main_frame = render_frame_host->frame_tree_node()->IsMainFrame();
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  // Do not allow browser plugin guests to navigate to non-web URLs, since they
  // cannot swap processes or grant bindings.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (render_process_host->IsForGuestsOnly() &&
      !policy->IsWebSafeScheme(validated_url.scheme())) {
    validated_url = GURL(url::kAboutBlankURL);
  }

  if (is_main_frame && !is_error_page) {
    DidStartMainFrameNavigation(validated_url,
                                render_frame_host->GetSiteInstance(),
                                render_frame_host->navigation_handle());
  }

  if (delegate_) {
    // Notify the observer about the start of the provisional load.
    delegate_->DidStartProvisionalLoad(render_frame_host, validated_url,
                                       is_error_page, is_iframe_srcdoc);
  }

  if (is_error_page || IsBrowserSideNavigationEnabled())
    return;

  if (render_frame_host->navigation_handle()) {
    if (render_frame_host->navigation_handle()->is_transferring()) {
      // If the navigation is completing a transfer, this
      // DidStartProvisionalLoad should not correspond to a new navigation.
      DCHECK_EQ(url, render_frame_host->navigation_handle()->GetURL());
      render_frame_host->navigation_handle()->set_is_transferring(false);
      return;
    }

    // This ensures that notifications about the end of the previous
    // navigation are sent before notifications about the start of the
    // new navigation.
    render_frame_host->SetNavigationHandle(
        std::unique_ptr<NavigationHandleImpl>());
  }

  // It is safer to assume navigations are renderer-initiated unless shown
  // otherwise. Browser navigations generally have more privileges, and they
  // should always have a pending NavigationEntry to distinguish them.
  bool is_renderer_initiated = true;
  int pending_nav_entry_id = 0;
  bool started_from_context_menu = false;
  NavigationEntryImpl* pending_entry = controller_->GetPendingEntry();
  if (pending_entry) {
    is_renderer_initiated = pending_entry->is_renderer_initiated();
    pending_nav_entry_id = pending_entry->GetUniqueID();
    started_from_context_menu = pending_entry->has_started_from_context_menu();
  }

  render_frame_host->SetNavigationHandle(NavigationHandleImpl::Create(
      validated_url, render_frame_host->frame_tree_node(),
      is_renderer_initiated,
      false,             // is_same_page
      is_iframe_srcdoc,  // is_srcdoc
      navigation_start, pending_nav_entry_id, started_from_context_menu));
}

void NavigatorImpl::DidFailProvisionalLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  VLOG(1) << "Failed Provisional Load: " << params.url.possibly_invalid_spec()
          << ", error_code: " << params.error_code
          << ", error_description: " << params.error_description
          << ", showing_repost_interstitial: " <<
            params.showing_repost_interstitial
          << ", frame_id: " << render_frame_host->GetRoutingID();
  GURL validated_url(params.url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  if (net::ERR_ABORTED == params.error_code) {
    // EVIL HACK ALERT! Ignore failed loads when we're showing interstitials.
    // This means that the interstitial won't be torn down properly, which is
    // bad. But if we have an interstitial, go back to another tab type, and
    // then load the same interstitial again, we could end up getting the first
    // interstitial's "failed" message (as a result of the cancel) when we're on
    // the second one. We can't tell this apart, so we think we're tearing down
    // the current page which will cause a crash later on.
    //
    // http://code.google.com/p/chromium/issues/detail?id=2855
    // Because this will not tear down the interstitial properly, if "back" is
    // back to another tab type, the interstitial will still be somewhat alive
    // in the previous tab type. If you navigate somewhere that activates the
    // tab with the interstitial again, you'll see a flash before the new load
    // commits of the interstitial page.
    FrameTreeNode* root =
        render_frame_host->frame_tree_node()->frame_tree()->root();
    if (root->render_manager()->interstitial_page() != NULL) {
      LOG(WARNING) << "Discarding message during interstitial.";
      return;
    }

    // We used to cancel the pending renderer here for cross-site downloads.
    // However, it's not safe to do that because the download logic repeatedly
    // looks for this WebContents based on a render ID. Instead, we just
    // leave the pending renderer around until the next navigation event
    // (Navigate, DidNavigate, etc), which will clean it up properly.
    //
    // TODO(creis): Find a way to cancel any pending RFH here.
  }

  // Discard the pending navigation entry if needed.
  DiscardPendingEntryIfNeeded(render_frame_host->navigation_handle());

  if (delegate_) {
    delegate_->DidFailProvisionalLoadWithError(
        render_frame_host, validated_url, params.error_code,
        params.error_description, params.was_ignored_by_handler);
  }
}

void NavigatorImpl::DidFailLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  if (delegate_) {
    delegate_->DidFailLoadWithError(
        render_frame_host, url, error_code,
        error_description, was_ignored_by_handler);
  }
}

bool NavigatorImpl::NavigateToEntry(
    FrameTreeNode* frame_tree_node,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    ReloadType reload_type,
    bool is_same_document_history_load,
    bool is_history_navigation_in_new_child,
    bool is_pending_entry,
    const scoped_refptr<ResourceRequestBodyImpl>& post_body) {
  TRACE_EVENT0("browser,navigation", "NavigatorImpl::NavigateToEntry");

  GURL dest_url = frame_entry.url();
  Referrer dest_referrer = frame_entry.referrer();
  if (reload_type == ReloadType::ORIGINAL_REQUEST_URL &&
      entry.GetOriginalRequestURL().is_valid() && !entry.GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit, if it's valid and if a
    // POST wasn't involved; the latter case avoids issues with sending data to
    // the wrong page.
    dest_url = entry.GetOriginalRequestURL();
    dest_referrer = Referrer();
  }

  // Don't attempt to navigate if the virtual URL is non-empty and invalid.
  if (frame_tree_node->IsMainFrame()) {
    const GURL& virtual_url = entry.GetVirtualURL();
    if (!virtual_url.is_valid() && !virtual_url.is_empty()) {
      LOG(WARNING) << "Refusing to load for invalid virtual URL: "
                   << virtual_url.possibly_invalid_spec();
      return false;
    }
  }

  // Don't attempt to navigate to non-empty invalid URLs.
  if (!dest_url.is_valid() && !dest_url.is_empty()) {
    LOG(WARNING) << "Refusing to load invalid URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (dest_url.spec().size() > url::kMaxURLChars) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << url::kMaxURLChars
                 << " characters.";
    return false;
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). We need to keep it above RFHM::Navigate() call to
  // capture the time needed for the RenderFrameHost initialization.
  base::TimeTicks navigation_start = base::TimeTicks::Now();
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "navigation,rail", "NavigationTiming navigationStart",
      TRACE_EVENT_SCOPE_GLOBAL, navigation_start);

  // Determine if LoFi should be used for the navigation.
  LoFiState lofi_state = LOFI_UNSPECIFIED;
  if (!frame_tree_node->IsMainFrame()) {
    // For subframes, use the state of the top-level frame.
    lofi_state = frame_tree_node->frame_tree()
                     ->root()
                     ->current_frame_host()
                     ->last_navigation_lofi_state();
  } else if (reload_type == ReloadType::DISABLE_LOFI_MODE) {
    // Disable LoFi when asked for it explicitly.
    lofi_state = LOFI_OFF;
  }

  // PlzNavigate: the RenderFrameHosts are no longer asked to navigate.
  if (IsBrowserSideNavigationEnabled()) {
    navigation_data_.reset(new NavigationMetricsData(navigation_start, dest_url,
                                                     entry.restore_type()));
    RequestNavigation(frame_tree_node, dest_url, dest_referrer, frame_entry,
                      entry, reload_type, lofi_state,
                      is_same_document_history_load,
                      is_history_navigation_in_new_child, navigation_start);
    if (frame_tree_node->IsMainFrame() &&
        frame_tree_node->navigation_request()) {
      // TODO(carlosk): extend these traces to support subframes and
      // non-PlzNavigate navigations.
      // For the trace below we're using the navigation handle as the async
      // trace id, |navigation_start| as the timestamp and reporting the
      // FrameTreeNode id as a parameter. For navigations where no network
      // request is made (data URLs, JavaScript URLs, etc) there is no handle
      // and so no tracing is done.
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
          "navigation", "Navigation timeToNetworkStack",
          frame_tree_node->navigation_request()->navigation_handle(),
          navigation_start,
          "FrameTreeNode id", frame_tree_node->frame_tree_node_id());
    }

  } else {
    RenderFrameHostImpl* dest_render_frame_host =
        frame_tree_node->render_manager()->Navigate(
            dest_url, frame_entry, entry, reload_type != ReloadType::NONE);
    if (!dest_render_frame_host)
      return false;  // Unable to create the desired RenderFrameHost.

    // Make sure no code called via RFHM::Navigate clears the pending entry.
    if (is_pending_entry)
      CHECK_EQ(controller_->GetPendingEntry(), &entry);

    // For security, we should never send non-Web-UI URLs to a Web UI renderer.
    // Double check that here.
    CheckWebUIRendererDoesNotDisplayNormalURL(dest_render_frame_host, dest_url);

    // In the case of a transfer navigation, set the destination
    // RenderFrameHost as loading.  This ensures that the RenderFrameHost gets
    // in a loading state without emitting a spurious DidStartLoading
    // notification at the FrameTreeNode level (since the FrameTreeNode was
    // already loading). Note that this works both for a transfer to a
    // different RenderFrameHost and in the rare case where the navigation is
    // transferred back to the same RenderFrameHost.
    bool is_transfer = entry.transferred_global_request_id().child_id != -1;
    if (is_transfer)
      dest_render_frame_host->set_is_loading(true);

    // A session history navigation should have been accompanied by state.
    // TODO(creis): This is known to be failing in UseSubframeNavigationEntries
    // in https://crbug.com/568703, when the PageState on a FrameNavigationEntry
    // is unexpectedly empty.  Until the cause is found, keep this as a DCHECK
    // and load the URL without PageState.
    if (is_pending_entry && controller_->GetPendingEntryIndex() != -1)
      DCHECK(frame_entry.page_state().IsValid());

    // Navigate in the desired RenderFrameHost.
    // We can skip this step in the rare case that this is a transfer navigation
    // which began in the chosen RenderFrameHost, since the request has already
    // been issued.  In that case, simply resume the response.
    bool is_transfer_to_same =
        is_transfer &&
        entry.transferred_global_request_id().child_id ==
            dest_render_frame_host->GetProcess()->GetID();
    if (!is_transfer_to_same) {
      navigation_data_.reset(new NavigationMetricsData(
          navigation_start, dest_url, entry.restore_type()));
      // Create the navigation parameters.
      FrameMsg_Navigate_Type::Value navigation_type = GetNavigationType(
          controller_->GetBrowserContext(), entry, reload_type);
      dest_render_frame_host->Navigate(
          entry.ConstructCommonNavigationParams(
              frame_entry, post_body, dest_url, dest_referrer, navigation_type,
              lofi_state, navigation_start),
          entry.ConstructStartNavigationParams(),
          entry.ConstructRequestNavigationParams(
              frame_entry, is_same_document_history_load,
              is_history_navigation_in_new_child,
              entry.GetSubframeUniqueNames(frame_tree_node),
              frame_tree_node->has_committed_real_load(),
              controller_->GetPendingEntryIndex() == -1,
              controller_->GetIndexOfEntry(&entry),
              controller_->GetLastCommittedEntryIndex(),
              controller_->GetEntryCount()));
    } else {
      dest_render_frame_host->navigation_handle()->set_is_transferring(false);
    }
  }

  // Make sure no code called via RFH::Navigate clears the pending entry.
  if (is_pending_entry)
    CHECK_EQ(controller_->GetPendingEntry(), &entry);

  if (controller_->GetPendingEntryIndex() == -1 &&
      dest_url.SchemeIs(url::kJavaScriptScheme)) {
    // If the pending entry index is -1 (which means a new navigation rather
    // than a history one), and the user typed in a javascript: URL, don't add
    // it to the session history.
    //
    // This is a hack. What we really want is to avoid adding to the history any
    // URL that doesn't generate content, and what would be great would be if we
    // had a message from the renderer telling us that a new page was not
    // created. The same message could be used for mailto: URLs and the like.
    return false;
  }

  // Notify observers about navigation.
  if (delegate_ && is_pending_entry)
    delegate_->DidStartNavigationToPendingEntry(dest_url, reload_type);

  return true;
}

bool NavigatorImpl::NavigateToPendingEntry(
    FrameTreeNode* frame_tree_node,
    const FrameNavigationEntry& frame_entry,
    ReloadType reload_type,
    bool is_same_document_history_load) {
  return NavigateToEntry(frame_tree_node, frame_entry,
                         *controller_->GetPendingEntry(), reload_type,
                         is_same_document_history_load, false, true, nullptr);
}

bool NavigatorImpl::NavigateNewChildFrame(
    RenderFrameHostImpl* render_frame_host,
    const GURL& default_url) {
  NavigationEntryImpl* entry =
      controller_->GetEntryWithUniqueID(render_frame_host->nav_entry_id());
  if (!entry)
    return false;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(render_frame_host->frame_tree_node());
  if (!frame_entry)
    return false;

  // Track how often history navigations load a different URL into a subframe
  // than the frame's default URL.
  bool restoring_different_url = frame_entry->url() != default_url;
  UMA_HISTOGRAM_BOOLEAN("SessionRestore.RestoredSubframeURL",
                        restoring_different_url);
  // If this frame's unique name uses a frame path, record the name length.
  // If these names are long in practice, then a proposed plan to truncate
  // unique names might affect restore behavior, since it is complex to deal
  // with truncated names inside frame paths.
  if (restoring_different_url) {
    const std::string& unique_name =
        render_frame_host->frame_tree_node()->unique_name();
    const char kFramePathPrefix[] = "<!--framePath ";
    if (base::StartsWith(unique_name, kFramePathPrefix,
                         base::CompareCase::SENSITIVE)) {
      UMA_HISTOGRAM_COUNTS("SessionRestore.RestoreSubframeFramePathLength",
                           unique_name.size());
    }
  }

  return NavigateToEntry(render_frame_host->frame_tree_node(), *frame_entry,
                         *entry, ReloadType::NONE, false, true, false, nullptr);
}

void NavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    std::unique_ptr<NavigationHandleImpl> navigation_handle) {
  FrameTree* frame_tree = render_frame_host->frame_tree_node()->frame_tree();
  bool oopifs_possible = SiteIsolationPolicy::AreCrossProcessFramesPossible();

  bool has_embedded_credentials =
      params.url.has_username() || params.url.has_password();
  UMA_HISTOGRAM_BOOLEAN("Navigation.FrameHasEmbeddedCredentials",
                        has_embedded_credentials);

  bool is_navigation_within_page = controller_->IsURLInPageNavigation(
      params.url, params.origin, params.was_within_same_page,
      render_frame_host);

  // If a frame claims it navigated within page, it must be the current frame,
  // not a pending one.
  if (is_navigation_within_page &&
      render_frame_host !=
          render_frame_host->frame_tree_node()
              ->render_manager()
              ->current_frame_host()) {
    bad_message::ReceivedBadMessage(render_frame_host->GetProcess(),
                                    bad_message::NI_IN_PAGE_NAVIGATION);
    is_navigation_within_page = false;
  }

  if (ui::PageTransitionIsMainFrame(params.transition)) {
    if (delegate_) {
      // When overscroll navigation gesture is enabled, a screenshot of the page
      // in its current state is taken so that it can be used during the
      // nav-gesture. It is necessary to take the screenshot here, before
      // calling RenderFrameHostManager::DidNavigateMainFrame, because that can
      // change WebContents::GetRenderViewHost to return the new host, instead
      // of the one that may have just been swapped out.
      if (delegate_->CanOverscrollContent()) {
        // Don't take screenshots if we are staying on the same page. We want
        // in-page navigations to be super fast, and taking a screenshot
        // currently blocks GPU for a longer time than we are willing to
        // tolerate in this use case.
        if (!params.was_within_same_page)
          controller_->TakeScreenshot();
      }

      // Run tasks that must execute just before the commit.
      delegate_->DidNavigateMainFramePreCommit(is_navigation_within_page);

      UMA_HISTOGRAM_BOOLEAN("Navigation.MainFrameHasEmbeddedCredentials",
                            has_embedded_credentials);
    }

    if (!oopifs_possible)
      frame_tree->root()->render_manager()->DidNavigateFrame(
          render_frame_host, params.gesture == NavigationGestureUser);
  }

  // Save the origin of the new page.  Do this before calling
  // DidNavigateFrame(), because the origin needs to be included in the SwapOut
  // message, which is sent inside DidNavigateFrame().  SwapOut needs the
  // origin because it creates a RenderFrameProxy that needs this to initialize
  // its security context. This origin will also be sent to RenderFrameProxies
  // created via mojom::Renderer::CreateView and
  // mojom::Renderer::CreateFrameProxy.
  render_frame_host->frame_tree_node()->SetCurrentOrigin(
      params.origin, params.has_potentially_trustworthy_unique_origin);

  render_frame_host->frame_tree_node()->SetInsecureRequestPolicy(
      params.insecure_request_policy);

  // Navigating to a new location means a new, fresh set of http headers and/or
  // <meta> elements - we need to reset CSP policy to an empty set.
  if (!is_navigation_within_page)
    render_frame_host->frame_tree_node()->ResetContentSecurityPolicy();

  // When using --site-per-process, we notify the RFHM for all navigations,
  // not just main frame navigations.
  if (oopifs_possible) {
    FrameTreeNode* frame = render_frame_host->frame_tree_node();
    frame->render_manager()->DidNavigateFrame(
        render_frame_host, params.gesture == NavigationGestureUser);
  }

  // Update the site of the SiteInstance if it doesn't have one yet, unless
  // assigning a site is not necessary for this URL.  In that case, the
  // SiteInstance can still be considered unused until a navigation to a real
  // page.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance->HasSite() &&
      ShouldAssignSiteForURL(params.url)) {
    site_instance->SetSite(params.url);
  }

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  // TODO(nasko): Verify the correctness of the above comment, since some of the
  // code doesn't exist anymore. Also, move this code in the
  // PageTransitionIsMainFrame code block above.
  if (ui::PageTransitionIsMainFrame(params.transition) && delegate_)
    delegate_->SetMainFrameMimeType(params.contents_mime_type);

  int old_entry_count = controller_->GetEntryCount();
  LoadCommittedDetails details;
  bool did_navigate = controller_->RendererDidNavigate(
      render_frame_host, params, &details, is_navigation_within_page,
      navigation_handle.get());

  // If the history length and/or offset changed, update other renderers in the
  // FrameTree.
  if (old_entry_count != controller_->GetEntryCount() ||
      details.previous_entry_index !=
          controller_->GetLastCommittedEntryIndex()) {
    frame_tree->root()->render_manager()->SendPageMessage(
        new PageMsg_SetHistoryOffsetAndLength(
            MSG_ROUTING_NONE, controller_->GetLastCommittedEntryIndex(),
            controller_->GetEntryCount()),
        site_instance);
  }

  // Keep track of each frame's URL in its FrameTreeNode, whether it's for a net
  // error or not.
  // TODO(creis): Move the last committed URL to RenderFrameHostImpl.
  render_frame_host->frame_tree_node()->SetCurrentURL(params.url);

  // Separately, update the frame's last successful URL except for net error
  // pages, since those do not end up in the correct process after transfers
  // (see https://crbug.com/560511).  Instead, the next cross-process navigation
  // or transfer should decide whether to swap as if the net error had not
  // occurred.
  // TODO(creis): Remove this block and always set the URL once transfers handle
  // network errors or PlzNavigate is enabled.  See https://crbug.com/588314.
  if (!params.url_is_unreachable)
    render_frame_host->set_last_successful_url(params.url);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NAVIGATION_TYPE_NAV_IGNORE && delegate_) {
    DCHECK_EQ(!render_frame_host->GetParent(),
              did_navigate ? details.is_main_frame : false);
    ui::PageTransition transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_->GetLastCommittedEntry()->GetTransitionType() &
            ui::PAGE_TRANSITION_FORWARD_BACK)) {
      transition_type = ui::PageTransitionFromInt(
          params.transition | ui::PAGE_TRANSITION_FORWARD_BACK);
    }

    delegate_->DidCommitProvisionalLoad(render_frame_host,
                                        params.url,
                                        transition_type);
    navigation_handle->DidCommitNavigation(params, is_navigation_within_page,
                                           render_frame_host);
    navigation_handle.reset();
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // TODO(carlosk): Move this out when PlzNavigate implementation properly calls
  // the observer methods.
  RecordNavigationMetrics(details, params, site_instance);

  // Run post-commit tasks.
  if (delegate_) {
    if (details.is_main_frame) {
      delegate_->DidNavigateMainFramePostCommit(render_frame_host,
                                                details, params);
    }

    delegate_->DidNavigateAnyFramePostCommit(
        render_frame_host, details, params);
  }
}

bool NavigatorImpl::ShouldAssignSiteForURL(const GURL& url) {
  // about:blank should not "use up" a new SiteInstance.  The SiteInstance can
  // still be used for a normal web site.
  if (url == url::kAboutBlankURL)
    return false;

  // The embedder will then have the opportunity to determine if the URL
  // should "use up" the SiteInstance.
  return GetContentClient()->browser()->ShouldAssignSiteForURL(url);
}

void NavigatorImpl::RequestOpenURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    bool uses_post,
    const scoped_refptr<ResourceRequestBodyImpl>& body,
    const std::string& extra_headers,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    bool should_replace_current_entry,
    bool user_gesture) {
  // Note: This can be called for subframes (even when OOPIFs are not possible)
  // if the disposition calls for a different window.

  // Only the current RenderFrameHost should be sending an OpenURL request.
  // Pending RenderFrameHost should know where it is navigating and pending
  // deletion RenderFrameHost shouldn't be trying to navigate.
  if (render_frame_host !=
      render_frame_host->frame_tree_node()->current_frame_host()) {
    return;
  }

  SiteInstance* current_site_instance = render_frame_host->GetSiteInstance();

  // TODO(creis): Pass the redirect_chain into this method to support client
  // redirects.  http://crbug.com/311721.
  std::vector<GURL> redirect_chain;

  // Note that unlike RequestTransferURL, this uses the navigating
  // RenderFrameHost's current SiteInstance, as that's where this navigation
  // originated.
  GURL dest_url(url);
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(
          current_site_instance, url)) {
    dest_url = GURL(url::kAboutBlankURL);
  }

  int frame_tree_node_id = -1;

  // Send the navigation to the current FrameTreeNode if it's destined for a
  // subframe in the current tab.  We'll assume it's for the main frame
  // (possibly of a new or different WebContents) otherwise.
  if (SiteIsolationPolicy::UseSubframeNavigationEntries() &&
      disposition == WindowOpenDisposition::CURRENT_TAB &&
      render_frame_host->GetParent()) {
    frame_tree_node_id =
        render_frame_host->frame_tree_node()->frame_tree_node_id();
  }

  OpenURLParams params(dest_url, referrer, frame_tree_node_id, disposition,
                       ui::PAGE_TRANSITION_LINK,
                       true /* is_renderer_initiated */);
  params.uses_post = uses_post;
  params.post_data = body;
  params.extra_headers = extra_headers;
  if (redirect_chain.size() > 0)
    params.redirect_chain = redirect_chain;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = user_gesture;

  // RequestOpenURL is used only for local frames, so we can get here only if
  // the navigation is initiated by a frame in the same SiteInstance as this
  // frame.  Note that navigations on RenderFrameProxies do not use
  // RequestOpenURL and go through RequestTransferURL instead.
  params.source_site_instance = current_site_instance;

  if (render_frame_host->web_ui()) {
    // Note that we hide the referrer for Web UI pages. We don't really want
    // web sites to see a referrer of "chrome://blah" (and some chrome: URLs
    // might have search terms or other stuff we don't want to send to the
    // site), so we send no referrer.
    params.referrer = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    params.is_renderer_initiated = false;
  }

  GetContentClient()->browser()->OverrideNavigationParams(
      current_site_instance, &params.transition, &params.is_renderer_initiated,
      &params.referrer);

  if (delegate_)
    delegate_->RequestOpenURL(render_frame_host, params);
}

void NavigatorImpl::RequestTransferURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    SiteInstance* source_site_instance,
    const std::vector<GURL>& redirect_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    const GlobalRequestID& transferred_global_request_id,
    bool should_replace_current_entry,
    const std::string& method,
    scoped_refptr<ResourceRequestBodyImpl> post_body,
    const std::string& extra_headers) {
  // |method != "POST"| should imply absence of |post_body|.
  if (method != "POST" && post_body) {
    NOTREACHED();
    post_body = nullptr;
  }

  // This call only makes sense for subframes if OOPIFs are possible.
  DCHECK(!render_frame_host->GetParent() ||
         SiteIsolationPolicy::AreCrossProcessFramesPossible());

  // Allow the delegate to cancel the transfer.
  if (!delegate_->ShouldTransferNavigation(
          render_frame_host->frame_tree_node()->IsMainFrame()))
    return;

  GURL dest_url(url);
  Referrer referrer_to_use(referrer);
  FrameTreeNode* node = render_frame_host->frame_tree_node();
  SiteInstance* current_site_instance = render_frame_host->GetSiteInstance();
  // It is important to pass in the source_site_instance if it is available
  // (such as when navigating a proxy).  See https://crbug.com/656752.
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(
          source_site_instance ? source_site_instance : current_site_instance,
          url)) {
    // It is important to return here, rather than rewrite the dest_url to
    // about:blank.  The latter won't actually have any effect when
    // transferring, as NavigateToEntry will think that the transfer is to the
    // same RFH that started the navigation and let the existing navigation
    // (for the disallowed URL) proceed.
    return;
  }

  // TODO(creis): Determine if this transfer started as a browser-initiated
  // navigation.  See https://crbug.com/495161.
  bool is_renderer_initiated = true;
  if (render_frame_host->web_ui()) {
    // Note that we hide the referrer for Web UI pages. We don't really want
    // web sites to see a referrer of "chrome://blah" (and some chrome: URLs
    // might have search terms or other stuff we don't want to send to the
    // site), so we send no referrer.
    referrer_to_use = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    is_renderer_initiated = false;
  }

  GetContentClient()->browser()->OverrideNavigationParams(
      current_site_instance, &page_transition, &is_renderer_initiated,
      &referrer_to_use);

  // Create a NavigationEntry for the transfer, without making it the pending
  // entry.  Subframe transfers should only be possible in OOPIF-enabled modes,
  // and should have a clone of the last committed entry with a
  // FrameNavigationEntry for the target frame.  Main frame transfers should
  // have a new NavigationEntry.
  // TODO(creis): Make this unnecessary by creating (and validating) the params
  // directly, passing them to the destination RenderFrameHost.  See
  // https://crbug.com/536906.
  std::unique_ptr<NavigationEntryImpl> entry;
  if (!node->IsMainFrame()) {
    // Subframe case: create FrameNavigationEntry.
    CHECK(SiteIsolationPolicy::UseSubframeNavigationEntries());
    if (controller_->GetLastCommittedEntry()) {
      entry = controller_->GetLastCommittedEntry()->Clone();
      entry->set_extra_headers(extra_headers);
    } else {
      // If there's no last committed entry, create an entry for about:blank
      // with a subframe entry for our destination.
      // TODO(creis): Ensure this case can't exist in https://crbug.com/524208.
      entry = NavigationEntryImpl::FromNavigationEntry(
          controller_->CreateNavigationEntry(
              GURL(url::kAboutBlankURL), referrer_to_use, page_transition,
              is_renderer_initiated, extra_headers,
              controller_->GetBrowserContext()));
    }
    entry->AddOrUpdateFrameEntry(
        node, -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance),
        dest_url, referrer_to_use, redirect_chain, PageState(), method,
        -1);
  } else {
    // Main frame case.
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller_->CreateNavigationEntry(
            dest_url, referrer_to_use, page_transition, is_renderer_initiated,
            extra_headers, controller_->GetBrowserContext()));
    entry->root_node()->frame_entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(source_site_instance));
    entry->SetRedirectChain(redirect_chain);
  }

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  if (should_replace_current_entry && controller_->GetEntryCount() > 0)
    entry->set_should_replace_entry(true);
  if (controller_->GetLastCommittedEntry() &&
      controller_->GetLastCommittedEntry()->GetIsOverridingUserAgent()) {
    entry->SetIsOverridingUserAgent(true);
  }
  entry->set_transferred_global_request_id(transferred_global_request_id);
  // TODO(creis): Set user gesture and intent received timestamp on Android.

  // We may not have successfully added the FrameNavigationEntry to |entry|
  // above (per https://crbug.com/608402), in which case we create it from
  // scratch.  This works because we do not depend on |frame_entry| being inside
  // |entry| during NavigateToEntry.  This will go away when we shortcut this
  // further in https://crbug.com/536906.
  scoped_refptr<FrameNavigationEntry> frame_entry(entry->GetFrameEntry(node));
  if (!frame_entry) {
    frame_entry = new FrameNavigationEntry(
        node->unique_name(), -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), dest_url,
        referrer_to_use, method, -1);
  }
  NavigateToEntry(node, *frame_entry, *entry.get(), ReloadType::NONE, false,
                  false, false, post_body);
}

// PlzNavigate
void NavigatorImpl::OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                                      bool proceed) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();

  // The NavigationRequest may have been canceled while the renderer was
  // executing the BeforeUnload event.
  if (!navigation_request)
    return;

  DCHECK_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            navigation_request->state());

  // If the navigation is allowed to proceed, send the request to the IO thread.
  if (proceed)
    navigation_request->BeginNavigation();
  else
    CancelNavigation(frame_tree_node);
}

// PlzNavigate
void NavigatorImpl::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params) {
  // TODO(clamy): the url sent by the renderer should be validated with
  // FilterURL.
  // This is a renderer-initiated navigation.
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  NavigationRequest* ongoing_navigation_request =
      frame_tree_node->navigation_request();

  // The renderer-initiated navigation request is ignored iff a) there is an
  // ongoing request b) which is browser or user-initiated and c) the renderer
  // request is not user-initiated.
  if (ongoing_navigation_request &&
      (ongoing_navigation_request->browser_initiated() ||
       ongoing_navigation_request->begin_params().has_user_gesture) &&
      !begin_params.has_user_gesture) {
    RenderFrameHost* current_frame_host =
        frame_tree_node->render_manager()->current_frame_host();
    current_frame_host->Send(
        new FrameMsg_Stop(current_frame_host->GetRoutingID()));
    return;
  }

  // In all other cases the current navigation, if any, is canceled and a new
  // NavigationRequest is created for the node.
  frame_tree_node->CreatedNavigationRequest(
      NavigationRequest::CreateRendererInitiated(
          frame_tree_node, common_params, begin_params,
          controller_->GetLastCommittedEntryIndex(),
          controller_->GetEntryCount()));
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  if (frame_tree_node->IsMainFrame()) {
    // Renderer-initiated main-frame navigations that need to swap processes
    // will go to the browser via a OpenURL call, and then be handled by the
    // same code path as browser-initiated navigations. For renderer-initiated
    // main frame navigation that start via a BeginNavigation IPC, the
    // RenderFrameHost will not be swapped. Therefore it is safe to call
    // DidStartMainFrameNavigation with the SiteInstance from the current
    // RenderFrameHost.
    DidStartMainFrameNavigation(
        common_params.url,
        frame_tree_node->current_frame_host()->GetSiteInstance(), nullptr);
    navigation_data_.reset();
  }

  // For main frames, NavigationHandle will be created after the call to
  // |DidStartMainFrameNavigation|, so it receives the most up to date pending
  // entry from the NavigationController.
  NavigationEntry* pending_entry = controller_->GetPendingEntry();
  navigation_request->CreateNavigationHandle(
      pending_entry ? pending_entry->GetUniqueID() : 0);
  navigation_request->BeginNavigation();
}

// PlzNavigate
void NavigatorImpl::FailedNavigation(FrameTreeNode* frame_tree_node,
                                     bool has_stale_copy_in_cache,
                                     int error_code) {
  CHECK(IsBrowserSideNavigationEnabled());

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  DCHECK(navigation_request);

  // With PlzNavigate, debug URLs will give a failed navigation because the
  // WebUI backend won't find a handler for them. They will be processed in the
  // renderer, however do not discard the pending entry so that the URL bar
  // shows them correctly.
  if (!IsRendererDebugURL(navigation_request->navigation_handle()->GetURL()))
    DiscardPendingEntryIfNeeded(navigation_request->navigation_handle());

  // If the request was canceled by the user do not show an error page.
  if (error_code == net::ERR_ABORTED) {
    frame_tree_node->ResetNavigationRequest(false);
    return;
  }

  // Select an appropriate renderer to show the error page.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node->render_manager()->GetFrameHostForNavigation(
          *navigation_request);
  CheckWebUIRendererDoesNotDisplayNormalURL(
      render_frame_host, navigation_request->common_params().url);

  navigation_request->TransferNavigationHandleOwnership(render_frame_host);
  render_frame_host->navigation_handle()->ReadyToCommitNavigation(
      render_frame_host);
  render_frame_host->FailedNavigation(navigation_request->common_params(),
                                      navigation_request->request_params(),
                                      has_stale_copy_in_cache, error_code);
}

// PlzNavigate
void NavigatorImpl::CancelNavigation(FrameTreeNode* frame_tree_node) {
  CHECK(IsBrowserSideNavigationEnabled());
  frame_tree_node->ResetNavigationRequest(false);
  if (frame_tree_node->IsMainFrame())
    navigation_data_.reset();
}

void NavigatorImpl::LogResourceRequestTime(
    base::TimeTicks timestamp, const GURL& url) {
  if (navigation_data_ && navigation_data_->url_ == url) {
    navigation_data_->url_job_start_time_ = timestamp;
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart",
        navigation_data_->url_job_start_time_ - navigation_data_->start_time_);
  }
}

void NavigatorImpl::LogBeforeUnloadTime(
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // Only stores the beforeunload delay if we're tracking a browser initiated
  // navigation and it happened later than the navigation request.
  if (navigation_data_ &&
      renderer_before_unload_start_time > navigation_data_->start_time_) {
    navigation_data_->before_unload_delay_ =
        renderer_before_unload_end_time - renderer_before_unload_start_time;
  }
}

void NavigatorImpl::DiscardPendingEntryIfNeeded(NavigationHandleImpl* handle) {
  // Racy conditions can cause a fail message to arrive after its corresponding
  // pending entry has been replaced by another navigation. If
  // |DiscardPendingEntry| is called in this case, then the completely valid
  // entry for the new navigation would be discarded. See crbug.com/513742. To
  // catch this case, the current pending entry is compared against the current
  // navigation handle's entry id, which should correspond to the failed load.
  NavigationEntry* pending_entry = controller_->GetPendingEntry();
  bool pending_matches_fail_msg =
      handle && pending_entry &&
      handle->pending_nav_entry_id() == pending_entry->GetUniqueID();
  if (!pending_matches_fail_msg)
    return;

  // We usually clear the pending entry when it fails, so that an arbitrary URL
  // isn't left visible above a committed page. This must be enforced when the
  // pending entry isn't visible (e.g., renderer-initiated navigations) to
  // prevent URL spoofs for in-page navigations that don't go through
  // DidStartProvisionalLoadForFrame.
  //
  // However, we do preserve the pending entry in some cases, such as on the
  // initial navigation of an unmodified blank tab. We also allow the delegate
  // to say when it's safe to leave aborted URLs in the omnibox, to let the
  // user edit the URL and try again. This may be useful in cases that the
  // committed page cannot be attacker-controlled. In these cases, we still
  // allow the view to clear the pending entry and typed URL if the user
  // requests (e.g., hitting Escape with focus in the address bar).
  //
  // Note: don't touch the transient entry, since an interstitial may exist.
  bool should_preserve_entry = controller_->IsUnmodifiedBlankTab() ||
                               delegate_->ShouldPreserveAbortedURLs();
  if (pending_entry != controller_->GetVisibleEntry() ||
      !should_preserve_entry) {
    controller_->DiscardPendingEntry(true);

    // Also force the UI to refresh.
    controller_->delegate()->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
  }
}

// PlzNavigate
void NavigatorImpl::RequestNavigation(FrameTreeNode* frame_tree_node,
                                      const GURL& dest_url,
                                      const Referrer& dest_referrer,
                                      const FrameNavigationEntry& frame_entry,
                                      const NavigationEntryImpl& entry,
                                      ReloadType reload_type,
                                      LoFiState lofi_state,
                                      bool is_same_document_history_load,
                                      bool is_history_navigation_in_new_child,
                                      base::TimeTicks navigation_start) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  // This value must be set here because creating a NavigationRequest might
  // change the renderer live/non-live status and change this result.
  bool should_dispatch_beforeunload =
      !is_same_document_history_load &&
      frame_tree_node->current_frame_host()->ShouldDispatchBeforeUnload();
  FrameMsg_Navigate_Type::Value navigation_type =
      GetNavigationType(controller_->GetBrowserContext(), entry, reload_type);
  std::unique_ptr<NavigationRequest> scoped_request =
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, dest_url, dest_referrer, frame_entry, entry,
          navigation_type, lofi_state, is_same_document_history_load,
          is_history_navigation_in_new_child, navigation_start, controller_);
  NavigationRequest* navigation_request = scoped_request.get();

  // Navigation to a javascript URL is not a "real" navigation so there is no
  // need to create a NavigationHandle. The navigation commits immediately and
  // the NavigationRequest is not assigned to the FrameTreeNode as navigating to
  // a Javascript URL should not interrupt a previous navigation.
  // Note: The scoped_request will be destroyed at the end of this function.
  if (dest_url.SchemeIs(url::kJavaScriptScheme)) {
    RenderFrameHostImpl* render_frame_host =
        frame_tree_node->render_manager()->GetFrameHostForNavigation(
            *navigation_request);
    render_frame_host->CommitNavigation(nullptr,  // response
                                        nullptr,  // body
                                        navigation_request->common_params(),
                                        navigation_request->request_params(),
                                        navigation_request->is_view_source());
    return;
  }

  frame_tree_node->CreatedNavigationRequest(std::move(scoped_request));
  navigation_request->CreateNavigationHandle(entry.GetUniqueID());

  // Have the current renderer execute its beforeunload event if needed. If it
  // is not needed then NavigationRequest::BeginNavigation should be directly
  // called instead.
  if (should_dispatch_beforeunload && !IsRendererDebugURL(dest_url)) {
    navigation_request->SetWaitingForRendererResponse();
    frame_tree_node->current_frame_host()->DispatchBeforeUnload(
        true, reload_type != ReloadType::NONE);
  } else {
    navigation_request->BeginNavigation();
  }
}

void NavigatorImpl::RecordNavigationMetrics(
    const LoadCommittedDetails& details,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    SiteInstance* site_instance) {
  DCHECK(site_instance->HasProcess());

  if (!details.is_main_frame || !navigation_data_ ||
      navigation_data_->url_job_start_time_.is_null() ||
      navigation_data_->url_ != params.original_request_url) {
    return;
  }

  base::TimeDelta time_to_commit =
      base::TimeTicks::Now() - navigation_data_->start_time_;
  UMA_HISTOGRAM_TIMES("Navigation.TimeToCommit", time_to_commit);

  time_to_commit -= navigation_data_->before_unload_delay_;
  base::TimeDelta time_to_network = navigation_data_->url_job_start_time_ -
                                    navigation_data_->start_time_ -
                                    navigation_data_->before_unload_delay_;
  if (navigation_data_->is_restoring_from_last_session_) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_SessionRestored_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_SessionRestored_BeforeUnloadDiscounted",
        time_to_network);
    navigation_data_.reset();
    return;
  }
  bool navigation_created_new_renderer_process =
      site_instance->GetProcess()->GetInitTimeForNavigationMetrics() >
      navigation_data_->start_time_;
  if (navigation_created_new_renderer_process) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_NewRenderer_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_NewRenderer_BeforeUnloadDiscounted",
        time_to_network);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_ExistingRenderer_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_ExistingRenderer_BeforeUnloadDiscounted",
        time_to_network);
  }
  navigation_data_.reset();
}

void NavigatorImpl::DidStartMainFrameNavigation(
    const GURL& url,
    SiteInstanceImpl* site_instance,
    NavigationHandleImpl* navigation_handle) {
  // If there is no browser-initiated pending entry for this navigation and it
  // is not for the error URL, create a pending entry using the current
  // SiteInstance, and ensure the address bar updates accordingly.  We don't
  // know the referrer or extra headers at this point, but the referrer will
  // be set properly upon commit.
  NavigationEntryImpl* pending_entry = controller_->GetPendingEntry();
  bool has_browser_initiated_pending_entry =
      pending_entry && !pending_entry->is_renderer_initiated();

  // If there is a transient entry, creating a new pending entry will result
  // in deleting it, which leads to inconsistent state.
  bool has_transient_entry = !!controller_->GetTransientEntry();

  if (!has_browser_initiated_pending_entry && !has_transient_entry) {
    std::unique_ptr<NavigationEntryImpl> entry =
        NavigationEntryImpl::FromNavigationEntry(
            controller_->CreateNavigationEntry(
                url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                true /* is_renderer_initiated */, std::string(),
                controller_->GetBrowserContext()));
    entry->set_site_instance(site_instance);
    // TODO(creis): If there's a pending entry already, find a safe way to
    // update it instead of replacing it and copying over things like this.
    // That will allow us to skip the NavigationHandle update below as well.
    if (pending_entry) {
      entry->set_transferred_global_request_id(
          pending_entry->transferred_global_request_id());
      entry->set_should_replace_entry(pending_entry->should_replace_entry());
      entry->SetRedirectChain(pending_entry->GetRedirectChain());
    }

    // If there's a current NavigationHandle, update its pending NavEntry ID.
    // This is necessary for transfer navigations.  The handle may be null in
    // PlzNavigate.
    if (navigation_handle)
      navigation_handle->update_entry_id_for_transfer(entry->GetUniqueID());

    controller_->SetPendingEntry(std::move(entry));
    if (delegate_)
      delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);
  }
}

}  // namespace content
