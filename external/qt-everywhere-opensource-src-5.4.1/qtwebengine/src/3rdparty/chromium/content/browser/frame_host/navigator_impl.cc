// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include "base/command_line.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

namespace content {

namespace {

FrameMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context, const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationControllerImpl::RELOAD:
      return FrameMsg_Navigate_Type::RELOAD;
    case NavigationControllerImpl::RELOAD_IGNORING_CACHE:
      return FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE;
    case NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL:
      return FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case NavigationControllerImpl::NO_RELOAD:
      break;  // Fall through to rest of function.
  }

  // |RenderViewImpl::PopulateStateFromPendingNavigationParams| differentiates
  // between |RESTORE_WITH_POST| and |RESTORE|.
  if (entry.restore_type() ==
      NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return FrameMsg_Navigate_Type::RESTORE_WITH_POST;
    return FrameMsg_Navigate_Type::RESTORE;
  }

  return FrameMsg_Navigate_Type::NORMAL;
}

void MakeNavigateParams(const NavigationEntryImpl& entry,
                        const NavigationControllerImpl& controller,
                        NavigationController::ReloadType reload_type,
                        FrameMsg_Navigate_Params* params) {
  params->page_id = entry.GetPageID();
  params->should_clear_history_list = entry.should_clear_history_list();
  params->should_replace_current_entry = entry.should_replace_entry();
  if (entry.should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    params->pending_history_list_offset = -1;
    params->current_history_list_offset = -1;
    params->current_history_list_length = 0;
  } else {
    params->pending_history_list_offset = controller.GetIndexOfEntry(&entry);
    params->current_history_list_offset =
        controller.GetLastCommittedEntryIndex();
    params->current_history_list_length = controller.GetEntryCount();
  }
  params->url = entry.GetURL();
  if (!entry.GetBaseURLForDataURL().is_empty()) {
    params->base_url_for_data_url = entry.GetBaseURLForDataURL();
    params->history_url_for_data_url = entry.GetVirtualURL();
  }
  params->referrer = entry.GetReferrer();
  params->transition = entry.GetTransitionType();
  params->page_state = entry.GetPageState();
  params->navigation_type =
      GetNavigationType(controller.GetBrowserContext(), entry, reload_type);
  params->request_time = base::Time::Now();
  params->extra_headers = entry.extra_headers();
  params->transferred_request_child_id =
      entry.transferred_global_request_id().child_id;
  params->transferred_request_request_id =
      entry.transferred_global_request_id().request_id;
  params->is_overriding_user_agent = entry.GetIsOverridingUserAgent();
  // Avoid downloading when in view-source mode.
  params->allow_download = !entry.IsViewSourceMode();
  params->is_post = entry.GetHasPostData();
  if (entry.GetBrowserInitiatedPostData()) {
    params->browser_initiated_post_data.assign(
        entry.GetBrowserInitiatedPostData()->front(),
        entry.GetBrowserInitiatedPostData()->front() +
            entry.GetBrowserInitiatedPostData()->size());
  }

  // Set the redirect chain to the navigation's redirects, unless we are
  // returning to a completed navigation (whose previous redirects don't apply).
  if (PageTransitionIsNewNavigation(params->transition)) {
    params->redirects = entry.GetRedirectChain();
  } else {
    params->redirects.clear();
  }

  params->can_load_local_resources = entry.GetCanLoadLocalResources();
  params->frame_to_navigate = entry.GetFrameToNavigate();
}

RenderFrameHostManager* GetRenderManager(RenderFrameHostImpl* rfh) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess))
    return rfh->frame_tree_node()->render_manager();

  return rfh->frame_tree_node()->frame_tree()->root()->render_manager();
}

}  // namespace


NavigatorImpl::NavigatorImpl(
    NavigationControllerImpl* navigation_controller,
    NavigatorDelegate* delegate)
    : controller_(navigation_controller),
      delegate_(delegate) {
}

NavigationController* NavigatorImpl::GetController() {
  return controller_;
}

void NavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    int parent_routing_id,
    const GURL& url) {
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  bool is_main_frame = render_frame_host->frame_tree_node()->IsMainFrame();
  NavigationEntryImpl* pending_entry =
      NavigationEntryImpl::FromNavigationEntry(controller_->GetPendingEntry());
  if (is_main_frame) {
    // If there is no browser-initiated pending entry for this navigation and it
    // is not for the error URL, create a pending entry using the current
    // SiteInstance, and ensure the address bar updates accordingly.  We don't
    // know the referrer or extra headers at this point, but the referrer will
    // be set properly upon commit.
    bool has_browser_initiated_pending_entry = pending_entry &&
        !pending_entry->is_renderer_initiated();
    if (!has_browser_initiated_pending_entry && !is_error_page) {
      NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
          controller_->CreateNavigationEntry(validated_url,
                                             content::Referrer(),
                                             content::PAGE_TRANSITION_LINK,
                                             true /* is_renderer_initiated */,
                                             std::string(),
                                             controller_->GetBrowserContext()));
      entry->set_site_instance(
          static_cast<SiteInstanceImpl*>(
              render_frame_host->render_view_host()->GetSiteInstance()));
      // TODO(creis): If there's a pending entry already, find a safe way to
      // update it instead of replacing it and copying over things like this.
      if (pending_entry) {
        entry->set_transferred_global_request_id(
            pending_entry->transferred_global_request_id());
        entry->set_should_replace_entry(pending_entry->should_replace_entry());
        entry->SetRedirectChain(pending_entry->GetRedirectChain());
      }
      controller_->SetPendingEntry(entry);
      if (delegate_)
        delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);
    }
  }

  if (delegate_) {
    // Notify the observer about the start of the provisional load.
    delegate_->DidStartProvisionalLoad(
        render_frame_host, parent_routing_id,
        validated_url, is_error_page, is_iframe_srcdoc);
  }
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
    // looks for this WebContents based on a render ID.  Instead, we just
    // leave the pending renderer around until the next navigation event
    // (Navigate, DidNavigate, etc), which will clean it up properly.
    //
    // TODO(creis): Find a way to cancel any pending RFH here.
  }

  // We usually clear the pending entry when it fails, so that an arbitrary URL
  // isn't left visible above a committed page.  This must be enforced when
  // the pending entry isn't visible (e.g., renderer-initiated navigations) to
  // prevent URL spoofs for in-page navigations that don't go through
  // DidStartProvisionalLoadForFrame.
  //
  // However, we do preserve the pending entry in some cases, such as on the
  // initial navigation of an unmodified blank tab.  We also allow the delegate
  // to say when it's safe to leave aborted URLs in the omnibox, to let the user
  // edit the URL and try again.  This may be useful in cases that the committed
  // page cannot be attacker-controlled.  In these cases, we still allow the
  // view to clear the pending entry and typed URL if the user requests
  // (e.g., hitting Escape with focus in the address bar).
  //
  // Note: don't touch the transient entry, since an interstitial may exist.
  bool should_preserve_entry = controller_->IsUnmodifiedBlankTab() ||
      delegate_->ShouldPreserveAbortedURLs();
  if (controller_->GetPendingEntry() != controller_->GetVisibleEntry() ||
      !should_preserve_entry) {
    controller_->DiscardPendingEntry();

    // Also force the UI to refresh.
    controller_->delegate()->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
  }

  if (delegate_)
    delegate_->DidFailProvisionalLoadWithError(render_frame_host, params);
}

void NavigatorImpl::DidFailLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  if (delegate_) {
    delegate_->DidFailLoadWithError(
        render_frame_host, url, error_code,
        error_description);
  }
}

void NavigatorImpl::DidRedirectProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    int32 page_id,
    const GURL& source_url,
    const GURL& target_url) {
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // WebContentsObserver::DidGetRedirectForResourceRequest instead.
  // See http://crbug.com/78512.
  GURL validated_source_url(source_url);
  GURL validated_target_url(target_url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_source_url);
  render_process_host->FilterURL(false, &validated_target_url);
  NavigationEntry* entry;
  if (page_id == -1) {
    entry = controller_->GetPendingEntry();
  } else {
    entry = controller_->GetEntryWithPageID(
        render_frame_host->GetSiteInstance(), page_id);
  }
  if (!entry || entry->GetURL() != validated_source_url)
    return;

  if (delegate_) {
    delegate_->DidRedirectProvisionalLoad(
        render_frame_host, validated_target_url);
  }
}

bool NavigatorImpl::NavigateToEntry(
    RenderFrameHostImpl* render_frame_host,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  TRACE_EVENT0("browser", "NavigatorImpl::NavigateToEntry");

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (entry.GetURL().spec().size() > GetMaxURLChars()) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << GetMaxURLChars()
                 << " characters.";
    return false;
  }

  RenderFrameHostManager* manager =
      render_frame_host->frame_tree_node()->render_manager();
  RenderFrameHostImpl* dest_render_frame_host = manager->Navigate(entry);
  if (!dest_render_frame_host)
    return false;  // Unable to create the desired RenderFrameHost.

  // Make sure no code called via RFHM::Navigate clears the pending entry.
  CHECK_EQ(controller_->GetPendingEntry(), &entry);

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  int enabled_bindings =
      dest_render_frame_host->render_view_host()->GetEnabledBindings();
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          controller_->GetBrowserContext(), entry.GetURL());
  if ((enabled_bindings & BINDINGS_POLICY_WEB_UI) &&
      !is_allowed_in_web_ui_renderer) {
    // Log the URL to help us diagnose any future failures of this CHECK.
    GetContentClient()->SetActiveURL(entry.GetURL());
    CHECK(0);
  }

  // Notify observers that we will navigate in this RenderFrame.
  if (delegate_)
    delegate_->AboutToNavigateRenderFrame(dest_render_frame_host);

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderFrameHost.
  FrameMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, *controller_, reload_type, &navigate_params);
  dest_render_frame_host->Navigate(navigate_params);

  // Make sure no code called via RFH::Navigate clears the pending entry.
  CHECK_EQ(controller_->GetPendingEntry(), &entry);

  if (entry.GetPageID() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.GetURL().SchemeIs(url::kJavaScriptScheme))
      return false;
  }

  // Notify observers about navigation.
  if (delegate_) {
    delegate_->DidStartNavigationToPendingEntry(render_frame_host,
                                                entry.GetURL(),
                                                reload_type);
  }

  return true;
}

bool NavigatorImpl::NavigateToPendingEntry(
    RenderFrameHostImpl* render_frame_host,
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(
      render_frame_host,
      *NavigationEntryImpl::FromNavigationEntry(controller_->GetPendingEntry()),
      reload_type);
}

base::TimeTicks NavigatorImpl::GetCurrentLoadStart() {
  return current_load_start_;
}

void NavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& input_params) {
  FrameHostMsg_DidCommitProvisionalLoad_Params params(input_params);
  FrameTree* frame_tree = render_frame_host->frame_tree_node()->frame_tree();
  bool use_site_per_process =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess);

  if (use_site_per_process) {
    // TODO(creis): Until we mirror the frame tree in the subframe's process,
    // cross-process subframe navigations happen in a renderer's main frame.
    // Correct the transition type here if we know it is for a subframe.
    NavigationEntryImpl* pending_entry =
        NavigationEntryImpl::FromNavigationEntry(
            controller_->GetPendingEntry());
    if (!render_frame_host->frame_tree_node()->IsMainFrame() &&
        pending_entry &&
        pending_entry->frame_tree_node_id() ==
            render_frame_host->frame_tree_node()->frame_tree_node_id()) {
      params.transition = PAGE_TRANSITION_AUTO_SUBFRAME;
    }
  }

  if (PageTransitionIsMainFrame(params.transition)) {
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
      delegate_->DidNavigateMainFramePreCommit(params);
    }

    if (!use_site_per_process)
      frame_tree->root()->render_manager()->DidNavigateFrame(render_frame_host);
  }

  // When using --site-per-process, we notify the RFHM for all navigations,
  // not just main frame navigations.
  if (use_site_per_process) {
    FrameTreeNode* frame = render_frame_host->frame_tree_node();
    frame->render_manager()->DidNavigateFrame(render_frame_host);
  }

  // Update the site of the SiteInstance if it doesn't have one yet, unless
  // assigning a site is not necessary for this URL.  In that case, the
  // SiteInstance can still be considered unused until a navigation to a real
  // page.
  SiteInstanceImpl* site_instance =
      static_cast<SiteInstanceImpl*>(render_frame_host->GetSiteInstance());
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
  if (PageTransitionIsMainFrame(params.transition) && delegate_)
    delegate_->SetMainFrameMimeType(params.contents_mime_type);

  LoadCommittedDetails details;
  bool did_navigate = controller_->RendererDidNavigate(render_frame_host,
                                                       params, &details);

  // For now, keep track of each frame's URL in its FrameTreeNode.  This lets
  // us estimate our process count for implementing OOP iframes.
  // TODO(creis): Remove this when we track which pages commit in each frame.
  render_frame_host->frame_tree_node()->set_current_url(params.url);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NAVIGATION_TYPE_NAV_IGNORE && delegate_) {
    // For AUTO_SUBFRAME navigations, an event for the main frame is generated
    // that is not recorded in the navigation history. For the purpose of
    // tracking navigation events, we treat this event as a sub frame navigation
    // event.
    bool is_main_frame = did_navigate ? details.is_main_frame : false;
    PageTransition transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_->GetLastCommittedEntry()->GetTransitionType() &
            PAGE_TRANSITION_FORWARD_BACK)) {
      transition_type = PageTransitionFromInt(
          params.transition | PAGE_TRANSITION_FORWARD_BACK);
    }

    delegate_->DidCommitProvisionalLoad(render_frame_host,
                                        params.frame_unique_name,
                                        is_main_frame,
                                        params.url,
                                        transition_type);
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // Run post-commit tasks.
  if (delegate_) {
    if (details.is_main_frame)
      delegate_->DidNavigateMainFramePostCommit(details, params);

    delegate_->DidNavigateAnyFramePostCommit(
        render_frame_host, details, params);
  }
}

bool NavigatorImpl::ShouldAssignSiteForURL(const GURL& url) {
  // about:blank should not "use up" a new SiteInstance.  The SiteInstance can
  // still be used for a normal web site.
  if (url == GURL(url::kAboutBlankURL))
    return false;

  // The embedder will then have the opportunity to determine if the URL
  // should "use up" the SiteInstance.
  return GetContentClient()->browser()->ShouldAssignSiteForURL(url);
}

void NavigatorImpl::RequestOpenURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    bool should_replace_current_entry,
    bool user_gesture) {
  SiteInstance* current_site_instance =
      GetRenderManager(render_frame_host)->current_frame_host()->
          GetSiteInstance();
  // If this came from a swapped out RenderViewHost, we only allow the request
  // if we are still in the same BrowsingInstance.
  if (render_frame_host->render_view_host()->IsSwappedOut() &&
      !render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
          current_site_instance)) {
    return;
  }

  // Delegate to RequestTransferURL because this is just the generic
  // case where |old_request_id| is empty.
  // TODO(creis): Pass the redirect_chain into this method to support client
  // redirects.  http://crbug.com/311721.
  std::vector<GURL> redirect_chain;
  RequestTransferURL(
      render_frame_host, url, redirect_chain, referrer, PAGE_TRANSITION_LINK,
      disposition, GlobalRequestID(),
      should_replace_current_entry, user_gesture);
}

void NavigatorImpl::RequestTransferURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const std::vector<GURL>& redirect_chain,
    const Referrer& referrer,
    PageTransition page_transition,
    WindowOpenDisposition disposition,
    const GlobalRequestID& transferred_global_request_id,
    bool should_replace_current_entry,
    bool user_gesture) {
  GURL dest_url(url);
  SiteInstance* current_site_instance =
      GetRenderManager(render_frame_host)->current_frame_host()->
          GetSiteInstance();
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(
          current_site_instance, url)) {
    dest_url = GURL(url::kAboutBlankURL);
  }

  int64 frame_tree_node_id = -1;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess)) {
    frame_tree_node_id =
        render_frame_host->frame_tree_node()->frame_tree_node_id();
  }
  OpenURLParams params(
      dest_url, referrer, frame_tree_node_id, disposition, page_transition,
      true /* is_renderer_initiated */);
  if (redirect_chain.size() > 0)
    params.redirect_chain = redirect_chain;
  params.transferred_global_request_id = transferred_global_request_id;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = user_gesture;

  if (GetRenderManager(render_frame_host)->web_ui()) {
    // Web UI pages sometimes want to override the page transition type for
    // link clicks (e.g., so the new tab page can specify AUTO_BOOKMARK for
    // automatically generated suggestions).  We don't override other types
    // like TYPED because they have different implications (e.g., autocomplete).
    if (PageTransitionCoreTypeIs(params.transition, PAGE_TRANSITION_LINK))
      params.transition =
          GetRenderManager(render_frame_host)->web_ui()->
              GetLinkTransitionType();

    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    params.referrer = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    params.is_renderer_initiated = false;
  }

  if (delegate_)
    delegate_->RequestOpenURL(render_frame_host, params);
}

}  // namespace content
