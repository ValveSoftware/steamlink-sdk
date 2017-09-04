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
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/referrer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

// Used in unit tests to make UI thread checks succeed even if there is no
// NavigationHandle.
bool g_ui_checks_always_succeed = false;

// Used in unit tests to transfer all navigations.
bool g_force_transfer = false;

typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
    UIChecksPerformedCallback;

void SendCheckResultToIOThread(UIChecksPerformedCallback callback,
                               NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(result, NavigationThrottle::DEFER);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, result));
}

// Returns the NavigationHandle to use for a navigation in the frame specified
// by |render_process_id| and |render_frame_host_id|. If not found, |callback|
// will be invoked to cancel the request.
//
// Note: in unit test |callback| may be invoked with a value of proceed if no
// handle is found. This happens when
// NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing is
// called with a value of true.
NavigationHandleImpl* FindNavigationHandle(
    int render_process_id,
    int render_frame_host_id,
    const UIChecksPerformedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (g_ui_checks_always_succeed) {
    SendCheckResultToIOThread(callback, NavigationThrottle::PROCEED);
    return nullptr;
  }

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host_id);
  if (!render_frame_host) {
    SendCheckResultToIOThread(callback, NavigationThrottle::CANCEL);
    return nullptr;
  }

  NavigationHandleImpl* navigation_handle =
      render_frame_host->navigation_handle();
  if (!navigation_handle) {
    SendCheckResultToIOThread(callback, NavigationThrottle::CANCEL);
    return nullptr;
  }
  return navigation_handle;
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
    bool is_external_protocol,
    RequestContextType request_context_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NavigationHandleImpl* navigation_handle =
      FindNavigationHandle(render_process_id, render_frame_host_id, callback);
  if (!navigation_handle)
    return;

  navigation_handle->WillStartRequest(
      method, resource_request_body, sanitized_referrer, has_user_gesture,
      transition, is_external_protocol, request_context_type,
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
    scoped_refptr<net::HttpResponseHeaders> headers,
    net::HttpResponseInfo::ConnectionInfo connection_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NavigationHandleImpl* navigation_handle =
      FindNavigationHandle(render_process_id, render_frame_host_id, callback);
  if (!navigation_handle)
    return;

  GURL new_validated_url(new_url);
  RenderProcessHost::FromID(render_process_id)
      ->FilterURL(false, &new_validated_url);
  navigation_handle->WillRedirectRequest(
      new_validated_url, new_method, new_referrer_url, new_is_external_protocol,
      headers, connection_info,
      base::Bind(&SendCheckResultToIOThread, callback));
}

void WillProcessResponseOnUIThread(
    UIChecksPerformedCallback callback,
    int render_process_id,
    int render_frame_host_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    const SSLStatus& ssl_status,
    const GlobalRequestID& request_id,
    bool should_replace_current_entry,
    bool is_download,
    bool is_stream,
    const base::Closure& transfer_callback,
    std::unique_ptr<NavigationData> navigation_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (g_force_transfer) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, transfer_callback);
  }

  NavigationHandleImpl* navigation_handle =
      FindNavigationHandle(render_process_id, render_frame_host_id, callback);
  if (!navigation_handle)
    return;

  if (navigation_data)
    navigation_handle->set_navigation_data(std::move(navigation_data));

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host_id);
  DCHECK(render_frame_host);
  navigation_handle->WillProcessResponse(
      render_frame_host, headers, connection_info, ssl_status, request_id,
      should_replace_current_entry, is_download, is_stream, transfer_callback,
      base::Bind(&SendCheckResultToIOThread, callback));
}

}  // namespace

NavigationResourceThrottle::NavigationResourceThrottle(
    net::URLRequest* request,
    ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate,
    RequestContextType request_context_type)
    : request_(request),
      resource_dispatcher_host_delegate_(resource_dispatcher_host_delegate),
      request_context_type_(request_context_type),
      in_cross_site_transition_(false),
      on_transfer_done_result_(NavigationThrottle::DEFER),
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
                 is_external_protocol, request_context_type_));
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
                 new_is_external_protocol, response_headers,
                 request_->response_info().connection_info));
  *defer = true;
}

void NavigationResourceThrottle::WillProcessResponse(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
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
  base::Closure transfer_callback =
      base::Bind(&NavigationResourceThrottle::InitiateTransfer,
                 weak_ptr_factory_.GetWeakPtr());

  SSLStatus ssl_status;
  if (request_->ssl_info().cert.get()) {
    NavigationResourceHandler::GetSSLStatusForRequest(
        request_->url(), request_->ssl_info(), info->GetChildID(), &ssl_status);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WillProcessResponseOnUIThread, callback, render_process_id,
                 render_frame_id, response_headers,
                 request_->response_info().connection_info, ssl_status,
                 info->GetGlobalRequestID(),
                 info->should_replace_current_entry(), info->IsDownload(),
                 info->is_stream(), transfer_callback,
                 base::Passed(&cloned_data)));
  *defer = true;
}

const char* NavigationResourceThrottle::GetNameForLogging() const {
  return "NavigationResourceThrottle";
}

void NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(
    bool ui_checks_always_succeed) {
  g_ui_checks_always_succeed = ui_checks_always_succeed;
}

void NavigationResourceThrottle::set_force_transfer_for_testing(
    bool force_transfer) {
  g_force_transfer = force_transfer;
}

void NavigationResourceThrottle::OnUIChecksPerformed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(NavigationThrottle::DEFER, result);
  if (in_cross_site_transition_) {
    on_transfer_done_result_ = result;
    return;
  }

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

void NavigationResourceThrottle::InitiateTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  in_cross_site_transition_ = true;
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  ResourceDispatcherHostImpl::Get()->MarkAsTransferredNavigation(
      info->GetGlobalRequestID(),
      base::Bind(&NavigationResourceThrottle::OnTransferComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NavigationResourceThrottle::OnTransferComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(in_cross_site_transition_);
  in_cross_site_transition_ = false;

  // If the results of the checks on the UI thread are known, unblock the
  // navigation. Otherwise, wait until the callback has executed.
  if (on_transfer_done_result_ != NavigationThrottle::DEFER) {
    OnUIChecksPerformed(on_transfer_done_result_);
    on_transfer_done_result_ = NavigationThrottle::DEFER;
  }
}

}  // namespace content
