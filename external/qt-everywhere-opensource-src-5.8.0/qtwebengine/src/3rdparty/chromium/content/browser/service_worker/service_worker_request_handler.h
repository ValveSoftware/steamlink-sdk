// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestInterceptor;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;
class ResourceMessageFilter;
class ResourceRequestBodyImpl;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;

// Abstract base class for routing network requests to ServiceWorkers.
// Created one per URLRequest and attached to each request.
class CONTENT_EXPORT ServiceWorkerRequestHandler
    : public base::SupportsUserData::Data {
 public:
  // Attaches a newly created handler if the given |request| needs to
  // be handled by ServiceWorker.
  // TODO(kinuko): While utilizing UserData to attach data to URLRequest
  // has some precedence, it might be better to attach this handler in a more
  // explicit way within content layer, e.g. have ResourceRequestInfoImpl
  // own it.
  static void InitializeHandler(
      net::URLRequest* request,
      ServiceWorkerContextWrapper* context_wrapper,
      storage::BlobStorageContext* blob_storage_context,
      int process_id,
      int provider_id,
      bool skip_service_worker,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBodyImpl> body);

  // Returns the handler attached to |request|. This may return NULL
  // if no handler is attached.
  static ServiceWorkerRequestHandler* GetHandler(
      const net::URLRequest* request);

  // Creates a protocol interceptor for ServiceWorker.
  static std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor(
      ResourceContext* resource_context);

  // Returns true if the request falls into the scope of a ServiceWorker.
  // It's only reliable after the ServiceWorkerRequestHandler MaybeCreateJob
  // method runs to completion for this request. The AppCache handler uses
  // this to avoid colliding with ServiceWorkers.
  static bool IsControlledByServiceWorker(const net::URLRequest* request);

  // Returns the ServiceWorkerProviderHost the request is associated with.
  // Only valid after InitializeHandler has been called. Can return null.
  static ServiceWorkerProviderHost* GetProviderHost(
      const net::URLRequest* request);

  ~ServiceWorkerRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* context) = 0;

  // Methods to support cross site navigations.
  void PrepareForCrossSiteTransfer(int old_process_id);
  void CompleteCrossSiteTransfer(int new_process_id,
                                 int new_provider_id);
  void MaybeCompleteCrossSiteTransferInOldProcess(
      int old_process_id);

  // Useful for detecting storage partition mismatches in the context of cross
  // site transfer navigations.
  bool SanityCheckIsSameContext(ServiceWorkerContextWrapper* wrapper);

 protected:
  ServiceWorkerRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  ResourceType resource_type_;

 private:
  std::unique_ptr<ServiceWorkerProviderHost> host_for_cross_site_transfer_;
  int old_process_id_;
  int old_provider_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
