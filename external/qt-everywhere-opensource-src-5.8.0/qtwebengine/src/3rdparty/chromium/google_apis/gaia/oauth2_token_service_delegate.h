// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"

namespace net {
class URLRequestContextGetter;
}

class SigninClient;

// Abstract base class to fetch and maintain refresh tokens from various
// entities. Concrete subclasses should implement RefreshTokenIsAvailable and
// CreateAccessTokenFetcher properly.
class OAuth2TokenServiceDelegate {
 public:
  OAuth2TokenServiceDelegate();
  virtual ~OAuth2TokenServiceDelegate();

  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) = 0;

  virtual bool RefreshTokenIsAvailable(const std::string& account_id) const = 0;
  virtual bool RefreshTokenHasError(const std::string& account_id) const;
  virtual void UpdateAuthError(const std::string& account_id,
                               const GoogleServiceAuthError& error) {}

  virtual std::vector<std::string> GetAccounts();
  virtual void RevokeAllCredentials(){};

  virtual void InvalidateAccessToken(const std::string& account_id,
                                     const std::string& client_id,
                                     const std::set<std::string>& scopes,
                                     const std::string& access_token) {}

  virtual void Shutdown() {}
  virtual void LoadCredentials(const std::string& primary_account_id) {}
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token) {}
  virtual void RevokeCredentials(const std::string& account_id) {}
  virtual net::URLRequestContextGetter* GetRequestContext() const;

  bool ValidateAccountId(const std::string& account_id) const;

  // Add or remove observers of this token service.
  void AddObserver(OAuth2TokenService::Observer* observer);
  void RemoveObserver(OAuth2TokenService::Observer* observer);

  // Returns a pointer to its instance of net::BackoffEntry if it has one, or
  // a nullptr otherwise.
  virtual const net::BackoffEntry* BackoffEntry() const;

 protected:
  // Called by subclasses to notify observers.
  virtual void FireRefreshTokenAvailable(const std::string& account_id);
  virtual void FireRefreshTokenRevoked(const std::string& account_id);
  virtual void FireRefreshTokensLoaded();

  // Helper class to scope batch changes.
  class ScopedBatchChange {
   public:
    explicit ScopedBatchChange(OAuth2TokenServiceDelegate* delegate);
    ~ScopedBatchChange();

   private:
    OAuth2TokenServiceDelegate* delegate_;  // Weak.
    DISALLOW_COPY_AND_ASSIGN(ScopedBatchChange);
  };

  // This function is called by derived classes to help implement
  // RefreshTokenHasError().  It centralizes the code for determining if
  // |error| is worthy of being reported as an error for purposes of
  // RefreshTokenHasError().
  static bool IsError(const GoogleServiceAuthError& error);

 private:
  // List of observers to notify when refresh token availability changes.
  // Makes sure list is empty on destruction.
  base::ObserverList<OAuth2TokenService::Observer, true> observer_list_;

  void StartBatchChanges();
  void EndBatchChanges();

  // The depth of batch changes.
  int batch_change_depth_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceDelegate);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
