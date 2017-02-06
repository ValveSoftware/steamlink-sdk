// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_resource_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/site_isolation_policy.h"
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
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry)
      : render_frame_id(render_frame_id),
        global_request_id(global_request_id),
        transfer_url_chain(transfer_url_chain),
        referrer(referrer),
        page_transition(page_transition),
        should_replace_current_entry(should_replace_current_entry) {
  }

  int render_frame_id;
  GlobalRequestID global_request_id;
  std::vector<GURL> transfer_url_chain;
  Referrer referrer;
  ui::PageTransition page_transition;
  bool should_replace_current_entry;
};

void OnCrossSiteResponseHelper(const CrossSiteResponseParams& params) {
  std::unique_ptr<CrossSiteTransferringRequest> cross_site_transferring_request(
      new CrossSiteTransferringRequest(params.global_request_id));

  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(params.global_request_id.child_id,
                                  params.render_frame_id);
  if (rfh && rfh->is_active()) {
    if (rfh->GetParent()) {
      // We should only swap processes for subframes in --site-per-process mode.
      // CrossSiteResourceHandler is not installed on subframe requests in
      // default Chrome.
      CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible());
    }
    rfh->OnCrossSiteResponse(
        params.global_request_id, std::move(cross_site_transferring_request),
        params.transfer_url_chain, params.referrer, params.page_transition,
        params.should_replace_current_entry);
  } else if (leak_requests_for_testing_) {
    // Some unit tests expect requests to be leaked in this case, so they can
    // pass them along manually.
    cross_site_transferring_request->ReleaseRequest();
  }
}

// Returns whether a transfer is needed by doing a check on the UI thread.
CrossSiteResourceHandler::NavigationDecision
CheckNavigationPolicyOnUI(GURL real_url, int process_id, int render_frame_id) {
  CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible());
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, render_frame_id);

  // Without a valid RFH against which to check, we must cancel the request,
  // to prevent the resource at |url| from being delivered to a potentially
  // unsuitable renderer process.
  if (!rfh || !rfh->is_active())
    return CrossSiteResourceHandler::NavigationDecision::CANCEL_REQUEST;

  RenderFrameHostManager* manager = rfh->frame_tree_node()->render_manager();
  if (manager->IsRendererTransferNeededForNavigation(rfh, real_url))
    return CrossSiteResourceHandler::NavigationDecision::TRANSFER_REQUIRED;
  else
    return CrossSiteResourceHandler::NavigationDecision::USE_EXISTING_RENDERER;
}

}  // namespace

CrossSiteResourceHandler::CrossSiteResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, std::move(next_handler)),
      has_started_response_(false),
      in_cross_site_transition_(false),
      completed_during_transition_(false),
      did_defer_(false),
      weak_ptr_factory_(this) {}

CrossSiteResourceHandler::~CrossSiteResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  GetRequestInfo()->set_cross_site_handler(NULL);
}

bool CrossSiteResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  // We should not have started the transition before being redirected.
  DCHECK(!in_cross_site_transition_);
  return next_handler_->OnRequestRedirected(redirect_info, response, defer);
}

bool CrossSiteResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  response_ = response;
  has_started_response_ = true;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when we are ready to resume.
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_cross_site_handler(this);

  return OnNormalResponseStarted(response, defer);
}

bool CrossSiteResourceHandler::OnNormalResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  // At this point, we know that the response is safe to send back to the
  // renderer: it is not a download, and it has passed the SSL and safe
  // browsing checks.
  // We should not have already started the transition before now.
  DCHECK(!in_cross_site_transition_);

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // The content embedder can decide that a transfer to a different process is
  // required for this URL.  If so, pause the response now.  Other cross process
  // navigations can proceed immediately, since we run the unload handler at
  // commit time.  Note that a process swap may no longer be necessary if we
  // transferred back into the original process due to a redirect.
  bool definitely_transfer =
      GetContentClient()->browser()->ShouldSwapProcessesForRedirect(
          info->GetContext(), request()->original_url(), request()->url());

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
  if (info->IsDownload() || info->is_stream() ||
      (response->head.headers.get() &&
       response->head.headers->response_code() == 204)) {
    return next_handler_->OnResponseStarted(response, defer);
  }

  if (definitely_transfer) {
    // Now that we know a transfer is needed and we have something to commit, we
    // pause to let the UI thread set up the transfer.
    StartCrossSiteTransition(response);

    // Defer loading until after the new renderer process has issued a
    // corresponding request.
    *defer = true;
    OnDidDefer();
    return true;
  }

  // In the site-per-process model, we may also decide (independently from the
  // content embedder's ShouldSwapProcessesForRedirect decision above) that a
  // process transfer is needed.  For that we need to consult the navigation
  // policy on the UI thread, so pause the response.  Process transfers are
  // skipped for WebUI processes for now, since e.g. chrome://settings has
  // multiple "cross-site" chrome:// frames, and that doesn't yet work cross-
  // process.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          info->GetChildID())) {
    return DeferForNavigationPolicyCheck(info, response, defer);
  }

  // No deferral needed. Pass the response through.
  return next_handler_->OnResponseStarted(response, defer);
}

void CrossSiteResourceHandler::ResumeOrTransfer(NavigationDecision decision) {
  switch (decision) {
    case NavigationDecision::CANCEL_REQUEST:
      // TODO(nick): What kind of cleanup do we need here?
      controller()->Cancel();
      break;
    case NavigationDecision::USE_EXISTING_RENDERER:
      ResumeResponse();
      break;
    case NavigationDecision::TRANSFER_REQUIRED:
      StartCrossSiteTransition(response_.get());
      break;
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
    // If we're not transferring, then we should pass this through.
    next_handler_->OnResponseCompleted(status, security_info, defer);
    return;
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
  TRACE_EVENT_ASYNC_END0(
      "navigation", "CrossSiteResourceHandler transition", this);
  DCHECK(request());
  in_cross_site_transition_ = false;
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_.get());
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

// Prepare to transfer the response to a new RenderFrameHost.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    ResourceResponse* response) {
  TRACE_EVENT_ASYNC_BEGIN0(
      "navigation", "CrossSiteResourceHandler transition", this);
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
  transfer_url_chain = request()->url_chain();
  referrer = Referrer(GURL(request()->referrer()), info->GetReferrerPolicy());
  ResourceDispatcherHostImpl::Get()->MarkAsTransferredNavigation(global_id,
                                                                 response_);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OnCrossSiteResponseHelper,
          CrossSiteResponseParams(render_frame_id,
                                  global_id,
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
