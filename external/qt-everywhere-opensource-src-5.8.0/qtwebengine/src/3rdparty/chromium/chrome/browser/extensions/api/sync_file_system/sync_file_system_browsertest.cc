// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "components/drive/service/fake_drive_service.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {

namespace {

class FakeDriveServiceFactory
    : public drive_backend::SyncEngine::DriveServiceFactory {
 public:
  explicit FakeDriveServiceFactory(
      drive::FakeDriveService::ChangeObserver* change_observer)
      : change_observer_(change_observer) {}
  ~FakeDriveServiceFactory() override {}

  std::unique_ptr<drive::DriveServiceInterface> CreateDriveService(
      OAuth2TokenService* oauth2_token_service,
      net::URLRequestContextGetter* url_request_context_getter,
      base::SequencedTaskRunner* blocking_task_runner) override {
    std::unique_ptr<drive::FakeDriveService> drive_service(
        new drive::FakeDriveService);
    drive_service->AddChangeObserver(change_observer_);
    return std::move(drive_service);
  }

 private:
  drive::FakeDriveService::ChangeObserver* change_observer_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveServiceFactory);
};

}  // namespace

class SyncFileSystemTest : public extensions::PlatformAppBrowserTest,
                           public drive::FakeDriveService::ChangeObserver {
 public:
  SyncFileSystemTest()
      : remote_service_(NULL) {
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    real_minimum_preserved_space_ =
        storage::QuotaManager::kMinimumPreserveForSystem;
    storage::QuotaManager::kMinimumPreserveForSystem = 0;
  }

  void TearDownInProcessBrowserTestFixture() override {
    storage::QuotaManager::kMinimumPreserveForSystem =
        real_minimum_preserved_space_;
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  scoped_refptr<base::SequencedTaskRunner> MakeSequencedTaskRunner() {
    scoped_refptr<base::SequencedWorkerPool> worker_pool =
        content::BrowserThread::GetBlockingPool();

    return worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
        worker_pool->GetSequenceToken(),
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());

    SyncFileSystemServiceFactory* factory =
        SyncFileSystemServiceFactory::GetInstance();

    content::BrowserContext* context = browser()->profile();
    ExtensionServiceInterface* extension_service =
        extensions::ExtensionSystem::Get(context)->extension_service();

    std::unique_ptr<drive_backend::SyncEngine::DriveServiceFactory>
        drive_service_factory(new FakeDriveServiceFactory(this));

    fake_signin_manager_.reset(new FakeSigninManagerForTesting(
        browser()->profile()));

    remote_service_ = new drive_backend::SyncEngine(
        base::ThreadTaskRunnerHandle::Get(),  // ui_task_runner
        MakeSequencedTaskRunner(), MakeSequencedTaskRunner(),
        content::BrowserThread::GetBlockingPool(), base_dir_.path(),
        NULL,  // task_logger
        NULL,  // notification_manager
        extension_service,
        fake_signin_manager_.get(),  // signin_manager
        NULL,                        // token_service
        NULL,                        // request_context
        std::move(drive_service_factory), in_memory_env_.get());
    remote_service_->SetSyncEnabled(true);
    factory->set_mock_remote_file_service(
        std::unique_ptr<RemoteFileSyncService>(remote_service_));
  }

  // drive::FakeDriveService::ChangeObserver override.
  void OnNewChangeAvailable() override {
    sync_engine()->OnNotificationReceived();
  }

  SyncFileSystemService* sync_file_system_service() {
    return SyncFileSystemServiceFactory::GetForProfile(browser()->profile());
  }

  drive_backend::SyncEngine* sync_engine() {
    return static_cast<drive_backend::SyncEngine*>(
        sync_file_system_service()->remote_service_.get());
  }

  LocalFileSyncService* local_file_sync_service() {
    return sync_file_system_service()->local_service_.get();
  }

  void SignIn() {
    fake_signin_manager_->SetAuthenticatedAccountInfo("12345", "tester");
    sync_engine()->GoogleSigninSucceeded("12345", "tester", "password");
  }

  void SetSyncEnabled(bool enabled) {
    sync_file_system_service()->SetSyncEnabledForTesting(enabled);
  }

  void WaitUntilIdle() {
    base::RunLoop run_loop;
    sync_file_system_service()->CallOnIdleForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::ScopedTempDir base_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<FakeSigninManagerForTesting> fake_signin_manager_;

  drive_backend::SyncEngine* remote_service_;

  int64_t real_minimum_preserved_space_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemTest);
};

IN_PROC_BROWSER_TEST_F(SyncFileSystemTest, AuthorizationTest) {
  ExtensionTestMessageListener open_failure(
      "checkpoint: Failed to get syncfs", true);
  ExtensionTestMessageListener bar_created(
      "checkpoint: \"/bar\" created", true);
  ExtensionTestMessageListener foo_created(
      "checkpoint: \"/foo\" created", true);
  extensions::ResultCatcher catcher;

  LoadAndLaunchPlatformApp("sync_file_system/authorization_test", "Launched");

  // Application sync is disabled at the initial state.  Thus first
  // syncFilesystem.requestFileSystem call should fail.
  ASSERT_TRUE(open_failure.WaitUntilSatisfied());

  // Enable Application sync and let the app retry.
  SignIn();
  SetSyncEnabled(true);

  open_failure.Reply("resume");

  ASSERT_TRUE(foo_created.WaitUntilSatisfied());

  // The app creates a file "/foo", that should successfully sync to the remote
  // service.  Wait for the completion and resume the app.
  WaitUntilIdle();

  sync_engine()->GoogleSignedOut("test_account", std::string());
  foo_created.Reply("resume");

  ASSERT_TRUE(bar_created.WaitUntilSatisfied());

  // The app creates anohter file "/bar".  Since the user signed out from chrome
  // The synchronization should fail and the service state should be
  // AUTHENTICATION_REQUIRED.

  WaitUntilIdle();
  EXPECT_EQ(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
            sync_engine()->GetCurrentState());

  sync_engine()->GoogleSigninSucceeded("test_account", "tester", "testing");
  WaitUntilIdle();

  bar_created.Reply("resume");

  EXPECT_TRUE(catcher.GetNextResult());
}

}  // namespace sync_file_system
