// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/worker_storage_partition.h"

#include <string>

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {

WorkerStoragePartition::WorkerStoragePartition(
    net::URLRequestContextGetter* url_request_context,
    net::URLRequestContextGetter* media_url_request_context,
    ChromeAppCacheService* appcache_service,
    storage::QuotaManager* quota_manager,
    storage::FileSystemContext* filesystem_context,
    storage::DatabaseTracker* database_tracker,
    IndexedDBContextImpl* indexed_db_context,
    ServiceWorkerContextWrapper* service_worker_context)
    : url_request_context_(url_request_context),
      media_url_request_context_(media_url_request_context),
      appcache_service_(appcache_service),
      quota_manager_(quota_manager),
      filesystem_context_(filesystem_context),
      database_tracker_(database_tracker),
      indexed_db_context_(indexed_db_context),
      service_worker_context_(service_worker_context) {
}

WorkerStoragePartition::WorkerStoragePartition(
    const WorkerStoragePartition& other) {
  Copy(other);
}

const WorkerStoragePartition& WorkerStoragePartition::operator=(
    const WorkerStoragePartition& rhs) {
  Copy(rhs);
  return *this;
}

bool WorkerStoragePartition::Equals(
    const WorkerStoragePartition& other) const {
  return url_request_context_.get() == other.url_request_context_.get() &&
         media_url_request_context_.get() ==
             other.media_url_request_context_.get() &&
         appcache_service_.get() == other.appcache_service_.get() &&
         quota_manager_.get() == other.quota_manager_.get() &&
         filesystem_context_.get() == other.filesystem_context_.get() &&
         database_tracker_.get() == other.database_tracker_.get() &&
         indexed_db_context_.get() == other.indexed_db_context_.get() &&
         service_worker_context_.get() == other.service_worker_context_.get();
}

WorkerStoragePartition::~WorkerStoragePartition() {
}

void WorkerStoragePartition::Copy(const WorkerStoragePartition& other) {
  url_request_context_ = other.url_request_context_;
  media_url_request_context_ = other.media_url_request_context_;
  appcache_service_ = other.appcache_service_;
  quota_manager_ = other.quota_manager_;
  filesystem_context_ = other.filesystem_context_;
  database_tracker_ = other.database_tracker_;
  indexed_db_context_ = other.indexed_db_context_;
  service_worker_context_ = other.service_worker_context_;
}

WorkerStoragePartitionId::WorkerStoragePartitionId(
    const WorkerStoragePartition& partition)
    : url_request_context_(partition.url_request_context()),
      media_url_request_context_(partition.media_url_request_context()),
      appcache_service_(partition.appcache_service()),
      quota_manager_(partition.quota_manager()),
      filesystem_context_(partition.filesystem_context()),
      database_tracker_(partition.database_tracker()),
      indexed_db_context_(partition.indexed_db_context()),
      service_worker_context_(partition.service_worker_context()) {
}

WorkerStoragePartitionId::~WorkerStoragePartitionId() {
}

bool WorkerStoragePartitionId::Equals(
    const WorkerStoragePartitionId& other) const {
  return url_request_context_ == other.url_request_context_ &&
         media_url_request_context_ == other.media_url_request_context_ &&
         appcache_service_ == other.appcache_service_ &&
         quota_manager_ == other.quota_manager_ &&
         filesystem_context_ == other.filesystem_context_ &&
         database_tracker_ == other.database_tracker_ &&
         indexed_db_context_ == other.indexed_db_context_ &&
         service_worker_context_ == other.service_worker_context_;
}

}  // namespace content
