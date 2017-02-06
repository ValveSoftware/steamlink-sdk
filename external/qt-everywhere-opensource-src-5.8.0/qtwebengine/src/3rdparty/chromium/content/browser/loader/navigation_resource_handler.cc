// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_resource_handler.h"

#include <memory>

#include "base/logging.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace content {

NavigationResourceHandler::NavigationResourceHandler(
    net::URLRequest* request,
    NavigationURLLoaderImplCore* core,
    ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate)
    : ResourceHandler(request),
      core_(core),
      resource_dispatcher_host_delegate_(resource_dispatcher_host_delegate) {
  core_->set_resource_handler(this);
  writer_.set_immediate_mode(true);
}

NavigationResourceHandler::~NavigationResourceHandler() {
  if (core_) {
    core_->NotifyRequestFailed(false, net::ERR_ABORTED);
    DetachFromCore();
  }
}

void NavigationResourceHandler::Cancel() {
  controller()->Cancel();
  core_ = nullptr;
}

void NavigationResourceHandler::FollowRedirect() {
  controller()->Resume();
}

void NavigationResourceHandler::ProceedWithResponse() {
  // Detach from the loader; at this point, the request is now owned by the
  // StreamHandle sent in OnResponseStarted.
  DetachFromCore();
  controller()->Resume();
}

void NavigationResourceHandler::SetController(ResourceController* controller) {
  writer_.set_controller(controller);
  ResourceHandler::SetController(controller);
}

bool NavigationResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  DCHECK(core_);

  // TODO(davidben): Perform a CSP check here, and anything else that would have
  // been done renderer-side.
  NetLogObserver::PopulateResponseInfo(request(), response);
  core_->NotifyRequestRedirected(redirect_info, response);
  *defer = true;
  return true;
}

bool NavigationResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                  bool* defer) {
  DCHECK(core_);

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // If the MimeTypeResourceHandler intercepted this request and converted it
  // into a download, it will still call OnResponseStarted and immediately
  // cancel. Ignore the call; OnReadCompleted will happen shortly.
  //
  // TODO(davidben): Move the dispatch out of MimeTypeResourceHandler. Perhaps
  // all the way to the UI thread. Downloads, user certificates, etc., should be
  // dispatched at the navigation layer.
  if (info->IsDownload() || info->is_stream())
    return true;

  StreamContext* stream_context =
      GetStreamContextForResourceContext(info->GetContext());
  writer_.InitializeStream(stream_context->registry(),
                           request()->url().GetOrigin());

  NetLogObserver::PopulateResponseInfo(request(), response);

  std::unique_ptr<NavigationData> cloned_data;
  if (resource_dispatcher_host_delegate_) {
    // Ask the embedder for a NavigationData instance.
    NavigationData* navigation_data =
        resource_dispatcher_host_delegate_->GetNavigationData(request());

    // Clone the embedder's NavigationData before moving it to the UI thread.
    if (navigation_data)
      cloned_data = navigation_data->Clone();
  }

  core_->NotifyResponseStarted(response, writer_.stream()->CreateHandle(),
                               std::move(cloned_data));
  *defer = true;

  return true;
}

bool NavigationResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool NavigationResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                     bool* defer) {
  return true;
}

bool NavigationResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                           int* buf_size,
                                           int min_size) {
  writer_.OnWillRead(buf, buf_size, min_size);
  return true;
}

bool NavigationResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  writer_.OnReadCompleted(bytes_read, defer);
  return true;
}

void NavigationResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  // If the request has already committed, close the stream and leave it as-is.
  //
  // TODO(davidben): The net error code should be passed through StreamWriter
  // down to the stream's consumer. See https://crbug.com/426162.
  if (writer_.stream()) {
    writer_.Finalize();
    return;
  }

  if (core_) {
    DCHECK_NE(net::OK, status.error());
    core_->NotifyRequestFailed(request()->response_info().was_cached,
                               status.error());
    DetachFromCore();
  }
}

void NavigationResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void NavigationResourceHandler::DetachFromCore() {
  DCHECK(core_);
  core_->set_resource_handler(nullptr);
  core_ = nullptr;
}

}  // namespace content
