// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl_core.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"

namespace content {

NavigationURLLoaderImplCore::NavigationURLLoaderImplCore(
    const base::WeakPtr<NavigationURLLoaderImpl>& loader)
    : loader_(loader),
      resource_handler_(nullptr),
      resource_context_(nullptr),
      factory_(this) {
  // This object is created on the UI thread but otherwise is accessed and
  // deleted on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

NavigationURLLoaderImplCore::~NavigationURLLoaderImplCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->Cancel();
}

void NavigationURLLoaderImplCore::Start(
    ResourceContext* resource_context,
    ServiceWorkerContextWrapper* service_worker_context_wrapper,
    std::unique_ptr<NavigationRequestInfo> request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigationURLLoaderImpl::NotifyRequestStarted, loader_,
                 base::TimeTicks::Now()));
  request_info_ = std::move(request_info);
  resource_context_ = resource_context;

  // Check if there is a ServiceWorker registered for this navigation if
  // ServiceWorkers are allowed for this navigation. Otherwise start the
  // request right away.
  // ServiceWorkerContextWrapper can be null in tests.
  if (!request_info_->begin_params.skip_service_worker &&
      service_worker_context_wrapper) {
    service_worker_context_wrapper->FindReadyRegistrationForDocument(
        request_info_->common_params.url,
        base::Bind(&NavigationURLLoaderImplCore::OnServiceWorkerChecksPerformed,
                   factory_.GetWeakPtr()));
  } else {
    ResourceDispatcherHostImpl::Get()->BeginNavigationRequest(
        resource_context_, *request_info_, this);
  }
}

void NavigationURLLoaderImplCore::FollowRedirect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->FollowRedirect();
}

void NavigationURLLoaderImplCore::ProceedWithResponse() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->ProceedWithResponse();
}

void NavigationURLLoaderImplCore::NotifyRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);

  // Make a copy of the ResourceResponse before it is passed to another thread.
  //
  // TODO(davidben): This copy could be avoided if ResourceResponse weren't
  // reference counted and the loader stack passed unique ownership of the
  // response. https://crbug.com/416050
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigationURLLoaderImpl::NotifyRequestRedirected, loader_,
                 redirect_info, response->DeepCopy()));

  // TODO(carlosk): extend this trace to support non-PlzNavigate navigations.
  // For the trace below we're using the NavigationURLLoaderImplCore as the
  // async trace id and reporting the new redirect URL as a parameter.
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "Navigation redirectDelay", this,
                           "&NavigationURLLoaderImplCore", this, "New URL",
                           redirect_info.new_url.spec());
}

void NavigationURLLoaderImplCore::NotifyResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<StreamHandle> body,
    std::unique_ptr<NavigationData> navigation_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImplCore", this, "success", true);

  // If, by the time the task reaches the UI thread, |loader_| has already been
  // destroyed, NotifyResponseStarted will not run. |body| will be destructed
  // and the request released at that point.

  // Make a copy of the ResourceResponse before it is passed to another thread.
  //
  // TODO(davidben): This copy could be avoided if ResourceResponse weren't
  // reference counted and the loader stack passed unique ownership of the
  // response. https://crbug.com/416050
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigationURLLoaderImpl::NotifyResponseStarted, loader_,
                 response->DeepCopy(), base::Passed(&body),
                 base::Passed(&navigation_data)));
}

void NavigationURLLoaderImplCore::NotifyRequestFailed(bool in_cache,
                                                      int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImplCore", this, "success",
                         false);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigationURLLoaderImpl::NotifyRequestFailed, loader_,
                 in_cache, net_error));
}

void NavigationURLLoaderImplCore::OnServiceWorkerChecksPerformed(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // If the navigation has a ServiceWorker, bail out immediately.
  // TODO(clamy): only bail out when the ServiceWorker has a Fetch event
  // handler.
  if (status == SERVICE_WORKER_OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NavigationURLLoaderImpl::NotifyServiceWorkerEncountered,
                   loader_));
    return;
  }

  // Otherwise, start the navigation in the network stack.

  // The ResourceDispatcherHostImpl can be null in unit tests.
  if (!ResourceDispatcherHostImpl::Get())
    return;

  ResourceDispatcherHostImpl::Get()->BeginNavigationRequest(
      resource_context_, *request_info_, this);
}

}  // namespace content
