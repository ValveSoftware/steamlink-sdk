// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_filter.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/host_zoom_level_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
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
    HostZoomLevelContext* host_zoom_level_context,
    const GetContextsCallback& get_contexts_callback)
    : BrowserMessageFilter(ResourceMsgStart),
      child_id_(child_id),
      process_type_(process_type),
      appcache_service_(appcache_service),
      blob_storage_context_(blob_storage_context),
      file_system_context_(file_system_context),
      service_worker_context_(service_worker_context),
      host_zoom_level_context_(host_zoom_level_context),
      get_contexts_callback_(get_contexts_callback),
      weak_ptr_factory_(this) {
}

ResourceMessageFilter::~ResourceMessageFilter() {
}

void ResourceMessageFilter::OnChannelClosing() {
  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  ResourceDispatcherHostImpl::Get()->CancelRequestsForProcess(child_id_);
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return ResourceDispatcherHostImpl::Get()->OnMessageReceived(message, this);
}

void ResourceMessageFilter::GetContexts(
    ResourceType resource_type,
    int origin_pid,
    ResourceContext** resource_context,
    net::URLRequestContext** request_context) {
  return get_contexts_callback_.Run(resource_type, origin_pid, resource_context,
                                    request_context);
}

const HostZoomMap* ResourceMessageFilter::GetHostZoomMap() const {
  if (host_zoom_level_context_.get())
    return host_zoom_level_context_->GetHostZoomMap();
  return NULL;
}

base::WeakPtr<ResourceMessageFilter> ResourceMessageFilter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
