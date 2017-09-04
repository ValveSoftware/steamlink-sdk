// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace storage {

// The proxy may be called and finally released on any thread.
class STORAGE_EXPORT QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  typedef QuotaManager::GetUsageAndQuotaCallback
      GetUsageAndQuotaCallback;

  virtual void RegisterClient(QuotaClient* client);
  virtual void NotifyStorageAccessed(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type);
  virtual void NotifyStorageModified(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type,
                                     int64_t delta);
  virtual void NotifyOriginInUse(const GURL& origin);
  virtual void NotifyOriginNoLongerInUse(const GURL& origin);

  virtual void SetUsageCacheEnabled(QuotaClient::ID client_id,
                                    const GURL& origin,
                                    StorageType type,
                                    bool enabled);
  virtual void GetUsageAndQuota(
      base::SequencedTaskRunner* original_task_runner,
      const GURL& origin,
      StorageType type,
      const GetUsageAndQuotaCallback& callback);

  // This method may only be called on the IO thread.
  // It may return NULL if the manager has already been deleted.
  QuotaManager* quota_manager() const;

 protected:
  friend class QuotaManager;
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;

  QuotaManagerProxy(
      QuotaManager* manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread);
  virtual ~QuotaManagerProxy();

  QuotaManager* manager_;  // only accessed on the io thread
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerProxy);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
