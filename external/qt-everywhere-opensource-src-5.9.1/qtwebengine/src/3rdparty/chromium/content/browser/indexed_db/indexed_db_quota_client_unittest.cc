// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/quota/mock_quota_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

// Declared to shorten the line lengths.
static const storage::StorageType kTemp = storage::kStorageTypeTemporary;
static const storage::StorageType kPerm = storage::kStorageTypePersistent;

namespace content {

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const GURL kOriginA;
  const GURL kOriginB;
  const GURL kOriginOther;

  IndexedDBQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        kOriginOther("http://other"),
        usage_(0),
        task_runner_(new base::TestSimpleTaskRunner),
        weak_factory_(this) {
    browser_context_.reset(new TestBrowserContext());

    scoped_refptr<storage::QuotaManager> quota_manager =
        new MockQuotaManager(false /*in_memory*/, browser_context_->GetPath(),
                             base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get(),
                             browser_context_->GetSpecialStoragePolicy());

    idb_context_ =
        new IndexedDBContextImpl(browser_context_->GetPath(),
                                 browser_context_->GetSpecialStoragePolicy(),
                                 quota_manager->proxy(),
                                 task_runner_.get());
    base::RunLoop().RunUntilIdle();
    setup_temp_dir();
  }

  void FlushIndexedDBTaskRunner() { task_runner_->RunUntilIdle(); }

  void setup_temp_dir() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath indexeddb_dir =
        temp_dir_.GetPath().Append(IndexedDBContextImpl::kIndexedDBDirectory);
    ASSERT_TRUE(base::CreateDirectory(indexeddb_dir));
    idb_context()->set_data_path_for_testing(indexeddb_dir);
  }

  ~IndexedDBQuotaClientTest() override {
    FlushIndexedDBTaskRunner();
    idb_context_ = NULL;
    browser_context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  int64_t GetOriginUsage(storage::QuotaClient* client,
                         const GURL& origin,
                         storage::StorageType type) {
    usage_ = -1;
    client->GetOriginUsage(
        origin,
        type,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginUsageComplete,
                   weak_factory_.GetWeakPtr()));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
    EXPECT_GT(usage_, -1);
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(storage::QuotaClient* client,
                                          storage::StorageType type) {
    origins_.clear();
    client->GetOriginsForType(
        type,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(storage::QuotaClient* client,
                                          storage::StorageType type,
                                          const std::string& host) {
    origins_.clear();
    client->GetOriginsForHost(
        type,
        host,
        base::Bind(&IndexedDBQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  storage::QuotaStatusCode DeleteOrigin(storage::QuotaClient* client,
                                        const GURL& origin_url) {
    delete_status_ = storage::kQuotaStatusUnknown;
    client->DeleteOriginData(
        origin_url,
        kTemp,
        base::Bind(&IndexedDBQuotaClientTest::OnDeleteOriginComplete,
                   weak_factory_.GetWeakPtr()));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
    return delete_status_;
  }

  IndexedDBContextImpl* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const base::FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_EQ(size, base::WriteFile(path, junk.c_str(), size));
  }

  void AddFakeIndexedDB(const GURL& origin, int size) {
    base::FilePath file_path_origin =
        idb_context()->GetFilePathForTesting(origin);
    if (!base::CreateDirectory(file_path_origin)) {
      LOG(ERROR) << "failed to base::CreateDirectory "
                 << file_path_origin.value();
    }
    file_path_origin = file_path_origin.Append(FILE_PATH_LITERAL("fake_file"));
    SetFileSizeTo(file_path_origin, size);
    idb_context()->ResetCaches();
  }

 private:
  void OnGetOriginUsageComplete(int64_t usage) { usage_ = usage; }

  void OnGetOriginsComplete(const std::set<GURL>& origins) {
    origins_ = origins;
  }

  void OnDeleteOriginComplete(storage::QuotaStatusCode code) {
    delete_status_ = code;
  }

  base::ScopedTempDir temp_dir_;
  int64_t usage_;
  std::set<GURL> origins_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  storage::QuotaStatusCode delete_status_;
  base::WeakPtrFactory<IndexedDBQuotaClientTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClientTest);
};

TEST_F(IndexedDBQuotaClientTest, GetOriginUsage) {
  IndexedDBQuotaClient client(idb_context());

  AddFakeIndexedDB(kOriginA, 6);
  AddFakeIndexedDB(kOriginB, 3);
  EXPECT_EQ(6, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));

  AddFakeIndexedDB(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));
  EXPECT_EQ(3, GetOriginUsage(&client, kOriginB, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForHost) {
  IndexedDBQuotaClient client(idb_context());

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<GURL> origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  AddFakeIndexedDB(kOriginA, 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  AddFakeIndexedDB(kOriginB, 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForHost(&client, kPerm, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(&client, kTemp, kOriginOther.host()).empty());
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForType) {
  IndexedDBQuotaClient client(idb_context());

  EXPECT_TRUE(GetOriginsForType(&client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());

  AddFakeIndexedDB(kOriginA, 1000);
  std::set<GURL> origins = GetOriginsForType(&client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());
}

TEST_F(IndexedDBQuotaClientTest, DeleteOrigin) {
  IndexedDBQuotaClient client(idb_context());

  AddFakeIndexedDB(kOriginA, 1000);
  AddFakeIndexedDB(kOriginB, 50);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(&client, kOriginB, kTemp));

  storage::QuotaStatusCode delete_status = DeleteOrigin(&client, kOriginA);
  EXPECT_EQ(storage::kQuotaStatusOk, delete_status);
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(&client, kOriginB, kTemp));
}

}  // namespace content
