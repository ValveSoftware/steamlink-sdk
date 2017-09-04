// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_filter.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_context.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

ResourceMessageFilter::ResourceMessageFilter(
    int child_id,
    int process_type,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    storage::FileSystemContext* file_system_context,
    ServiceWorkerContextWrapper* service_worker_context,
    const GetContextsCallback& get_contexts_callback)
    : BrowserMessageFilter(ResourceMsgStart),
      BrowserAssociatedInterface<mojom::URLLoaderFactory>(this, this),
      child_id_(child_id),
      process_type_(process_type),
      is_channel_closed_(false),
      appcache_service_(appcache_service),
      blob_storage_context_(blob_storage_context),
      file_system_context_(file_system_context),
      service_worker_context_(service_worker_context),
      get_contexts_callback_(get_contexts_callback),
      weak_ptr_factory_(this) {}

ResourceMessageFilter::~ResourceMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(is_channel_closed_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void ResourceMessageFilter::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  ResourceDispatcherHostImpl::Get()->CancelRequestsForProcess(child_id_);

  weak_ptr_factory_.InvalidateWeakPtrs();
  is_channel_closed_ = true;
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return ResourceDispatcherHostImpl::Get()->OnMessageReceived(message, this);
}

void ResourceMessageFilter::OnDestruct() const {
  // Destroy the filter on the IO thread since that's where its weak pointers
  // are being used.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void ResourceMessageFilter::GetContexts(
    ResourceType resource_type,
    ResourceContext** resource_context,
    net::URLRequestContext** request_context) {
  return get_contexts_callback_.Run(resource_type, resource_context,
                                    request_context);
}

base::WeakPtr<ResourceMessageFilter> ResourceMessageFilter::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return is_channel_closed_ ? nullptr : weak_ptr_factory_.GetWeakPtr();
}

void ResourceMessageFilter::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) {
  URLLoaderFactoryImpl::CreateLoaderAndStart(std::move(request), routing_id,
                                             request_id, url_request,
                                             std::move(client_ptr_info), this);
}

void ResourceMessageFilter::SyncLoad(int32_t routing_id,
                                     int32_t request_id,
                                     const ResourceRequest& url_request,
                                     const SyncLoadCallback& callback) {
  URLLoaderFactoryImpl::SyncLoad(routing_id, request_id, url_request, callback,
                                 this);
}

}  // namespace content
