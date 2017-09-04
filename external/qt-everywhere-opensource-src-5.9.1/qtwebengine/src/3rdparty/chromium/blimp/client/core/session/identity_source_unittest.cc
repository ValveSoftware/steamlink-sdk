// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/identity_source.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace blimp {
namespace client {
namespace {

class MockIdentitySource : public IdentitySource {
 public:
  MockIdentitySource(
      std::unique_ptr<IdentityProvider> identity_provider,
      base::Callback<void(const GoogleServiceAuthError&)> error_callback,
      const IdentitySource::TokenCallback& callback)
      : IdentitySource(std::move(identity_provider), error_callback, callback),
        success_(0),
        fail_(0),
        refresh_(0),
        token_callback_count_(0) {}
  ~MockIdentitySource() override{};

  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    IdentitySource::OnGetTokenSuccess(request, access_token, expiration_time);
    success_++;
    token_ = access_token;
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    IdentitySource::OnGetTokenFailure(request, error);
    fail_++;
    token_.clear();
  }

  void OnRefreshTokenAvailable(const std::string& account_id) override {
    IdentitySource::OnRefreshTokenAvailable(account_id);
    refresh_++;
  }

  void MockTokenCall(const std::string& token) {
    token_callback_count_++;
    callback_token_ = token;
  }

  IdentityProvider* GetIdentityProvider() { return identity_provider_.get(); }

  int Succeeded() { return success_; }
  int Failed() { return fail_; }
  int Refreshed() { return refresh_; }
  int TokenCallbackCount() { return token_callback_count_; }
  // Return the token passed to TokenCallback.
  const std::string& CallbackToken() { return callback_token_; }
  // Return the token get from OAuth2TokenService.
  const std::string& Token() { return token_; }

  // Reset test recording data.
  void ResetTestRecords() {
    success_ = 0;
    fail_ = 0;
    refresh_ = 0;
    token_callback_count_ = 0;
    callback_token_.clear();
    token_.clear();
  }

 private:
  std::string token_;
  int success_;
  int fail_;
  int refresh_;

  int token_callback_count_;
  std::string callback_token_;
  DISALLOW_COPY_AND_ASSIGN(MockIdentitySource);
};

class IdentitySourceTest : public testing::Test {
 public:
  IdentitySourceTest() = default;
  ~IdentitySourceTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentitySourceTest);
};

TEST_F(IdentitySourceTest, TestConnect) {
  TestBlimpClientContextDelegate mock_blimp_delegate;
  MockIdentitySource auth(
      mock_blimp_delegate.CreateIdentityProvider(),
      base::Bind(&TestBlimpClientContextDelegate::OnAuthenticationError,
                 base::Unretained(&mock_blimp_delegate)),
      base::Bind(&MockIdentitySource::MockTokenCall, base::Unretained(&auth)));
  FakeIdentityProvider* id_provider =
      static_cast<FakeIdentityProvider*>(auth.GetIdentityProvider());
  FakeOAuth2TokenService* token_service = mock_blimp_delegate.GetTokenService();

  // Connect when user is not signed in. Nothing happens.
  id_provider->LogOut();
  auth.Connect();
  DCHECK_EQ(auth.Succeeded(), 0);
  DCHECK_EQ(auth.Failed(), 0);
  DCHECK_EQ(auth.Refreshed(), 0);
  DCHECK_EQ(auth.Token(), std::string());
  auth.ResetTestRecords();

  FakeOAuth2TokenServiceDelegate* mock_token_service_delegate =
      token_service->GetFakeOAuth2TokenServiceDelegate();

  // Connect when user signed in, but no refresh token, refresh token observer
  // should be added.
  std::string account = "mock_account";
  id_provider->LogIn(account);
  mock_token_service_delegate->RevokeCredentials(account);
  // Mock duplicate connect calls in this test.
  auth.Connect();
  auth.Connect();
  DCHECK_EQ(auth.Succeeded(), 0);
  DCHECK_EQ(auth.Failed(), 0);
  DCHECK_EQ(auth.Refreshed(), 0);
  DCHECK_EQ(auth.TokenCallbackCount(), 0);
  auth.ResetTestRecords();

  // Issue refresh token, listener should be triggered, and request should be
  // sent.
  mock_token_service_delegate->UpdateCredentials(account, "mock_refresh_token");
  DCHECK_EQ(auth.Succeeded(), 0);
  DCHECK_EQ(auth.Failed(), 0);
  DCHECK_EQ(auth.Refreshed(), 1);
  DCHECK_EQ(auth.TokenCallbackCount(), 0);
  auth.ResetTestRecords();

  // Fire access token success, first request should be fulfilled.
  base::Time time;
  std::string mock_access_token = "mock_access_token";
  token_service->IssueAllTokensForAccount(account, mock_access_token, time);
  DCHECK_EQ(auth.Succeeded(), 1);
  DCHECK_EQ(auth.Failed(), 0);
  DCHECK_EQ(auth.Token(), mock_access_token);
  DCHECK_EQ(auth.TokenCallbackCount(), 1);
  DCHECK_EQ(auth.CallbackToken(), mock_access_token);
  auth.ResetTestRecords();

  // Connect again and fire access token failed.
  GoogleServiceAuthError error(GoogleServiceAuthError::State::REQUEST_CANCELED);
  auth.Connect();
  auth.Connect();
  token_service->IssueErrorForAllPendingRequestsForAccount(account, error);
  DCHECK_EQ(auth.Succeeded(), 0);
  DCHECK_EQ(auth.Failed(), 1);
  DCHECK_EQ(auth.Token(), std::string());
  DCHECK_EQ(auth.TokenCallbackCount(), 0);
  auth.ResetTestRecords();

  // Refresh token listener should have been removed.
  mock_token_service_delegate->UpdateCredentials(account, "mock_refresh_token");
  DCHECK_EQ(auth.Refreshed(), 0);
  auth.ResetTestRecords();

  // Direct connect with refresh token, and no listener should be
  // added. The request is a retry request.
  auth.Connect();
  auth.Connect();
  token_service->IssueAllTokensForAccount(account, mock_access_token, time);
  DCHECK_EQ(auth.Succeeded(), 1);
  DCHECK_EQ(auth.Token(), mock_access_token);
  mock_token_service_delegate->UpdateCredentials(account, "mock_refresh_token");
  DCHECK_EQ(auth.Refreshed(), 0);
  DCHECK_EQ(auth.TokenCallbackCount(), 1);
  DCHECK_EQ(auth.CallbackToken(), mock_access_token);
}

// Test retry on token fetching when refresh token is updated during token
// request.
TEST_F(IdentitySourceTest, TestConnectRetry) {
  TestBlimpClientContextDelegate mock_blimp_delegate;
  MockIdentitySource auth(
      mock_blimp_delegate.CreateIdentityProvider(),
      base::Bind(&TestBlimpClientContextDelegate::OnAuthenticationError,
                 base::Unretained(&mock_blimp_delegate)),
      base::Bind(&MockIdentitySource::MockTokenCall, base::Unretained(&auth)));
  FakeOAuth2TokenService* token_service = mock_blimp_delegate.GetTokenService();
  FakeIdentityProvider* id_provider =
      static_cast<FakeIdentityProvider*>(auth.GetIdentityProvider());

  std::string account = "mock_account";
  std::string mock_access_token = "mock_token";
  id_provider->LogIn(account);

  // Prepare refresh token.
  FakeOAuth2TokenServiceDelegate* mock_token_service_delegate =
      token_service->GetFakeOAuth2TokenServiceDelegate();
  mock_token_service_delegate->UpdateCredentials(account, "mock_refresh_token");

  // Connect and update the refresh token.
  auth.Connect();
  GoogleServiceAuthError error(GoogleServiceAuthError::State::REQUEST_CANCELED);
  token_service->IssueErrorForAllPendingRequestsForAccount(account, error);

  // At this point, the first request should be canceled, but there should be
  // another retry request.
  DCHECK_EQ(auth.Succeeded(), 0);
  DCHECK_EQ(auth.Failed(), 1);
  DCHECK_EQ(auth.Token(), std::string());
  DCHECK_EQ(auth.TokenCallbackCount(), 0);
  auth.ResetTestRecords();

  // Trigger the second request without calling connect.
  base::Time time;
  token_service->IssueAllTokensForAccount(account, mock_access_token, time);
  DCHECK_EQ(auth.Succeeded(), 1);
  DCHECK_EQ(auth.Token(), mock_access_token);
  DCHECK_EQ(auth.TokenCallbackCount(), 1);
  DCHECK_EQ(auth.CallbackToken(), mock_access_token);
}

TEST_F(IdentitySourceTest, TestConnectFailDelegateCallback) {
  TestBlimpClientContextDelegate mock_blimp_delegate;
  MockIdentitySource auth(
      mock_blimp_delegate.CreateIdentityProvider(),
      base::Bind(&TestBlimpClientContextDelegate::OnAuthenticationError,
                 base::Unretained(&mock_blimp_delegate)),
      base::Bind(&MockIdentitySource::MockTokenCall, base::Unretained(&auth)));
  FakeOAuth2TokenService* token_service = mock_blimp_delegate.GetTokenService();
  FakeIdentityProvider* id_provider =
      static_cast<FakeIdentityProvider*>(auth.GetIdentityProvider());

  std::string account = "mock_account";
  std::string mock_access_token = "mock_token";
  id_provider->LogIn(account);

  // Prepare refresh token.
  FakeOAuth2TokenServiceDelegate* mock_token_service_delegate =
      token_service->GetFakeOAuth2TokenServiceDelegate();
  mock_token_service_delegate->UpdateCredentials(account, "mock_refresh_token");

  // Expect delegate to show error message on non REQUEST_CANCELED errors.
  auth.Connect();
  GoogleServiceAuthError error(
      GoogleServiceAuthError::State::CONNECTION_FAILED);

  EXPECT_CALL(mock_blimp_delegate, OnAuthenticationError(error))
      .WillOnce(Return());
  token_service->IssueErrorForAllPendingRequestsForAccount(account, error);

  DCHECK_EQ(auth.Failed(), 1);
}

TEST_F(IdentitySourceTest, CheckUserName) {
  TestBlimpClientContextDelegate mock_blimp_delegate;
  MockIdentitySource auth(
      mock_blimp_delegate.CreateIdentityProvider(),
      base::Bind(&TestBlimpClientContextDelegate::OnAuthenticationError,
                 base::Unretained(&mock_blimp_delegate)),
      base::Bind(&MockIdentitySource::MockTokenCall, base::Unretained(&auth)));

  FakeIdentityProvider* id_provider =
      static_cast<FakeIdentityProvider*>(auth.GetIdentityProvider());
  std::string account = "mock_account";

  // Verify the user name before the login.
  EXPECT_EQ("", auth.GetActiveUsername());

  // Log in the mock user.
  id_provider->LogIn(account);

  // Verify that the identity source can return the correct user name.
  EXPECT_EQ(account, auth.GetActiveUsername());

  // Verify the user name after the logout.
  id_provider->LogOut();
  EXPECT_EQ("", auth.GetActiveUsername());
}

}  // namespace
}  // namespace client
}  // namespace blimp
