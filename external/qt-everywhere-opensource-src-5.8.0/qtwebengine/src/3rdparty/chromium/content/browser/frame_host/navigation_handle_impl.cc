// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include <utility>

#include "base/logging.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/frame_messages.h"
#include "content/common/resource_request_body_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "net/url_request/redirect_info.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

void UpdateThrottleCheckResult(
    NavigationThrottle::ThrottleCheckResult* to_update,
    NavigationThrottle::ThrottleCheckResult result) {
  *to_update = result;
}

}  // namespace

// static
std::unique_ptr<NavigationHandleImpl> NavigationHandleImpl::Create(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    bool is_renderer_initiated,
    bool is_synchronous,
    bool is_srcdoc,
    const base::TimeTicks& navigation_start,
    int pending_nav_entry_id) {
  return std::unique_ptr<NavigationHandleImpl>(
      new NavigationHandleImpl(url, frame_tree_node, is_renderer_initiated,
                               is_synchronous, is_srcdoc, navigation_start,
                               pending_nav_entry_id));
}

NavigationHandleImpl::NavigationHandleImpl(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    bool is_renderer_initiated,
    bool is_synchronous,
    bool is_srcdoc,
    const base::TimeTicks& navigation_start,
    int pending_nav_entry_id)
    : url_(url),
      has_user_gesture_(false),
      transition_(ui::PAGE_TRANSITION_LINK),
      is_external_protocol_(false),
      net_error_code_(net::OK),
      render_frame_host_(nullptr),
      is_renderer_initiated_(is_renderer_initiated),
      is_same_page_(false),
      is_synchronous_(is_synchronous),
      is_srcdoc_(is_srcdoc),
      was_redirected_(false),
      state_(INITIAL),
      is_transferring_(false),
      frame_tree_node_(frame_tree_node),
      next_index_(0),
      navigation_start_(navigation_start),
      pending_nav_entry_id_(pending_nav_entry_id) {
  DCHECK(!navigation_start.is_null());
  GetDelegate()->DidStartNavigation(this);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_start.ToInternalValue(), "Initial URL", url_.spec());
  }
}

NavigationHandleImpl::~NavigationHandleImpl() {
  GetDelegate()->DidFinishNavigation(this);

  // Cancel the navigation on the IO thread if the NavigationHandle is being
  // destroyed in the middle of the NavigationThrottles checks.
  if (!IsBrowserSideNavigationEnabled() && !complete_callback_.is_null())
    RunCompleteCallback(NavigationThrottle::CANCEL_AND_IGNORE);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation StartToCommit", this,
                           "URL", url_.spec(), "Net Error Code",
                           net_error_code_);
  }
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node_->navigator()->GetDelegate();
}

const GURL& NavigationHandleImpl::GetURL() {
  return url_;
}

bool NavigationHandleImpl::IsInMainFrame() {
  return frame_tree_node_->IsMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  if (frame_tree_node_->parent())
    return frame_tree_node_->parent()->IsMainFrame();

  return false;
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return is_renderer_initiated_;
}

bool NavigationHandleImpl::IsSynchronousNavigation() {
  return is_synchronous_;
}

bool NavigationHandleImpl::IsSrcdoc() {
  return is_srcdoc_;
}

bool NavigationHandleImpl::WasServerRedirect() {
  return was_redirected_;
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return frame_tree_node_->frame_tree_node_id();
}

int NavigationHandleImpl::GetParentFrameTreeNodeId() {
  if (frame_tree_node_->IsMainFrame())
    return FrameTreeNode::kFrameTreeNodeInvalidId;

  return frame_tree_node_->parent()->frame_tree_node_id();
}

const base::TimeTicks& NavigationHandleImpl::NavigationStart() {
  return navigation_start_;
}

bool NavigationHandleImpl::IsPost() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return method_ == "POST";
}

const Referrer& NavigationHandleImpl::GetReferrer() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return sanitized_referrer_;
}

bool NavigationHandleImpl::HasUserGesture() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return has_user_gesture_;
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return transition_;
}

bool NavigationHandleImpl::IsExternalProtocol() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return is_external_protocol_;
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return net_error_code_;
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  // TODO(mkwst): Change this to check against 'READY_TO_COMMIT' once
  // ReadyToCommitNavigation is available whether or not PlzNavigate is
  // enabled. https://crbug.com/621856
  CHECK_GE(state_, WILL_PROCESS_RESPONSE)
      << "This accessor should only be called after a response has been "
         "delivered for processing.";
  return render_frame_host_;
}

bool NavigationHandleImpl::IsSamePage() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE)
      << "This accessor should not be called before the navigation has "
         "committed.";
  return is_same_page_;
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
   return response_headers_.get();
}

bool NavigationHandleImpl::HasCommitted() {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

void NavigationHandleImpl::Resume() {
  if (state_ != DEFERRING_START && state_ != DEFERRING_REDIRECT &&
      state_ != DEFERRING_RESPONSE) {
    return;
  }

  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  if (state_ == DEFERRING_START) {
    result = CheckWillStartRequest();
  } else if (state_ == DEFERRING_REDIRECT) {
    result = CheckWillRedirectRequest();
  } else {
    result = CheckWillProcessResponse();

    // If the navigation is about to proceed after processing the response, then
    // it's ready to commit.
    if (result == NavigationThrottle::PROCEED)
      ReadyToCommitNavigation(render_frame_host_);
  }

  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::CancelDeferredNavigation(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(state_ == DEFERRING_START || state_ == DEFERRING_REDIRECT);
  DCHECK(result == NavigationThrottle::CANCEL_AND_IGNORE ||
         result == NavigationThrottle::CANCEL);
  state_ = CANCELING;
  RunCompleteCallback(result);
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  throttles_.push_back(std::move(navigation_throttle));
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillStartRequestForTesting(
    bool is_post,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol) {
  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;

  scoped_refptr<ResourceRequestBodyImpl> resource_request_body;
  std::string method = "GET";
  if (is_post) {
    method = "POST";

    std::string body = "test=body";
    resource_request_body = new ResourceRequestBodyImpl();
    resource_request_body->AppendBytes(body.data(), body.size());
  }

  WillStartRequest(method, resource_request_body, sanitized_referrer,
                   has_user_gesture, transition, is_external_protocol,
                   base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillRedirectRequestForTesting(
    const GURL& new_url,
    bool new_method_is_post,
    const GURL& new_referrer_url,
    bool new_is_external_protocol) {
  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  WillRedirectRequest(new_url, new_method_is_post ? "POST" : "GET",
                      new_referrer_url, new_is_external_protocol,
                      scoped_refptr<net::HttpResponseHeaders>(),
                      base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

NavigationData* NavigationHandleImpl::GetNavigationData() {
  return navigation_data_.get();
}

void NavigationHandleImpl::WillStartRequest(
    const std::string& method,
    scoped_refptr<content::ResourceRequestBodyImpl> resource_request_body,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol,
    const ThrottleChecksFinishedCallback& callback) {
  // |method != "POST"| should imply absence of |resource_request_body|.
  if (method != "POST" && resource_request_body) {
    NOTREACHED();
    resource_request_body = nullptr;
  }

  // Update the navigation parameters.
  method_ = method;
  if (method_ == "POST")
    resource_request_body_ = resource_request_body;
  sanitized_referrer_ = sanitized_referrer;
  has_user_gesture_ = has_user_gesture;
  transition_ = transition;
  is_external_protocol_ = is_external_protocol;

  state_ = WILL_SEND_REQUEST;
  complete_callback_ = callback;

  // Register the navigation throttles. The ScopedVector returned by
  // GetNavigationThrottles is not assigned to throttles_ directly because it
  // would overwrite any throttle previously added with
  // RegisterThrottleForTesting.
  ScopedVector<NavigationThrottle> throttles_to_register =
      GetContentClient()->browser()->CreateThrottlesForNavigation(this);
  if (throttles_to_register.size() > 0) {
    throttles_.insert(throttles_.end(), throttles_to_register.begin(),
                      throttles_to_register.end());
    throttles_to_register.weak_clear();
  }

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillStartRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::WillRedirectRequest(
    const GURL& new_url,
    const std::string& new_method,
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const ThrottleChecksFinishedCallback& callback) {
  // Update the navigation parameters.
  url_ = new_url;
  method_ = new_method;
  sanitized_referrer_.url = new_referrer_url;
  sanitized_referrer_ = Referrer::SanitizeForRequest(url_, sanitized_referrer_);
  is_external_protocol_ = new_is_external_protocol;
  response_headers_ = response_headers;
  was_redirected_ = true;
  if (new_method != "POST")
    resource_request_body_ = nullptr;

  state_ = WILL_REDIRECT_REQUEST;
  complete_callback_ = callback;

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillRedirectRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::WillProcessResponse(
    RenderFrameHostImpl* render_frame_host,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const ThrottleChecksFinishedCallback& callback) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  response_headers_ = response_headers;
  state_ = WILL_PROCESS_RESPONSE;
  complete_callback_ = callback;

  // Notify each throttle of the response.
  NavigationThrottle::ThrottleCheckResult result = CheckWillProcessResponse();

  // If the navigation is about to proceed, then it's ready to commit.
  if (result == NavigationThrottle::PROCEED)
    ReadyToCommitNavigation(render_frame_host);

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::ReadyToCommitNavigation(
    RenderFrameHostImpl* render_frame_host) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  state_ = READY_TO_COMMIT;

  // Only notify the WebContentsObservers when PlzNavigate is enabled, as
  // |render_frame_host_| may be wrong in the case of transfer navigations.
  if (IsBrowserSideNavigationEnabled())
    GetDelegate()->ReadyToCommitNavigation(this);
}

void NavigationHandleImpl::DidCommitNavigation(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool same_page,
    RenderFrameHostImpl* render_frame_host) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  DCHECK_EQ(frame_tree_node_, render_frame_host->frame_tree_node());
  CHECK_EQ(url_, params.url);

  method_ = params.method;
  has_user_gesture_ = (params.gesture == NavigationGestureUser);
  transition_ = params.transition;
  render_frame_host_ = render_frame_host;
  is_same_page_ = same_page;

  state_ = net_error_code_ == net::OK ? DID_COMMIT : DID_COMMIT_ERROR_PAGE;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillStartRequest() {
  DCHECK(state_ == WILL_SEND_REQUEST || state_ == DEFERRING_START);
  DCHECK(state_ != WILL_SEND_REQUEST || next_index_ == 0);
  DCHECK(state_ != DEFERRING_START || next_index_ != 0);
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillStartRequest();
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
      case NavigationThrottle::BLOCK_REQUEST:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_START;
        next_index_ = i + 1;
        return result;
    }
  }
  next_index_ = 0;
  state_ = WILL_SEND_REQUEST;
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillRedirectRequest() {
  DCHECK(state_ == WILL_REDIRECT_REQUEST || state_ == DEFERRING_REDIRECT);
  DCHECK(state_ != WILL_REDIRECT_REQUEST || next_index_ == 0);
  DCHECK(state_ != DEFERRING_REDIRECT || next_index_ != 0);
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillRedirectRequest();
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_REDIRECT;
        next_index_ = i + 1;
        return result;

      case NavigationThrottle::BLOCK_REQUEST:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_REDIRECT_REQUEST;

  // Notify the delegate that a redirect was encountered and will be followed.
  if (GetDelegate())
    GetDelegate()->DidRedirectNavigation(this);

  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillProcessResponse() {
  DCHECK(state_ == WILL_PROCESS_RESPONSE || state_ == DEFERRING_RESPONSE);
  DCHECK(state_ != WILL_PROCESS_RESPONSE || next_index_ == 0);
  DCHECK(state_ != DEFERRING_RESPONSE || next_index_ != 0);
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillProcessResponse();
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_RESPONSE;
        next_index_ = i + 1;
        return result;

      case NavigationThrottle::BLOCK_REQUEST:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_PROCESS_RESPONSE;
  return NavigationThrottle::PROCEED;
}

void NavigationHandleImpl::RunCompleteCallback(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result != NavigationThrottle::DEFER);

  ThrottleChecksFinishedCallback callback = complete_callback_;
  complete_callback_.Reset();

  if (!callback.is_null())
    callback.Run(result);

  // No code after running the callback, as it might have resulted in our
  // destruction.
}

}  // namespace content
