// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPrimaryAccountKey[] = "primary_account@example.com";

enum TrackingEventType {
  ADDED,
  REMOVED,
  SIGN_IN,
  SIGN_OUT
};

std::string AccountKeyToObfuscatedId(const std::string email) {
  return "obfid-" + email;
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const std::string& account_key,
                const std::string& gaia_id)
      : type_(type),
        account_key_(account_key),
        gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type,
                const std::string& account_key)
      : type_(type),
        account_key_(account_key),
        gaia_id_(AccountKeyToObfuscatedId(account_key)) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_key_ == event.account_key_ &&
        gaia_id_ == event.gaia_id_;
  }

  std::string ToString() const {
    const char * typestr = "INVALID";
    switch (type_) {
      case ADDED:
        typestr = "ADD";
        break;
      case REMOVED:
        typestr = "REM";
        break;
      case SIGN_IN:
        typestr = " IN";
        break;
      case SIGN_OUT:
        typestr = "OUT";
        break;
    }
    return base::StringPrintf("{ type: %s, email: %s, gaia: %s }",
                              typestr,
                              account_key_.c_str(),
                              gaia_id_.c_str());
  }

 private:
  friend bool CompareByUser(TrackingEvent a, TrackingEvent b);

  TrackingEventType type_;
  std::string account_key_;
  std::string gaia_id_;
};

bool CompareByUser(TrackingEvent a, TrackingEvent b) {
  return a.account_key_ < b.account_key_;
}

std::string Str(const std::vector<TrackingEvent>& events) {
  std::string str = "[";
  bool needs_comma = false;
  for (std::vector<TrackingEvent>::const_iterator it =
       events.begin(); it != events.end(); ++it) {
    if (needs_comma)
      str += ",\n ";
    needs_comma = true;
    str += it->ToString();
  }
  str += "]";
  return str;
}

}  // namespace

namespace gaia {

class AccountTrackerObserver : public AccountTracker::Observer {
 public:
  AccountTrackerObserver() {}
  virtual ~AccountTrackerObserver() {}

  testing::AssertionResult CheckEvents();
  testing::AssertionResult CheckEvents(const TrackingEvent& e1);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5,
                                       const TrackingEvent& e6);
  void Clear();
  void SortEventsByUser();

  // AccountTracker::Observer implementation
  virtual void OnAccountAdded(const AccountIds& ids) OVERRIDE;
  virtual void OnAccountRemoved(const AccountIds& ids) OVERRIDE;
  virtual void OnAccountSignInChanged(const AccountIds& ids, bool is_signed_in)
      OVERRIDE;

 private:
  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountAdded(const AccountIds& ids) {
  events_.push_back(TrackingEvent(ADDED, ids.email, ids.gaia));
}

void AccountTrackerObserver::OnAccountRemoved(const AccountIds& ids) {
  events_.push_back(TrackingEvent(REMOVED, ids.email, ids.gaia));
}

void AccountTrackerObserver::OnAccountSignInChanged(const AccountIds& ids,
                                                    bool is_signed_in) {
  events_.push_back(
      TrackingEvent(is_signed_in ? SIGN_IN : SIGN_OUT, ids.email, ids.gaia));
}

void AccountTrackerObserver::Clear() {
  events_.clear();
}

void AccountTrackerObserver::SortEventsByUser() {
  std::stable_sort(events_.begin(), events_.end(), CompareByUser);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents() {
  std::vector<TrackingEvent> events;
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5,
    const TrackingEvent& e6) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  events.push_back(e6);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const std::vector<TrackingEvent>& events) {
  std::string maybe_newline = (events.size() + events_.size()) > 2 ? "\n" : "";
  testing::AssertionResult result(
      (events_ == events)
          ? testing::AssertionSuccess()
          : (testing::AssertionFailure()
             << "Expected " << maybe_newline << Str(events) << ", "
             << maybe_newline << "Got " << maybe_newline << Str(events_)));
  events_.clear();
  return result;
}

class IdentityAccountTrackerTest : public testing::Test {
 public:
  IdentityAccountTrackerTest() {}

  virtual ~IdentityAccountTrackerTest() {}

  virtual void SetUp() OVERRIDE {

    fake_oauth2_token_service_.reset(new FakeOAuth2TokenService());

    fake_identity_provider_.reset(
        new FakeIdentityProvider(fake_oauth2_token_service_.get()));

    account_tracker_.reset(
        new AccountTracker(fake_identity_provider_.get(),
                           new net::TestURLRequestContextGetter(
                               message_loop_.message_loop_proxy())));
    account_tracker_->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    account_tracker_->RemoveObserver(&observer_);
    account_tracker_->Shutdown();
  }

  AccountTrackerObserver* observer() {
    return &observer_;
  }

  AccountTracker* account_tracker() {
    return account_tracker_.get();
  }

  // Helpers to pass fake events to the tracker.

  void NotifyLogin(const std::string account_key) {
    identity_provider()->LogIn(account_key);
  }

  void NotifyLogout() { identity_provider()->LogOut(); }

  void NotifyTokenAvailable(const std::string& username) {
    fake_oauth2_token_service_->AddAccount(username);
  }

  void NotifyTokenRevoked(const std::string& username) {
    fake_oauth2_token_service_->RemoveAccount(username);
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(const std::string& username) {
    fake_oauth2_token_service_->IssueAllTokensForAccount(
        username, "access_token-" + username, base::Time::Max());
  }

  std::string GetValidTokenInfoResponse(const std::string account_key) {
    return std::string("{ \"id\": \"") + AccountKeyToObfuscatedId(account_key) +
           "\" }";
  }

  void ReturnOAuthUrlFetchResults(int fetcher_id,
                                  net::HttpStatusCode response_code,
                                  const std::string& response_string);

  void ReturnOAuthUrlFetchSuccess(const std::string& account_key);
  void ReturnOAuthUrlFetchFailure(const std::string& account_key);

  void SetupPrimaryLogin() {
    // Initial setup for tests that start with a signed in profile.
    NotifyLogin(kPrimaryAccountKey);
    NotifyTokenAvailable(kPrimaryAccountKey);
    ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
    observer()->Clear();
  }

  std::string active_account_id() {
    return identity_provider()->GetActiveAccountId();
  }

 private:
  FakeIdentityProvider* identity_provider() {
    return static_cast<FakeIdentityProvider*>(
        account_tracker_->identity_provider());
  }

  base::MessageLoopForIO message_loop_;  // net:: stuff needs IO message loop.
  net::TestURLFetcherFactory test_fetcher_factory_;
  scoped_ptr<FakeOAuth2TokenService> fake_oauth2_token_service_;
  scoped_ptr<FakeIdentityProvider> fake_identity_provider_;

  scoped_ptr<AccountTracker> account_tracker_;
  AccountTrackerObserver observer_;
};

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchResults(
    int fetcher_id,
    net::HttpStatusCode response_code,
    const std::string&  response_string) {

  net::TestURLFetcher* fetcher =
      test_fetcher_factory_.GetFetcherByID(fetcher_id);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchSuccess(
    const std::string& account_key) {
  IssueAccessToken(account_key);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId,
                             net::HTTP_OK,
                             GetValidTokenInfoResponse(account_key));
}

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchFailure(
    const std::string& account_key) {
  IssueAccessToken(account_key);
  ReturnOAuthUrlFetchResults(
      gaia::GaiaOAuthClient::kUrlFetcherId, net::HTTP_BAD_REQUEST, "");
}

// Primary tests just involve the Active account

TEST_F(IdentityAccountTrackerTest, PrimaryNoEventsBeforeLogin) {
  NotifyTokenAvailable(kPrimaryAccountKey);
  NotifyTokenRevoked(kPrimaryAccountKey);
  NotifyLogout();
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, PrimaryLoginThenTokenAvailable) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(ADDED, kPrimaryAccountKey),
                              TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimaryTokenAvailableThenLogin) {
  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyLogin(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(ADDED, kPrimaryAccountKey),
                              TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimaryTokenAvailableAndRevokedThenLogin) {
  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyLogin(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(ADDED, kPrimaryAccountKey),
                              TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimaryRevokeThenLogout) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  observer()->Clear();

  NotifyTokenRevoked(kPrimaryAccountKey);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, kPrimaryAccountKey)));

  NotifyLogout();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(REMOVED, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimaryRevokeThenLogin) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  NotifyTokenRevoked(kPrimaryAccountKey);
  observer()->Clear();

  NotifyLogin(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, PrimaryRevokeThenTokenAvailable) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  NotifyTokenRevoked(kPrimaryAccountKey);
  observer()->Clear();

  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimaryLogoutThenRevoke) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  observer()->Clear();

  NotifyLogout();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, kPrimaryAccountKey),
                              TrackingEvent(REMOVED, kPrimaryAccountKey)));

  NotifyTokenRevoked(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, PrimaryLogoutFetchCancelAvailable) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  // TokenAvailable kicks off a fetch. Logout without satisfying it.
  NotifyLogout();
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, kPrimaryAccountKey),
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

// Non-primary accounts

TEST_F(IdentityAccountTrackerTest, Available) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, Revoke) {
  SetupPrimaryLogin();

  account_tracker()->OnRefreshTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeAvailable) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com")));

  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeAvailableWithPendingFetch) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeRevoke) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com")));

  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, AvailableAvailable) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));

  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, TwoAccounts) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "alpha@example.com"),
      TrackingEvent(SIGN_IN, "alpha@example.com")));

  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com")));

  NotifyTokenRevoked("alpha@example.com");
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, "alpha@example.com")));

  NotifyTokenRevoked("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "beta@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableTokenFetchFailAvailable) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchFailure("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, MultiSignOutSignIn) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");
  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");

  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "alpha@example.com"),
      TrackingEvent(SIGN_IN, "alpha@example.com"),
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com")));

  NotifyLogout();
  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "alpha@example.com"),
      TrackingEvent(REMOVED, "alpha@example.com"),
      TrackingEvent(SIGN_OUT, "beta@example.com"),
      TrackingEvent(REMOVED, "beta@example.com"),
      TrackingEvent(SIGN_OUT, kPrimaryAccountKey),
      TrackingEvent(REMOVED, kPrimaryAccountKey)));

  // No events fire at all while profile is signed out.
  NotifyTokenRevoked("alpha@example.com");
  NotifyTokenAvailable("gamma@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  // Signing the profile in again will resume tracking all accounts.
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess("beta@example.com");
  ReturnOAuthUrlFetchSuccess("gamma@example.com");
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com"),
      TrackingEvent(ADDED, "gamma@example.com"),
      TrackingEvent(SIGN_IN, "gamma@example.com"),
      TrackingEvent(ADDED, kPrimaryAccountKey),
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));

  // Revoking the primary token does not affect other accounts.
  NotifyTokenRevoked(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, kPrimaryAccountKey)));

  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

// Primary/non-primary interactions

TEST_F(IdentityAccountTrackerTest, MultiNoEventsBeforeLogin) {
  NotifyTokenAvailable(kPrimaryAccountKey);
  NotifyTokenAvailable("user@example.com");
  NotifyTokenRevoked("user@example.com");
  NotifyTokenRevoked(kPrimaryAccountKey);
  NotifyLogout();
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, MultiLogoutRemovesAllAccounts) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  observer()->Clear();

  NotifyLogout();
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, kPrimaryAccountKey),
                              TrackingEvent(REMOVED, kPrimaryAccountKey),
                              TrackingEvent(SIGN_OUT, "user@example.com"),
                              TrackingEvent(REMOVED, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, MultiRevokePrimaryDoesNotRemoveAllAccounts) {
  NotifyLogin(kPrimaryAccountKey);
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  observer()->Clear();

  NotifyTokenRevoked(kPrimaryAccountKey);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, GetAccountsPrimary) {
  SetupPrimaryLogin();

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(1ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
}

TEST_F(IdentityAccountTrackerTest, GetAccountsSignedOut) {
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

TEST_F(IdentityAccountTrackerTest, GetAccountsOnlyReturnAccountsWithTokens) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("alpha@example.com");
  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(2ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
  EXPECT_EQ("beta@example.com", ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("beta@example.com"), ids[1].gaia);
}

TEST_F(IdentityAccountTrackerTest, GetAccountsSortOrder) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("zeta@example.com");
  ReturnOAuthUrlFetchSuccess("zeta@example.com");
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");

  // The primary account will be first in the vector. Remaining accounts
  // will be sorted by gaia ID.
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(3ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
  EXPECT_EQ("alpha@example.com", ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("alpha@example.com"), ids[1].gaia);
  EXPECT_EQ("zeta@example.com", ids[2].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("zeta@example.com"), ids[2].gaia);
}

TEST_F(IdentityAccountTrackerTest,
       GetAccountsReturnNothingWhenPrimarySignedOut) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("zeta@example.com");
  ReturnOAuthUrlFetchSuccess("zeta@example.com");
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");

  NotifyTokenRevoked(kPrimaryAccountKey);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

TEST_F(IdentityAccountTrackerTest, FindAccountIdsByGaiaIdPrimary) {
  SetupPrimaryLogin();

  AccountIds ids = account_tracker()->FindAccountIdsByGaiaId(
      AccountKeyToObfuscatedId(kPrimaryAccountKey));
  EXPECT_EQ(kPrimaryAccountKey, ids.account_key);
  EXPECT_EQ(kPrimaryAccountKey, ids.email);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids.gaia);
}

TEST_F(IdentityAccountTrackerTest, FindAccountIdsByGaiaIdNotFound) {
  SetupPrimaryLogin();

  AccountIds ids = account_tracker()->FindAccountIdsByGaiaId(
      AccountKeyToObfuscatedId("notfound@example.com"));
  EXPECT_TRUE(ids.account_key.empty());
  EXPECT_TRUE(ids.email.empty());
  EXPECT_TRUE(ids.gaia.empty());
}

TEST_F(IdentityAccountTrackerTest,
       FindAccountIdsByGaiaIdReturnEmptyWhenPrimarySignedOut) {
  SetupPrimaryLogin();

  NotifyTokenAvailable("zeta@example.com");
  ReturnOAuthUrlFetchSuccess("zeta@example.com");
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");

  NotifyTokenRevoked(kPrimaryAccountKey);

  AccountIds ids =
      account_tracker()->FindAccountIdsByGaiaId(kPrimaryAccountKey);
  EXPECT_TRUE(ids.account_key.empty());
  EXPECT_TRUE(ids.email.empty());
  EXPECT_TRUE(ids.gaia.empty());

  ids = account_tracker()->FindAccountIdsByGaiaId("alpha@example.com");
  EXPECT_TRUE(ids.account_key.empty());
  EXPECT_TRUE(ids.email.empty());
  EXPECT_TRUE(ids.gaia.empty());
}

}  // namespace gaia
