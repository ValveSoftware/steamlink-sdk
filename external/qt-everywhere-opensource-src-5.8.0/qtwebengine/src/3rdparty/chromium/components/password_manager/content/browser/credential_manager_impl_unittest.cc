// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/credential_manager_impl.h"

#include <stdint.h>

#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/content/public/cpp/type_converters.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;

using testing::_;

namespace password_manager {

namespace {

const char kTestWebOrigin[] = "https://example.com/";
const char kTestAndroidRealm1[] = "android://hash@com.example.one.android/";
const char kTestAndroidRealm2[] = "android://hash@com.example.two.android/";

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_CONST_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD1(NotifyUserAutoSigninPtr,
               bool(const std::vector<autofill::PasswordForm*>& local_forms));
  MOCK_METHOD1(NotifyUserCouldBeAutoSignedInPtr,
               bool(autofill::PasswordForm* form));
  MOCK_METHOD0(NotifyStorePasswordCalled, void());
  MOCK_METHOD2(PromptUserToSavePasswordPtr,
               void(PasswordFormManager*, CredentialSourceType type));
  MOCK_METHOD4(PromptUserToChooseCredentialsPtr,
               bool(const std::vector<autofill::PasswordForm*>& local_forms,
                    const std::vector<autofill::PasswordForm*>& federated_forms,
                    const GURL& origin,
                    const CredentialsCallback& callback));

  explicit MockPasswordManagerClient(PasswordStore* store) : store_(store) {
    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableAutosignin,
                                           true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, true);
  }
  ~MockPasswordManagerClient() override {}

  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManager> manager,
      CredentialSourceType type,
      bool update_password) override {
    manager_.swap(manager);
    PromptUserToSavePasswordPtr(manager_.get(), type);
    return true;
  }

  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) override {
    NotifyUserCouldBeAutoSignedInPtr(form.get());
  }

  PasswordStore* GetPasswordStore() const override { return store_; }

  PrefService* GetPrefs() override { return &prefs_; }

  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      const CredentialsCallback& callback) {
    EXPECT_FALSE(local_forms.empty() && federated_forms.empty());
    const autofill::PasswordForm* form =
        local_forms.empty() ? federated_forms[0] : local_forms[0];
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::Owned(new autofill::PasswordForm(*form))));
    PromptUserToChooseCredentialsPtr(local_forms.get(), federated_forms.get(),
                                     origin, callback);
    return true;
  }

  void NotifyUserAutoSignin(ScopedVector<autofill::PasswordForm> local_forms,
                            const GURL& origin) override {
    EXPECT_FALSE(local_forms.empty());
    NotifyUserAutoSigninPtr(local_forms.get());
  }

  PasswordFormManager* pending_manager() const { return manager_.get(); }

  void set_zero_click_enabled(bool zero_click_enabled) {
    prefs_.SetBoolean(prefs::kCredentialsEnableAutosignin, zero_click_enabled);
  }

  void set_first_run_seen(bool first_run_seen) {
    prefs_.SetBoolean(prefs::kWasAutoSignInFirstRunExperienceShown,
                      first_run_seen);
  }

 private:
  TestingPrefServiceSimple prefs_;
  PasswordStore* store_;
  std::unique_ptr<PasswordFormManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class TestCredentialManagerImpl : public CredentialManagerImpl {
 public:
  TestCredentialManagerImpl(content::WebContents* web_contents,
                            PasswordManagerClient* client,
                            PasswordManagerDriver* driver);

 private:
  base::WeakPtr<PasswordManagerDriver> GetDriver() override;

  base::WeakPtr<PasswordManagerDriver> driver_;
};

TestCredentialManagerImpl::TestCredentialManagerImpl(
    content::WebContents* web_contents,
    PasswordManagerClient* client,
    PasswordManagerDriver* driver)
    : CredentialManagerImpl(web_contents, client),
      driver_(driver->AsWeakPtr()) {}

base::WeakPtr<PasswordManagerDriver> TestCredentialManagerImpl::GetDriver() {
  return driver_;
}

void RunAllPendingTasks() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  run_loop.Run();
}

class SlightlyLessStubbyPasswordManagerDriver
    : public StubPasswordManagerDriver {
 public:
  explicit SlightlyLessStubbyPasswordManagerDriver(
      PasswordManagerClient* client)
      : password_manager_(client) {}

  PasswordManager* GetPasswordManager() override { return &password_manager_; }

 private:
  PasswordManager password_manager_;
};

// Callbacks from CredentialManagerImpl methods
void RespondCallback(bool* called) {
  *called = true;
}

void GetCredentialCallback(bool* called,
                           mojom::CredentialManagerError* out_error,
                           mojom::CredentialInfoPtr* out_info,
                           mojom::CredentialManagerError error,
                           mojom::CredentialInfoPtr info) {
  *called = true;
  *out_error = error;
  *out_info = std::move(info);
}

}  // namespace

class CredentialManagerImplTest : public content::RenderViewHostTestHarness {
 public:
  CredentialManagerImplTest() {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    store_ = new TestPasswordStore;
    client_.reset(
        new testing::NiceMock<MockPasswordManagerClient>(store_.get()));
    stub_driver_.reset(
        new SlightlyLessStubbyPasswordManagerDriver(client_.get()));
    cm_service_impl_.reset(new TestCredentialManagerImpl(
        web_contents(), client_.get(), stub_driver_.get()));
    ON_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
        .WillByDefault(testing::Return(true));
    ON_CALL(*client_, IsOffTheRecord()).WillByDefault(testing::Return(false));
    ON_CALL(*client_, DidLastPageLoadEncounterSSLErrors())
        .WillByDefault(testing::Return(false));

    NavigateAndCommit(GURL("https://example.com/test.html"));

    form_.username_value = base::ASCIIToUTF16("Username");
    form_.display_name = base::ASCIIToUTF16("Display Name");
    form_.icon_url = GURL("https://example.com/icon.png");
    form_.password_value = base::ASCIIToUTF16("Password");
    form_.origin = web_contents()->GetLastCommittedURL().GetOrigin();
    form_.signon_realm = form_.origin.spec();
    form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    form_.skip_zero_click = false;
    form_.ssl_valid = true;

    affiliated_form1_.username_value = base::ASCIIToUTF16("Affiliated 1");
    affiliated_form1_.display_name = base::ASCIIToUTF16("Display Name");
    affiliated_form1_.password_value = base::ASCIIToUTF16("Password");
    affiliated_form1_.origin = GURL();
    affiliated_form1_.signon_realm = kTestAndroidRealm1;
    affiliated_form1_.scheme = autofill::PasswordForm::SCHEME_HTML;
    affiliated_form1_.skip_zero_click = false;
    affiliated_form1_.ssl_valid = true;

    affiliated_form2_.username_value = base::ASCIIToUTF16("Affiliated 2");
    affiliated_form2_.display_name = base::ASCIIToUTF16("Display Name");
    affiliated_form2_.password_value = base::ASCIIToUTF16("Password");
    affiliated_form2_.origin = GURL();
    affiliated_form2_.signon_realm = kTestAndroidRealm2;
    affiliated_form2_.scheme = autofill::PasswordForm::SCHEME_HTML;
    affiliated_form2_.skip_zero_click = false;
    affiliated_form2_.ssl_valid = true;

    origin_path_form_.username_value = base::ASCIIToUTF16("Username 2");
    origin_path_form_.display_name = base::ASCIIToUTF16("Display Name 2");
    origin_path_form_.password_value = base::ASCIIToUTF16("Password 2");
    origin_path_form_.origin = GURL("https://example.com/path");
    origin_path_form_.signon_realm = origin_path_form_.origin.spec();
    origin_path_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    origin_path_form_.skip_zero_click = false;

    subdomain_form_.username_value = base::ASCIIToUTF16("Username 2");
    subdomain_form_.display_name = base::ASCIIToUTF16("Display Name 2");
    subdomain_form_.password_value = base::ASCIIToUTF16("Password 2");
    subdomain_form_.origin = GURL("https://subdomain.example.com/path");
    subdomain_form_.signon_realm = subdomain_form_.origin.spec();
    subdomain_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    subdomain_form_.skip_zero_click = false;

    cross_origin_form_.username_value = base::ASCIIToUTF16("Username");
    cross_origin_form_.display_name = base::ASCIIToUTF16("Display Name");
    cross_origin_form_.password_value = base::ASCIIToUTF16("Password");
    cross_origin_form_.origin = GURL("https://example.net/");
    cross_origin_form_.signon_realm = cross_origin_form_.origin.spec();
    cross_origin_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    cross_origin_form_.skip_zero_click = false;

    store_->Clear();
    EXPECT_TRUE(store_->IsEmpty());
  }

  void TearDown() override {
    cm_service_impl_.reset();

    store_->ShutdownOnUIThread();
    content::RenderViewHostTestHarness::TearDown();
  }

  void ExpectZeroClickSignInFailure(bool zero_click_only,
                                    bool include_passwords,
                                    const std::vector<GURL>& federations) {
    bool called = false;
    mojom::CredentialManagerError error;
    mojom::CredentialInfoPtr credential;
    CallGet(zero_click_only, include_passwords, federations,
            base::Bind(&GetCredentialCallback, &called, &error, &credential));
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
        .Times(testing::Exactly(0));
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_))
        .Times(testing::Exactly(0));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(mojom::CredentialType::EMPTY, credential->type);
  }

  void ExpectZeroClickSignInSuccess(bool zero_click_only,
                                    bool include_passwords,
                                    const std::vector<GURL>& federations,
                                    mojom::CredentialType type) {
    bool called = false;
    mojom::CredentialManagerError error;
    mojom::CredentialInfoPtr credential;
    CallGet(zero_click_only, include_passwords, federations,
            base::Bind(&GetCredentialCallback, &called, &error, &credential));
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
        .Times(testing::Exactly(0));
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_))
        .Times(testing::Exactly(1));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(type, credential->type);
  }

  void ExpectCredentialType(bool zero_click_only,
                            bool include_passwords,
                            const std::vector<GURL>& federations,
                            mojom::CredentialType type) {
    bool called = false;
    mojom::CredentialManagerError error;
    mojom::CredentialInfoPtr credential;
    CallGet(zero_click_only, include_passwords, federations,
            base::Bind(&GetCredentialCallback, &called, &error, &credential));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(type, credential->type);
  }

  CredentialManagerImpl* cm_service_impl() { return cm_service_impl_.get(); }

  // Helpers for testing CredentialManagerImpl methods.
  void CallStore(const CredentialInfo& info,
                 const CredentialManagerImpl::StoreCallback& callback) {
    mojom::CredentialInfoPtr credential = mojom::CredentialInfo::From(info);
    cm_service_impl_->Store(std::move(credential), callback);
  }

  void CallRequireUserMediation(
      const CredentialManagerImpl::RequireUserMediationCallback& callback) {
    cm_service_impl_->RequireUserMediation(callback);
  }

  void CallGet(bool zero_click_only,
               bool include_passwords,
               const std::vector<GURL>& federations,
               const CredentialManagerImpl::GetCallback& callback) {
    cm_service_impl_->Get(zero_click_only, include_passwords, federations,
                          callback);
  }

 protected:
  autofill::PasswordForm form_;
  autofill::PasswordForm affiliated_form1_;
  autofill::PasswordForm affiliated_form2_;
  autofill::PasswordForm origin_path_form_;
  autofill::PasswordForm subdomain_form_;
  autofill::PasswordForm cross_origin_form_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<testing::NiceMock<MockPasswordManagerClient>> client_;
  std::unique_ptr<SlightlyLessStubbyPasswordManagerDriver> stub_driver_;
  std::unique_ptr<CredentialManagerImpl> cm_service_impl_;
};

TEST_F(CredentialManagerImplTest, IsZeroClickAllowed) {
  // IsZeroClickAllowed is uneffected by the first-run status.
  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(true);
  EXPECT_TRUE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(false);
  EXPECT_TRUE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(false);
  client_->set_first_run_seen(true);
  EXPECT_FALSE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(false);
  client_->set_first_run_seen(false);
  EXPECT_FALSE(cm_service_impl()->IsZeroClickAllowed());
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnStore) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  CallStore(info, base::Bind(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_TRUE(client_->pending_manager()->HasCompletedMatching());

  autofill::PasswordForm new_form =
      client_->pending_manager()->pending_credentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.origin, new_form.origin);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_TRUE(new_form.federation_origin.unique());
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, new_form.scheme);
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnStoreFederated) {
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  form_.federation_origin = url::Origin(GURL("https://google.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/google.com";
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  CallStore(info, base::Bind(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_TRUE(client_->pending_manager()->HasCompletedMatching());

  autofill::PasswordForm new_form =
      client_->pending_manager()->pending_credentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.origin, new_form.origin);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_EQ(form_.federation_origin, new_form.federation_origin);
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, new_form.scheme);
}

TEST_F(CredentialManagerImplTest, CredentialManagerStoreOverwrite) {
  // Populate the PasswordStore with a form.
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the password without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  info.password = base::ASCIIToUTF16("Totally new password.");
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_, _)).Times(0);
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  bool called = false;
  CallStore(info, base::Bind(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(base::ASCIIToUTF16("Totally new password."),
            passwords[form_.signon_realm][0].password_value);
}

TEST_F(CredentialManagerImplTest, CredentialManagerStoreOverwriteZeroClick) {
  // Set the global zero click flag on, and populate the PasswordStore with a
  // form that's set to skip zero click.
  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  CallStore(info, base::Bind(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerFederatedStoreOverwriteZeroClick) {
  // Set the global zero click flag on, and populate the PasswordStore with a
  // form that's set to skip zero click.
  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(true);
  form_.federation_origin = url::Origin(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.skip_zero_click = true;
  form_.signon_realm = "federation://example.com/example.com";
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  CallStore(info, base::Bind(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, CredentialManagerGetOverwriteZeroClick) {
  // Set the global zero click flag on, and populate the PasswordStore with a
  // form that's set to skip zero click and has a primary key that won't match
  // credentials initially created via `store()`.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  form_.username_element = base::ASCIIToUTF16("username-element");
  form_.password_element = base::ASCIIToUTF16("password-element");
  form_.signon_realm = "this is a realm";
  form_.origin = GURL("https://example.com/old_form.html");
  store_->AddLogin(form_);
  RunAllPendingTasks();

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  bool called = false;
  mojom::CredentialManagerError error;
  mojom::CredentialInfoPtr credential;
  CallGet(false, true, federations,
          base::Bind(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);

  // Verify that the update toggled the skip_zero_click flag.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerSignInWithSavingDisabledForCurrentPage) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled()).Times(0);

  bool called = false;
  CallStore(info, base::Bind(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_FALSE(client_->pending_manager());
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnRequireUserMediation) {
  store_->AddLogin(form_);
  store_->AddLogin(subdomain_form_);
  store_->AddLogin(cross_origin_form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(3U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[subdomain_form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[subdomain_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallRequireUserMediation(base::Bind(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);

  passwords = store_->stored_passwords();
  EXPECT_EQ(3U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[subdomain_form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[subdomain_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequireUserMediationIncognito) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  store_->AddLogin(form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallRequireUserMediation(base::Bind(&RespondCallback, &called));
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequireUserMediationWithAffiliation) {
  store_->AddLogin(form_);
  store_->AddLogin(cross_origin_form_);
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(4U, passwords.size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form1_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form2_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallRequireUserMediation(base::Bind(&RespondCallback, &called));
  RunAllPendingTasks();

  passwords = store_->stored_passwords();
  EXPECT_EQ(4U, passwords.size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[affiliated_form1_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form2_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  ExpectCredentialType(false, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyUsernames) {
  form_.username_value.clear();
  store_->AddLogin(form_);
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  std::vector<GURL> federations;
  ExpectCredentialType(false, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyAndNonUsernames) {
  store_->AddLogin(form_);
  autofill::PasswordForm empty = form_;
  empty.username_value.clear();
  store_->AddLogin(empty);

  std::vector<GURL> federations;
  ExpectZeroClickSignInSuccess(false, true, federations,
                               mojom::CredentialType::PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  ExpectCredentialType(false, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  bool called = false;
  mojom::CredentialManagerError error;
  mojom::CredentialInfoPtr credential;
  CallGet(false, true, federations,
          base::Bind(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);
}

TEST_F(
    CredentialManagerImplTest,
    CredentialManagerOnRequestCredentialWithZeroClickOnlyEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  bool called = false;
  mojom::CredentialManagerError error;
  mojom::CredentialInfoPtr credential;
  CallGet(true, true, federations,
          base::Bind(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyFullPasswordStore) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithoutPasswords) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInFailure(true, false, federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedMatch) {
  form_.federation_origin = url::Origin(GURL("https://example.com/"));
  form_.password_value = base::string16();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.push_back(GURL("https://example.com/"));

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::FEDERATED);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedNoMatch) {
  form_.federation_origin = url::Origin(GURL("https://example.com/"));
  form_.password_value = base::string16();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.push_back(GURL("https://not-example.com/"));

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'true' for the 'include_passwords' argument to ensure that
  // password-type credentials are included as potential matches.
  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordNoMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'false' for the 'include_passwords' argument to ensure that
  // password-type credentials are excluded as potential matches.
  ExpectZeroClickSignInFailure(true, false, federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedMatch) {
  affiliated_form1_.federation_origin =
      url::Origin(GURL("https://example.com/"));
  affiliated_form1_.password_value = base::string16();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  federations.push_back(GURL("https://example.com/"));

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::FEDERATED);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedNoMatch) {
  affiliated_form1_.federation_origin =
      url::Origin(GURL("https://example.com/"));
  affiliated_form1_.password_value = base::string16();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  federations.push_back(GURL("https://not-example.com/"));

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithoutFirstRun) {
  client_->set_first_run_seen(false);

  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_,
              NotifyUserCouldBeAutoSignedInPtr(testing::Pointee(form_)))
      .Times(1);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithFirstRunAndSkip) {
  client_->set_first_run_seen(true);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_,
              NotifyUserCouldBeAutoSignedInPtr(testing::Pointee(form_)))
      .Times(1);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithTLSErrors) {
  // If we encounter TLS errors, we won't return credentials.
  EXPECT_CALL(*client_, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(testing::Return(true));

  store_->AddLogin(form_);

  std::vector<GURL> federations;

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyTwoPasswordStore) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  // With two items in the password store, we shouldn't get credentials back.
  ExpectCredentialType(true, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyAndSkipZeroClickPasswordStore) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  // With two items in the password store, we shouldn't get credentials back,
  // even though only one item has |skip_zero_click| set |false|.
  ExpectCredentialType(true, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  // We only have cross-origin zero-click credentials; they should not be
  // returned.
  ExpectCredentialType(true, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  // 1st request.
  bool called_1 = false;
  mojom::CredentialManagerError error_1;
  mojom::CredentialInfoPtr credential_1;
  CallGet(
      false, true, federations,
      base::Bind(&GetCredentialCallback, &called_1, &error_1, &credential_1));
  // 2nd request.
  bool called_2 = false;
  mojom::CredentialManagerError error_2;
  mojom::CredentialInfoPtr credential_2;
  CallGet(
      false, true, federations,
      base::Bind(&GetCredentialCallback, &called_2, &error_2, &credential_2));

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  // Execute the PasswordStore asynchronousness.
  RunAllPendingTasks();

  // Check that the second request triggered a rejection.
  EXPECT_TRUE(called_2);
  EXPECT_EQ(mojom::CredentialManagerError::PENDINGREQUEST, error_2);
  EXPECT_TRUE(credential_2.is_null());

  // Check that the first request resolves.
  EXPECT_TRUE(called_1);
  EXPECT_EQ(mojom::CredentialManagerError::SUCCESS, error_1);
  EXPECT_NE(mojom::CredentialType::EMPTY, credential_1->type);
}

TEST_F(CredentialManagerImplTest, ResetSkipZeroClickAfterPrompt) {
  // Turn on the global zero-click flag, and add two credentials in separate
  // origins, both set to skip zero-click.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  cross_origin_form_.skip_zero_click = true;
  store_->AddLogin(cross_origin_form_);

  // Execute the PasswordStore asynchronousness to ensure everything is
  // written before proceeding.
  RunAllPendingTasks();

  // Sanity check.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  std::vector<GURL> federations;
  // Check that the form in the database has been updated. `OnRequestCredential`
  // generates a call to prompt the user to choose a credential.
  // MockPasswordManagerClient mocks a user choice, and when users choose a
  // credential (and have the global zero-click flag enabled), we make sure that
  // they'll be logged in again next time.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  bool called = false;
  mojom::CredentialManagerError error;
  mojom::CredentialInfoPtr credential;
  CallGet(false, true, federations,
          base::Bind(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, NoResetSkipZeroClickAfterPromptInIncognito) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  // Turn on the global zero-click flag which should be overriden by Incognito.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Sanity check.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  bool called = false;
  mojom::CredentialManagerError error;
  mojom::CredentialInfoPtr credential;
  CallGet(false, true, std::vector<GURL>(),
          base::Bind(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  // The form shouldn't become a zero-click one.
  passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, IncognitoZeroClickRequestCredential) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  ExpectCredentialType(true, true, federations, mojom::CredentialType::EMPTY);
}

TEST_F(CredentialManagerImplTest, ZeroClickWithAffiliatedFormInPasswordStore) {
  // Insert the affiliated form into the store, and mock out the association
  // with the current origin. As it's the only form matching the origin, it
  // ought to be returned automagically.
  store_->AddLogin(affiliated_form1_);

  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithTwoAffiliatedFormsInPasswordStore) {
  // Insert two affiliated forms into the store, and mock out the association
  // with the current origin. Multiple forms === no zero-click sign in.
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  affiliated_realms.push_back(kTestAndroidRealm2);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, but don't mock out the
  // association with the current origin. No association === no zero-click sign
  // in.
  store_->AddLogin(affiliated_form1_);

  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(true, true, federations);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithFormAndUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, along with a real form for the
  // origin, and don't mock out the association with the current origin. No
  // association + existing form === zero-click sign in.
  store_->AddLogin(form_);
  store_->AddLogin(affiliated_form1_);

  auto mock_helper = base::WrapUnique(new MockAffiliatedMatchHelper);
  store_->SetAffiliatedMatchHelper(std::move(mock_helper));

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(true, true, federations,
                               mojom::CredentialType::PASSWORD);
}

TEST_F(CredentialManagerImplTest, GetSynthesizedFormForOrigin) {
  autofill::PasswordForm synthesized =
      cm_service_impl_->GetSynthesizedFormForOrigin();
  EXPECT_EQ(kTestWebOrigin, synthesized.origin.spec());
  EXPECT_EQ(kTestWebOrigin, synthesized.signon_realm);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, synthesized.scheme);
  EXPECT_TRUE(synthesized.ssl_valid);
}

TEST_F(CredentialManagerImplTest, BlacklistPasswordCredential) {
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API));

  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  CallStore(info, base::Bind(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  client_->pending_manager()->PermanentlyBlacklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blacklisted.
  autofill::PasswordForm blacklisted;
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = form_.signon_realm;
  blacklisted.type = autofill::PasswordForm::TYPE_API;
  blacklisted.ssl_valid = true;
  blacklisted.date_created = passwords[form_.signon_realm][0].date_created;
  EXPECT_THAT(passwords[form_.signon_realm], testing::ElementsAre(blacklisted));
}

TEST_F(CredentialManagerImplTest, BlacklistFederatedCredential) {
  form_.federation_origin = url::Origin(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/example.com";

  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API));
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  CallStore(info, base::Bind(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  client_->pending_manager()->PermanentlyBlacklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blacklisted.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_TRUE(passwords.count(form_.origin.spec()));
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.type = autofill::PasswordForm::TYPE_API;
  blacklisted.ssl_valid = true;
  blacklisted.date_created =
      passwords[blacklisted.signon_realm][0].date_created;
  EXPECT_THAT(passwords[blacklisted.signon_realm],
              testing::ElementsAre(blacklisted));
}

TEST_F(CredentialManagerImplTest, RespectBlacklistingPasswordCredential) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.ssl_valid = true;
  store_->AddLogin(blacklisted);

  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API));
  CallStore(info, base::Bind(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  EXPECT_TRUE(client_->pending_manager()->IsBlacklisted());
}

TEST_F(CredentialManagerImplTest, RespectBlacklistingFederatedCredential) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.ssl_valid = true;
  store_->AddLogin(blacklisted);

  form_.federation_origin = url::Origin(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/example.com";
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(
                            _, CredentialSourceType::CREDENTIAL_SOURCE_API));
  CallStore(info, base::Bind(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  EXPECT_TRUE(client_->pending_manager()->IsBlacklisted());
}

}  // namespace password_manager
