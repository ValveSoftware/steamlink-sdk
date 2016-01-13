// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_UBERTOKEN_FETCHER_H_
#define GOOGLE_APIS_GAIA_UBERTOKEN_FETCHER_H_

#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/oauth2_token_service.h"

// Allow to retrieves an uber-auth token for the user. This class uses the
// |OAuth2TokenService| and considers that the user is already logged in. It
// will use the OAuth2 access token to generate the uber-auth token.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time.

class GaiaAuthFetcher;
class GoogleServiceAuthError;

namespace net {
class URLRequestContextGetter;
}

// Callback for the |UbertokenFetcher| class.
class UbertokenConsumer {
 public:
  UbertokenConsumer() {}
  virtual ~UbertokenConsumer() {}
  virtual void OnUbertokenSuccess(const std::string& token) {}
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) {}
};

// Allows to retrieve an uber-auth token.
class UbertokenFetcher : public GaiaAuthConsumer,
                         public OAuth2TokenService::Consumer {
 public:
  UbertokenFetcher(OAuth2TokenService* token_service,
                   UbertokenConsumer* consumer,
                   net::URLRequestContextGetter* request_context);
  virtual ~UbertokenFetcher();

  // Start fetching the token for |account_id|.
  virtual void StartFetchingToken(const std::string& account_id);

  // Overriden from GaiaAuthConsumer
  virtual void OnUberAuthTokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUberAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from OAuth2TokenService::Consumer:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  OAuth2TokenService* token_service_;
  UbertokenConsumer* consumer_;
  net::URLRequestContextGetter* request_context_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(UbertokenFetcher);
};

#endif  // GOOGLE_APIS_GAIA_UBERTOKEN_FETCHER_H_
