// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_PROXY_H_
#define CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/quota/mock_quota_manager.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

using storage::QuotaManagerProxy;

namespace content {

class MockQuotaManager;

class MockQuotaManagerProxy : public QuotaManagerProxy {
 public:
  // It is ok to give NULL to |quota_manager|.
  MockQuotaManagerProxy(MockQuotaManager* quota_manager,
                        base::SingleThreadTaskRunner* task_runner);

  void RegisterClient(QuotaClient* client) override;

  void SimulateQuotaManagerDestroyed();

  // We don't mock them.
  void NotifyOriginInUse(const GURL& origin) override {}
  void NotifyOriginNoLongerInUse(const GURL& origin) override {}
  void SetUsageCacheEnabled(QuotaClient::ID client_id,
                            const GURL& origin,
                            StorageType type,
                            bool enabled) override {}
  void GetUsageAndQuota(
      base::SequencedTaskRunner* original_task_runner,
      const GURL& origin,
      StorageType type,
      const QuotaManager::GetUsageAndQuotaCallback& callback) override;

  // Validates the |client_id| and updates the internal access count
  // which can be accessed via notify_storage_accessed_count().
  // The also records the |origin| and |type| in last_notified_origin_ and
  // last_notified_type_.
  void NotifyStorageAccessed(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type) override;

  // Records the |origin|, |type| and |delta| as last_notified_origin_,
  // last_notified_type_ and last_notified_delta_ respecitvely.
  // If non-null MockQuotaManager is given to the constructor this also
  // updates the manager's internal usage information.
  void NotifyStorageModified(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type,
                             int64_t delta) override;

  int notify_storage_accessed_count() const { return storage_accessed_count_; }
  int notify_storage_modified_count() const { return storage_modified_count_; }
  GURL last_notified_origin() const { return last_notified_origin_; }
  StorageType last_notified_type() const { return last_notified_type_; }
  int64_t last_notified_delta() const { return last_notified_delta_; }

 protected:
  ~MockQuotaManagerProxy() override;

 private:
  MockQuotaManager* mock_manager() const {
    return static_cast<MockQuotaManager*>(quota_manager());
  }

  int storage_accessed_count_;
  int storage_modified_count_;
  GURL last_notified_origin_;
  StorageType last_notified_type_;
  int64_t last_notified_delta_;

  QuotaClient* registered_client_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_PROXY_H_
