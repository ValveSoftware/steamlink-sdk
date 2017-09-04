// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_IDENTITY_SOURCE_H_
#define BLIMP_CLIENT_CORE_SESSION_IDENTITY_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "google_apis/gaia/identity_provider.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace blimp {
namespace client {

// IdentitySource handles OAuth2 token request and forward user sign in state
// change to Blimp with ProfileIdentityProvider.
class IdentitySource : public OAuth2TokenService::Consumer,
                       public OAuth2TokenService::Observer,
                       public IdentityProvider::Observer {
 public:
  using TokenCallback = base::Callback<void(const std::string&)>;
  using TokenErrorCallback =
      base::Callback<void(const GoogleServiceAuthError&)>;

  explicit IdentitySource(std::unique_ptr<IdentityProvider> identity_provider,
                          const TokenErrorCallback& error_callback,
                          const TokenCallback& callback);
  ~IdentitySource() override;

  // Start Blimp authentication by requesting OAuth2 token from Google.
  // Duplicate connect calls during token fetching will be ignored.
  void Connect();

  // Returns the account name for the current user.
  std::string GetActiveUsername();

  // Add sign in state observer.
  void AddObserver(IdentityProvider::Observer* observer);

  // Remove sign in state observer.
  void RemoveObserver(IdentityProvider::Observer* observer);

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;

 protected:
  // Provide OAuth2 token service and listen to Google account sign in state,
  // protected for testing.
  std::unique_ptr<IdentityProvider> identity_provider_;

 private:
  // Fetch OAuth token.
  void FetchAuthToken();

  // OAuth2 token request. The request is created when we start to fetch the
  // access token in OAuth2TokenService::StartRequest, and destroyed when
  // OAuth2TokenService::Consumer callback get called.
  std::unique_ptr<OAuth2TokenService::Request> token_request_;

  TokenErrorCallback error_callback_;

  // Callback after OAuth2 token is fetched.
  TokenCallback token_callback_;

  // If we are fetching OAuth2 token. Connect call during token fetching will
  // be ignored.
  bool is_fetching_token_;

  // Account id of current active user, used during token request.
  std::string account_id_;

  // Current retry count due to request cancellation.
  int retry_times_;

  DISALLOW_COPY_AND_ASSIGN(IdentitySource);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SESSION_IDENTITY_SOURCE_H_
