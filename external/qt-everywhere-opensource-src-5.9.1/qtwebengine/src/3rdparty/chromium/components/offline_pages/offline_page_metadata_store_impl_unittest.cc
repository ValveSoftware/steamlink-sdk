// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store_sql.h"
#include "components/offline_pages/offline_page_model.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

#define OFFLINE_PAGES_TABLE_V1 "offlinepages_v1"

const char kTestClientNamespace[] = "CLIENT_NAMESPACE";
const char kTestURL[] = "https://example.com";
const char kOriginalTestURL[] = "https://example.com/foo";
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/offline_pages/example_com.mhtml");
int64_t kFileSize = 234567LL;
int64_t kOfflineId = 12345LL;

// Build a store with outdated schema to simulate the upgrading process.
// TODO(romax): move it to sql_unittests.
void BuildTestStoreWithSchemaFromM52(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "version INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "status INTEGER NOT NULL DEFAULT 0, "
                                 "user_initiated INTEGER, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "offline_url VARCHAR NOT NULL DEFAULT '', "
                                 "file_path VARCHAR NOT NULL "
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, version, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 0);
  statement.BindInt(5, 1);
  statement.BindCString(6, kTestClientNamespace);
  statement.BindString(7, kTestClientId2.id);
  statement.BindCString(8, kTestURL);
  statement.BindString(9, base::FilePath(kFilePath).MaybeAsASCII());
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_FALSE(
      connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "expiration_time"));
}

void BuildTestStoreWithSchemaFromM53(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "version INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "status INTEGER NOT NULL DEFAULT 0, "
                                 "user_initiated INTEGER, "
                                 "expiration_time INTEGER NOT NULL DEFAULT 0, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "offline_url VARCHAR NOT NULL DEFAULT '', "
                                 "file_path VARCHAR NOT NULL "
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, version, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path, expiration_time) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 0);
  statement.BindInt(5, 1);
  statement.BindCString(6, kTestClientNamespace);
  statement.BindString(7, kTestClientId2.id);
  statement.BindCString(8, kTestURL);
  statement.BindString(9, base::FilePath(kFilePath).MaybeAsASCII());
  statement.BindInt64(10, base::Time::Now().ToInternalValue());
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_FALSE(connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "title"));
}

void BuildTestStoreWithSchemaFromM54(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "version INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "status INTEGER NOT NULL DEFAULT 0, "
                                 "user_initiated INTEGER, "
                                 "expiration_time INTEGER NOT NULL DEFAULT 0, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "offline_url VARCHAR NOT NULL DEFAULT '', "
                                 "file_path VARCHAR NOT NULL "
                                 "title VARCHAR NOT NULL DEFAULT ''"
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, version, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path, expiration_time, title) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 0);
  statement.BindInt(5, 1);
  statement.BindCString(6, kTestClientNamespace);
  statement.BindString(7, kTestClientId2.id);
  statement.BindCString(8, kTestURL);
  statement.BindString(9, base::FilePath(kFilePath).MaybeAsASCII());
  statement.BindInt64(10, base::Time::Now().ToInternalValue());
  statement.BindString16(11, base::UTF8ToUTF16("Test title"));
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_TRUE(connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "version"));
  ASSERT_TRUE(connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "status"));
  ASSERT_TRUE(
      connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "user_initiated"));
  ASSERT_TRUE(
      connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "offline_url"));
}

void BuildTestStoreWithSchemaFromM55(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "expiration_time INTEGER NOT NULL DEFAULT 0, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "file_path VARCHAR NOT NULL, "
                                 "title VARCHAR NOT NULL DEFAULT ''"
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path, expiration_time, title) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 1);
  statement.BindCString(5, kTestClientNamespace);
  statement.BindString(6, kTestClientId2.id);
  statement.BindCString(7, kTestURL);
  statement.BindString(8, base::FilePath(kFilePath).MaybeAsASCII());
  statement.BindInt64(9, base::Time::Now().ToInternalValue());
  statement.BindString16(10, base::UTF8ToUTF16("Test title"));
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_TRUE(connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "title"));
  ASSERT_FALSE(
      connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "original_url"));
}

class OfflinePageMetadataStoreFactory {
 public:
  OfflinePageMetadataStore* BuildStore(const base::FilePath& file_path) {
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file_path);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM52(const base::FilePath& file_path) {
    BuildTestStoreWithSchemaFromM52(file_path);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file_path);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM53(const base::FilePath& file_path) {
    BuildTestStoreWithSchemaFromM53(file_path);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file_path);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM54(const base::FilePath& file_path) {
    BuildTestStoreWithSchemaFromM54(file_path);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file_path);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM55(const base::FilePath& file_path) {
    BuildTestStoreWithSchemaFromM55(file_path);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file_path);
    return store;
  }
};

enum CalledCallback { NONE, LOAD, ADD, UPDATE, REMOVE, RESET };
enum Status { STATUS_NONE, STATUS_TRUE, STATUS_FALSE };

class OfflinePageMetadataStoreTest : public testing::Test {
 public:
  OfflinePageMetadataStoreTest();
  ~OfflinePageMetadataStoreTest() override;

  void TearDown() override {
    // Wait for all the pieces of the store to delete itself properly.
    PumpLoop();
  }

  std::unique_ptr<OfflinePageMetadataStore> BuildStore();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM52();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM53();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM54();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM55();

  void PumpLoop();

  void InitializeCallback(bool success);
  void GetOfflinePagesCallback(
      const std::vector<OfflinePageItem>& offline_pages);
  void AddCallback(ItemActionStatus status);
  void UpdateCallback(CalledCallback called_callback,
                      std::unique_ptr<OfflinePagesUpdateResult> result);
  void ResetCallback(bool success);

  void ClearResults();

  OfflinePageItem CheckThatStoreHasOneItem();
  void CheckThatOfflinePageCanBeSaved(
      std::unique_ptr<OfflinePageMetadataStore> store);

  OfflinePagesUpdateResult* last_update_result() {
    return last_update_result_.get();
  }

 protected:
  CalledCallback last_called_callback_;
  Status last_status_;
  std::unique_ptr<OfflinePagesUpdateResult> last_update_result_;
  std::vector<OfflinePageItem> offline_pages_;
  OfflinePageMetadataStoreFactory factory_;

  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

OfflinePageMetadataStoreTest::OfflinePageMetadataStoreTest()
    : last_called_callback_(NONE),
      last_status_(STATUS_NONE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

OfflinePageMetadataStoreTest::~OfflinePageMetadataStoreTest() {}

void OfflinePageMetadataStoreTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageMetadataStoreTest::InitializeCallback(bool success) {
  last_status_ = success ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreTest::GetOfflinePagesCallback(
    const std::vector<OfflinePageItem>& offline_pages) {
  last_called_callback_ = LOAD;
  offline_pages_.swap(const_cast<std::vector<OfflinePageItem>&>(offline_pages));
}

void OfflinePageMetadataStoreTest::AddCallback(ItemActionStatus status) {
  last_called_callback_ = ADD;
  // TODO(fgorski): Add specific add status.
  // last_item_status_ = status;
  last_status_ =
      status == ItemActionStatus::SUCCESS ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreTest::UpdateCallback(
    CalledCallback called_callback,
    std::unique_ptr<OfflinePagesUpdateResult> result) {
  last_called_callback_ = called_callback;
  last_status_ = result->updated_items.size() > 0 ? STATUS_TRUE : STATUS_FALSE;
  last_update_result_ = std::move(result);
}

void OfflinePageMetadataStoreTest::ResetCallback(bool success) {
  last_called_callback_ = RESET;
  last_status_ = success ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreTest::ClearResults() {
  last_called_callback_ = NONE;
  last_status_ = STATUS_NONE;
  offline_pages_.clear();
  last_update_result_.reset(nullptr);
}

OfflinePageItem OfflinePageMetadataStoreTest::CheckThatStoreHasOneItem() {
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());

  return offline_pages_[0];
}

void OfflinePageMetadataStoreTest::CheckThatOfflinePageCanBeSaved(
    std::unique_ptr<OfflinePageMetadataStore> store) {
  size_t store_size = offline_pages_.size();
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  offline_page.title = base::UTF8ToUTF16("a title");
  base::Time expiration_time = base::Time::Now();
  offline_page.expiration_time = expiration_time;
  offline_page.original_url = GURL(kOriginalTestURL);

  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = BuildStore();
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(store_size + 1, offline_pages_.size());
  if (store_size > 0 &&
      offline_pages_[0].offline_id != offline_page.offline_id) {
    std::swap(offline_pages_[0], offline_pages_[1]);
  }
  EXPECT_EQ(offline_page, offline_pages_[0]);
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStore() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStore(temp_directory_.GetPath()));
  PumpLoop();
  store->Initialize(
      base::Bind(&OfflinePageMetadataStoreTest::InitializeCallback,
                 base::Unretained(this)));
  PumpLoop();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM52() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM52(temp_directory_.GetPath()));
  PumpLoop();
  store->Initialize(
      base::Bind(&OfflinePageMetadataStoreTest::InitializeCallback,
                 base::Unretained(this)));
  PumpLoop();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM53() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM53(temp_directory_.GetPath()));
  PumpLoop();
  store->Initialize(
      base::Bind(&OfflinePageMetadataStoreTest::InitializeCallback,
                 base::Unretained(this)));
  PumpLoop();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM54() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM53(temp_directory_.GetPath()));
  PumpLoop();
  store->Initialize(
      base::Bind(&OfflinePageMetadataStoreTest::InitializeCallback,
                 base::Unretained(this)));
  PumpLoop();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM55() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM55(temp_directory_.GetPath()));
  PumpLoop();
  store->Initialize(
      base::Bind(&OfflinePageMetadataStoreTest::InitializeCallback,
                 base::Unretained(this)));
  PumpLoop();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  return store;
}

// Loads empty store and makes sure that there are no offline pages stored in
// it.
TEST_F(OfflinePageMetadataStoreTest, LoadEmptyStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

TEST_F(OfflinePageMetadataStoreTest, GetOfflinePagesFromInvalidStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());
  OfflinePageMetadataStoreSQL* sql_store =
      static_cast<OfflinePageMetadataStoreSQL*>(store.get());

  sql_store->SetStateForTesting(StoreState::NOT_LOADED, false);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());
  EXPECT_EQ(StoreState::NOT_LOADED, store->state());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::FAILED_LOADING, false);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());
  EXPECT_EQ(StoreState::FAILED_LOADING, store->state());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::FAILED_RESET, false);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());
  EXPECT_EQ(StoreState::FAILED_RESET, store->state());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::LOADED, true);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::NOT_LOADED, true);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::FAILED_LOADING, false);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());

  ClearResults();
  sql_store->SetStateForTesting(StoreState::FAILED_RESET, false);
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0UL, offline_pages_.size());
}

// Loads a store which has an outdated schema.
// This test case would crash if it's not handling correctly when we're loading
// old version stores.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion52Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM52());

  CheckThatStoreHasOneItem();
  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Loads a store which has an outdated schema.
// This test case would crash if it's not handling correctly when we're loading
// old version stores.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion53Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM53());

  OfflinePageItem item = CheckThatStoreHasOneItem();
  // We should have a valid expiration time after upgrade.
  EXPECT_NE(base::Time::FromInternalValue(0),
            offline_pages_[0].expiration_time);

  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Loads a string with schema from M54.
// Because for now we only reduce the number of fields it just makes sure there
// are no crashes in the process.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion54Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM54());

  OfflinePageItem item = CheckThatStoreHasOneItem();

  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Loads a string with schema from M55.
// Because for now we only reduce the number of fields it just makes sure there
// are no crashes in the process.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion55Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM55());

  OfflinePageItem item = CheckThatStoreHasOneItem();

  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Adds metadata of an offline page into a store and then opens the store
// again to make sure that stored metadata survives store restarts.
TEST_F(OfflinePageMetadataStoreTest, AddOfflinePage) {
  CheckThatOfflinePageCanBeSaved(BuildStore());
}

TEST_F(OfflinePageMetadataStoreTest, AddSameOfflinePageTwice) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  offline_page.title = base::UTF8ToUTF16("a title");
  base::Time expiration_time = base::Time::Now();
  offline_page.expiration_time = expiration_time;

  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ClearResults();

  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_FALSE, last_status_);
}

// Tests removing offline page metadata from the store, for which it first adds
// metadata of an offline page.
TEST_F(OfflinePageMetadataStoreTest, RemoveOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Get all pages from the store.
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(1U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_TRUE(last_update_result() != nullptr);
  EXPECT_EQ(1UL, last_update_result()->item_statuses.size());
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_update_result()->item_statuses.begin()->second);
  EXPECT_EQ(1UL, last_update_result()->updated_items.size());
  EXPECT_EQ(offline_page, *(last_update_result()->updated_items.begin()));

  ClearResults();

  // Get all pages from the store.
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0U, offline_pages_.size());

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

// Adds metadata of multiple offline pages into a store and removes some.
TEST_F(OfflinePageMetadataStoreTest, AddRemoveMultipleOfflinePages) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page_1(GURL(kTestURL), 12345LL, kTestClientId1,
                                 base::FilePath(kFilePath), kFileSize);
  store->AddOfflinePage(offline_page_1,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Add anther offline page.
  base::FilePath file_path_2 =
      base::FilePath(FILE_PATH_LITERAL("//other.page.com.mhtml"));
  OfflinePageItem offline_page_2(GURL("https://other.page.com"), 5678LL,
                                 kTestClientId2, file_path_2, 12345,
                                 base::Time::Now());
  offline_page_2.expiration_time = base::Time::Now();
  offline_page_2.original_url = GURL("https://example.com/bar");
  store->AddOfflinePage(offline_page_2,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Get all pages from the store.
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(2U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page_1.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_TRUE(last_update_result() != nullptr);
  EXPECT_EQ(1UL, last_update_result()->item_statuses.size());
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_update_result()->item_statuses.begin()->second);
  EXPECT_EQ(1UL, last_update_result()->updated_items.size());
  EXPECT_EQ(offline_page_1, *(last_update_result()->updated_items.begin()));

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page_2.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page_2.offline_id, offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page_2.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page_2.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page_2.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page_2.last_access_time,
            offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page_2.expiration_time, offline_pages_[0].expiration_time);
  EXPECT_EQ(offline_page_2.access_count, offline_pages_[0].access_count);
  EXPECT_EQ(offline_page_2.client_id, offline_pages_[0].client_id);
}

// Tests updating offline page metadata from the store.
TEST_F(OfflinePageMetadataStoreTest, UpdateOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // First, adds a fresh page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page, offline_pages_[0]);

  // Then update some data.
  offline_page.file_size = kFileSize + 1;
  offline_page.access_count++;
  offline_page.expiration_time = base::Time::Now();
  offline_page.original_url = GURL("https://example.com/bar");
  std::vector<OfflinePageItem> items_to_update;
  items_to_update.push_back(offline_page);
  store->UpdateOfflinePages(
      items_to_update, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                  base::Unretained(this), UPDATE));
  PumpLoop();
  EXPECT_EQ(UPDATE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_TRUE(last_update_result() != nullptr);
  EXPECT_EQ(1UL, last_update_result()->item_statuses.size());
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_update_result()->item_statuses.begin()->second);
  EXPECT_EQ(1UL, last_update_result()->updated_items.size());
  EXPECT_EQ(offline_page, *(last_update_result()->updated_items.begin()));

  ClearResults();
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page, offline_pages_[0]);
}

TEST_F(OfflinePageMetadataStoreTest, ClearAllOfflinePages) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add 2 offline pages.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOfflinePage(offline_page,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  OfflinePageItem offline_page2(GURL("http://test.com"), 5678LL, kTestClientId2,
                                base::FilePath(kFilePath), kFileSize);
  store->AddOfflinePage(offline_page2,
                        base::Bind(&OfflinePageMetadataStoreTest::AddCallback,
                                   base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Get all pages from the store.
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(2U, offline_pages_.size());

  // Clear all records from the store.
  store->Reset(base::Bind(&OfflinePageMetadataStoreTest::ResetCallback,
                          base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(RESET, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Get all pages from the store.
  store->GetOfflinePages(
      base::Bind(&OfflinePageMetadataStoreTest::GetOfflinePagesCallback,
                 base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  ASSERT_EQ(0U, offline_pages_.size());
}

TEST_F(OfflinePageMetadataStoreTest, ResetStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  store->Reset(base::Bind(&OfflinePageMetadataStoreTest::ResetCallback,
                          base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(STATUS_TRUE, last_status_);
}

}  // namespace
}  // namespace offline_pages
