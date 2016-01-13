// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_request_handler.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_read_from_cache_job.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerContextRequestHandler::ServiceWorkerContextRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context,
    ResourceType::Type resource_type)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      version_(provider_host_->running_hosted_version()) {
  DCHECK(provider_host_->IsHostToRunningServiceWorker());
}

ServiceWorkerContextRequestHandler::~ServiceWorkerContextRequestHandler() {
}

net::URLRequestJob* ServiceWorkerContextRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  if (!provider_host_ || !version_ || !context_)
    return NULL;

  // We currently have no use case for hijacking a redirected request.
  if (request->url_chain().size() > 1)
    return NULL;

  // We only use the script cache for main script loading and
  // importScripts(), even if a cached script is xhr'd, we don't
  // retrieve it from the script cache.
  // TODO(michaeln): Get the desired behavior clarified in the spec,
  // and make tweak the behavior here to match.
  if (resource_type_ != ResourceType::SERVICE_WORKER &&
      resource_type_ != ResourceType::SCRIPT) {
    return NULL;
  }

  if (ShouldAddToScriptCache(request->url())) {
    int64 response_id = context_->storage()->NewResourceId();
    if (response_id == kInvalidServiceWorkerResponseId)
      return NULL;
    return new ServiceWorkerWriteToCacheJob(
        request, network_delegate, context_, version_, response_id);
  }

  int64 response_id = kInvalidServiceWorkerResponseId;
  if (ShouldReadFromScriptCache(request->url(), &response_id)) {
    return new ServiceWorkerReadFromCacheJob(
        request, network_delegate, context_, response_id);
  }

  // NULL means use the network.
  return NULL;
}

bool ServiceWorkerContextRequestHandler::ShouldAddToScriptCache(
    const GURL& url) {
  // We only write imports that occur during the initial eval.
  if (version_->status() != ServiceWorkerVersion::NEW)
    return false;
  return version_->script_cache_map()->Lookup(url) ==
            kInvalidServiceWorkerResponseId;
}

bool ServiceWorkerContextRequestHandler::ShouldReadFromScriptCache(
    const GURL& url, int64* response_id_out) {
  // We don't read from the script cache until the version is INSTALLED.
  if (version_->status() == ServiceWorkerVersion::NEW ||
      version_->status() == ServiceWorkerVersion::INSTALLING)
    return false;
  *response_id_out = version_->script_cache_map()->Lookup(url);
  return *response_id_out != kInvalidServiceWorkerResponseId;
}

}  // namespace content
