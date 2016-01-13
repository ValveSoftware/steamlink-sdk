// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_BACKEND_IMPL_H_
#define WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_BACKEND_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend_delegate.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_status_code.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class QuotaBackendImplTest;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemUsageCache;
class ObfuscatedFileUtil;

// An instance of this class is owned by QuotaReservationManager.
class WEBKIT_STORAGE_BROWSER_EXPORT QuotaBackendImpl
    : public QuotaReservationManager::QuotaBackend {
 public:
  typedef QuotaReservationManager::ReserveQuotaCallback
      ReserveQuotaCallback;

  QuotaBackendImpl(base::SequencedTaskRunner* file_task_runner,
                   ObfuscatedFileUtil* obfuscated_file_util,
                   FileSystemUsageCache* file_system_usage_cache,
                   quota::QuotaManagerProxy* quota_manager_proxy);
  virtual ~QuotaBackendImpl();

  // QuotaReservationManager::QuotaBackend overrides.
  virtual void ReserveQuota(
      const GURL& origin,
      FileSystemType type,
      int64 delta,
      const ReserveQuotaCallback& callback) OVERRIDE;
  virtual void ReleaseReservedQuota(
      const GURL& origin,
      FileSystemType type,
      int64 size) OVERRIDE;
  virtual void CommitQuotaUsage(
      const GURL& origin,
      FileSystemType type,
      int64 delta) OVERRIDE;
  virtual void IncrementDirtyCount(
      const GURL& origin,
      FileSystemType type) OVERRIDE;
  virtual void DecrementDirtyCount(
      const GURL& origin,
      FileSystemType type) OVERRIDE;

 private:
  friend class content::QuotaBackendImplTest;

  struct QuotaReservationInfo {
    QuotaReservationInfo(const GURL& origin, FileSystemType type, int64 delta);
    ~QuotaReservationInfo();

    GURL origin;
    FileSystemType type;
    int64 delta;
  };

  void DidGetUsageAndQuotaForReserveQuota(
      const QuotaReservationInfo& info,
      const ReserveQuotaCallback& callback,
      quota::QuotaStatusCode status,
      int64 usage,
      int64 quota);

  void ReserveQuotaInternal(
      const QuotaReservationInfo& info);
  base::File::Error GetUsageCachePath(
      const GURL& origin,
      FileSystemType type,
      base::FilePath* usage_file_path);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Owned by SandboxFileSystemBackendDelegate.
  ObfuscatedFileUtil* obfuscated_file_util_;
  FileSystemUsageCache* file_system_usage_cache_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  base::WeakPtrFactory<QuotaBackendImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaBackendImpl);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_BACKEND_IMPL_H_
