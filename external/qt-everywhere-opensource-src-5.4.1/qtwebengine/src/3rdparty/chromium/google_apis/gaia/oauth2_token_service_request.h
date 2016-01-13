// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_REQUEST_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_REQUEST_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "google_apis/gaia/oauth2_token_service.h"

// OAuth2TokenServiceRequest represents an asynchronous request to an
// OAuth2TokenService that may live in another thread.
//
// An OAuth2TokenServiceRequest can be created and started from any thread.
class OAuth2TokenServiceRequest : public OAuth2TokenService::Request,
                                  public base::NonThreadSafe {
 public:
  class Core;

  // Interface for providing an OAuth2TokenService.
  class TokenServiceProvider {
   public:
    TokenServiceProvider();
    virtual ~TokenServiceProvider();

    // Returns the task runner on which the token service lives.
    //
    // This method may be called from any thread.
    virtual scoped_refptr<base::SingleThreadTaskRunner>
        GetTokenServiceTaskRunner() = 0;

    // Returns a pointer to a token service.
    //
    // Caller does not own the token service and must not delete it.  The token
    // service must outlive all instances of OAuth2TokenServiceRequest.
    //
    // This method may only be called from the task runner returned by
    // |GetTokenServiceTaskRunner|.
    virtual OAuth2TokenService* GetTokenService() = 0;
  };

  // Creates and starts an access token request for |account_id| and |scopes|.
  //
  // |provider| is used to get the OAuth2TokenService and must outlive the
  // returned request object.
  //
  // |account_id| must not be empty.
  //
  // |scopes| must not be empty.
  //
  // |consumer| will be invoked in the same thread that invoked CreateAndStart
  // and must outlive the returned request object.  Destroying the request
  // object ensure that |consumer| will not be called.  However, the actual
  // network activities may not be canceled and the cache in OAuth2TokenService
  // may be populated with the fetched results.
  static scoped_ptr<OAuth2TokenServiceRequest> CreateAndStart(
      TokenServiceProvider* provider,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // Invalidates |access_token| for |account_id| and |scopes|.
  //
  // |provider| is used to get the OAuth2TokenService and must outlive the
  // returned request object.
  //
  // |account_id| must not be empty.
  //
  // |scopes| must not be empty.
  static void InvalidateToken(TokenServiceProvider* provider,
                              const std::string& account_id,
                              const OAuth2TokenService::ScopeSet& scopes,
                              const std::string& access_token);

  virtual ~OAuth2TokenServiceRequest();

  // OAuth2TokenService::Request.
  virtual std::string GetAccountId() const OVERRIDE;

 private:
  OAuth2TokenServiceRequest(const std::string& account_id);

  void StartWithCore(const scoped_refptr<Core>& core);

  const std::string account_id_;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceRequest);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_REQUEST_H_
