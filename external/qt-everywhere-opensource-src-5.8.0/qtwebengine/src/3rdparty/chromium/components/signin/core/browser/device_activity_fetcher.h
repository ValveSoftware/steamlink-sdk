// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DEVICE_ACTVITIY_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DEVICE_ACTVITIY_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

class GaiaAuthFetcher;
class SigninClient;

namespace net {
class URLFetcher;
}

class DeviceActivityFetcher : public GaiaAuthConsumer,
                              public net::URLFetcherDelegate {
 public:
  struct DeviceActivity {
    base::Time last_active;
    std::string name;
  };

  class Observer {
   public:
    virtual void OnFetchDeviceActivitySuccess(
        const std::vector<DeviceActivityFetcher::DeviceActivity>& devices) = 0;
    virtual void OnFetchDeviceActivityFailure() {}
  };

  DeviceActivityFetcher(SigninClient* signin_client,
                        DeviceActivityFetcher::Observer* observer);
  ~DeviceActivityFetcher() override;

  void Start();
  void Stop();

  // GaiaAuthConsumer:
  void OnListIdpSessionsSuccess(const std::string& login_hint) override;
  void OnListIdpSessionsError(const GoogleServiceAuthError& error) override;
  void OnGetTokenResponseSuccess(const ClientOAuthResult& result) override;
  void OnGetTokenResponseError(const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  void StartFetchingListIdpSessions();
  void StartFetchingGetTokenResponse();
  void StartFetchingListDevices();

  // Gaia fetcher used for acquiring an access token.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  // URL Fetcher used for calling List Devices.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // If either fetcher fails, retry with exponential backoff.
  net::BackoffEntry fetcher_backoff_;
  base::OneShotTimer fetcher_timer_;
  int fetcher_retries_;

  std::string access_token_;
  std::string login_hint_;

  SigninClient* signin_client_;  // Weak pointer.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceActivityFetcher);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DEVICE_ACTVITIY_FETCHER_H_
