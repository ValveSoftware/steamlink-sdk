// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

bool leak_requests_for_testing_ = false;

// The parameters to OnCrossSiteResponseHelper exceed the number of arguments
// base::Bind supports.
struct CrossSiteResponseParams {
  CrossSiteResponseParams(
      int render_frame_id,
      const GlobalRequestID& global_request_id,
      bool is_transfer,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      PageTransition page_transition,
      bool should_replace_current_entry)
      : render_frame_id(render_frame_id),
        global_request_id(global_request_id),
        is_transfer(is_transfer),
        transfer_url_chain(transfer_url_chain),
        referrer(referrer),
        page_transition(page_transition),
        should_replace_current_entry(should_replace_current_entry) {
  }

  int render_frame_id;
  GlobalRequestID global_request_id;
  bool is_transfer;
  std::vector<GURL> transfer_url_chain;
  Referrer referrer;
  PageTransition page_transition;
  bool should_replace_current_entry;
};

void OnCrossSiteResponseHelper(const CrossSiteResponseParams& params) {
  scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request;
  if (params.is_transfer) {
    cross_site_transferring_request.reset(new CrossSiteTransferringRequest(
        params.global_request_id));
  }

  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(params.global_request_id.child_id,
                                  params.render_frame_id);
  if (rfh) {
    rfh->OnCrossSiteResponse(
        params.global_request_id, cross_site_transferring_request.Pass(),
        params.transfer_url_chain, params.referrer,
        params.page_transition, params.should_replace_current_entry);
  } else if (leak_requests_for_testing_ && cross_site_transferring_request) {
    // Some unit tests expect requests to be leaked in this case, so they can
    // pass them along manually.
    cross_site_transferring_request->ReleaseRequest();
  }
}

bool CheckNavigationPolicyOnUI(GURL url, int process_id, int render_frame_id) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, render_frame_id);
  if (!rfh)
    return false;

  // TODO(nasko): This check is very simplistic and is used temporarily only
  // for --site-per-process. It should be updated to match the check performed
  // by RenderFrameHostManager::UpdateStateForNavigate.
  return !SiteInstance::IsSameWebSite(
      rfh->GetSiteInstance()->GetBrowserContext(),
      rfh->GetSiteInstance()->GetSiteURL(), url);
}

}  // namespace

CrossSiteResourceHandler::CrossSiteResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, next_handler.Pass()),
      has_started_response_(false),
      in_cross_site_transition_(false),
      completed_during_transition_(false),
      did_defer_(false),
      weak_ptr_factory_(this) {
}

CrossSiteResourceHandler::~CrossSiteResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  GetRequestInfo()->set_cross_site_handler(NULL);
}

bool CrossSiteResourceHandler::OnRequestRedirected(
    const GURL& new_url,
    ResourceResponse* response,
    bool* defer) {
  // Top-level requests change their cookie first-party URL on redirects, while
  // subframes retain the parent's value.
  if (GetRequestInfo()->GetResourceType() == ResourceType::MAIN_FRAME)
    request()->set_first_party_for_cookies(new_url);

  // We should not have started the transition before being redirected.
  DCHECK(!in_cross_site_transition_);
  return next_handler_->OnRequestRedirected(new_url, response, defer);
}

bool CrossSiteResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  // At this point, we know that the response is safe to send back to the
  // renderer: it is not a download, and it has passed the SSL and safe
  // browsing checks.
  // We should not have already started the transition before now.
  DCHECK(!in_cross_site_transition_);
  has_started_response_ = true;

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // We will need to swap processes if either (1) a redirect that requires a
  // transfer occurred before we got here, or (2) a pending cross-site request
  // was already in progress.  Note that a swap may no longer be needed if we
  // transferred back into the original process due to a redirect.
  bool should_transfer =
      GetContentClient()->browser()->ShouldSwapProcessesForRedirect(
          info->GetContext(), request()->original_url(), request()->url());

  // When the --site-per-process flag is passed, we transfer processes for
  // cross-site navigations. This is skipped if a transfer is already required
  // or for WebUI processes for now, since pages like the NTP host multiple
  // cross-site WebUI iframes.
  if (!should_transfer &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess) &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          info->GetChildID())) {
    return DeferForNavigationPolicyCheck(info, response, defer);
  }

  bool swap_needed = should_transfer ||
      CrossSiteRequestManager::GetInstance()->
          HasPendingCrossSiteRequest(info->GetChildID(), info->GetRouteID());

  // If this is a download, just pass the response through without doing a
  // cross-site check.  The renderer will see it is a download and abort the
  // request.
  //
  // Similarly, HTTP 204 (No Content) responses leave us showing the previous
  // page.  We should allow the navigation to finish without running the unload
  // handler or swapping in the pending RenderFrameHost.
  //
  // In both cases, any pending RenderFrameHost (if one was created for this
  // navigation) will stick around until the next cross-site navigation, since
  // we are unable to tell when to destroy it.
  // See RenderFrameHostManager::RendererAbortedProvisionalLoad.
  //
  // TODO(davidben): Unify IsDownload() and is_stream(). Several places need to
  // check for both and remembering about streams is error-prone.
  if (!swap_needed || info->IsDownload() || info->is_stream() ||
      (response->head.headers.get() &&
       response->head.headers->response_code() == 204)) {
    return next_handler_->OnResponseStarted(response, defer);
  }

  // Now that we know a swap is needed and we have something to commit, we
  // pause to let the UI thread run the unload handler of the previous page
  // and set up a transfer if needed.
  StartCrossSiteTransition(response, should_transfer);

  // Defer loading until after the onunload event handler has run.
  *defer = true;
  OnDidDefer();
  return true;
}

void CrossSiteResourceHandler::ResumeOrTransfer(bool is_transfer) {
  if (is_transfer) {
    StartCrossSiteTransition(response_, is_transfer);
  } else {
    ResumeResponse();
  }
}

bool CrossSiteResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  CHECK(!in_cross_site_transition_);
  return next_handler_->OnReadCompleted(bytes_read, defer);
}

void CrossSiteResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  if (!in_cross_site_transition_) {
    ResourceRequestInfoImpl* info = GetRequestInfo();
    // If we've already completed the transition, or we're canceling the
    // request, or an error occurred with no cross-process navigation in
    // progress, then we should just pass this through.
    if (has_started_response_ ||
        status.status() != net::URLRequestStatus::FAILED ||
        !CrossSiteRequestManager::GetInstance()->HasPendingCrossSiteRequest(
            info->GetChildID(), info->GetRouteID())) {
      next_handler_->OnResponseCompleted(status, security_info, defer);
      return;
    }

    // An error occurred. We should wait now for the cross-process transition,
    // so that the error message (e.g., 404) can be displayed to the user.
    // Also continue with the logic below to remember that we completed
    // during the cross-site transition.
    StartCrossSiteTransition(NULL, false);
  }

  // We have to buffer the call until after the transition completes.
  completed_during_transition_ = true;
  completed_status_ = status;
  completed_security_info_ = security_info;

  // Defer to tell RDH not to notify the world or clean up the pending request.
  // We will do so in ResumeResponse.
  *defer = true;
  OnDidDefer();
}

// We can now send the response to the new renderer, which will cause
// WebContentsImpl to swap in the new renderer and destroy the old one.
void CrossSiteResourceHandler::ResumeResponse() {
  DCHECK(request());
  in_cross_site_transition_ = false;
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_);
    bool defer = false;
    if (!next_handler_->OnResponseStarted(response_.get(), &defer)) {
      controller()->Cancel();
    } else if (!defer) {
      // Unpause the request to resume reading.  Any further reads will be
      // directed toward the new renderer.
      ResumeIfDeferred();
    }
  }

  // Remove ourselves from the ExtraRequestInfo.
  info->set_cross_site_handler(NULL);

  // If the response completed during the transition, notify the next
  // event handler.
  if (completed_during_transition_) {
    bool defer = false;
    next_handler_->OnResponseCompleted(completed_status_,
                                       completed_security_info_,
                                       &defer);
    if (!defer)
      ResumeIfDeferred();
  }
}

// static
void CrossSiteResourceHandler::SetLeakRequestsForTesting(
    bool leak_requests_for_testing) {
  leak_requests_for_testing_ = leak_requests_for_testing;
}

// Prepare to render the cross-site response in a new RenderFrameHost, by
// telling the old RenderFrameHost to run its onunload handler.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    ResourceResponse* response,
    bool should_transfer) {
  in_cross_site_transition_ = true;
  response_ = response;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when we are ready to resume.
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_cross_site_handler(this);

  GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());

  // Tell the contents responsible for this request that a cross-site response
  // is starting, so that it can tell its old renderer to run its onunload
  // handler now.  We will wait until the unload is finished and (if a transfer
  // is needed) for the new renderer's request to arrive.
  // The |transfer_url_chain| contains any redirect URLs that have already
  // occurred, plus the destination URL at the end.
  std::vector<GURL> transfer_url_chain;
  Referrer referrer;
  int render_frame_id = info->GetRenderFrameID();
  if (should_transfer) {
    transfer_url_chain = request()->url_chain();
    referrer = Referrer(GURL(request()->referrer()), info->GetReferrerPolicy());

    AppCacheInterceptor::PrepareForCrossSiteTransfer(
        request(), global_id.child_id);
    ResourceDispatcherHostImpl::Get()->MarkAsTransferredNavigation(global_id);
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OnCrossSiteResponseHelper,
          CrossSiteResponseParams(render_frame_id,
                                  global_id,
                                  should_transfer,
                                  transfer_url_chain,
                                  referrer,
                                  info->GetPageTransition(),
                                  info->should_replace_current_entry())));
}

bool CrossSiteResourceHandler::DeferForNavigationPolicyCheck(
    ResourceRequestInfoImpl* info,
    ResourceResponse* response,
    bool* defer) {
  // Store the response_ object internally, since the navigation is deferred
  // regardless of whether it will be a transfer or not.
  response_ = response;

  // Always defer the navigation to the UI thread to make a policy decision.
  // It will send the result back to the IO thread to either resume or
  // transfer it to a new renderer.
  // TODO(nasko): If the UI thread result is that transfer is required, the
  // IO thread will defer to the UI thread again through
  // StartCrossSiteTransition. This is unnecessary and the policy check on the
  // UI thread should be refactored to avoid the extra hop.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CheckNavigationPolicyOnUI,
                 request()->url(),
                 info->GetChildID(),
                 info->GetRenderFrameID()),
      base::Bind(&CrossSiteResourceHandler::ResumeOrTransfer,
                 weak_ptr_factory_.GetWeakPtr()));

  // Defer loading until it is known whether the navigation will transfer
  // to a new process or continue in the existing one.
  *defer = true;
  OnDidDefer();
  return true;
}

void CrossSiteResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    request()->LogUnblocked();
    did_defer_ = false;
    controller()->Resume();
  }
}

void CrossSiteResourceHandler::OnDidDefer() {
  did_defer_ = true;
  request()->LogBlockedBy("CrossSiteResourceHandler");
}

}  // namespace content
