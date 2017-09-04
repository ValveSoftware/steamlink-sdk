// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_state/web_state_policy_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {
// URL of the Google domain where the CHROME_CONNECTED cookie is set/removed.
NSURL* const kGoogleUrl = [NSURL URLWithString:@"https://google.com/"];
// URL of the Youtube domain where the CHROME_CONNECTED cookie is set/removed.
NSURL* const kYoutubeUrl = [NSURL URLWithString:@"https://youtube.com/"];
// URL of a country Google domain where the CHROME_CONNECTED cookie is
// set/removed.
NSURL* const kCountryGoogleUrl = [NSURL URLWithString:@"https://google.de/"];

// Google domain.
const char* kGoogleDomain = "google.com";
// Youtube domain.
const char* kYoutubeDomain = "youtube.com";

// AccountConsistencyService specialization that fakes the creation of the
// WKWebView in order to mock it. This allows tests to intercept the calls to
// the Web view and control they are correct.
class FakeAccountConsistencyService : public AccountConsistencyService {
 public:
  FakeAccountConsistencyService(
      web::BrowserState* browser_state,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      SigninClient* signin_client,
      SigninManager* signin_manager)
      : AccountConsistencyService(browser_state,
                                  account_reconcilor,
                                  cookie_settings,
                                  gaia_cookie_manager_service,
                                  signin_client,
                                  signin_manager),
        mock_web_view_(nil) {}

 private:
  WKWebView* CreateWKWebView() override {
    if (!mock_web_view_) {
      mock_web_view_ = [OCMockObject niceMockForClass:[WKWebView class]];
    }
    return [mock_web_view_ retain];
  }
  id mock_web_view_;
};

// Mock AccountReconcilor to catch call to OnReceivedManageAccountsResponse.
class MockAccountReconcilor : public AccountReconcilor {
 public:
  MockAccountReconcilor()
      : AccountReconcilor(nullptr, nullptr, nullptr, nullptr) {}
  MOCK_METHOD1(OnReceivedManageAccountsResponse, void(signin::GAIAServiceType));
};

// Mock GaiaCookieManagerService to catch call to ForceOnCookieChangedProcessing
class MockGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  MockGaiaCookieManagerService()
      : GaiaCookieManagerService(nullptr,
                                 GaiaConstants::kChromeSource,
                                 nullptr) {}
  MOCK_METHOD0(ForceOnCookieChangedProcessing, void());
};

// TestWebState that allows control over its policy decider.
class TestWebState : public web::TestWebState {
 public:
  TestWebState() : web::TestWebState(), decider_(nullptr) {}
  void AddPolicyDecider(web::WebStatePolicyDecider* decider) override {
    EXPECT_FALSE(decider_);
    decider_ = decider;
  }
  void RemovePolicyDecider(web::WebStatePolicyDecider* decider) override {
    EXPECT_EQ(decider_, decider);
    decider_ = nullptr;
  }
  bool ShouldAllowResponse(NSURLResponse* response) {
    return decider_ ? decider_->ShouldAllowResponse(response) : true;
  }
  void WebStateDestroyed() {
    if (!decider_)
      return;
    decider_->WebStateDestroyed();
  }

 private:
  web::WebStatePolicyDecider* decider_;
};

}  // namespace

class AccountConsistencyServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
    AccountConsistencyService::RegisterPrefs(prefs_.registry());
    AccountTrackerService::RegisterPrefs(prefs_.registry());
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    SigninManagerBase::RegisterProfilePrefs(prefs_.registry());

    gaia_cookie_manager_service_.reset(new MockGaiaCookieManagerService());
    signin_client_.reset(new TestSigninClient(&prefs_));
    signin_manager_.reset(new FakeSigninManager(
        signin_client_.get(), nullptr, &account_tracker_service_, nullptr));
    account_tracker_service_.Initialize(signin_client_.get());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* incognito_profile */, false /* guest_profile */);
    cookie_settings_ =
        new content_settings::CookieSettings(settings_map_.get(), &prefs_, "");
    ResetAccountConsistencyService();
  }

  void TearDown() override {
    account_consistency_service_->Shutdown();
    settings_map_->ShutdownOnUIThread();
    web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
    PlatformTest::TearDown();
  }

  // Adds an expectation for |url| to be loaded in the web view of
  // |account_consistency_service_|.
  // |continue_navigation| controls whether navigation will continue or be
  // stopped on page load.
  void AddPageLoadedExpectation(NSURL* url, bool continue_navigation) {
    void (^continueBlock)(NSInvocation*) = ^(NSInvocation* invocation) {
      if (!continue_navigation)
        return;
      WKWebView* web_view = nil;
      [invocation getArgument:&web_view atIndex:0];
      [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
    };
    [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:continueBlock])
        loadHTMLString:[OCMArg any]
               baseURL:url];
  }

  void ResetAccountConsistencyService() {
    if (account_consistency_service_) {
      account_consistency_service_->Shutdown();
    }
    account_consistency_service_.reset(new FakeAccountConsistencyService(
        &browser_state_, &account_reconcilor_, cookie_settings_,
        gaia_cookie_manager_service_.get(), signin_client_.get(),
        signin_manager_.get()));
  }

  void SignIn() {
    signin_manager_->SignIn("12345", "user@gmail.com", "password");
  }

  void SignOut() { signin_manager_->ForceSignOut(); }

  id GetMockWKWebView() { return account_consistency_service_->GetWKWebView(); }
  id GetNavigationDelegate() {
    return account_consistency_service_->navigation_delegate_;
  }

  bool ShouldAddCookieToDomain(const std::string& domain,
                               bool should_check_last_update_time) {
    return account_consistency_service_->ShouldAddChromeConnectedCookieToDomain(
        domain, should_check_last_update_time);
  }

  void CheckDomainHasCookie(const std::string& domain) {
    EXPECT_GE(
        account_consistency_service_->last_cookie_update_map_.count(domain),
        1u);
  }

  // Creates test threads, necessary for ActiveStateManager that needs a UI
  // thread.
  web::TestWebThreadBundle thread_bundle_;
  MockAccountReconcilor account_reconcilor_;
  AccountTrackerService account_tracker_service_;
  web::TestBrowserState browser_state_;
  user_prefs::TestingPrefServiceSyncable prefs_;
  TestWebState web_state_;
  // AccountConsistencyService being tested. Actually a
  // FakeAccountConsistencyService to be able to use a mock web view.
  std::unique_ptr<AccountConsistencyService> account_consistency_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<FakeSigninManager> signin_manager_;
  std::unique_ptr<MockGaiaCookieManagerService> gaia_cookie_manager_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

// Tests whether the WKWebView is actually stopped when the browser state is
// inactive.
TEST_F(AccountConsistencyServiceTest, OnInactive) {
  [[GetMockWKWebView() expect] stopLoading];
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that cookies that are added during SignIn and subsequent navigations
// are correctly removed during the SignOut.
TEST_F(AccountConsistencyServiceTest, SignInSignOut) {
  // Check that main Google domains are added.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  // Check that other Google domains are added on navigation.
  AddPageLoadedExpectation(kCountryGoogleUrl, true /* continue_navigation */);

  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  NSDictionary* headers = [NSDictionary dictionary];
  base::scoped_nsobject<NSHTTPURLResponse> response([[NSHTTPURLResponse alloc]
       initWithURL:kCountryGoogleUrl
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers]);
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(web_state_.ShouldAllowResponse(response));
  web_state_.WebStateDestroyed();

  // Check that all domains are removed.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kCountryGoogleUrl, true /* continue_navigation */);
  SignOut();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that pending cookie requests are correctly applied when the browser
// state becomes active.
TEST_F(AccountConsistencyServiceTest, ApplyOnActive) {
  // No request is made until the browser state is active, then a WKWebView and
  // its navigation delegate are created, and the requests are processed.
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNotNil]];
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  SignIn();
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that cookie request being processed is correctly cancelled when the
// browser state becomes inactives and correctly re-started later when the
// browser state becomes active.
TEST_F(AccountConsistencyServiceTest, CancelOnInactiveReApplyOnActive) {
  // The first request starts to get applied and get cancelled as the browser
  // state becomes inactive. It is resumed after the browser state becomes
  // active again.
  AddPageLoadedExpectation(kGoogleUrl, false /* continue_navigation */);
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that the X-Chrome-Manage-Accounts header is ignored unless it comes
// from Gaia signon realm.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsNotOnGaia) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  base::scoped_nsobject<NSHTTPURLResponse> response([[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.com"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers]);
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(web_state_.ShouldAllowResponse(response));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that navigation to Gaia signon realm with no X-Chrome-Manage-Accounts
// header in the response are simply untouched.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsNoHeader) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];

  NSDictionary* headers = [NSDictionary dictionary];
  base::scoped_nsobject<NSHTTPURLResponse> response([[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers]);
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(web_state_.ShouldAllowResponse(response));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with action
// DEFAULT.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsDefault) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  // Default action is |onManageAccounts|.
  [[delegate expect] onManageAccounts];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  base::scoped_nsobject<NSHTTPURLResponse> response([[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers]);
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(account_reconcilor_, OnReceivedManageAccountsResponse(
                                       signin::GAIA_SERVICE_TYPE_DEFAULT))
      .Times(1);
  EXPECT_FALSE(web_state_.ShouldAllowResponse(response));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that domains with cookie are added to the prefs only after the request
// has been applied.
TEST_F(AccountConsistencyServiceTest, DomainsWithCookiePrefsOnApplied) {
  // Second request is not completely applied. Ensure prefs reflect that.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, false /* continue_navigation */);
  SignIn();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());

  const base::DictionaryValue* dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(1u, dict->size());
  EXPECT_TRUE(dict->GetBooleanWithoutPathExpansion("google.com", nullptr));
  EXPECT_FALSE(dict->GetBooleanWithoutPathExpansion("youtube.com", nullptr));
}

// Tests that domains with cookie are correctly loaded from the prefs on service
// startup.
TEST_F(AccountConsistencyServiceTest, DomainsWithCookieLoadedFromPrefs) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());

  ResetAccountConsistencyService();
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignOut();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that domains with cookie are cleared when browsing data is removed.
TEST_F(AccountConsistencyServiceTest, DomainsClearedOnBrowsingDataRemoved) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
  const base::DictionaryValue* dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(2u, dict->size());

  EXPECT_CALL(*gaia_cookie_manager_service_, ForceOnCookieChangedProcessing())
      .Times(1);
  account_consistency_service_->OnBrowsingDataRemoved();
  dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(0u, dict->size());
}

// Tests that cookie requests are correctly processed or ignored when the update
// time isn't checked.
TEST_F(AccountConsistencyServiceTest, ShouldAddCookieDontCheckUpdateTime) {
  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, false));

  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, false));

  ResetAccountConsistencyService();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, false));
}

// Tests that cookie requests are correctly processed or ignored when the update
// time is checked.
TEST_F(AccountConsistencyServiceTest, ShouldAddCookieCheckUpdateTime) {
  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, true));

  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, true));

  ResetAccountConsistencyService();

  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, true));
}

// Tests that main domains are added to the internal map when cookies are set in
// reaction to signin.
TEST_F(AccountConsistencyServiceTest, SigninAddCookieOnMainDomains) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  CheckDomainHasCookie(kGoogleDomain);
  CheckDomainHasCookie(kYoutubeDomain);
}
