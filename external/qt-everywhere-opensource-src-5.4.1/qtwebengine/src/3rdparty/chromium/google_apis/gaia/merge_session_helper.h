// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_MERGE_SESSION_HELPER_H_
#define GOOGLE_APIS_GAIA_MERGE_SESSION_HELPER_H_

#include <deque>

#include "base/observer_list.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class GaiaAuthFetcher;
class GoogleServiceAuthError;
class OAuth2TokenService;

namespace net {
class URLRequestContextGetter;
}

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// By default instances of MergeSessionHelper delete themselves when done.
class MergeSessionHelper : public GaiaAuthConsumer,
                           public UbertokenConsumer,
                           public net::URLFetcherDelegate {
 public:
  class Observer {
   public:
    // Called whenever a merge session is completed.  The account that was
    // merged is given by |account_id|.  If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the merge succeeeded.
    virtual void MergeSessionCompleted(const std::string& account_id,
                                       const GoogleServiceAuthError& error) = 0;
   protected:
    virtual ~Observer() {}
  };

  MergeSessionHelper(OAuth2TokenService* token_service,
                     net::URLRequestContextGetter* request_context,
                     Observer* observer);
  virtual ~MergeSessionHelper();

  void LogIn(const std::string& account_id);

  // Add or remove observers of this helper.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Cancel all login requests.
  void CancelAll();

  // Signout of |account_id| given a list of accounts already signed in.
  // Since this involves signing out of all accounts and resigning back in,
  // the order which |accounts| are given is important as it will dictate
  // the sign in order. |account_id| does not have to be in |accounts|.
  void LogOut(const std::string& account_id,
              const std::vector<std::string>& accounts);

  // Signout all accounts.
  void LogOutAllAccounts();

  // Call observers when merge session completes.  This public so that callers
  // that know that a given account is already in the cookie jar can simply
  // inform the observers.
  void SignalComplete(const std::string& account_id,
                      const GoogleServiceAuthError& error);

 private:
  // Overridden from UbertokenConsumer.
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden from GaiaAuthConsumer.
  virtual void OnMergeSessionSuccess(const std::string& data) OVERRIDE;
  virtual void OnMergeSessionFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  void LogOutInternal(const std::string& account_id,
                      const std::vector<std::string>& accounts);

  // Starts the proess of fetching the uber token and performing a merge session
  // for the next account.  Virtual so that it can be overriden in tests.
  virtual void StartFetching();

  // Virtual for testing purpose.
  virtual void StartLogOutUrlFetch();

  // Start the next merge session, if needed.
  void HandleNextAccount();

  // Overridden from URLFetcherDelgate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  scoped_ptr<UbertokenFetcher> uber_token_fetcher_;

  // A worklist for this class. Accounts names are stored here if
  // we are pending a signin action for that account. Empty strings
  // represent a signout request.
  std::deque<std::string> accounts_;

  // List of observers to notify when merge session completes.
  // Makes sure list is empty on destruction.
  ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(MergeSessionHelper);
};

#endif  // GOOGLE_APIS_GAIA_MERGE_SESSION_HELPER_H_
