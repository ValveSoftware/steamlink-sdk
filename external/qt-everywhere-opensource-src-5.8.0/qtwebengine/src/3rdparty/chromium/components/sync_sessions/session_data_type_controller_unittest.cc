// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_driver::LocalDeviceInfoProviderMock;

namespace browser_sync {

namespace {

const char* kSavingBrowserHistoryDisabled = "history_disabled";

class MockSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  MockSyncedWindowDelegate() : is_restore_in_progress_(false) {}
  ~MockSyncedWindowDelegate() override {}

  bool HasWindow() const override { return false; }
  SessionID::id_type GetSessionId() const override { return 0; }
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  bool IsApp() const override { return false; }
  bool IsTypeTabbed() const override { return false; }
  bool IsTypePopup() const override { return false; }
  bool IsTabPinned(const SyncedTabDelegate* tab) const override {
    return false;
  }
  SyncedTabDelegate* GetTabAt(int index) const override { return NULL; }
  SessionID::id_type GetTabIdAt(int index) const override { return 0; }

  bool IsSessionRestoreInProgress() const override {
    return is_restore_in_progress_;
  }

  bool ShouldSync() const override { return false; }

  void SetSessionRestoreInProgress(bool is_restore_in_progress) {
    is_restore_in_progress_ = is_restore_in_progress;
  }

 private:
  bool is_restore_in_progress_;
};

class MockSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() override {
    return delegates_;
  }

  const SyncedWindowDelegate* FindById(SessionID::id_type id) override {
    return nullptr;
  }

  void Add(SyncedWindowDelegate* delegate) { delegates_.insert(delegate); }

 private:
  std::set<const SyncedWindowDelegate*> delegates_;
};

class TestSyncSessionsClient : public sync_sessions::FakeSyncSessionsClient {
 public:
  browser_sync::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return synced_window_getter_;
  }

  void SetSyncedWindowDelegatesGetter(
      browser_sync::SyncedWindowDelegatesGetter* synced_window_getter) {
    synced_window_getter_ = synced_window_getter;
  }

 private:
  browser_sync::SyncedWindowDelegatesGetter* synced_window_getter_;
};

class SessionDataTypeControllerTest : public testing::Test,
                                      public sync_driver::FakeSyncClient {
 public:
  SessionDataTypeControllerTest()
      : sync_driver::FakeSyncClient(&profile_sync_factory_),
        load_finished_(false),
        last_type_(syncer::UNSPECIFIED),
        weak_ptr_factory_(this) {}
  ~SessionDataTypeControllerTest() override {}

  // FakeSyncClient overrides.
  PrefService* GetPrefService() override { return &prefs_; }

  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override {
    return sync_sessions_client_.get();
  }

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(kSavingBrowserHistoryDisabled,
                                           false);

    synced_window_delegate_.reset(new MockSyncedWindowDelegate());
    synced_window_getter_.reset(new MockSyncedWindowDelegatesGetter());
    sync_sessions_client_.reset(new TestSyncSessionsClient());
    synced_window_getter_->Add(synced_window_delegate_.get());
    sync_sessions_client_->SetSyncedWindowDelegatesGetter(
        synced_window_getter_.get());

    local_device_.reset(new LocalDeviceInfoProviderMock(
        "cache_guid", "Wayne Gretzky's Hacking Box", "Chromium 10k",
        "Chrome 10k", sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id"));

    controller_ = new SessionDataTypeController(
        base::ThreadTaskRunnerHandle::Get(), base::Bind(&base::DoNothing), this,
        local_device_.get(), kSavingBrowserHistoryDisabled);

    load_finished_ = false;
    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  void TearDown() override {
    controller_ = NULL;
    local_device_.reset();
    synced_window_getter_.reset();
    synced_window_delegate_.reset();
    sync_sessions_client_.reset();
  }

  void Start() {
    controller_->LoadModels(
        base::Bind(&SessionDataTypeControllerTest::OnLoadFinished,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLoadFinished(syncer::ModelType type, syncer::SyncError error) {
    load_finished_ = true;
    last_type_ = type;
    last_error_ = error;
  }

  testing::AssertionResult LoadResult() {
    if (!load_finished_) {
      return testing::AssertionFailure() << "OnLoadFinished wasn't called";
    }

    if (last_error_.IsSet()) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a SyncError: "
             << last_error_.ToString();
    }

    if (last_type_ != syncer::SESSIONS) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a wrong sync type: "
             << last_type_;
    }

    return testing::AssertionSuccess();
  }

 protected:
  void SetSessionRestoreInProgress(bool is_restore_in_progress) {
    synced_window_delegate_->SetSessionRestoreInProgress(
        is_restore_in_progress);

    if (!is_restore_in_progress)
      controller_->OnSessionRestoreComplete();
  }

  scoped_refptr<SessionDataTypeController> controller_;
  std::unique_ptr<MockSyncedWindowDelegatesGetter> synced_window_getter_;
  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;
  std::unique_ptr<MockSyncedWindowDelegate> synced_window_delegate_;
  std::unique_ptr<TestSyncSessionsClient> sync_sessions_client_;
  bool load_finished_;

 private:
  base::MessageLoop message_loop_;
  TestingPrefServiceSimple prefs_;
  SyncApiComponentFactoryMock profile_sync_factory_;
  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
  base::WeakPtrFactory<SessionDataTypeControllerTest> weak_ptr_factory_;
};

TEST_F(SessionDataTypeControllerTest, StartModels) {
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest, StartModelsDelayedByLocalDevice) {
  local_device_->SetInitialized(false);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest, StartModelsDelayedByRestoreInProgress) {
  SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  SetSessionRestoreInProgress(false);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest,
       StartModelsDelayedByLocalDeviceThenRestoreInProgress) {
  local_device_->SetInitialized(false);
  SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  SetSessionRestoreInProgress(false);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest,
       StartModelsDelayedByRestoreInProgressThenLocalDevice) {
  local_device_->SetInitialized(false);
  SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  SetSessionRestoreInProgress(false);
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

}  // namespace

}  // namespace browser_sync
