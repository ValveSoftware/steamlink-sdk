// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_

#include "content/browser/service_worker/service_worker_request_handler.h"

namespace content {

class ServiceWorkerVersion;

// A request handler derivative used to handle requests from
// service workers.
class CONTENT_EXPORT ServiceWorkerContextRequestHandler
    : public ServiceWorkerRequestHandler {
 public:
  ServiceWorkerContextRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context,
      ResourceType::Type resource_type);
  virtual ~ServiceWorkerContextRequestHandler();

  // Called via custom URLRequestJobFactory.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) OVERRIDE;

 private:
  bool ShouldAddToScriptCache(const GURL& url);
  bool ShouldReadFromScriptCache(const GURL& url, int64* response_id_out);

  scoped_refptr<ServiceWorkerVersion> version_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_
