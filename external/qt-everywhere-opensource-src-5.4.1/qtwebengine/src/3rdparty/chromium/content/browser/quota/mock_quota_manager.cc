// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/mock_quota_manager.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "url/gurl.h"

using quota::kQuotaStatusOk;

namespace content {

MockQuotaManager::OriginInfo::OriginInfo(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    base::Time modified)
    : origin(origin),
      type(type),
      quota_client_mask(quota_client_mask),
      modified(modified) {
}

MockQuotaManager::OriginInfo::~OriginInfo() {}

MockQuotaManager::StorageInfo::StorageInfo() : usage(0), quota(kint64max) {}
MockQuotaManager::StorageInfo::~StorageInfo() {}

MockQuotaManager::MockQuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    base::SingleThreadTaskRunner* io_thread,
    base::SequencedTaskRunner* db_thread,
    SpecialStoragePolicy* special_storage_policy)
    : QuotaManager(is_incognito, profile_path, io_thread, db_thread,
        special_storage_policy),
      weak_factory_(this) {
}

void MockQuotaManager::GetUsageAndQuota(
    const GURL& origin,
    quota::StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  StorageInfo& info = usage_and_quota_map_[std::make_pair(origin, type)];
  callback.Run(quota::kQuotaStatusOk, info.usage, info.quota);
}

void MockQuotaManager::SetQuota(const GURL& origin, StorageType type,
                                int64 quota) {
  usage_and_quota_map_[std::make_pair(origin, type)].quota = quota;
}

bool MockQuotaManager::AddOrigin(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    base::Time modified) {
  origins_.push_back(OriginInfo(origin, type, quota_client_mask, modified));
  return true;
}

bool MockQuotaManager::OriginHasData(
    const GURL& origin,
    StorageType type,
    QuotaClient::ID quota_client) const {
  for (std::vector<OriginInfo>::const_iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin &&
        current->type == type &&
        current->quota_client_mask & quota_client)
      return true;
  }
  return false;
}

void MockQuotaManager::GetOriginsModifiedSince(
    StorageType type,
    base::Time modified_since,
    const GetOriginsCallback& callback) {
  std::set<GURL>* origins_to_return = new std::set<GURL>();
  for (std::vector<OriginInfo>::const_iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->type == type && current->modified >= modified_since)
      origins_to_return->insert(current->origin);
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockQuotaManager::DidGetModifiedSince,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(origins_to_return),
                 type));
}

void MockQuotaManager::DeleteOriginData(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    const StatusCallback& callback) {
  for (std::vector<OriginInfo>::iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin && current->type == type) {
      // Modify the mask: if it's 0 after "deletion", remove the origin.
      current->quota_client_mask &= ~quota_client_mask;
      if (current->quota_client_mask == 0)
        origins_.erase(current);
      break;
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockQuotaManager::DidDeleteOriginData,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 kQuotaStatusOk));
}

MockQuotaManager::~MockQuotaManager() {}

void MockQuotaManager::UpdateUsage(
    const GURL& origin, StorageType type, int64 delta) {
  usage_and_quota_map_[std::make_pair(origin, type)].usage += delta;
}

void MockQuotaManager::DidGetModifiedSince(
    const GetOriginsCallback& callback,
    std::set<GURL>* origins,
    StorageType storage_type) {
  callback.Run(*origins, storage_type);
}

void MockQuotaManager::DidDeleteOriginData(
    const StatusCallback& callback,
    QuotaStatusCode status) {
  callback.Run(status);
}

}  // namespace content
