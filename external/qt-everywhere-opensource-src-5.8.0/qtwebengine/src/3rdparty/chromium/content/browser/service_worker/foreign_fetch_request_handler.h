// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_FOREIGN_FETCH_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_FOREIGN_FETCH_REQUEST_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
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
class ResourceRequestBodyImpl;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
struct ResourceResponseInfo;

// Class for routing network requests to ServiceWorkers for foreign fetch
// events. Created one per URLRequest and attached to each request.
// TODO(mek): Does this need something similar to ServiceWorkerRequestHandler's
// GetExtraResponseInfo method?
class CONTENT_EXPORT ForeignFetchRequestHandler
    : public base::SupportsUserData::Data,
      public ServiceWorkerURLRequestJob::Delegate {
 public:
  // Attaches a newly created handler if the given |request| needs to
  // be handled by a foreign fetch handling ServiceWorker.
  static void InitializeHandler(
      net::URLRequest* request,
      ServiceWorkerContextWrapper* context_wrapper,
      storage::BlobStorageContext* blob_storage_context,
      int process_id,
      int provider_id,
      SkipServiceWorker skip_service_worker,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBodyImpl> body,
      bool initiated_in_secure_context);

  // Returns the handler attached to |request|. This may return null
  // if no handler is attached.
  static ForeignFetchRequestHandler* GetHandler(net::URLRequest* request);

  // Creates a protocol interceptor for foreign fetch.
  static std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor(
      ResourceContext* resource_context);

  ~ForeignFetchRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  net::URLRequestJob* MaybeCreateJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     ResourceContext* resource_context);

 private:
  ForeignFetchRequestHandler(
      ServiceWorkerContextWrapper* context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBodyImpl> body);

  // Called when a ServiceWorkerRegistration has (or hasn't) been found for the
  // request being handled.
  void DidFindRegistration(
      const base::WeakPtr<ServiceWorkerURLRequestJob>& job,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  // ServiceWorkerURLRequestJob::Delegate implementation:
  void OnPrepareToRestart() override;
  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override;

  // Sets |job_| to nullptr, and clears all extra response info associated with
  // that job.
  void ClearJob();

  scoped_refptr<ServiceWorkerContextWrapper> context_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  ResourceType resource_type_;
  FetchRequestMode request_mode_;
  FetchCredentialsMode credentials_mode_;
  FetchRedirectMode redirect_mode_;
  RequestContextType request_context_type_;
  RequestContextFrameType frame_type_;
  scoped_refptr<ResourceRequestBodyImpl> body_;

  base::WeakPtr<ServiceWorkerURLRequestJob> job_;
  scoped_refptr<ServiceWorkerVersion> target_worker_;

  // True if the next time this request is started, the response should be
  // delivered from the network, bypassing the ServiceWorker.
  bool use_network_ = false;

  base::WeakPtrFactory<ForeignFetchRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ForeignFetchRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_FOREIGN_FETCH_REQUEST_HANDLER_H_
