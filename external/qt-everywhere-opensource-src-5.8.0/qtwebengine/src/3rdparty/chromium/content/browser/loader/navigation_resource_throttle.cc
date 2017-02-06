// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_resource_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/referrer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {
typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
    UIChecksPerformedCallback;

void SendCheckResultToIOThread(UIChecksPerformedCallback callback,
                               NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(result, NavigationThrottle::DEFER);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, result));
}

void CheckWillStartRequestOnUIThread(
    UIChecksPerformedCallback callback,
    int render_process_id,
    int render_frame_host_id,
    const std::string& method,
    const scoped_refptr<content::ResourceRequestBodyImpl>&
        resource_request_body,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host_id);
  if (!render_frame_host) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  NavigationHandleImpl* navigation_handle =
      render_frame_host->navigation_handle();
  if (!navigation_handle) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  navigation_handle->WillStartRequest(
      method, resource_request_body, sanitized_referrer, has_user_gesture,
      transition, is_external_protocol,
      base::Bind(&SendCheckResultToIOThread, callback));
}

void CheckWillRedirectRequestOnUIThread(
    UIChecksPerformedCallback callback,
    int render_process_id,
    int render_frame_host_id,
    const GURL& new_url,
    const std::string& new_method,
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host_id);
  if (!render_frame_host) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  NavigationHandleImpl* navigation_handle =
      render_frame_host->navigation_handle();
  if (!navigation_handle) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  GURL new_validated_url(new_url);
  RenderProcessHost::FromID(render_process_id)
      ->FilterURL(false, &new_validated_url);
  navigation_handle->WillRedirectRequest(
      new_validated_url, new_method, new_referrer_url, new_is_external_protocol,
      headers, base::Bind(&SendCheckResultToIOThread, callback));
}

void WillProcessResponseOnUIThread(
    UIChecksPerformedCallback callback,
    int render_process_id,
    int render_frame_host_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::unique_ptr<NavigationData> navigation_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host_id);
  if (!render_frame_host) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  NavigationHandleImpl* navigation_handle =
      render_frame_host->navigation_handle();
  if (!navigation_handle) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return;
  }

  if (navigation_data)
    navigation_handle->set_navigation_data(std::move(navigation_data));

  navigation_handle->WillProcessResponse(
      render_frame_host, headers,
      base::Bind(&SendCheckResultToIOThread, callback));
}

}  // namespace

NavigationResourceThrottle::NavigationResourceThrottle(
    net::URLRequest* request,
    ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate)
    : request_(request),
      resource_dispatcher_host_delegate_(resource_dispatcher_host_delegate),
      weak_ptr_factory_(this) {}

NavigationResourceThrottle::~NavigationResourceThrottle() {}

void NavigationResourceThrottle::WillStartRequest(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  if (!info)
    return;

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    return;

  bool is_external_protocol =
      !info->GetContext()->GetRequestContext()->job_factory()->IsHandledURL(
          request_->url());
  UIChecksPerformedCallback callback =
      base::Bind(&NavigationResourceThrottle::OnUIChecksPerformed,
                 weak_ptr_factory_.GetWeakPtr());
  DCHECK(request_->method() == "POST" || request_->method() == "GET");
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckWillStartRequestOnUIThread, callback, render_process_id,
                 render_frame_id, request_->method(), info->body(),
                 Referrer::SanitizeForRequest(
                     request_->url(), Referrer(GURL(request_->referrer()),
                                               info->GetReferrerPolicy())),
                 info->HasUserGesture(), info->GetPageTransition(),
                 is_external_protocol));
  *defer = true;
}

void NavigationResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request_);
  if (!info)
    return;

  if (redirect_info.new_method != "POST")
    info->ResetBody();

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    return;

  bool new_is_external_protocol =
      !info->GetContext()->GetRequestContext()->job_factory()->IsHandledURL(
          request_->url());
  DCHECK(redirect_info.new_method == "POST" ||
         redirect_info.new_method == "GET");
  UIChecksPerformedCallback callback =
      base::Bind(&NavigationResourceThrottle::OnUIChecksPerformed,
                 weak_ptr_factory_.GetWeakPtr());

  // Send the redirect info to the NavigationHandle on the UI thread.
  // Note: to avoid threading issues, a copy of the HttpResponseHeaders is sent
  // in lieu of the original.
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  if (request_->response_headers()) {
    response_headers = new net::HttpResponseHeaders(
        request_->response_headers()->raw_headers());
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckWillRedirectRequestOnUIThread, callback,
                 render_process_id, render_frame_id, redirect_info.new_url,
                 redirect_info.new_method, GURL(redirect_info.new_referrer),
                 new_is_external_protocol, response_headers));
  *defer = true;
}

void NavigationResourceThrottle::WillProcessResponse(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (!info)
    return;

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    return;

  // Send a copy of the response headers to the NavigationHandle on the UI
  // thread.
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  if (request_->response_headers()) {
    response_headers = new net::HttpResponseHeaders(
        request_->response_headers()->raw_headers());
  }

  std::unique_ptr<NavigationData> cloned_data;
  if (resource_dispatcher_host_delegate_) {
    // Ask the embedder for a NavigationData instance.
    NavigationData* navigation_data =
        resource_dispatcher_host_delegate_->GetNavigationData(request_);

    // Clone the embedder's NavigationData before moving it to the UI thread.
    if (navigation_data)
      cloned_data = navigation_data->Clone();
  }

  UIChecksPerformedCallback callback =
      base::Bind(&NavigationResourceThrottle::OnUIChecksPerformed,
                 weak_ptr_factory_.GetWeakPtr());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WillProcessResponseOnUIThread, callback, render_process_id,
                 render_frame_id, response_headers,
                 base::Passed(&cloned_data)));
  *defer = true;
}

const char* NavigationResourceThrottle::GetNameForLogging() const {
  return "NavigationResourceThrottle";
}

void NavigationResourceThrottle::OnUIChecksPerformed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result == NavigationThrottle::CANCEL_AND_IGNORE) {
    controller()->CancelAndIgnore();
  } else if (result == NavigationThrottle::CANCEL) {
    controller()->Cancel();
  } else if (result == NavigationThrottle::BLOCK_REQUEST) {
    controller()->CancelWithError(net::ERR_BLOCKED_BY_CLIENT);
  } else {
    controller()->Resume();
  }
}

}  // namespace content
