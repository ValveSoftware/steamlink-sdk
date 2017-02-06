// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class FakeV4Store : public V4Store {
 public:
  FakeV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path,
              const bool reset_succeeds)
      : V4Store(
            task_runner,
            base::FilePath(store_path.value() + FILE_PATH_LITERAL(".fake"))),
        reset_succeeds_(reset_succeeds) {}

  bool Reset() override { return reset_succeeds_; }

 private:
  bool reset_succeeds_;
};

// This factory creates a "fake" store. It allows the caller to specify that the
// first store should fail on Reset() i.e. return false. All subsequent stores
// always return true. This is used to test the Reset() method in V4Database.
class FakeV4StoreFactory : public V4StoreFactory {
 public:
  FakeV4StoreFactory(bool next_store_reset_fails)
      : next_store_reset_fails_(next_store_reset_fails) {}

  V4Store* CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override {
    bool reset_succeeds = !next_store_reset_fails_;
    next_store_reset_fails_ = false;
    return new FakeV4Store(task_runner, store_path, reset_succeeds);
  }

 private:
  bool next_store_reset_fails_;
};

class V4DatabaseTest : public PlatformTest {
 public:
  V4DatabaseTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dirname_ = temp_dir_.path().AppendASCII("V4DatabaseTest");

    created_but_not_called_back_ = false;
    created_and_called_back_ = false;

    callback_db_updated_ =
        base::Bind(&V4DatabaseTest::DatabaseUpdated, base::Unretained(this));

    callback_db_ready_ = base::Bind(
        &V4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
        base::Unretained(this));

    SetupInfoMapAndExpectedState();
  }

  void TearDown() override {
    V4Database::RegisterStoreFactoryForTest(nullptr);
    PlatformTest::TearDown();
  }

  void RegisterFactory(bool fails_first_reset) {
    factory_.reset(new FakeV4StoreFactory(fails_first_reset));
    V4Database::RegisterStoreFactoryForTest(factory_.get());
  }

  void SetupInfoMapAndExpectedState() {
    UpdateListIdentifier win_malware_id(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
    store_file_name_map_[win_malware_id] = "win_url_malware";
    expected_identifiers_.push_back(win_malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware.fake"));

    UpdateListIdentifier linux_malware_id(LINUX_PLATFORM, URL, MALWARE_THREAT);
    store_file_name_map_[linux_malware_id] = "linux_url_malware";
    expected_identifiers_.push_back(linux_malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware.fake"));
  }

  void DatabaseUpdated() {}
  void NewDatabaseReadyWithExpectedStorePathsAndIds(
      std::unique_ptr<V4Database> v4_database) {
    ASSERT_TRUE(v4_database);
    ASSERT_TRUE(v4_database->store_map_);

    // The following check ensures that the callback was called asynchronously.
    EXPECT_TRUE(created_but_not_called_back_);

    ASSERT_EQ(expected_store_paths_.size(), v4_database->store_map_->size());
    ASSERT_EQ(expected_identifiers_.size(), v4_database->store_map_->size());
    for (size_t i = 0; i < expected_identifiers_.size(); i++) {
      const auto& expected_identifier = expected_identifiers_[i];
      const auto& store = (*v4_database->store_map_)[expected_identifier];
      ASSERT_TRUE(store);
      const auto& expected_store_path = expected_store_paths_[i];
      EXPECT_EQ(expected_store_path, store->store_path());
    }

    EXPECT_EQ(expected_resets_successfully_, v4_database->ResetDatabase());

    EXPECT_FALSE(created_and_called_back_);
    created_and_called_back_ = true;

    v4_database_ = std::move(v4_database);
  }

  std::unique_ptr<ParsedServerResponse> CreateFakeServerResponse(
      StoreStateMap store_state_map) {
    std::unique_ptr<ParsedServerResponse> parsed_server_response(
        new ParsedServerResponse);
    for (const auto& store_state_iter : store_state_map) {
      UpdateListIdentifier identifier = store_state_iter.first;
      ListUpdateResponse* list_update_response = new ListUpdateResponse;
      list_update_response->set_platform_type(identifier.platform_type);
      list_update_response->set_threat_entry_type(identifier.threat_entry_type);
      list_update_response->set_threat_type(identifier.threat_type);
      list_update_response->set_new_client_state(store_state_iter.second);
      parsed_server_response->push_back(base::WrapUnique(list_update_response));
    }
    return parsed_server_response;
  }

  void VerifyExpectedStoresState(bool expect_new_stores) {
    const StoreMap* new_store_map = v4_database_->store_map_.get();
    std::unique_ptr<StoreStateMap> new_store_state_map =
        v4_database_->GetStoreStateMap();
    EXPECT_EQ(expected_store_state_map_.size(), new_store_map->size());
    EXPECT_EQ(expected_store_state_map_.size(), new_store_state_map->size());
    for (const auto& expected_iter : expected_store_state_map_) {
      const UpdateListIdentifier& identifier = expected_iter.first;
      const std::string& state = expected_iter.second;
      ASSERT_EQ(1u, new_store_map->count(identifier));
      ASSERT_EQ(1u, new_store_state_map->count(identifier));

      // Verify the expected state in the store map and the state map.
      EXPECT_EQ(state, new_store_map->at(identifier)->state());
      EXPECT_EQ(state, new_store_state_map->at(identifier));

      if (expect_new_stores) {
        // Verify that a new store was created.
        EXPECT_NE(old_stores_map_.at(identifier),
                  new_store_map->at(identifier).get());
      } else {
        // Verify that NO new store was created.
        EXPECT_EQ(old_stores_map_.at(identifier),
                  new_store_map->at(identifier).get());
      }
    }
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<V4Database> v4_database_;
  base::FilePath database_dirname_;
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  bool created_but_not_called_back_;
  bool created_and_called_back_;
  StoreFileNameMap store_file_name_map_;
  std::vector<UpdateListIdentifier> expected_identifiers_;
  std::vector<base::FilePath> expected_store_paths_;
  bool expected_resets_successfully_;
  std::unique_ptr<FakeV4StoreFactory> factory_;
  DatabaseUpdatedCallback callback_db_updated_;
  NewDatabaseReadyCallback callback_db_ready_;
  StoreStateMap expected_store_state_map_;
  base::hash_map<UpdateListIdentifier, V4Store*> old_stores_map_;
};

// Test to set up the database with fake stores.
TEST_F(V4DatabaseTest, TestSetupDatabaseWithFakeStores) {
  expected_resets_successfully_ = true;
  RegisterFactory(!expected_resets_successfully_);

  V4Database::Create(task_runner_, database_dirname_, store_file_name_map_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, created_and_called_back_);
}

// Test to set up the database with fake stores that fail to reset.
TEST_F(V4DatabaseTest, TestSetupDatabaseWithFakeStoresFailsReset) {
  expected_resets_successfully_ = false;
  RegisterFactory(!expected_resets_successfully_);

  V4Database::Create(task_runner_, database_dirname_, store_file_name_map_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, created_and_called_back_);
}

// Test to check database updates as expected.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNewStates) {
  expected_resets_successfully_ = true;
  RegisterFactory(!expected_resets_successfully_);

  V4Database::Create(task_runner_, database_dirname_, store_file_name_map_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state() + "_fake";
    old_stores_map_[store_iter.first] = store;
  }

  v4_database_->ApplyUpdate(CreateFakeServerResponse(expected_store_state_map_),
                            callback_db_updated_);

  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  VerifyExpectedStoresState(true);
}

// Test to ensure no state updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNoNewState) {
  expected_resets_successfully_ = true;
  RegisterFactory(!expected_resets_successfully_);

  V4Database::Create(task_runner_, database_dirname_, store_file_name_map_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  v4_database_->ApplyUpdate(CreateFakeServerResponse(expected_store_state_map_),
                            callback_db_updated_);

  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  VerifyExpectedStoresState(false);
}

// Test to ensure no updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithEmptyUpdate) {
  expected_resets_successfully_ = true;
  RegisterFactory(!expected_resets_successfully_);

  V4Database::Create(task_runner_, database_dirname_, store_file_name_map_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  std::unique_ptr<ParsedServerResponse> parsed_server_response(
      new ParsedServerResponse);
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            callback_db_updated_);

  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  VerifyExpectedStoresState(false);
}

}  // namespace safe_browsing
