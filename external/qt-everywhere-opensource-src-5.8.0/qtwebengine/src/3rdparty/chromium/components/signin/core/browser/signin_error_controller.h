// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_

#include <set>
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

// Keep track of auth errors and expose them to observers in the UI. Services
// that wish to expose auth errors to the user should register an
// AuthStatusProvider to report their current authentication state, and should
// invoke AuthStatusChanged() when their authentication state may have changed.
class SigninErrorController : public KeyedService {
 public:
  class AuthStatusProvider {
   public:
    AuthStatusProvider();
    virtual ~AuthStatusProvider();

    // Returns the account id with the status specified by GetAuthStatus().
    virtual std::string GetAccountId() const = 0;

    // API invoked by SigninErrorController to get the current auth status of
    // the various signed in services.
    virtual GoogleServiceAuthError GetAuthStatus() const = 0;
  };

  // The observer class for SigninErrorController lets the controller notify
  // observers when an error arises or changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnErrorChanged() = 0;
  };

  SigninErrorController();
  ~SigninErrorController() override;

  // Adds a provider which the SigninErrorController object will start querying
  // for auth status.
  void AddProvider(const AuthStatusProvider* provider);

  // Removes a provider previously added by SigninErrorController (generally
  // only called in preparation for shutdown).
  void RemoveProvider(const AuthStatusProvider* provider);

  // Invoked when the auth status of an AuthStatusProvider has changed.
  void AuthStatusChanged();

  // True if there exists an error worth elevating to the user.
  bool HasError() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::string& error_account_id() const { return error_account_id_; }
  const GoogleServiceAuthError& auth_error() const { return auth_error_; }

 private:
  std::set<const AuthStatusProvider*> provider_set_;

  // The account that generated the last auth error.
  std::string error_account_id_;

  // The auth error detected the last time AuthStatusChanged() was invoked (or
  // NONE if AuthStatusChanged() has never been invoked).
  GoogleServiceAuthError auth_error_;

  base::ObserverList<Observer, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorController);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
