// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "net/base/backoff_entry.h"

// ProfileOAuth2TokenService is a KeyedService that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh tokens.
//
// See |OAuth2TokenService| for usage details.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use OAuth2TokenServiceRequest.
class ProfileOAuth2TokenService : public OAuth2TokenService,
                                  public OAuth2TokenService::Observer,
                                  public KeyedService {
 public:
  ProfileOAuth2TokenService(OAuth2TokenServiceDelegate* delegate);
  ~ProfileOAuth2TokenService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // Only call this method if there is at least one account connected to the
  // profile, otherwise startup will cause unneeded work on the IO thread. The
  // primary account is specified with the |primary_account_id| argument. If
  // empty, no credentials will be loaded. For a regular profile, the primary
  // account id comes from SigninManager. For a supervised user, the id comes
  // from SupervisedUserService.
  virtual void LoadCredentials(const std::string& primary_account_id);

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and available through |LoadCredentials| after service is restarted.
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token);

  virtual void RevokeCredentials(const std::string& account_id);

  // Returns a pointer to its instance of net::BackoffEntry or nullptr if there
  // is no such instance.
  const net::BackoffEntry* GetDelegateBackoffEntry();

 private:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
