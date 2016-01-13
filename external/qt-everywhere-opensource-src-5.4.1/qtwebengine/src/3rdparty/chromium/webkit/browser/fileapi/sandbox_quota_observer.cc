// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_quota_observer.h"

#include "base/sequenced_task_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend_delegate.h"
#include "webkit/browser/fileapi/timed_task_helper.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

SandboxQuotaObserver::SandboxQuotaObserver(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* update_notify_runner,
    ObfuscatedFileUtil* sandbox_file_util,
    FileSystemUsageCache* file_system_usage_cache)
    : quota_manager_proxy_(quota_manager_proxy),
      update_notify_runner_(update_notify_runner),
      sandbox_file_util_(sandbox_file_util),
      file_system_usage_cache_(file_system_usage_cache) {}

SandboxQuotaObserver::~SandboxQuotaObserver() {}

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  file_system_usage_cache_->IncrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url,
                                    int64 delta) {
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());

  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()),
        delta);
  }

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  pending_update_notification_[usage_file_path] += delta;
  if (!delayed_cache_update_helper_) {
    delayed_cache_update_helper_.reset(
        new TimedTaskHelper(update_notify_runner_.get()));
    delayed_cache_update_helper_->Start(
        FROM_HERE,
        base::TimeDelta(),  // No delay.
        base::Bind(&SandboxQuotaObserver::ApplyPendingUsageUpdate,
                   base::Unretained(this)));
  }
}

void SandboxQuotaObserver::OnEndUpdate(const FileSystemURL& url) {
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  PendingUpdateNotificationMap::iterator found =
      pending_update_notification_.find(usage_file_path);
  if (found != pending_update_notification_.end()) {
    UpdateUsageCacheFile(found->first, found->second);
    pending_update_notification_.erase(found);
  }

  file_system_usage_cache_->DecrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnAccess(const FileSystemURL& url) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyStorageAccessed(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()));
  }
}

void SandboxQuotaObserver::SetUsageCacheEnabled(
    const GURL& origin,
    FileSystemType type,
    bool enabled) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->SetUsageCacheEnabled(
        quota::QuotaClient::kFileSystem,
        origin,
        FileSystemTypeToQuotaStorageType(type),
        enabled);
  }
}

base::FilePath SandboxQuotaObserver::GetUsageCachePath(
    const FileSystemURL& url) {
  DCHECK(sandbox_file_util_);
  base::File::Error error = base::File::FILE_OK;
  base::FilePath path =
      SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
          sandbox_file_util_, url.origin(), url.type(), &error);
  if (error != base::File::FILE_OK) {
    LOG(WARNING) << "Could not get usage cache path for: "
                 << url.DebugString();
    return base::FilePath();
  }
  return path;
}

void SandboxQuotaObserver::ApplyPendingUsageUpdate() {
  delayed_cache_update_helper_.reset();
  for (PendingUpdateNotificationMap::iterator itr =
           pending_update_notification_.begin();
       itr != pending_update_notification_.end();
       ++itr) {
    UpdateUsageCacheFile(itr->first, itr->second);
  }
  pending_update_notification_.clear();
}

void SandboxQuotaObserver::UpdateUsageCacheFile(
    const base::FilePath& usage_file_path,
    int64 delta) {
  DCHECK(!usage_file_path.empty());
  if (!usage_file_path.empty() && delta != 0)
    file_system_usage_cache_->AtomicUpdateUsageByDelta(usage_file_path, delta);
}

}  // namespace fileapi
