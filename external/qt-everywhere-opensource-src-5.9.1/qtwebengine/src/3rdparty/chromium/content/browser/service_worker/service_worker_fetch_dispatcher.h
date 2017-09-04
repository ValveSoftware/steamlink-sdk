// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/fetch_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_type.h"
#include "net/log/net_log_with_source.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class ServiceWorkerVersion;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          ServiceWorkerFetchEventResult,
                          const ServiceWorkerResponse&,
                          const scoped_refptr<ServiceWorkerVersion>&)>;

  ServiceWorkerFetchDispatcher(
      std::unique_ptr<ServiceWorkerFetchRequest> request,
      ServiceWorkerVersion* version,
      ResourceType resource_type,
      const net::NetLogWithSource& net_log,
      const base::Closure& prepare_callback,
      const FetchCallback& fetch_callback);
  ~ServiceWorkerFetchDispatcher();

  // If appropriate, starts the navigation preload request and creates
  // |preload_handle_|.
  void MaybeStartNavigationPreload(net::URLRequest* original_request);

  // Dispatches a fetch event to the |version| given in ctor, and fires
  // |fetch_callback| (also given in ctor) when finishes. It runs
  // |prepare_callback| as an intermediate step once the version is activated
  // and running.
  void Run();

 private:
  class ResponseCallback;

  void DidWaitForActivation();
  void StartWorker();
  void DidStartWorker();
  void DidFailToStartWorker(ServiceWorkerStatusCode status);
  void DispatchFetchEvent();
  void DidFailToDispatch(ServiceWorkerStatusCode status);
  void DidFail(ServiceWorkerStatusCode status);
  void DidFinish(int request_id,
                 ServiceWorkerFetchEventResult fetch_result,
                 const ServiceWorkerResponse& response);
  void Complete(ServiceWorkerStatusCode status,
                ServiceWorkerFetchEventResult fetch_result,
                const ServiceWorkerResponse& response);

  ServiceWorkerMetrics::EventType GetEventType() const;

  scoped_refptr<ServiceWorkerVersion> version_;
  net::NetLogWithSource net_log_;
  base::Closure prepare_callback_;
  FetchCallback fetch_callback_;
  std::unique_ptr<ServiceWorkerFetchRequest> request_;
  ResourceType resource_type_;
  bool did_complete_;
  mojom::URLLoaderFactoryPtr url_loader_factory_;
  std::unique_ptr<mojom::URLLoader> url_loader_;
  std::unique_ptr<mojom::URLLoaderClient> url_loader_client_;
  mojom::FetchEventPreloadHandlePtr preload_handle_;

  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
