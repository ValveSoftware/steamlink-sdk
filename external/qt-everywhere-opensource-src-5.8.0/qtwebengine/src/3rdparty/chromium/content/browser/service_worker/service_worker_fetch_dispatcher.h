// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "net/log/net_log.h"

namespace content {

class ServiceWorkerVersion;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&,
                              const scoped_refptr<ServiceWorkerVersion>&)>
      FetchCallback;

  ServiceWorkerFetchDispatcher(
      std::unique_ptr<ServiceWorkerFetchRequest> request,
      ServiceWorkerVersion* version,
      ResourceType resource_type,
      const net::BoundNetLog& net_log,
      const base::Closure& prepare_callback,
      const FetchCallback& fetch_callback);
  ~ServiceWorkerFetchDispatcher();

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
  net::BoundNetLog net_log_;
  base::Closure prepare_callback_;
  FetchCallback fetch_callback_;
  std::unique_ptr<ServiceWorkerFetchRequest> request_;
  ResourceType resource_type_;
  bool did_complete_;
  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
