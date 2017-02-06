// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_ACCOUNT_TRACKER_H_
#define GOOGLE_APIS_GAIA_ACCOUNT_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/identity_provider.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GoogleServiceAuthError;

namespace net {
class URLRequestContextGetter;
}

namespace gaia {

struct AccountIds {
  std::string account_key;  // The account ID used by OAuth2TokenService.
  std::string gaia;
  std::string email;
};

class AccountIdFetcher;

// The AccountTracker keeps track of what accounts exist on the
// profile and the state of their credentials. The tracker fetches the
// gaia ID of each account it knows about.
//
// The AccountTracker maintains these invariants:
// 1. Events are only fired after the gaia ID has been fetched.
// 2. Add/Remove and SignIn/SignOut pairs are always generated in order.
// 3. SignIn follows Add, and there will be a SignOut between SignIn & Remove.
// 4. If there is no primary account, there are no other accounts.
class AccountTracker : public OAuth2TokenService::Observer,
                       public IdentityProvider::Observer {
 public:
  AccountTracker(IdentityProvider* identity_provider,
                 net::URLRequestContextGetter* request_context_getter);
  ~AccountTracker() override;

  class Observer {
   public:
    virtual void OnAccountAdded(const AccountIds& ids) = 0;
    virtual void OnAccountRemoved(const AccountIds& ids) = 0;
    virtual void OnAccountSignInChanged(const AccountIds& ids,
                                        bool is_signed_in) = 0;
  };

  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the list of accounts that are signed in, and for which gaia IDs
  // have been fetched. The primary account for the profile will be first
  // in the vector. Additional accounts will be in order of their gaia IDs.
  std::vector<AccountIds> GetAccounts() const;
  AccountIds FindAccountIdsByGaiaId(const std::string& gaia_id);

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_key) override;
  void OnRefreshTokenRevoked(const std::string& account_key) override;

  void OnUserInfoFetchSuccess(AccountIdFetcher* fetcher,
                              const std::string& gaia_id);
  void OnUserInfoFetchFailure(AccountIdFetcher* fetcher);

  // IdentityProvider::Observer implementation.
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // Sets the state of an account. Does not fire notifications.
  void SetAccountStateForTest(AccountIds ids, bool is_signed_in);

  IdentityProvider* identity_provider() { return identity_provider_; }

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfininshed fetchers.
  virtual bool IsAllUserInfoFetched() const;

 private:
  struct AccountState {
    AccountIds ids;
    bool is_signed_in;
  };

  void NotifyAccountAdded(const AccountState& account);
  void NotifyAccountRemoved(const AccountState& account);
  void NotifySignInChanged(const AccountState& account);

  void UpdateSignInState(const std::string& account_key, bool is_signed_in);

  void StartTrackingAccount(const std::string& account_key);

  // Note: |account_key| is passed by value here, because the original
  // object may be stored in |accounts_| and if so, it will be destroyed
  // after erasing the key from the map.
  void StopTrackingAccount(const std::string account_key);

  void StopTrackingAllAccounts();
  void StartFetchingUserInfo(const std::string& account_key);
  void DeleteFetcher(AccountIdFetcher* fetcher);

  IdentityProvider* identity_provider_;  // Not owned.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::map<std::string, AccountIdFetcher*> user_info_requests_;
  std::map<std::string, AccountState> accounts_;
  base::ObserverList<Observer> observer_list_;
  bool shutdown_called_;
};

class AccountIdFetcher : public OAuth2TokenService::Consumer,
                         public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountIdFetcher(OAuth2TokenService* token_service,
                   net::URLRequestContextGetter* request_context_getter,
                   AccountTracker* tracker,
                   const std::string& account_key);
  ~AccountIdFetcher() override;

  const std::string& account_key() { return account_key_; }

  void Start();

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserIdResponse(const std::string& gaia_id) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_getter_;
  AccountTracker* tracker_;
  const std::string account_key_;

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
};

}  // namespace extensions

#endif  // GOOGLE_APIS_GAIA_ACCOUNT_TRACKER_H_
