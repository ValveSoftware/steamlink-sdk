// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/sync_credentials_filter.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/user_action_tester.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/sync/browser/sync_username_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  ~FakePasswordManagerClient() override {}

  // PasswordManagerClient:
  const GURL& GetLastCommittedEntryURL() const override {
    return last_committed_entry_url_;
  }

  void set_last_committed_entry_url(const char* url_spec) {
    last_committed_entry_url_ = GURL(url_spec);
  }

  GURL last_committed_entry_url_;
};

bool IsFormFiltered(const CredentialsFilter* filter, const PasswordForm& form) {
  ScopedVector<PasswordForm> vector;
  vector.push_back(new PasswordForm(form));
  vector = filter->FilterResults(std::move(vector));
  return vector.empty();
}

}  // namespace

class CredentialsFilterTest : public SyncUsernameTestBase {
 public:
  struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    PasswordForm form;
    std::string fake_sync_username;
    const char* const last_committed_entry_url;
    enum { FORM_FILTERED, FORM_NOT_FILTERED } is_form_filtered;
    enum { NO_HISTOGRAM, HISTOGRAM_REPORTED } histogram_reported;
  };

  CredentialsFilterTest()
      : filter_(&client_,
                base::Bind(&SyncUsernameTestBase::sync_service,
                           base::Unretained(this)),
                base::Bind(&SyncUsernameTestBase::signin_manager,
                           base::Unretained(this))) {}

  void CheckFilterResultsTestCase(const TestCase& test_case) {
    SetSyncingPasswords(test_case.password_sync == TestCase::SYNCING_PASSWORDS);
    FakeSigninAs(test_case.fake_sync_username);
    client()->set_last_committed_entry_url(test_case.last_committed_entry_url);
    base::HistogramTester tester;
    const bool expected_is_form_filtered =
        test_case.is_form_filtered == TestCase::FORM_FILTERED;
    EXPECT_EQ(expected_is_form_filtered,
              IsFormFiltered(filter(), test_case.form));
    if (test_case.histogram_reported == TestCase::HISTOGRAM_REPORTED) {
      tester.ExpectUniqueSample("PasswordManager.SyncCredentialFiltered",
                                expected_is_form_filtered, 1);
    } else {
      tester.ExpectTotalCount("PasswordManager.SyncCredentialFiltered", 0);
    }
    FakeSignout();
  }

  SyncCredentialsFilter* filter() { return &filter_; }

  FakePasswordManagerClient* client() { return &client_; }

 private:
  FakePasswordManagerClient client_;

  SyncCredentialsFilter filter_;
};

TEST_F(CredentialsFilterTest, FilterResults_AllowAll) {
  // By default, sync username is not filtered at all.
  const TestCase kTestCases[] = {
      // Reauth URL, not sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "another_user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Slightly invalid reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org", "https://accounts.google.com/login?param=123",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-GAIA "reauth" URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "user@example.org", "https://site.com/login?rart=678",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

TEST_F(CredentialsFilterTest, FilterResults_DisallowSyncOnReauth) {
  // Only 'protect-sync-credential-on-reauth' feature is kept enabled, fill the
  // sync credential everywhere but on reauth.
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  std::vector<const base::Feature*> enabled_features;
  std::vector<const base::Feature*> disabled_features;
  disabled_features.push_back(&features::kProtectSyncCredential);
  enabled_features.push_back(&features::kProtectSyncCredentialOnReauth);
  SetFeatures(enabled_features, disabled_features, std::move(feature_list));

  const TestCase kTestCases[] = {
      // Reauth URL, not sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "another_user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Slightly invalid reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Non-reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org", "https://accounts.google.com/login?param=123",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-GAIA "reauth" URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "user@example.org", "https://site.com/login?rart=678",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

TEST_F(CredentialsFilterTest, FilterResults_DisallowSync) {
  // Both features are kept enabled, should cause sync credential to be
  // filtered.
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  std::vector<const base::Feature*> enabled_features;
  std::vector<const base::Feature*> disabled_features;
  enabled_features.push_back(&features::kProtectSyncCredential);
  enabled_features.push_back(&features::kProtectSyncCredentialOnReauth);
  SetFeatures(enabled_features, disabled_features, std::move(feature_list));

  const TestCase kTestCases[] = {
      // Reauth URL, not sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "another_user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Slightly invalid reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org",
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Non-reauth URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "user@example.org", "https://accounts.google.com/login?param=123",
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Non-GAIA "reauth" URL, sync username.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "user@example.org", "https://site.com/login?rart=678",
       TestCase::FORM_NOT_FILTERED, TestCase::HISTOGRAM_REPORTED},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

TEST_F(CredentialsFilterTest, ReportFormUsed) {
  base::UserActionTester tester;
  ASSERT_EQ(0, tester.GetActionCount("PasswordManager_SyncCredentialUsed"));
  filter()->ReportFormUsed(PasswordForm());
  EXPECT_EQ(1, tester.GetActionCount("PasswordManager_SyncCredentialUsed"));
}

TEST_F(CredentialsFilterTest, ShouldSave_NotSyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  ASSERT_NE("user@example.org",
            signin_manager()->GetAuthenticatedAccountInfo().email);
  SetSyncingPasswords(true);
  EXPECT_TRUE(filter()->ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(true);
  EXPECT_FALSE(filter()->ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncCredential_NotSyncingPasswords) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(false);
  EXPECT_TRUE(filter()->ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldFilterOneForm) {
  // Both features are kept enabled, should cause sync credential to be
  // filtered.
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  std::vector<const base::Feature*> enabled_features;
  std::vector<const base::Feature*> disabled_features;
  enabled_features.push_back(&features::kProtectSyncCredential);
  enabled_features.push_back(&features::kProtectSyncCredentialOnReauth);
  SetFeatures(enabled_features, disabled_features, std::move(feature_list));

  ScopedVector<autofill::PasswordForm> results;
  results.push_back(new PasswordForm(SimpleGaiaForm("test1@gmail.com")));
  results.push_back(new PasswordForm(SimpleGaiaForm("test2@gmail.com")));

  FakeSigninAs("test1@gmail.com");

  results = filter()->FilterResults(std::move(results));

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(SimpleGaiaForm("test2@gmail.com"), *results[0]);
}

}  // namespace password_manager
