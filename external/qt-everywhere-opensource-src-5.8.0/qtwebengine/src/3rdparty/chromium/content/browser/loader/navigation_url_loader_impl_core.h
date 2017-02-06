// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace net {
class URLRequest;
struct RedirectInfo;
}

namespace content {

class FrameTreeNode;
class NavigationResourceHandler;
class NavigationData;
class ResourceContext;
class ResourceHandler;
class ResourceRequestBody;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class StreamHandle;
struct ResourceResponse;

// The IO-thread counterpart to the NavigationURLLoaderImpl. It lives on the IO
// thread and is owned by the UI-thread NavigationURLLoaderImpl.
// NavigationURLLoaderImplCore interacts with the ResourceDispatcherHost stack
// and forwards signals back to the loader on the UI thread.
class NavigationURLLoaderImplCore {
 public:
  // Creates a new NavigationURLLoaderImplCore that forwards signals back to
  // |loader| on the UI thread.
  explicit NavigationURLLoaderImplCore(
      const base::WeakPtr<NavigationURLLoaderImpl>& loader);
  ~NavigationURLLoaderImplCore();

  // Starts the request.
  void Start(ResourceContext* resource_context,
             ServiceWorkerContextWrapper* service_worker_context_wrapper,
             std::unique_ptr<NavigationRequestInfo> request_info);

  // Follows the current pending redirect.
  void FollowRedirect();

  // Proceeds with processing the response.
  void ProceedWithResponse();

  void set_resource_handler(NavigationResourceHandler* resource_handler) {
    resource_handler_ = resource_handler;
  }

  // Notifies |loader_| on the UI thread that the request was redirected.
  void NotifyRequestRedirected(const net::RedirectInfo& redirect_info,
                               ResourceResponse* response);

  // Notifies |loader_| on the UI thread that the response started.
  void NotifyResponseStarted(ResourceResponse* response,
                             std::unique_ptr<StreamHandle> body,
                             std::unique_ptr<NavigationData> navigation_data);

  // Notifies |loader_| on the UI thread that the request failed.
  void NotifyRequestFailed(bool in_cache, int net_error);

 private:
  // Called when done checking whether the navigation has a ServiceWorker
  // registered for it.
  void OnServiceWorkerChecksPerformed(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  base::WeakPtr<NavigationURLLoaderImpl> loader_;
  NavigationResourceHandler* resource_handler_;
  std::unique_ptr<NavigationRequestInfo> request_info_;
  ResourceContext* resource_context_;

  base::WeakPtrFactory<NavigationURLLoaderImplCore> factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImplCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
