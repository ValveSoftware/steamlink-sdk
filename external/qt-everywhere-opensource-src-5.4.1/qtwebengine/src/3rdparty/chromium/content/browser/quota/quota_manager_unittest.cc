// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/mock_storage_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_database.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

using base::MessageLoopProxy;
using quota::kQuotaErrorAbort;
using quota::kQuotaErrorInvalidModification;
using quota::kQuotaErrorNotSupported;
using quota::kQuotaStatusOk;
using quota::kQuotaStatusUnknown;
using quota::kStorageTypePersistent;
using quota::kStorageTypeSyncable;
using quota::kStorageTypeTemporary;
using quota::kStorageTypeUnknown;
using quota::QuotaClient;
using quota::QuotaManager;
using quota::QuotaStatusCode;
using quota::StorageType;
using quota::UsageAndQuota;
using quota::UsageInfo;
using quota::UsageInfoEntries;

namespace content {

namespace {

// For shorter names.
const StorageType kTemp = kStorageTypeTemporary;
const StorageType kPerm = kStorageTypePersistent;
const StorageType kSync = kStorageTypeSyncable;

const int kAllClients = QuotaClient::kAllClientsMask;

const int64 kAvailableSpaceForApp = 13377331U;

const int64 kMinimumPreserveForSystem = QuotaManager::kMinimumPreserveForSystem;
const int kPerHostTemporaryPortion = QuotaManager::kPerHostTemporaryPortion;

// Returns a deterministic value for the amount of available disk space.
int64 GetAvailableDiskSpaceForTest(const base::FilePath&) {
  return kAvailableSpaceForApp + kMinimumPreserveForSystem;
}

}  // namespace

class QuotaManagerTest : public testing::Test {
 protected:
  typedef QuotaManager::QuotaTableEntry QuotaTableEntry;
  typedef QuotaManager::QuotaTableEntries QuotaTableEntries;
  typedef QuotaManager::OriginInfoTableEntries OriginInfoTableEntries;

 public:
  QuotaManagerTest()
      : mock_time_counter_(0),
        weak_factory_(this) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    mock_special_storage_policy_ = new MockSpecialStoragePolicy;
    ResetQuotaManager(false /* is_incognito */);
  }

  virtual void TearDown() {
    // Make sure the quota manager cleans up correctly.
    quota_manager_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void ResetQuotaManager(bool is_incognito) {
    quota_manager_ = new QuotaManager(is_incognito,
                                      data_dir_.path(),
                                      MessageLoopProxy::current().get(),
                                      MessageLoopProxy::current().get(),
                                      mock_special_storage_policy_.get());
    // Don't (automatically) start the eviction for testing.
    quota_manager_->eviction_disabled_ = true;
    // Don't query the hard disk for remaining capacity.
    quota_manager_->get_disk_space_fn_ = &GetAvailableDiskSpaceForTest;
    additional_callback_count_ = 0;
  }

  MockStorageClient* CreateClient(
      const MockOriginData* mock_data,
      size_t mock_data_size,
      QuotaClient::ID id) {
    return new MockStorageClient(quota_manager_->proxy(),
                                 mock_data, id, mock_data_size);
  }

  void RegisterClient(MockStorageClient* client) {
    quota_manager_->proxy()->RegisterClient(client);
  }

  void GetUsageInfo() {
    usage_info_.clear();
    quota_manager_->GetUsageInfo(
        base::Bind(&QuotaManagerTest::DidGetUsageInfo,
                   weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForWebApps(const GURL& origin,
                                  StorageType type) {
    quota_status_ = kQuotaStatusUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuotaForWebApps(
        origin, type, base::Bind(&QuotaManagerTest::DidGetUsageAndQuota,
                                 weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForStorageClient(const GURL& origin,
                                        StorageType type) {
    quota_status_ = kQuotaStatusUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuota(
        origin, type, base::Bind(&QuotaManagerTest::DidGetUsageAndQuota,
                                 weak_factory_.GetWeakPtr()));
  }

  void GetTemporaryGlobalQuota() {
    quota_status_ = kQuotaStatusUnknown;
    quota_ = -1;
    quota_manager_->GetTemporaryGlobalQuota(
        base::Bind(&QuotaManagerTest::DidGetQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void SetTemporaryGlobalQuota(int64 new_quota) {
    quota_status_ = kQuotaStatusUnknown;
    quota_ = -1;
    quota_manager_->SetTemporaryGlobalOverrideQuota(
        new_quota,
        base::Bind(&QuotaManagerTest::DidGetQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void GetPersistentHostQuota(const std::string& host) {
    quota_status_ = kQuotaStatusUnknown;
    quota_ = -1;
    quota_manager_->GetPersistentHostQuota(
        host,
        base::Bind(&QuotaManagerTest::DidGetHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void SetPersistentHostQuota(const std::string& host, int64 new_quota) {
    quota_status_ = kQuotaStatusUnknown;
    quota_ = -1;
    quota_manager_->SetPersistentHostQuota(
        host, new_quota,
        base::Bind(&QuotaManagerTest::DidGetHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void GetGlobalUsage(StorageType type) {
    usage_ = -1;
    unlimited_usage_ = -1;
    quota_manager_->GetGlobalUsage(
        type,
        base::Bind(&QuotaManagerTest::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
  }

  void GetHostUsage(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_->GetHostUsage(
        host, type,
        base::Bind(&QuotaManagerTest::DidGetHostUsage,
                   weak_factory_.GetWeakPtr()));
  }

  void RunAdditionalUsageAndQuotaTask(const GURL& origin, StorageType type) {
    quota_manager_->GetUsageAndQuota(
        origin, type,
        base::Bind(&QuotaManagerTest::DidGetUsageAndQuotaAdditional,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteClientOriginData(QuotaClient* client,
                              const GURL& origin,
                              StorageType type) {
    DCHECK(client);
    quota_status_ = kQuotaStatusUnknown;
    client->DeleteOriginData(
        origin, type,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void EvictOriginData(const GURL& origin,
                       StorageType type) {
    quota_status_ = kQuotaStatusUnknown;
    quota_manager_->EvictOriginData(
        origin, type,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteOriginData(const GURL& origin,
                        StorageType type,
                        int quota_client_mask) {
    quota_status_ = kQuotaStatusUnknown;
    quota_manager_->DeleteOriginData(
        origin, type, quota_client_mask,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void DeleteHostData(const std::string& host,
                      StorageType type,
                      int quota_client_mask) {
    quota_status_ = kQuotaStatusUnknown;
    quota_manager_->DeleteHostData(
        host, type, quota_client_mask,
        base::Bind(&QuotaManagerTest::StatusCallback,
                   weak_factory_.GetWeakPtr()));
  }

  void GetAvailableSpace() {
    quota_status_ = kQuotaStatusUnknown;
    available_space_ = -1;
    quota_manager_->GetAvailableSpace(
        base::Bind(&QuotaManagerTest::DidGetAvailableSpace,
                   weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForEviction() {
    quota_status_ = kQuotaStatusUnknown;
    usage_ = -1;
    unlimited_usage_ = -1;
    quota_ = -1;
    available_space_ = -1;
    quota_manager_->GetUsageAndQuotaForEviction(
        base::Bind(&QuotaManagerTest::DidGetUsageAndQuotaForEviction,
                   weak_factory_.GetWeakPtr()));
  }

  void GetCachedOrigins(StorageType type, std::set<GURL>* origins) {
    ASSERT_TRUE(origins != NULL);
    origins->clear();
    quota_manager_->GetCachedOrigins(type, origins);
  }

  void NotifyStorageAccessed(QuotaClient* client,
                             const GURL& origin,
                             StorageType type) {
    DCHECK(client);
    quota_manager_->NotifyStorageAccessedInternal(
        client->id(), origin, type, IncrementMockTime());
  }

  void DeleteOriginFromDatabase(const GURL& origin, StorageType type) {
    quota_manager_->DeleteOriginFromDatabase(origin, type);
  }

  void GetLRUOrigin(StorageType type) {
    lru_origin_ = GURL();
    quota_manager_->GetLRUOrigin(
        type,
        base::Bind(&QuotaManagerTest::DidGetLRUOrigin,
                   weak_factory_.GetWeakPtr()));
  }

  void NotifyOriginInUse(const GURL& origin) {
    quota_manager_->NotifyOriginInUse(origin);
  }

  void NotifyOriginNoLongerInUse(const GURL& origin) {
    quota_manager_->NotifyOriginNoLongerInUse(origin);
  }

  void GetOriginsModifiedSince(StorageType type, base::Time modified_since) {
    modified_origins_.clear();
    modified_origins_type_ = kStorageTypeUnknown;
    quota_manager_->GetOriginsModifiedSince(
        type, modified_since,
        base::Bind(&QuotaManagerTest::DidGetModifiedOrigins,
                   weak_factory_.GetWeakPtr()));
  }

  void DumpQuotaTable() {
    quota_entries_.clear();
    quota_manager_->DumpQuotaTable(
        base::Bind(&QuotaManagerTest::DidDumpQuotaTable,
                   weak_factory_.GetWeakPtr()));
  }

  void DumpOriginInfoTable() {
    origin_info_entries_.clear();
    quota_manager_->DumpOriginInfoTable(
        base::Bind(&QuotaManagerTest::DidDumpOriginInfoTable,
                   weak_factory_.GetWeakPtr()));
  }

  void DidGetUsageInfo(const UsageInfoEntries& entries) {
    usage_info_.insert(usage_info_.begin(), entries.begin(), entries.end());
  }

  void DidGetUsageAndQuota(QuotaStatusCode status, int64 usage, int64 quota) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
  }

  void DidGetQuota(QuotaStatusCode status,
                   int64 quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetAvailableSpace(QuotaStatusCode status, int64 available_space) {
    quota_status_ = status;
    available_space_ = available_space;
  }

  void DidGetHostQuota(QuotaStatusCode status,
                       int64 quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetGlobalUsage(int64 usage,
                         int64 unlimited_usage) {
    usage_ = usage;
    unlimited_usage_ = unlimited_usage;
  }

  void DidGetHostUsage(int64 usage) {
    usage_ = usage;
  }

  void StatusCallback(QuotaStatusCode status) {
    ++status_callback_count_;
    quota_status_ = status;
  }

  void DidGetUsageAndQuotaForEviction(QuotaStatusCode status,
                                      const UsageAndQuota& usage_and_quota) {
    quota_status_ = status;
    limited_usage_ = usage_and_quota.global_limited_usage;
    quota_ = usage_and_quota.quota;
    available_space_ = usage_and_quota.available_disk_space;
  }

  void DidGetLRUOrigin(const GURL& origin) {
    lru_origin_ = origin;
  }

  void DidGetModifiedOrigins(const std::set<GURL>& origins, StorageType type) {
    modified_origins_ = origins;
    modified_origins_type_ = type;
  }

  void DidDumpQuotaTable(const QuotaTableEntries& entries) {
    quota_entries_ = entries;
  }

  void DidDumpOriginInfoTable(const OriginInfoTableEntries& entries) {
    origin_info_entries_ = entries;
  }

  void GetUsage_WithModifyTestBody(const StorageType type);

  void set_additional_callback_count(int c) { additional_callback_count_ = c; }
  int additional_callback_count() const { return additional_callback_count_; }
  void DidGetUsageAndQuotaAdditional(
      QuotaStatusCode status, int64 usage, int64 quota) {
    ++additional_callback_count_;
  }

  QuotaManager* quota_manager() const { return quota_manager_.get(); }
  void set_quota_manager(QuotaManager* quota_manager) {
    quota_manager_ = quota_manager;
  }

  MockSpecialStoragePolicy* mock_special_storage_policy() const {
    return mock_special_storage_policy_.get();
  }

  QuotaStatusCode status() const { return quota_status_; }
  const UsageInfoEntries& usage_info() const { return usage_info_; }
  int64 usage() const { return usage_; }
  int64 limited_usage() const { return limited_usage_; }
  int64 unlimited_usage() const { return unlimited_usage_; }
  int64 quota() const { return quota_; }
  int64 available_space() const { return available_space_; }
  const GURL& lru_origin() const { return lru_origin_; }
  const std::set<GURL>& modified_origins() const { return modified_origins_; }
  StorageType modified_origins_type() const { return modified_origins_type_; }
  const QuotaTableEntries& quota_entries() const { return quota_entries_; }
  const OriginInfoTableEntries& origin_info_entries() const {
    return origin_info_entries_;
  }
  base::FilePath profile_path() const { return data_dir_.path(); }
  int status_callback_count() const { return status_callback_count_; }
  void reset_status_callback_count() { status_callback_count_ = 0; }

 private:
  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
  }

  base::MessageLoop message_loop_;
  base::ScopedTempDir data_dir_;

  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  UsageInfoEntries usage_info_;
  int64 usage_;
  int64 limited_usage_;
  int64 unlimited_usage_;
  int64 quota_;
  int64 available_space_;
  GURL lru_origin_;
  std::set<GURL> modified_origins_;
  StorageType modified_origins_type_;
  QuotaTableEntries quota_entries_;
  OriginInfoTableEntries origin_info_entries_;
  int status_callback_count_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerTest);
};

TEST_F(QuotaManagerTest, GetUsageInfo) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",       kTemp,  10 },
    { "http://foo.com:8080/",  kTemp,  15 },
    { "http://bar.com/",       kTemp,  20 },
    { "http://bar.com/",       kPerm,  50 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",      kTemp,  30 },
    { "https://foo.com:8081/", kTemp,  35 },
    { "http://bar.com/",       kPerm,  40 },
    { "http://example.com/",   kPerm,  40 },
  };
  RegisterClient(CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem));
  RegisterClient(CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kDatabase));

  GetUsageInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4U, usage_info().size());
  for (size_t i = 0; i < usage_info().size(); ++i) {
    const UsageInfo& info = usage_info()[i];
    if (info.host == "foo.com" && info.type == kTemp) {
      EXPECT_EQ(10 + 15 + 30 + 35, info.usage);
    } else if (info.host == "bar.com" && info.type == kTemp) {
      EXPECT_EQ(20, info.usage);
    } else if (info.host == "bar.com" && info.type == kPerm) {
      EXPECT_EQ(50 + 40, info.usage);
    } else if (info.host == "example.com" && info.type == kPerm) {
      EXPECT_EQ(40, info.usage);
    } else {
      ADD_FAILURE()
          << "Unexpected host, type: " << info.host << ", " << info.type;
    }
  }
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(0, quota());
  int64 quota_returned_for_foo = quota();

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(quota_returned_for_foo, quota());
}

TEST_F(QuotaManagerTest, GetUsage_NoClient) {
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetUsage_EmptyClient) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp,  10 },
    { "http://foo.com:8080/",   kTemp,  20 },
    { "http://bar.com/",        kTemp,   5 },
    { "https://bar.com/",       kTemp,   7 },
    { "http://baz.com/",        kTemp,  30 },
    { "http://foo.com/",        kPerm,  40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));

  // This time explicitly sets a temporary global quota.
  SetTemporaryGlobalQuota(100);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());

  const int kPerHostQuota = 100 / kPerHostTemporaryPortion;

  // The host's quota should be its full portion of the global quota
  // since global usage is under the global quota.
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(5 + 7, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerTest, GetUsage_MultipleClients) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",              kTemp, 1 },
    { "http://bar.com/",              kTemp, 2 },
    { "http://bar.com/",              kPerm, 4 },
    { "http://unlimited/",            kPerm, 8 },
    { "http://installed/",            kPerm, 16 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",             kTemp, 128 },
    { "http://example.com/",          kPerm, 256 },
    { "http://unlimited/",            kTemp, 512 },
    { "http://installed/",            kTemp, 1024 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  mock_special_storage_policy()->GrantQueryDiskSize(GURL("http://installed/"));
  RegisterClient(CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem));
  RegisterClient(CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kDatabase));

  const int64 kTempQuotaBase =
      GetAvailableDiskSpaceForTest(base::FilePath()) / kPerHostTemporaryPortion;

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(1 + 128, usage());

  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4, usage());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(512, usage());
  EXPECT_EQ(std::min(kAvailableSpaceForApp, kTempQuotaBase) + usage(), quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(8, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetAvailableSpace();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_LE(0, available_space());

  GetUsageAndQuotaForWebApps(GURL("http://installed/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(1024, usage());
  EXPECT_EQ(std::min(kAvailableSpaceForApp, kTempQuotaBase) + usage(), quota());

  GetUsageAndQuotaForWebApps(GURL("http://installed/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(16, usage());
  EXPECT_EQ(usage(), quota());  // Over-budget case.

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(1 + 2 + 128 + 512 + 1024, usage());
  EXPECT_EQ(512, unlimited_usage());

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4 + 8 + 16 + 256, usage());
  EXPECT_EQ(8, unlimited_usage());
}

void QuotaManagerTest::GetUsage_WithModifyTestBody(const StorageType type) {
  const MockOriginData data[] = {
    { "http://foo.com/",   type,  10 },
    { "http://foo.com:1/", type,  20 },
  };
  MockStorageClient* client = CreateClient(data, ARRAYSIZE_UNSAFE(data),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());

  client->ModifyOriginAndNotify(GURL("http://foo.com/"), type, 30);
  client->ModifyOriginAndNotify(GURL("http://foo.com:1/"), type, -5);
  client->AddOriginAndNotify(GURL("https://foo.com/"), type, 1);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20 + 30 - 5 + 1, usage());
  int foo_usage = usage();

  client->AddOriginAndNotify(GURL("http://bar.com/"), type, 40);
  GetUsageAndQuotaForWebApps(GURL("http://bar.com/"), type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(40, usage());

  GetGlobalUsage(type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(foo_usage + 40, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsage_WithModify) {
  GetUsage_WithModifyTestBody(kTemp);
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));
  SetTemporaryGlobalQuota(100);
  base::RunLoop().RunUntilIdle();

  const int kPerHostQuota = 100 / QuotaManager::kPerHostTemporaryPortion;

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kTemp);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));
  SetTemporaryGlobalQuota(100);
  base::RunLoop().RunUntilIdle();

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kTemp);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"),
                                 kTemp);

  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://bar.com/"), kTemp, kAllClients);

  // Nuke before waiting for callbacks.
  set_quota_manager(NULL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const MockOriginData kData[] = {
    { "http://usage1/",    kTemp,   1 },
    { "http://usage10/",   kTemp,  10 },
    { "http://usage200/",  kTemp, 200 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));
  SetTemporaryGlobalQuota(100);
  base::RunLoop().RunUntilIdle();

  const int kPerHostQuota = 100 / QuotaManager::kPerHostTemporaryPortion;

  GetUsageAndQuotaForWebApps(GURL("http://usage1/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(1, usage());
  EXPECT_EQ(1, quota());  // should be clamped to our current usage

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(10, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage200/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(200, usage());
  EXPECT_EQ(kPerHostQuota, quota());  // should be clamped to the nominal quota
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const MockOriginData kData[] = {
    { "http://usage10/",   kTemp,    10 },
    { "http://usage50/",   kTemp,    50 },
    { "http://unlimited/", kTemp,  4000 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  // Test when not overbugdet.
  SetTemporaryGlobalQuota(1000);
  base::RunLoop().RunUntilIdle();

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(4000, unlimited_usage());

  const int kPerHostQuotaFor1000 =
      1000 / QuotaManager::kPerHostTemporaryPortion;

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Test when overbugdet.
  SetTemporaryGlobalQuota(100);
  base::RunLoop().RunUntilIdle();

  const int kPerHostQuotaFor100 =
      100 / QuotaManager::kPerHostTemporaryPortion;

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kAvailableSpaceForApp + usage(), quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetUsageAndQuotaForWebApps(GURL("http://usage10/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(10, quota());  // should be clamped to our current usage

  GetUsageAndQuotaForWebApps(GURL("http://usage50/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());
}

TEST_F(QuotaManagerTest, OriginInUse) {
  const GURL kFooOrigin("http://foo.com/");
  const GURL kBarOrigin("http://bar.com/");

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 2
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginInUse(kBarOrigin);
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kBarOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));

  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
}

TEST_F(QuotaManagerTest, GetAndSetPerststentHostQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  GetPersistentHostQuota("foo.com");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(100, quota());

  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", 200);
  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", QuotaManager::kPerHostPersistentQuotaLimit);
  GetPersistentHostQuota("foo.com");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());

  // Persistent quota should be capped at the per-host quota limit.
  SetPersistentHostQuota("foo.com",
                         QuotaManager::kPerHostPersistentQuotaLimit + 100);
  GetPersistentHostQuota("foo.com");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());
}

TEST_F(QuotaManagerTest, GetAndSetPersistentUsageAndQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(100, quota());

  // For installed app GetUsageAndQuotaForWebApps returns the capped quota.
  mock_special_storage_policy()->GrantQueryDiskSize(GURL("http://installed/"));
  SetPersistentHostQuota("installed", kAvailableSpaceForApp + 100);
  GetUsageAndQuotaForWebApps(GURL("http://installed/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kAvailableSpaceForApp, quota());

  // Ditto for unlimited apps.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(GURL("http://unlimited/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kAvailableSpaceForApp, quota());

  // GetUsageAndQuotaForStorageClient should just return 0 usage and
  // kNoLimit quota.
  GetUsageAndQuotaForStorageClient(GURL("http://unlimited/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());
}

TEST_F(QuotaManagerTest, GetSyncableQuota) {
  RegisterClient(CreateClient(NULL, 0, QuotaClient::kFileSystem));

  // Pre-condition check: available disk space (for testing) is less than
  // the default quota for syncable storage.
  EXPECT_LE(kAvailableSpaceForApp,
            QuotaManager::kSyncableStorageDefaultHostQuota);

  // For installed apps the quota manager should return
  // kAvailableSpaceForApp as syncable quota (because of the pre-condition).
  mock_special_storage_policy()->GrantQueryDiskSize(GURL("http://installed/"));
  GetUsageAndQuotaForWebApps(GURL("http://installed/"), kSync);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(kAvailableSpaceForApp, quota());

  // If it's not installed (which shouldn't happen in real case) it
  // should just return the default host quota for syncable.
  GetUsageAndQuotaForWebApps(GURL("http://foo/"), kSync);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kSyncableStorageDefaultHostQuota, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm, 10 },
    { "http://foo.com:8080/",   kPerm, 20 },
    { "https://foo.com/",       kPerm, 13 },
    { "https://foo.com:8081/",  kPerm, 19 },
    { "http://bar.com/",        kPerm,  5 },
    { "https://bar.com/",       kPerm,  7 },
    { "http://baz.com/",        kPerm, 30 },
    { "http://foo.com/",        kTemp, 40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20 + 13 + 19, usage());
  EXPECT_EQ(100, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsage_WithModify) {
  GetUsage_WithModifyTestBody(kPerm);
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(100, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"),
                                 kPerm);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(GURL("http://bar.com/"), kPerm);

  // Nuke before waiting for callbacks.
  set_quota_manager(NULL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetUsage_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20);

  GetHostUsage("buz.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);
}

TEST_F(QuotaManagerTest, GetUsage_WithModification) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };

  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(
      GURL("http://foo.com/"), kPerm, 80000000);

  GetGlobalUsage(kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(
      GURL("http://foo.com/"), kTemp, 1);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("buz.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);

  client->ModifyOriginAndNotify(
      GURL("http://buz.com/"), kTemp, 900000000);

  GetHostUsage("buz.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000 + 900000000);
}

TEST_F(QuotaManagerTest, GetUsage_WithDeleteOrigin) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_pers = usage();

  DeleteClientOriginData(client, GURL("http://foo.com/"),
                         kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetAvailableSpaceTest) {
  GetAvailableSpace();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, EvictOriginData) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_pers = usage();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData1); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData1[i].origin), kData1[i].type);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData2); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData2[i].origin), kData2[i].type);
  base::RunLoop().RunUntilIdle();

  EvictOriginData(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();

  DumpOriginInfoTable();
  base::RunLoop().RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp)
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
  }

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, EvictOriginDataWithDeletionError) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://bar.com/",   kTemp,    4000 },
  };
  static const int kNumberOfTemporaryOrigins = 3;
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_pers = usage();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData); ++i)
    NotifyStorageAccessed(client, GURL(kData[i].origin), kData[i].type);
  base::RunLoop().RunUntilIdle();

  client->AddOriginToErrorSet(GURL("http://foo.com/"), kTemp);

  for (int i = 0;
       i < QuotaManager::kThresholdOfErrorsToBeBlacklisted + 1;
       ++i) {
    EvictOriginData(GURL("http://foo.com/"), kTemp);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(kQuotaErrorInvalidModification, status());
  }

  DumpOriginInfoTable();
  base::RunLoop().RunUntilIdle();

  bool found_origin_in_database = false;
  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp &&
        GURL("http://foo.com/") == itr->origin) {
      found_origin_in_database = true;
      break;
    }
  }
  // The origin "http://foo.com/" should be in the database.
  EXPECT_TRUE(found_origin_in_database);

  for (size_t i = 0; i < kNumberOfTemporaryOrigins - 1; ++i) {
    GetLRUOrigin(kTemp);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(lru_origin().is_empty());
    // The origin "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"), lru_origin().spec());
    DeleteOriginFromDatabase(lru_origin(), kTemp);
    base::RunLoop().RunUntilIdle();
  }

  // Now the LRU list must be empty.
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lru_origin().is_empty());

  // Deleting origins from the database should not affect the results of the
  // following checks.
  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetUsageAndQuotaForEviction) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://unlimited/", kTemp,    4000 },
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  SetTemporaryGlobalQuota(10000000);
  base::RunLoop().RunUntilIdle();

  GetUsageAndQuotaForEviction();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(21, limited_usage());
  EXPECT_EQ(10000000, quota());
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, DeleteHostDataSimple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  int64 predelete_host_pers = usage();

  DeleteHostData(std::string(), kTemp, kAllClients);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());

  DeleteHostData("foo.com", kTemp, kAllClients);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, DeleteHostDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_bar_pers = usage();

  reset_status_callback_count();
  DeleteHostData("foo.com", kTemp, kAllClients);
  DeleteHostData("bar.com", kTemp, kAllClients);
  DeleteHostData("foo.com", kTemp, kAllClients);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  base::RunLoop().RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp) {
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://foo.com:1/"), itr->origin.spec());
      EXPECT_NE(std::string("https://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://bar.com/"), itr->origin.spec());
    }
  }

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9),
            usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 20 + 50000 + 6000 + 80), usage());

  GetHostUsage("bar.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

// Single-run DeleteOriginData cases must be well covered by
// EvictOriginData tests.
TEST_F(QuotaManagerTest, DeleteOriginDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_bar_pers = usage();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData1); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData1[i].origin), kData1[i].type);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData2); ++i)
    quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown,
        GURL(kData2[i].origin), kData2[i].type);
  base::RunLoop().RunUntilIdle();

  reset_status_callback_count();
  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://bar.com/"), kTemp, kAllClients);
  DeleteOriginData(GURL("http://foo.com/"), kTemp, kAllClients);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  base::RunLoop().RunUntilIdle();

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    if (itr->type == kTemp) {
      EXPECT_NE(std::string("http://foo.com/"), itr->origin.spec());
      EXPECT_NE(std::string("http://bar.com/"), itr->origin.spec());
    }
  }

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 4000 + 50000 + 9), usage());

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 50000), usage());

  GetHostUsage("bar.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerTest, GetCachedOrigins) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,       1 },
    { "http://a.com:1/", kTemp,      20 },
    { "http://b.com/",   kPerm,     300 },
    { "http://c.com/",   kTemp,    4000 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  // TODO(kinuko): Be careful when we add cache pruner.

  std::set<GURL> origins;
  GetCachedOrigins(kTemp, &origins);
  EXPECT_TRUE(origins.empty());

  // No matter how we make queries the quota manager tries to cache all
  // the origins at startup.
  GetHostUsage("a.com", kTemp);
  base::RunLoop().RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  GetHostUsage("b.com", kTemp);
  base::RunLoop().RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  GetCachedOrigins(kPerm, &origins);
  EXPECT_TRUE(origins.empty());

  GetGlobalUsage(kTemp);
  base::RunLoop().RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_TRUE(origins.find(GURL(kData[i].origin)) != origins.end());
  }
}

TEST_F(QuotaManagerTest, NotifyAndLRUOrigin) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lru_origin().is_empty());

  NotifyStorageAccessed(client, GURL("http://a.com/"), kTemp);
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("http://a.com/", lru_origin().spec());

  NotifyStorageAccessed(client, GURL("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, GURL("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("http://a.com/", lru_origin().spec());

  DeleteOriginFromDatabase(lru_origin(), kTemp);
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("https://a.com/", lru_origin().spec());

  DeleteOriginFromDatabase(lru_origin(), kTemp);
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("http://c.com/", lru_origin().spec());
}

TEST_F(QuotaManagerTest, GetLRUOriginWithOriginInUse) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lru_origin().is_empty());

  NotifyStorageAccessed(client, GURL("http://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, GURL("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);

  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("http://a.com/", lru_origin().spec());

  // Notify origin http://a.com is in use.
  NotifyOriginInUse(GURL("http://a.com/"));
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("https://a.com/", lru_origin().spec());

  // Notify origin https://a.com is in use while GetLRUOrigin is running.
  GetLRUOrigin(kTemp);
  NotifyOriginInUse(GURL("https://a.com/"));
  base::RunLoop().RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_TRUE(lru_origin().is_empty());

  // Notify access for http://c.com while GetLRUOrigin is running.
  GetLRUOrigin(kTemp);
  NotifyStorageAccessed(client, GURL("http://c.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_TRUE(lru_origin().is_empty());

  NotifyOriginNoLongerInUse(GURL("http://a.com/"));
  NotifyOriginNoLongerInUse(GURL("https://a.com/"));
  GetLRUOrigin(kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("http://a.com/", lru_origin().spec());
}

TEST_F(QuotaManagerTest, GetOriginsModifiedSince) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client = CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem);
  RegisterClient(client);

  GetOriginsModifiedSince(kTemp, base::Time());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  base::Time time1 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(GURL("http://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://a.com:1/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://b.com/"), kPerm, 10);
  base::Time time2 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(GURL("https://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(GURL("http://c.com/"), kTemp, 10);
  base::Time time3 = client->IncrementMockTime();

  GetOriginsModifiedSince(kTemp, time1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4U, modified_origins().size());
  EXPECT_EQ(modified_origins_type(), kTemp);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_EQ(1U, modified_origins().count(GURL(kData[i].origin)));
  }

  GetOriginsModifiedSince(kTemp, time2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2U, modified_origins().size());

  GetOriginsModifiedSince(kTemp, time3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  client->ModifyOriginAndNotify(GURL("http://a.com/"), kTemp, 10);

  GetOriginsModifiedSince(kTemp, time3);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, modified_origins().size());
  EXPECT_EQ(1U, modified_origins().count(GURL("http://a.com/")));
  EXPECT_EQ(modified_origins_type(), kTemp);
}

TEST_F(QuotaManagerTest, DumpQuotaTable) {
  SetPersistentHostQuota("example1.com", 1);
  SetPersistentHostQuota("example2.com", 20);
  SetPersistentHostQuota("example3.com", 300);
  base::RunLoop().RunUntilIdle();

  DumpQuotaTable();
  base::RunLoop().RunUntilIdle();

  const QuotaTableEntry kEntries[] = {
    QuotaTableEntry("example1.com", kPerm, 1),
    QuotaTableEntry("example2.com", kPerm, 20),
    QuotaTableEntry("example3.com", kPerm, 300),
  };
  std::set<QuotaTableEntry> entries
      (kEntries, kEntries + ARRAYSIZE_UNSAFE(kEntries));

  typedef QuotaTableEntries::const_iterator iterator;
  for (iterator itr(quota_entries().begin()), end(quota_entries().end());
       itr != end; ++itr) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << itr->host << ", "
                 << "quota = " << itr->quota);
    EXPECT_EQ(1u, entries.erase(*itr));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, DumpOriginInfoTable) {
  using std::make_pair;

  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kTemp);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kPerm);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown,
      GURL("http://example.com/"),
      kPerm);
  base::RunLoop().RunUntilIdle();

  DumpOriginInfoTable();
  base::RunLoop().RunUntilIdle();

  typedef std::pair<GURL, StorageType> TypedOrigin;
  typedef std::pair<TypedOrigin, int> Entry;
  const Entry kEntries[] = {
    make_pair(make_pair(GURL("http://example.com/"), kTemp), 1),
    make_pair(make_pair(GURL("http://example.com/"), kPerm), 2),
  };
  std::set<Entry> entries
      (kEntries, kEntries + ARRAYSIZE_UNSAFE(kEntries));

  typedef OriginInfoTableEntries::const_iterator iterator;
  for (iterator itr(origin_info_entries().begin()),
                end(origin_info_entries().end());
       itr != end; ++itr) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << itr->origin << ", "
                 << "type = " << itr->type << ", "
                 << "used_count = " << itr->used_count);
    EXPECT_EQ(1u, entries.erase(
        make_pair(make_pair(itr->origin, itr->type),
                  itr->used_count)));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, QuotaForEmptyHost) {
  GetPersistentHostQuota(std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota(std::string(), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaErrorNotSupported, status());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, ARRAYSIZE_UNSAFE(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, ARRAYSIZE_UNSAFE(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kFileSystem);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kAppcache);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp, QuotaClient::kDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kIndexedDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, ARRAYSIZE_UNSAFE(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, ARRAYSIZE_UNSAFE(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp, QuotaClient::kFileSystem);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kAppcache);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kIndexedDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, ARRAYSIZE_UNSAFE(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, ARRAYSIZE_UNSAFE(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kFileSystem | QuotaClient::kDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 1, usage());

  DeleteOriginData(GURL("http://foo.com/"), kTemp,
      QuotaClient::kAppcache | QuotaClient::kIndexedDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 = CreateClient(kData1, ARRAYSIZE_UNSAFE(kData1),
      QuotaClient::kFileSystem);
  MockStorageClient* client2 = CreateClient(kData2, ARRAYSIZE_UNSAFE(kData2),
      QuotaClient::kAppcache);
  MockStorageClient* client3 = CreateClient(kData3, ARRAYSIZE_UNSAFE(kData3),
      QuotaClient::kDatabase);
  MockStorageClient* client4 = CreateClient(kData4, ARRAYSIZE_UNSAFE(kData4),
      QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  const int64 predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kFileSystem | QuotaClient::kAppcache);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kDatabase | QuotaClient::kIndexedDatabase);
  base::RunLoop().RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManager(true);

  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(CreateClient(kData, ARRAYSIZE_UNSAFE(kData),
      QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  SetTemporaryGlobalQuota(100);
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(std::min(static_cast<int64>(100 / kPerHostTemporaryPortion),
                     QuotaManager::kIncognitoDefaultQuotaLimit), quota());

  mock_special_storage_policy()->AddUnlimited(GURL("http://foo.com/"));
  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kPerm);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(QuotaManager::kIncognitoDefaultQuotaLimit, quota());

  GetUsageAndQuotaForWebApps(GURL("http://foo.com/"), kTemp);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kQuotaStatusOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(QuotaManager::kIncognitoDefaultQuotaLimit, quota());
}

}  // namespace content
