// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "components/prefs/pref_service.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "extensions/common/extension_builder.h"
#endif
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/crx_file/id_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/test_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using guest_view::GuestViewBase;
using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace extensions {

namespace {

namespace errors = identity_constants;
namespace utils = extension_function_test_utils;

static const char kAccessToken[] = "auth_token";
static const char kExtensionId[] = "ext_id";

// This helps us be able to wait until an UIThreadExtensionFunction calls
// SendResponse.
class SendResponseDelegate
    : public UIThreadExtensionFunction::DelegateForTests {
 public:
  SendResponseDelegate() : should_post_quit_(false) {}

  virtual ~SendResponseDelegate() {}

  void set_should_post_quit(bool should_quit) {
    should_post_quit_ = should_quit;
  }

  bool HasResponse() {
    return response_.get() != NULL;
  }

  bool GetResponse() {
    EXPECT_TRUE(HasResponse());
    return *response_.get();
  }

  void OnSendResponse(UIThreadExtensionFunction* function,
                      bool success,
                      bool bad_message) override {
    ASSERT_FALSE(bad_message);
    ASSERT_FALSE(HasResponse());
    response_.reset(new bool);
    *response_ = success;
    if (should_post_quit_) {
      base::MessageLoopForUI::current()->QuitWhenIdle();
    }
  }

 private:
  std::unique_ptr<bool> response_;
  bool should_post_quit_;
};

class AsyncExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  // Asynchronous function runner allows tests to manipulate the browser window
  // after the call happens.
  void RunFunctionAsync(
      UIThreadExtensionFunction* function,
      const std::string& args) {
    response_delegate_.reset(new SendResponseDelegate);
    function->set_test_delegate(response_delegate_.get());
    std::unique_ptr<base::ListValue> parsed_args(utils::ParseList(args));
    EXPECT_TRUE(parsed_args.get()) <<
        "Could not parse extension function arguments: " << args;
    function->SetArgs(parsed_args.get());

    if (!function->extension()) {
      scoped_refptr<Extension> empty_extension(
          test_util::CreateEmptyExtension());
      function->set_extension(empty_extension.get());
    }

    function->set_browser_context(browser()->profile());
    function->set_has_callback(true);
    function->RunWithValidation()->Execute();
  }

  std::string WaitForError(UIThreadExtensionFunction* function) {
    RunMessageLoopUntilResponse();
    EXPECT_FALSE(function->GetResultList()) << "Did not expect a result";
    return function->GetError();
  }

  base::Value* WaitForSingleResult(UIThreadExtensionFunction* function) {
    RunMessageLoopUntilResponse();
    EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
                                              << function->GetError();
    const base::Value* single_result = NULL;
    if (function->GetResultList() != NULL &&
        function->GetResultList()->Get(0, &single_result)) {
      return single_result->DeepCopy();
    }
    return NULL;
  }

 private:
  void RunMessageLoopUntilResponse() {
    // If the RunAsync of |function| didn't already call SendResponse, run the
    // message loop until they do.
    if (!response_delegate_->HasResponse()) {
      response_delegate_->set_should_post_quit(true);
      content::RunMessageLoop();
    }
    EXPECT_TRUE(response_delegate_->HasResponse());
  }

  std::unique_ptr<SendResponseDelegate> response_delegate_;
};

class TestHangOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  TestHangOAuth2MintTokenFlow()
      : OAuth2MintTokenFlow(NULL, OAuth2MintTokenFlow::Parameters()) {}

  void Start(net::URLRequestContextGetter* context,
             const std::string& access_token) override {
    // Do nothing, simulating a hanging network call.
  }
};

class TestOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  enum ResultType {
    ISSUE_ADVICE_SUCCESS,
    MINT_TOKEN_SUCCESS,
    MINT_TOKEN_FAILURE,
    MINT_TOKEN_BAD_CREDENTIALS,
    MINT_TOKEN_SERVICE_ERROR
  };

  TestOAuth2MintTokenFlow(ResultType result,
                          OAuth2MintTokenFlow::Delegate* delegate)
      : OAuth2MintTokenFlow(delegate, OAuth2MintTokenFlow::Parameters()),
        result_(result),
        delegate_(delegate) {}

  void Start(net::URLRequestContextGetter* context,
             const std::string& access_token) override {
    switch (result_) {
      case ISSUE_ADVICE_SUCCESS: {
        IssueAdviceInfo info;
        delegate_->OnIssueAdviceSuccess(info);
        break;
      }
      case MINT_TOKEN_SUCCESS: {
        delegate_->OnMintTokenSuccess(kAccessToken, 3600);
        break;
      }
      case MINT_TOKEN_FAILURE: {
        GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
        delegate_->OnMintTokenFailure(error);
        break;
      }
      case MINT_TOKEN_BAD_CREDENTIALS: {
        GoogleServiceAuthError error(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
        delegate_->OnMintTokenFailure(error);
        break;
      }
      case MINT_TOKEN_SERVICE_ERROR: {
        GoogleServiceAuthError error =
            GoogleServiceAuthError::FromServiceError("invalid_scope");
        delegate_->OnMintTokenFailure(error);
        break;
      }
    }
  }

 private:
  ResultType result_;
  OAuth2MintTokenFlow::Delegate* delegate_;
};

// Waits for a specific GURL to generate a NOTIFICATION_LOAD_STOP event and
// saves a pointer to the window embedding the WebContents, which can be later
// closed.
class WaitForGURLAndCloseWindow : public content::WindowedNotificationObserver {
 public:
  explicit WaitForGURLAndCloseWindow(GURL url)
      : WindowedNotificationObserver(
            content::NOTIFICATION_LOAD_STOP,
            content::NotificationService::AllSources()),
        url_(url),
        embedder_web_contents_(nullptr) {}

  // NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    content::NavigationController* web_auth_flow_controller =
        content::Source<content::NavigationController>(source).ptr();
    content::WebContents* web_contents =
        web_auth_flow_controller->GetWebContents();

    if (web_contents->GetURL() == url_) {
      // It is safe to keep the pointer here, because we know in a test, that
      // the WebContents won't go away before CloseEmbedderWebContents is
      // called. Don't copy this code to production.
      GuestViewBase* guest = GuestViewBase::FromWebContents(web_contents);
      embedder_web_contents_ = guest->embedder_web_contents();
      // Condtionally invoke parent class so that Wait will not exit
      // until the target URL arrives.
      content::WindowedNotificationObserver::Observe(type, source, details);
    }
  }

  // Closes the window embedding the WebContents. The action is separated from
  // the Observe method to make sure the list of observers is not deleted,
  // while some event is already being processed. (That causes ASAN failures.)
  void CloseEmbedderWebContents() {
    if (embedder_web_contents_)
      embedder_web_contents_->Close();
  }

 private:
  GURL url_;
  content::WebContents* embedder_web_contents_;
};

}  // namespace

class FakeGetAuthTokenFunction : public IdentityGetAuthTokenFunction {
 public:
  FakeGetAuthTokenFunction()
      : login_access_token_result_(true),
        auto_login_access_token_(true),
        login_ui_result_(true),
        scope_ui_result_(true),
        scope_ui_failure_(GaiaWebAuthFlow::WINDOW_CLOSED),
        login_ui_shown_(false),
        scope_ui_shown_(false) {}

  void set_login_access_token_result(bool result) {
    login_access_token_result_ = result;
  }

  void set_auto_login_access_token(bool automatic) {
    auto_login_access_token_ = automatic;
  }

  void set_login_ui_result(bool result) { login_ui_result_ = result; }

  void set_mint_token_flow(std::unique_ptr<OAuth2MintTokenFlow> flow) {
    flow_ = std::move(flow);
  }

  void set_mint_token_result(TestOAuth2MintTokenFlow::ResultType result_type) {
    set_mint_token_flow(
        base::WrapUnique(new TestOAuth2MintTokenFlow(result_type, this)));
  }

  void set_scope_ui_failure(GaiaWebAuthFlow::Failure failure) {
    scope_ui_result_ = false;
    scope_ui_failure_ = failure;
  }

  void set_scope_ui_oauth_error(const std::string& oauth_error) {
    scope_ui_result_ = false;
    scope_ui_failure_ = GaiaWebAuthFlow::OAUTH_ERROR;
    scope_ui_oauth_error_ = oauth_error;
  }

  bool login_ui_shown() const { return login_ui_shown_; }

  bool scope_ui_shown() const { return scope_ui_shown_; }

  std::string login_access_token() const { return login_access_token_; }

  void StartLoginAccessTokenRequest() override {
    if (auto_login_access_token_) {
      if (login_access_token_result_) {
        OnGetTokenSuccess(login_token_request_.get(),
                          "access_token",
                          base::Time::Now() + base::TimeDelta::FromHours(1LL));
      } else {
        GoogleServiceAuthError error(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
        OnGetTokenFailure(login_token_request_.get(), error);
      }
    } else {
      // Make a request to the token service. The test now must tell
      // the token service to issue an access token (or an error).
      IdentityGetAuthTokenFunction::StartLoginAccessTokenRequest();
    }
  }

#if defined(OS_CHROMEOS)
  void StartDeviceLoginAccessTokenRequest() override {
    StartLoginAccessTokenRequest();
  }
#endif

  void ShowLoginPopup() override {
    EXPECT_FALSE(login_ui_shown_);
    login_ui_shown_ = true;
    if (login_ui_result_)
      SigninSuccess();
    else
      SigninFailed();
  }

  void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice) override {
    scope_ui_shown_ = true;

    if (scope_ui_result_) {
      OnGaiaFlowCompleted(kAccessToken, "3600");
    } else if (scope_ui_failure_ == GaiaWebAuthFlow::SERVICE_AUTH_ERROR) {
      GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
      OnGaiaFlowFailure(scope_ui_failure_, error, "");
    } else {
      GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
      OnGaiaFlowFailure(scope_ui_failure_, error, scope_ui_oauth_error_);
    }
  }

  void StartGaiaRequest(const std::string& login_access_token) override {
    EXPECT_TRUE(login_access_token_.empty());
    // Save the login token used in the mint token flow so tests can see
    // what account was used.
    login_access_token_ = login_access_token;
    IdentityGetAuthTokenFunction::StartGaiaRequest(login_access_token);
  }

  OAuth2MintTokenFlow* CreateMintTokenFlow() override {
    return flow_.release();
  }

 private:
  ~FakeGetAuthTokenFunction() override {}
  bool login_access_token_result_;
  bool auto_login_access_token_;
  bool login_ui_result_;
  bool scope_ui_result_;
  GaiaWebAuthFlow::Failure scope_ui_failure_;
  std::string scope_ui_oauth_error_;
  bool login_ui_shown_;
  bool scope_ui_shown_;

  std::unique_ptr<OAuth2MintTokenFlow> flow_;

  std::string login_access_token_;
};

class MockQueuedMintRequest : public IdentityMintRequestQueue::Request {
 public:
  MOCK_METHOD1(StartMintToken, void(IdentityMintRequestQueue::MintType));
};

gaia::AccountIds CreateIds(const std::string& email, const std::string& obfid) {
  gaia::AccountIds ids;
  ids.account_key = email;
  ids.email = email;
  ids.gaia = obfid;
  return ids;
}

class IdentityGetAccountsFunctionTest : public ExtensionBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExtensionsMultiAccount);
  }

 protected:
  void SetAccountState(gaia::AccountIds ids, bool is_signed_in) {
    IdentityAPI::GetFactoryInstance()->Get(profile())->SetAccountStateForTest(
        ids, is_signed_in);
  }

  testing::AssertionResult ExpectGetAccounts(
      const std::vector<std::string>& accounts) {
    scoped_refptr<IdentityGetAccountsFunction> func(
        new IdentityGetAccountsFunction);
    func->set_extension(test_util::CreateEmptyExtension(kExtensionId).get());
    if (!utils::RunFunction(
            func.get(), std::string("[]"), browser(), utils::NONE)) {
      return GenerateFailureResult(accounts, NULL)
             << "getAccounts did not return a result.";
    }
    const base::ListValue* callback_arguments = func->GetResultList();
    if (!callback_arguments)
      return GenerateFailureResult(accounts, NULL) << "NULL result";

    if (callback_arguments->GetSize() != 1) {
      return GenerateFailureResult(accounts, NULL)
             << "Expected 1 argument but got " << callback_arguments->GetSize();
    }

    const base::ListValue* results;
    if (!callback_arguments->GetList(0, &results))
      GenerateFailureResult(accounts, NULL) << "Result was not an array";

    std::set<std::string> result_ids;
    for (base::ListValue::const_iterator it = results->begin();
         it != results->end();
         ++it) {
      std::unique_ptr<api::identity::AccountInfo> info =
          api::identity::AccountInfo::FromValue(**it);
      if (info.get())
        result_ids.insert(info->id);
      else
        return GenerateFailureResult(accounts, results);
    }

    for (std::vector<std::string>::const_iterator it = accounts.begin();
         it != accounts.end();
         ++it) {
      if (result_ids.find(*it) == result_ids.end())
        return GenerateFailureResult(accounts, results);
    }

    return testing::AssertionResult(true);
  }

  testing::AssertionResult GenerateFailureResult(
      const ::std::vector<std::string>& accounts,
      const base::ListValue* results) {
    testing::Message msg("Expected: ");
    for (std::vector<std::string>::const_iterator it = accounts.begin();
         it != accounts.end();
         ++it) {
      msg << *it << " ";
    }
    msg << "Actual: ";
    if (!results) {
      msg << "NULL";
    } else {
      for (const auto& result : *results) {
        std::unique_ptr<api::identity::AccountInfo> info =
            api::identity::AccountInfo::FromValue(*result);
        if (info.get())
          msg << info->id << " ";
        else
          msg << *result << "<-" << result->GetType() << " ";
      }
    }

    return testing::AssertionFailure(msg);
  }
};

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, MultiAccountOn) {
  EXPECT_TRUE(switches::IsExtensionsMultiAccount());
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, NoneSignedIn) {
  EXPECT_TRUE(ExpectGetAccounts(std::vector<std::string>()));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest,
                       PrimaryAccountSignedIn) {
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  std::vector<std::string> primary;
  primary.push_back("1");
  EXPECT_TRUE(ExpectGetAccounts(primary));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, TwoAccountsSignedIn) {
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);
  std::vector<std::string> two_accounts;
  two_accounts.push_back("1");
  two_accounts.push_back("2");
  EXPECT_TRUE(ExpectGetAccounts(two_accounts));
}

class IdentityOldProfilesGetAccountsFunctionTest
    : public IdentityGetAccountsFunctionTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Don't add the multi-account switch that parent class would have.
  }
};

IN_PROC_BROWSER_TEST_F(IdentityOldProfilesGetAccountsFunctionTest,
                       MultiAccountOff) {
  EXPECT_FALSE(switches::IsExtensionsMultiAccount());
}

IN_PROC_BROWSER_TEST_F(IdentityOldProfilesGetAccountsFunctionTest,
                       TwoAccountsSignedIn) {
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);
  std::vector<std::string> only_primary;
  only_primary.push_back("1");
  EXPECT_TRUE(ExpectGetAccounts(only_primary));
}

class IdentityTestWithSignin : public AsyncExtensionBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    AsyncExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(
                    &IdentityTestWithSignin::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Replace the signin manager and token service with fakes. Do this ahead of
    // creating the browser so that a bunch of classes don't register as
    // observers and end up needing to unregister when the fake is substituted.
    SigninManagerFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeSigninManagerBase);
    ProfileOAuth2TokenServiceFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeProfileOAuth2TokenService);
    GaiaCookieManagerServiceFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeGaiaCookieManagerService);
  }

  void SetUpOnMainThread() override {
    AsyncExtensionBrowserTest::SetUpOnMainThread();

    // Grab references to the fake signin manager and token service.
    signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetInstance()->GetForProfile(profile()));
    ASSERT_TRUE(signin_manager_);
    token_service_ = static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetInstance()->GetForProfile(
            profile()));
    ASSERT_TRUE(token_service_);
    GaiaCookieManagerServiceFactory::GetInstance()->GetForProfile(profile())
        ->Init();
  }

 protected:
  void SignIn(const std::string& account_key) {
    SignIn(account_key, account_key);
  }

  void SignIn(const std::string& email, const std::string& gaia) {
    AccountTrackerService* account_tracker =
        AccountTrackerServiceFactory::GetForProfile(profile());
    std::string account_id =
        account_tracker->SeedAccountInfo(gaia, email);

#if defined(OS_CHROMEOS)
    signin_manager_->SetAuthenticatedAccountInfo(gaia, email);
#else
    signin_manager_->SignIn(gaia, email, "password");
#endif
    token_service_->UpdateCredentials(account_id, "refresh_token");
  }

  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

class IdentityGetProfileUserInfoFunctionTest : public IdentityTestWithSignin {
 protected:
  std::unique_ptr<api::identity::ProfileUserInfo> RunGetProfileUserInfo() {
    scoped_refptr<IdentityGetProfileUserInfoFunction> func(
        new IdentityGetProfileUserInfoFunction);
    func->set_extension(test_util::CreateEmptyExtension(kExtensionId).get());
    std::unique_ptr<base::Value> value(
        utils::RunFunctionAndReturnSingleResult(func.get(), "[]", browser()));
    return api::identity::ProfileUserInfo::FromValue(*value.get());
  }

  std::unique_ptr<api::identity::ProfileUserInfo>
  RunGetProfileUserInfoWithEmail() {
    scoped_refptr<IdentityGetProfileUserInfoFunction> func(
        new IdentityGetProfileUserInfoFunction);
    func->set_extension(CreateExtensionWithEmailPermission());
    std::unique_ptr<base::Value> value(
        utils::RunFunctionAndReturnSingleResult(func.get(), "[]", browser()));
    return api::identity::ProfileUserInfo::FromValue(*value.get());
  }

 private:
  scoped_refptr<Extension> CreateExtensionWithEmailPermission() {
    std::unique_ptr<base::DictionaryValue> test_extension_value(
        api_test_utils::ParseDictionary(
            "{\"name\": \"Test\", \"version\": \"1.0\", "
            "\"permissions\": [\"identity.email\"]}"));
    return api_test_utils::CreateExtension(test_extension_value.get());
  }
};

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest, NotSignedIn) {
  std::unique_ptr<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithEmail();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest, SignedIn) {
  SignIn("president@example.com", "12345");
  std::unique_ptr<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithEmail();
  EXPECT_EQ("president@example.com", info->email);
  EXPECT_EQ("12345", info->id);
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest,
                       NotSignedInNoEmail) {
  std::unique_ptr<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfo();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest,
                       SignedInNoEmail) {
  SignIn("president@example.com", "12345");
  std::unique_ptr<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfo();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

class GetAuthTokenFunctionTest : public IdentityTestWithSignin {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IdentityTestWithSignin::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExtensionsMultiAccount);
  }

  void IssueLoginRefreshTokenForAccount(const std::string& account_key) {
    token_service_->UpdateCredentials(account_key, "refresh_token");
  }

  void IssueLoginAccessTokenForAccount(const std::string& account_key) {
    token_service_->IssueAllTokensForAccount(
        account_key,
        "access_token-" + account_key,
        base::Time::Now() + base::TimeDelta::FromSeconds(3600));
  }

  void SetAccountState(gaia::AccountIds ids, bool is_signed_in) {
    IdentityAPI::GetFactoryInstance()->Get(profile())->SetAccountStateForTest(
        ids, is_signed_in);
  }

 protected:
  enum OAuth2Fields {
    NONE = 0,
    CLIENT_ID = 1,
    SCOPES = 2,
    AS_COMPONENT = 4
  };

  ~GetAuthTokenFunctionTest() override {}

  // Helper to create an extension with specific OAuth2Info fields set.
  // |fields_to_set| should be computed by using fields of Oauth2Fields enum.
  const Extension* CreateExtension(int fields_to_set) {
    const Extension* ext;
    base::FilePath manifest_path =
        test_data_dir_.AppendASCII("platform_apps/oauth2");
    base::FilePath component_manifest_path =
        test_data_dir_.AppendASCII("packaged_app/component_oauth2");
    if ((fields_to_set & AS_COMPONENT) == 0)
      ext = LoadExtension(manifest_path);
    else
      ext = LoadExtensionAsComponent(component_manifest_path);
    OAuth2Info& oauth2_info =
        const_cast<OAuth2Info&>(OAuth2Info::GetOAuth2Info(ext));
    if ((fields_to_set & CLIENT_ID) != 0)
      oauth2_info.client_id = "client1";
    if ((fields_to_set & SCOPES) != 0) {
      oauth2_info.scopes.push_back("scope1");
      oauth2_info.scopes.push_back("scope2");
    }

    extension_id_ = ext->id();
    oauth_scopes_ = std::set<std::string>(oauth2_info.scopes.begin(),
                                          oauth2_info.scopes.end());
    return ext;
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

  const std::string& GetPrimaryAccountId() {
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(browser()->profile());
    return signin_manager->GetAuthenticatedAccountId();
  }

  void SetCachedToken(const IdentityTokenCacheValue& token_data) {
    ExtensionTokenKey key(extension_id_, GetPrimaryAccountId(), oauth_scopes_);
    id_api()->SetCachedToken(key, token_data);
  }

  const IdentityTokenCacheValue& GetCachedToken(const std::string& account_id) {
    ExtensionTokenKey key(
        extension_id_, account_id.empty() ? GetPrimaryAccountId() : account_id,
        oauth_scopes_);
    return id_api()->GetCachedToken(key);
  }

  void QueueRequestStart(IdentityMintRequestQueue::MintType type,
                         IdentityMintRequestQueue::Request* request) {
    ExtensionTokenKey key(extension_id_, GetPrimaryAccountId(), oauth_scopes_);
    id_api()->mint_queue()->RequestStart(type, key, request);
  }

  void QueueRequestComplete(IdentityMintRequestQueue::MintType type,
                            IdentityMintRequestQueue::Request* request) {
    ExtensionTokenKey key(extension_id_, GetPrimaryAccountId(), oauth_scopes_);
    id_api()->mint_queue()->RequestComplete(type, key, request);
  }

 private:
  std::string extension_id_;
  std::set<std::string> oauth_scopes_;
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NoClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(SCOPES));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidClientId), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NoScopes) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidScopes), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveNotSignedIn) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintFailure) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveLoginAccessTokenFailure) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintAdviceSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_ADVICE,
            GetCachedToken(std::string()).status());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintBadCredentials) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS);
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintServiceError) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SERVICE_ERROR);
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NoOptionsSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginCanceled) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveMintBadCredentialsLoginCanceled) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS);
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessNoToken) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintFailure) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessLoginAccessTokenFailure) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintSuccess) {
  // TODO(courage): verify that account_id in token service requests
  // is correct once manual token minting for tests is implemented.
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalAborted) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalSuccess) {
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->set_login_ui_result(true);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalAborted) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalLoadFailed) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::LOAD_FAILED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kPageLoadFailure), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalInvalidRedirect) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::INVALID_REDIRECT);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidRedirect), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalConnectionFailure) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::SERVICE_AUTH_ERROR);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalOAuthErrors) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));

  std::map<std::string, std::string> error_map;
  error_map.insert(std::make_pair("access_denied", errors::kUserRejected));
  error_map.insert(std::make_pair("invalid_scope", errors::kInvalidScopes));
  error_map.insert(std::make_pair(
      "unmapped_error", std::string(errors::kAuthFailure) + "unmapped_error"));

  for (std::map<std::string, std::string>::const_iterator
           it = error_map.begin();
       it != error_map.end();
       ++it) {
    scoped_refptr<FakeGetAuthTokenFunction> func(
        new FakeGetAuthTokenFunction());
    func->set_extension(extension.get());
    // Make sure we don't get a cached issue_advice result, which would cause
    // flow to be leaked.
    id_api()->EraseAllCachedTokens();
    func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
    func->set_scope_ui_oauth_error(it->first);
    std::string error = utils::RunFunctionAndReturnError(
        func.get(), "[{\"interactive\": true}]", browser());
    EXPECT_EQ(it->second, error);
    EXPECT_FALSE(func->login_ui_shown());
    EXPECT_TRUE(func->scope_ui_shown());
  }
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoninteractiveQueue) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  RunFunctionAsync(func.get(), "[{}]");
  // Verify that we have fetched the login token at this point.
  testing::Mock::VerifyAndClearExpectations(func.get());

  // The flow will be created after the first queued request clears.
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  QueueRequestComplete(type, &queued_request);

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveQueue) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  // Verify that we have fetched the login token and run the first flow.
  testing::Mock::VerifyAndClearExpectations(func.get());
  EXPECT_FALSE(func->scope_ui_shown());

  // The UI will be displayed and a token retrieved after the first
  // queued request clears.
  QueueRequestComplete(type, &queued_request);

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveQueueShutdown) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  // Verify that we have fetched the login token and run the first flow.
  testing::Mock::VerifyAndClearExpectations(func.get());
  EXPECT_FALSE(func->scope_ui_shown());

  // After the request is canceled, the function will complete.
  func->OnShutdown();
  EXPECT_EQ(std::string(errors::kCanceled), WaitForError(func.get()));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  QueueRequestComplete(type, &queued_request);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoninteractiveShutdown) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  func->set_mint_token_flow(
      base::WrapUnique(new TestHangOAuth2MintTokenFlow()));
  RunFunctionAsync(func.get(), "[{\"interactive\": false}]");

  // After the request is canceled, the function will complete.
  func->OnShutdown();
  EXPECT_EQ(std::string(errors::kCanceled), WaitForError(func.get()));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveQueuedNoninteractiveFails) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the interactive queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // Non-interactive requests fail without hitting GAIA, because a
  // consent UI is known to be up.
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  QueueRequestComplete(type, &queued_request);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // pre-populate the cache with a token
  IdentityTokenCacheValue token(kAccessToken,
                                base::TimeDelta::FromSeconds(3600));
  SetCachedToken(token);

  // Get a token. Should not require a GAIA request.
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveIssueAdviceCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // pre-populate the cache with advice
  IssueAdviceInfo info;
  IdentityTokenCacheValue token(info);
  SetCachedToken(token);

  // Should return an error without a GAIA request.
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");

  // Populate the cache with a token while the request is blocked.
  IdentityTokenCacheValue token(kAccessToken,
                                base::TimeDelta::FromSeconds(3600));
  SetCachedToken(token);

  // When we wake up the request, it returns the cached token without
  // displaying a UI, or hitting GAIA.

  QueueRequestComplete(type, &queued_request);

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       LoginInvalidatesTokenCache) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  // pre-populate the cache with a token
  IdentityTokenCacheValue token(kAccessToken,
                                base::TimeDelta::FromSeconds(3600));
  SetCachedToken(token);

  // Because the user is not signed in, the token will be removed,
  // and we'll hit GAIA for new tokens.
  func->set_login_ui_result(true);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ComponentWithChromeClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->ignore_did_respond_for_testing();
  scoped_refptr<const Extension> extension(
      CreateExtension(SCOPES | AS_COMPONENT));
  func->set_extension(extension.get());
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension.get());
  EXPECT_TRUE(oauth2_info.client_id.empty());
  EXPECT_FALSE(func->GetOAuth2ClientId().empty());
  EXPECT_NE("client1", func->GetOAuth2ClientId());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ComponentWithNormalClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->ignore_did_respond_for_testing();
  scoped_refptr<const Extension> extension(
      CreateExtension(CLIENT_ID | SCOPES | AS_COMPONENT));
  func->set_extension(extension.get());
  EXPECT_EQ("client1", func->GetOAuth2ClientId());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, MultiDefaultUser) {
  SignIn("primary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  RunFunctionAsync(func.get(), "[{}]");

  IssueLoginAccessTokenForAccount("primary@example.com");

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
  EXPECT_EQ("access_token-primary@example.com", func->login_access_token());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, MultiPrimaryUser) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  RunFunctionAsync(func.get(), "[{\"account\": { \"id\": \"1\" } }]");

  IssueLoginAccessTokenForAccount("primary@example.com");

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(std::string()).status());
  EXPECT_EQ("access_token-primary@example.com", func->login_access_token());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, MultiSecondaryUser) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  RunFunctionAsync(func.get(), "[{\"account\": { \"id\": \"2\" } }]");

  IssueLoginAccessTokenForAccount("secondary@example.com");

  std::unique_ptr<base::Value> value(WaitForSingleResult(func.get()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken("secondary@example.com").status());
  EXPECT_EQ("access_token-secondary@example.com", func->login_access_token());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, MultiUnknownUser) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);

  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"account\": { \"id\": \"3\" } }]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryNonInteractiveMintFailure) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"account\": { \"id\": \"2\" } }]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryNonInteractiveLoginAccessTokenFailure) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"account\": { \"id\": \"2\" } }]", browser());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryInteractiveApprovalAborted) {
  SignIn("primary@example.com");
  IssueLoginRefreshTokenForAccount("secondary@example.com");
  SetAccountState(CreateIds("primary@example.com", "1"), true);
  SetAccountState(CreateIds("secondary@example.com", "2"), true);

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS);
  func->set_scope_ui_failure(GaiaWebAuthFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(),
      "[{\"account\": { \"id\": \"2\" }, \"interactive\": true}]",
      browser());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesDefault) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(2ul, token_key->scopes.size());
  EXPECT_TRUE(ContainsKey(token_key->scopes, "scope1"));
  EXPECT_TRUE(ContainsKey(token_key->scopes, "scope2"));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmpty) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::string error(utils::RunFunctionAndReturnError(
      func.get(), "[{\"scopes\": []}]", browser()));

  EXPECT_EQ(errors::kInvalidScopes, error);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmail) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"scopes\": [\"email\"]}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(1ul, token_key->scopes.size());
  EXPECT_TRUE(ContainsKey(token_key->scopes, "email"));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmailFooBar) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"scopes\": [\"email\", \"foo\", \"bar\"]}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(3ul, token_key->scopes.size());
  EXPECT_TRUE(ContainsKey(token_key->scopes, "email"));
  EXPECT_TRUE(ContainsKey(token_key->scopes, "foo"));
  EXPECT_TRUE(ContainsKey(token_key->scopes, "bar"));
}


#if defined(OS_CHROMEOS)
class GetAuthTokenFunctionPublicSessionTest : public GetAuthTokenFunctionTest {
 public:
  GetAuthTokenFunctionPublicSessionTest()
      : user_manager_(new chromeos::MockUserManager) {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
     GetAuthTokenFunctionTest::SetUpInProcessBrowserTestFixture();

     // Set up the user manager to fake a public session.
     EXPECT_CALL(*user_manager_, IsLoggedInAsKioskApp())
         .WillRepeatedly(Return(false));
     EXPECT_CALL(*user_manager_, IsLoggedInAsPublicAccount())
         .WillRepeatedly(Return(true));

    // Set up fake install attributes to make the device appeared as
    // enterprise-managed.
     std::unique_ptr<policy::StubEnterpriseInstallAttributes> attributes(
         new policy::StubEnterpriseInstallAttributes());
     attributes->SetDomain("example.com");
     attributes->SetRegistrationUser("user@example.com");
     policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
         attributes.release());
  }

  scoped_refptr<Extension> CreateTestExtension(const std::string& id) {
    return ExtensionBuilder()
        .SetManifest(
            DictionaryBuilder()
                .Set("name", "Test")
                .Set("version", "1.0")
                .Set("oauth2",
                     DictionaryBuilder()
                         .Set("client_id", "clientId")
                         .Set("scopes", ListBuilder().Append("scope1").Build())
                         .Build())
                .Build())
        .SetLocation(Manifest::UNPACKED)
        .SetID(id)
        .Build();
  }

  // Owned by |user_manager_enabler|.
  chromeos::MockUserManager* user_manager_;
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionPublicSessionTest, NonWhitelisted) {
  // GetAuthToken() should return UserNotSignedIn in public sessions for
  // non-whitelisted extensions.
  chromeos::ScopedUserManagerEnabler user_manager_enabler(user_manager_);
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateTestExtension("test-id"));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionPublicSessionTest, Whitelisted) {
  // GetAuthToken() should return a token for whitelisted extensions.
  chromeos::ScopedUserManagerEnabler user_manager_enabler(user_manager_);
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateTestExtension("ljacajndfccfgnfohlgkdphmbnpkjflk"));
  func->set_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
}

#endif


class RemoveCachedAuthTokenFunctionTest : public ExtensionBrowserTest {
 protected:
  bool InvalidateDefaultToken() {
    scoped_refptr<IdentityRemoveCachedAuthTokenFunction> func(
        new IdentityRemoveCachedAuthTokenFunction);
    func->set_extension(test_util::CreateEmptyExtension(kExtensionId).get());
    return utils::RunFunction(
        func.get(),
        std::string("[{\"token\": \"") + kAccessToken + "\"}]",
        browser(),
        extension_function_test_utils::NONE);
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

  void SetCachedToken(const IdentityTokenCacheValue& token_data) {
    ExtensionTokenKey key(
        kExtensionId, "test@example.com", std::set<std::string>());
    id_api()->SetCachedToken(key, token_data);
  }

  const IdentityTokenCacheValue& GetCachedToken() {
    return id_api()->GetCachedToken(ExtensionTokenKey(
        kExtensionId, "test@example.com", std::set<std::string>()));
  }
};

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, NotFound) {
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetCachedToken().status());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, Advice) {
  IssueAdviceInfo info;
  IdentityTokenCacheValue advice(info);
  SetCachedToken(advice);
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_ADVICE,
            GetCachedToken().status());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, NonMatchingToken) {
  IdentityTokenCacheValue token("non_matching_token",
                                base::TimeDelta::FromSeconds(3600));
  SetCachedToken(token);
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken().status());
  EXPECT_EQ("non_matching_token", GetCachedToken().token());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, MatchingToken) {
  IdentityTokenCacheValue token(kAccessToken,
                                base::TimeDelta::FromSeconds(3600));
  SetCachedToken(token);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken().status());
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetCachedToken().status());
}

class LaunchWebAuthFlowFunctionTest : public AsyncExtensionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AsyncExtensionBrowserTest::SetUpCommandLine(command_line);
    // Reduce performance test variance by disabling background networking.
    command_line->AppendSwitch(switches::kDisableBackgroundNetworking);
  }
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, UserCloseWindow) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      "chrome/test/data/extensions/api_test/identity");
  ASSERT_TRUE(https_server.Start());
  GURL auth_url(https_server.GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  WaitForGURLAndCloseWindow popup_observer(auth_url);

  std::string args = "[{\"interactive\": true, \"url\": \"" +
      auth_url.spec() + "\"}]";
  RunFunctionAsync(function.get(), args);

  popup_observer.Wait();
  popup_observer.CloseEmbedderWebContents();

  EXPECT_EQ(std::string(errors::kUserRejected), WaitForError(function.get()));
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, InteractionRequired) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      "chrome/test/data/extensions/api_test/identity");
  ASSERT_TRUE(https_server.Start());
  GURL auth_url(https_server.GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  std::string args = "[{\"interactive\": false, \"url\": \"" +
      auth_url.spec() + "\"}]";
  std::string error =
      utils::RunFunctionAndReturnError(function.get(), args, browser());

  EXPECT_EQ(std::string(errors::kInteractionRequired), error);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, LoadFailed) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      "chrome/test/data/extensions/api_test/identity");
  ASSERT_TRUE(https_server.Start());
  GURL auth_url(https_server.GetURL("/five_hundred.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  std::string args = "[{\"interactive\": true, \"url\": \"" +
      auth_url.spec() + "\"}]";
  std::string error =
      utils::RunFunctionAndReturnError(function.get(), args, browser());

  EXPECT_EQ(std::string(errors::kPageLoadFailure), error);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, NonInteractiveSuccess) {
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixForTest("abcdefghij");
  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"interactive\": false,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

IN_PROC_BROWSER_TEST_F(
    LaunchWebAuthFlowFunctionTest, InteractiveFirstNavigationSuccess) {
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixForTest("abcdefghij");
  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"interactive\": true,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       DISABLED_InteractiveSecondNavigationSuccess) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      "chrome/test/data/extensions/api_test/identity");
  ASSERT_TRUE(https_server.Start());
  GURL auth_url(https_server.GetURL("/redirect_to_chromiumapp.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixForTest("abcdefghij");
  std::string args = "[{\"interactive\": true, \"url\": \"" +
      auth_url.spec() + "\"}]";
  std::unique_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(function.get(), args, browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

}  // namespace extensions

// Tests the chrome.identity API implemented by custom JS bindings .
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeIdentityJsBindings) {
  ASSERT_TRUE(RunExtensionTest("identity/js_bindings")) << message_;
}
