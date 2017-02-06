// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_default.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_origin_unittest.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::_;

namespace password_manager {

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<PasswordForm*>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(ScopedVector<PasswordForm> results) override {
    OnGetPasswordStoreResultsConstRef(results.get());
  }
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public LoginDatabase {
 public:
  BadLoginDatabase() : LoginDatabase(base::FilePath()) {}
  ~BadLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BadLoginDatabase);
};

PasswordFormData CreateTestPasswordFormData() {
  PasswordFormData data = {
    PasswordForm::SCHEME_HTML,
    "http://bar.example.com",
    "http://bar.example.com/origin",
    "http://bar.example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"username_value",
    L"password_value",
    true,
    false,
    1
  };
  return data;
}

class PasswordStoreDefaultTestDelegate {
 public:
  PasswordStoreDefaultTestDelegate();
  explicit PasswordStoreDefaultTestDelegate(
      std::unique_ptr<LoginDatabase> database);
  ~PasswordStoreDefaultTestDelegate();

  PasswordStoreDefault* store() { return store_.get(); }

  static void FinishAsyncProcessing();

 private:
  void SetupTempDir();

  void ClosePasswordStore();

  scoped_refptr<PasswordStoreDefault> CreateInitializedStore(
      std::unique_ptr<LoginDatabase> database);

  base::FilePath test_login_db_file_path() const;

  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<PasswordStoreDefault> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefaultTestDelegate);
};

PasswordStoreDefaultTestDelegate::PasswordStoreDefaultTestDelegate() {
  SetupTempDir();
  store_ = CreateInitializedStore(
      base::WrapUnique(new LoginDatabase(test_login_db_file_path())));
}

PasswordStoreDefaultTestDelegate::PasswordStoreDefaultTestDelegate(
    std::unique_ptr<LoginDatabase> database) {
  SetupTempDir();
  store_ = CreateInitializedStore(std::move(database));
}

PasswordStoreDefaultTestDelegate::~PasswordStoreDefaultTestDelegate() {
  ClosePasswordStore();
}

void PasswordStoreDefaultTestDelegate::FinishAsyncProcessing() {
  base::RunLoop().RunUntilIdle();
}

void PasswordStoreDefaultTestDelegate::SetupTempDir() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void PasswordStoreDefaultTestDelegate::ClosePasswordStore() {
  store_->ShutdownOnUIThread();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(temp_dir_.Delete());
}

scoped_refptr<PasswordStoreDefault>
PasswordStoreDefaultTestDelegate::CreateInitializedStore(
    std::unique_ptr<LoginDatabase> database) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get(),
      std::move(database)));
  store->Init(syncer::SyncableService::StartSyncFlare());

  return store;
}

base::FilePath PasswordStoreDefaultTestDelegate::test_login_db_file_path()
    const {
  return temp_dir_.path().Append(FILE_PATH_LITERAL("login_test"));
}

}  // anonymous namespace

INSTANTIATE_TYPED_TEST_CASE_P(Default,
                              PasswordStoreOriginTest,
                              PasswordStoreDefaultTestDelegate);

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST(PasswordStoreDefaultTest, NonASCIIData) {
  PasswordStoreDefaultTestDelegate delegate;
  PasswordStoreDefault* store = delegate.store();

  // Some non-ASCII password form data.
  static const PasswordFormData form_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://foo.example.com",
      "http://foo.example.com/origin",
      "http://foo.example.com/action",
      L"มีสีสัน",
      L"お元気ですか?",
      L"盆栽",
      L"أحب كرة",
      L"£éä국수çà",
      true, false, 1 },
  };

  // Build the expected forms vector and add the forms to the store.
  ScopedVector<PasswordForm> expected_forms;
  for (unsigned int i = 0; i < arraysize(form_data); ++i) {
    expected_forms.push_back(
        CreatePasswordFormFromDataForTesting(form_data[i]));
    store->AddLogin(*expected_forms.back());
  }

  base::RunLoop().RunUntilIdle();

  MockPasswordStoreConsumer consumer;

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            password_manager::UnorderedPasswordFormElementsAre(
                                expected_forms.get())));
  store->GetAutofillableLogins(&consumer);

  base::RunLoop().RunUntilIdle();
}

TEST(PasswordStoreDefaultTest, Notifications) {
  PasswordStoreDefaultTestDelegate delegate;
  PasswordStoreDefault* store = delegate.store();

  std::unique_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(CreateTestPasswordFormData());

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);
  base::RunLoop().RunUntilIdle();

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);
  base::RunLoop().RunUntilIdle();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);
  base::RunLoop().RunUntilIdle();

  store->RemoveObserver(&observer);
}

// Verify that operations on a PasswordStore with a bad database cause no
// explosions, but fail without side effect, return no data and trigger no
// notifications.
TEST(PasswordStoreDefaultTest, OperationsOnABadDatabaseSilentlyFail) {
  PasswordStoreDefaultTestDelegate delegate(
      base::WrapUnique(new BadLoginDatabase));
  PasswordStoreDefault* bad_store = delegate.store();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(nullptr, bad_store->login_db());

  testing::StrictMock<MockPasswordStoreObserver> mock_observer;
  bad_store->AddObserver(&mock_observer);

  // Add a new autofillable login + a blacklisted login.
  std::unique_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(CreateTestPasswordFormData());
  std::unique_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  bad_store->AddLogin(*form);
  bad_store->AddLogin(*blacklisted_form);
  base::RunLoop().RunUntilIdle();

  // Get all logins; autofillable logins; blacklisted logins.
  testing::StrictMock<MockPasswordStoreConsumer> mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetLogins(*form, &mock_consumer);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetAutofillableLogins(&mock_consumer);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetBlacklistLogins(&mock_consumer);
  base::RunLoop().RunUntilIdle();

  // Report metrics.
  bad_store->ReportMetrics("Test Username", true);
  base::RunLoop().RunUntilIdle();

  // Change the login.
  form->password_value = base::ASCIIToUTF16("a different password");
  bad_store->UpdateLogin(*form);
  base::RunLoop().RunUntilIdle();

  // Delete one login; a range of logins.
  bad_store->RemoveLogin(*form);
  base::RunLoop().RunUntilIdle();
  base::RunLoop run_loop;
  bad_store->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                        run_loop.QuitClosure());
  run_loop.Run();

  bad_store->RemoveLoginsSyncedBetween(base::Time(), base::Time::Max());
  base::RunLoop().RunUntilIdle();

  // Ensure no notifications and no explosions during shutdown either.
  bad_store->RemoveObserver(&mock_observer);
}

}  // namespace password_manager
