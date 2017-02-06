// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data_ui/history_notice_utils.h"

#include <memory>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data_ui {

namespace {

class TestSyncService : public sync_driver::FakeSyncService {
 public:
  // Getters (FakeSyncService implementation). ---------------------------------
  bool IsSyncActive() const override {
   return sync_active_;
  }

  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return active_data_types_;
  }

  bool IsUsingSecondaryPassphrase() const override {
    return using_secondary_passphrase_;
  }

  // Setters. ------------------------------------------------------------------
  void set_sync_active(bool active) {
    sync_active_ = active;
  }

  void set_active_data_types(syncer::ModelTypeSet data_types) {
    active_data_types_ = data_types;
  }

  void set_using_secondary_passphrase(bool passphrase) {
    using_secondary_passphrase_ = passphrase;
  }

 private:
  syncer::ModelTypeSet active_data_types_;
  bool using_secondary_passphrase_ = false;
  bool sync_active_ = false;
};

}  // namespace


class HistoryNoticeUtilsTest : public ::testing::Test {
 public:
  HistoryNoticeUtilsTest()
    : signin_client_(nullptr),
      signin_manager_(&signin_client_, &account_tracker_) {
  }

  void SetUp() override {
    sync_service_.reset(new TestSyncService());
    history_service_.reset(new history::FakeWebHistoryService(
        &oauth2_token_service_,
        &signin_manager_,
        url_request_context_));
    history_service_->SetupFakeResponse(true /* success */, net::HTTP_OK);
  }

  TestSyncService* sync_service() {
    return sync_service_.get();
  }

  history::FakeWebHistoryService* history_service() {
    return history_service_.get();
  }

  void ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(
      bool expected_test_case_result) {
    bool got_result = false;

    ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_.get(),
        history_service_.get(),
        version_info::Channel::STABLE,
        base::Bind(
            &HistoryNoticeUtilsTest::Callback,
            base::Unretained(this),
            base::Unretained(&got_result)));

    if (!got_result) {
      run_loop_.reset(new base::RunLoop());
      run_loop_->Run();
    }

    // Process the DeleteSoon() called on MergeBooleanCallbacks, otherwise
    // this it will be considered to be leaked.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_test_case_result, result_);
  }

 private:
  void Callback(bool* got_result, bool result) {
    *got_result = true;
    result_ = result;

    if (run_loop_)
      run_loop_->Quit();
  }

  FakeProfileOAuth2TokenService oauth2_token_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeSigninManagerBase signin_manager_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  std::unique_ptr<TestSyncService> sync_service_;
  std::unique_ptr<history::FakeWebHistoryService> history_service_;

  std::unique_ptr<base::RunLoop> run_loop_;
  bool result_;

  base::MessageLoop message_loop_;
};

TEST_F(HistoryNoticeUtilsTest, NotSyncing) {
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

TEST_F(HistoryNoticeUtilsTest, SyncingWithWrongParameters) {
  sync_service()->set_sync_active(true);

  // Regardless of the state of the web history...
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);

  // ...the response is false if there's custom passphrase...
  sync_service()->set_active_data_types(syncer::ModelTypeSet::All());
  sync_service()->set_using_secondary_passphrase(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);

  // ...or even if there's no custom passphrase, but we're not syncing history.
  syncer::ModelTypeSet only_passwords(syncer::PASSWORDS);
  sync_service()->set_active_data_types(only_passwords);
  sync_service()->set_using_secondary_passphrase(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

TEST_F(HistoryNoticeUtilsTest, WebHistoryStates) {
  // If history Sync is active...
  sync_service()->set_sync_active(true);
  sync_service()->set_active_data_types(syncer::ModelTypeSet::All());

  // ...the result is true if both web history queries return true...
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(true);

  // ...but not otherwise.
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetWebAndAppActivityEnabled(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);

  // Invalid responses from the web history are interpreted as false.
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  history_service()->SetupFakeResponse(true, net::HTTP_INTERNAL_SERVER_ERROR);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetupFakeResponse(false, net::HTTP_OK);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

}  // namespace browsing_data_ui
