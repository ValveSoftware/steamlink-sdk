// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/database/database_quota_client.h"
#include "webkit/browser/database/database_tracker.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/common/database/database_identifier.h"

using webkit_database::DatabaseQuotaClient;
using webkit_database::DatabaseTracker;
using webkit_database::OriginInfo;

namespace content {

// Declared to shorten the line lengths.
static const quota::StorageType kTemp = quota::kStorageTypeTemporary;
static const quota::StorageType kPerm = quota::kStorageTypePersistent;

// Mock tracker class the mocks up those methods of the tracker
// that are used by the QuotaClient.
class MockDatabaseTracker : public DatabaseTracker {
 public:
  MockDatabaseTracker()
      : DatabaseTracker(base::FilePath(), false, NULL, NULL, NULL),
        delete_called_count_(0),
        async_delete_(false) {}

  virtual bool GetOriginInfo(
      const std::string& origin_identifier,
      OriginInfo* info) OVERRIDE {
    std::map<GURL, MockOriginInfo>::const_iterator found =
        mock_origin_infos_.find(
            webkit_database::GetOriginFromIdentifier(origin_identifier));
    if (found == mock_origin_infos_.end())
      return false;
    *info = OriginInfo(found->second);
    return true;
  }

  virtual bool GetAllOriginIdentifiers(
      std::vector<std::string>* origins_identifiers) OVERRIDE {
    std::map<GURL, MockOriginInfo>::const_iterator iter;
    for (iter = mock_origin_infos_.begin();
         iter != mock_origin_infos_.end();
         ++iter) {
      origins_identifiers->push_back(iter->second.GetOriginIdentifier());
    }
    return true;
  }

  virtual bool GetAllOriginsInfo(
      std::vector<OriginInfo>* origins_info) OVERRIDE {
    std::map<GURL, MockOriginInfo>::const_iterator iter;
    for (iter = mock_origin_infos_.begin();
         iter != mock_origin_infos_.end();
         ++iter) {
      origins_info->push_back(OriginInfo(iter->second));
    }
    return true;
  }

  virtual int DeleteDataForOrigin(
      const std::string& origin_identifier,
      const net::CompletionCallback& callback) OVERRIDE {
    ++delete_called_count_;
    if (async_delete()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(&MockDatabaseTracker::AsyncDeleteDataForOrigin, this,
                     callback));
      return net::ERR_IO_PENDING;
    }
    return net::OK;
  }

  void AsyncDeleteDataForOrigin(const net::CompletionCallback& callback) {
    callback.Run(net::OK);
  }

  void AddMockDatabase(const GURL& origin,  const char* name, int size) {
    MockOriginInfo& info = mock_origin_infos_[origin];
    info.set_origin(webkit_database::GetIdentifierFromOrigin(origin));
    info.AddMockDatabase(base::ASCIIToUTF16(name), size);
  }

  int delete_called_count() { return delete_called_count_; }
  bool async_delete() { return async_delete_; }
  void set_async_delete(bool async) { async_delete_ = async; }

 protected:
  virtual ~MockDatabaseTracker() {}

 private:
  class MockOriginInfo : public OriginInfo {
   public:
    void set_origin(const std::string& origin_identifier) {
      origin_identifier_ = origin_identifier;
    }

    void AddMockDatabase(const base::string16& name, int size) {
      EXPECT_TRUE(database_info_.find(name) == database_info_.end());
      database_info_[name].first = size;
      total_size_ += size;
    }
  };

  int delete_called_count_;
  bool async_delete_;
  std::map<GURL, MockOriginInfo> mock_origin_infos_;
};


// Base class for our test fixtures.
class DatabaseQuotaClientTest : public testing::Test {
 public:
  const GURL kOriginA;
  const GURL kOriginB;
  const GURL kOriginOther;

  DatabaseQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        kOriginOther("http://other"),
        usage_(0),
        mock_tracker_(new MockDatabaseTracker),
        weak_factory_(this) {
  }

  int64 GetOriginUsage(
      quota::QuotaClient* client,
      const GURL& origin,
      quota::StorageType type) {
    usage_ = 0;
    client->GetOriginUsage(
        origin, type,
        base::Bind(&DatabaseQuotaClientTest::OnGetOriginUsageComplete,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(
      quota::QuotaClient* client,
      quota::StorageType type) {
    origins_.clear();
    client->GetOriginsForType(
        type,
        base::Bind(&DatabaseQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(
      quota::QuotaClient* client,
      quota::StorageType type,
      const std::string& host) {
    origins_.clear();
    client->GetOriginsForHost(
        type, host,
        base::Bind(&DatabaseQuotaClientTest::OnGetOriginsComplete,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  bool DeleteOriginData(
      quota::QuotaClient* client,
      quota::StorageType type,
      const GURL& origin) {
    delete_status_ = quota::kQuotaStatusUnknown;
    client->DeleteOriginData(
        origin, type,
        base::Bind(&DatabaseQuotaClientTest::OnDeleteOriginDataComplete,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return delete_status_ == quota::kQuotaStatusOk;
  }

  MockDatabaseTracker* mock_tracker() { return mock_tracker_.get(); }


 private:
  void OnGetOriginUsageComplete(int64 usage) {
    usage_ = usage;
  }

  void OnGetOriginsComplete(const std::set<GURL>& origins) {
    origins_ = origins;
  }

  void OnDeleteOriginDataComplete(quota::QuotaStatusCode status) {
    delete_status_ = status;
  }

  base::MessageLoop message_loop_;
  int64 usage_;
  std::set<GURL> origins_;
  quota::QuotaStatusCode delete_status_;
  scoped_refptr<MockDatabaseTracker> mock_tracker_;
  base::WeakPtrFactory<DatabaseQuotaClientTest> weak_factory_;
};


TEST_F(DatabaseQuotaClientTest, GetOriginUsage) {
  DatabaseQuotaClient client(base::MessageLoopProxy::current().get(),
                             mock_tracker());

  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));

  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kTemp));
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForHost) {
  DatabaseQuotaClient client(base::MessageLoopProxy::current().get(),
                             mock_tracker());

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<GURL> origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  mock_tracker()->AddMockDatabase(kOriginB, "barDB", 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForHost(&client, kPerm, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(&client, kTemp, kOriginOther.host()).empty());
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForType) {
  DatabaseQuotaClient client(base::MessageLoopProxy::current().get(),
                             mock_tracker());

  EXPECT_TRUE(GetOriginsForType(&client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  std::set<GURL> origins = GetOriginsForType(&client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());
}

TEST_F(DatabaseQuotaClientTest, DeleteOriginData) {
  DatabaseQuotaClient client(base::MessageLoopProxy::current().get(),
                             mock_tracker());

  // Perm deletions are short circuited in the Client and
  // should not reach the DatabaseTracker.
  EXPECT_TRUE(DeleteOriginData(&client, kPerm, kOriginA));
  EXPECT_EQ(0, mock_tracker()->delete_called_count());

  mock_tracker()->set_async_delete(false);
  EXPECT_TRUE(DeleteOriginData(&client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_tracker()->delete_called_count());

  mock_tracker()->set_async_delete(true);
  EXPECT_TRUE(DeleteOriginData(&client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_tracker()->delete_called_count());
}

}  // namespace content
