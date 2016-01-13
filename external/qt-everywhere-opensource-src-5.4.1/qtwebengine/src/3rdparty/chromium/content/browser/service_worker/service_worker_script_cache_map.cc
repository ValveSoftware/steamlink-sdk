// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include "base/logging.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : owner_(owner),
      context_(context),
      has_error_(false) {
}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64 ServiceWorkerScriptCacheMap::Lookup(const GURL& url) {
  ResourceIDMap::const_iterator found = resource_ids_.find(url);
  if (found == resource_ids_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second;
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(
    const GURL& url, int64 resource_id) {
  DCHECK_EQ(kInvalidServiceWorkerResponseId, Lookup(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW);
  resource_ids_[url] = resource_id;
  context_->storage()->StoreUncommittedReponseId(resource_id);
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url, bool success) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, Lookup(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW);
  if (!success) {
    context_->storage()->DoomUncommittedResponse(Lookup(url));
    has_error_ = true;
    resource_ids_.erase(url);
  }
}

void ServiceWorkerScriptCacheMap::GetResources(
    std::vector<ServiceWorkerDatabase::ResourceRecord>* resources) {
  DCHECK(resources->empty());
  for (ResourceIDMap::const_iterator it = resource_ids_.begin();
       it != resource_ids_.end(); ++it) {
    resources->push_back(
        ServiceWorkerDatabase::ResourceRecord(it->second, it->first));
  }
}

void ServiceWorkerScriptCacheMap::SetResources(
    const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources) {
  DCHECK(resource_ids_.empty());
  typedef std::vector<ServiceWorkerDatabase::ResourceRecord> RecordVector;
  for (RecordVector::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    resource_ids_[it->url] = it->resource_id;
  }
}

}  // namespace content
