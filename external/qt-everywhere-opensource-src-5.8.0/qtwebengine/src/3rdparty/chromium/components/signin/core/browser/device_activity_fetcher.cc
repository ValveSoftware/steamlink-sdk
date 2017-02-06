// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/device_activity_fetcher.h"

#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

namespace {

// TODO(mlerman,anthonyvd): Replace the three parameters following with the
// necessary parameters for calling the ListDevices endpoint, once live.
// crbug.com/463611 for details!
const char kSyncListDevicesScope[] =
    "https://www.googleapis.com/auth/chromesynclistdevices";
const char kChromeDomain[] = "http://www.chrome.com";
const char kListDevicesEndpoint[] = "http://127.0.0.1";
// Template for optional authorization header when using an OAuth access token.
const char kAuthorizationHeader[] = "Authorization: Bearer %s";

// In case of an error while fetching using the GaiaAuthFetcher, retry with
// exponential backoff. Try up to 9 times within 10 minutes.
const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,
    // Initial delay for exponential backoff in ms.
    1000,
    // Factor by which the waiting time will be multiplied.
    2,
    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,  // 10%
    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 15,  // 15 minutes.
    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,
    // Don't use initial delay unless the last request was an error.
    false,
};

const int kMaxFetcherRetries = 9;

}  // namespace

DeviceActivityFetcher::DeviceActivityFetcher(
    SigninClient* signin_client,
    DeviceActivityFetcher::Observer* observer)
    : fetcher_backoff_(&kBackoffPolicy),
      fetcher_retries_(0),
      signin_client_(signin_client),
      observer_(observer) {
}

DeviceActivityFetcher::~DeviceActivityFetcher() {
}

void DeviceActivityFetcher::Start() {
  fetcher_retries_ = 0;
  login_hint_ = std::string();
  StartFetchingListIdpSessions();
}

void DeviceActivityFetcher::Stop() {
  if (gaia_auth_fetcher_)
    gaia_auth_fetcher_->CancelRequest();
  if (url_fetcher_)
    url_fetcher_.reset();
}

void DeviceActivityFetcher::StartFetchingListIdpSessions() {
  gaia_auth_fetcher_.reset(signin_client_->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource,
      signin_client_->GetURLRequestContext()));
  gaia_auth_fetcher_->StartListIDPSessions(kSyncListDevicesScope,
                                           kChromeDomain);
}

void DeviceActivityFetcher::StartFetchingGetTokenResponse() {
  gaia_auth_fetcher_.reset(signin_client_->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource,
      signin_client_->GetURLRequestContext()));
  gaia_auth_fetcher_->StartGetTokenResponse(kSyncListDevicesScope,
                                            kChromeDomain, login_hint_);
}

void DeviceActivityFetcher::StartFetchingListDevices() {
  // Call the Sync Endpoint.
  url_fetcher_ = net::URLFetcher::Create(GURL(kListDevicesEndpoint),
                                         net::URLFetcher::GET, this);
  url_fetcher_->SetRequestContext(signin_client_->GetURLRequestContext());
  if (!access_token_.empty()) {
    url_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(kAuthorizationHeader, access_token_.c_str()));
  }
  url_fetcher_->Start();
}

void DeviceActivityFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(mlerman): Uncomment the code below once we have a working proto.
  // Work done under crbug.com/463611

  // std::string response_string;
  // ListDevices list_devices;

  // if (!source->GetStatus().is_success()) {
  //   VLOG(1) << "Failed to fetch listdevices response. Retrying.";
  //   if (++fetcher_retries_ < kMaxFetcherRetries) {
  //     fetcher_backoff_.InformOfRequest(false);
  //     fetcher_timer_.Start(
  //         FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(), this,
  //         &DeviceActivityFetcher::StartFetchingListDevices);
  //     return;
  //   } else {
  //     observer_->OnFetchDeviceActivityFailure();
  //     return;
  //   }
  // }

  // net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
  //     source->GetResponseCode());
  // if (response_status == net::HTTP_BAD_REQUEST ||
  //     response_status == net::HTTP_UNAUTHORIZED) {
  //   // BAD_REQUEST indicates that the request was malformed.
  //   // UNAUTHORIZED indicates that security token didn't match the id.
  //   VLOG(1) << "No point retrying the checkin with status: "
  //              << response_status << ". Checkin failed.";
  //   CheckinRequestStatus status = response_status == net::HTTP_BAD_REQUEST ?
  //       HTTP_BAD_REQUEST : HTTP_UNAUTHORIZED;
  //   RecordCheckinStatusAndReportUMA(status, recorder_, false);
  //   callback_.Run(response_proto);
  //   return;
  // }

  // if (response_status != net::HTTP_OK ||
  //     !source->GetResponseAsString(&response_string) ||
  //     !list_devices.ParseFromString(response_string)) {
  //   LOG(ERROR) << "Failed to get list devices response. HTTP Status: "
  //              << response_status;
  //   if (++fetcher_retries_ < kMaxFetcherRetries) {
  //     fetcher_backoff_.InformOfRequest(false);
  //     fetcher_timer_.Start(
  //         FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(), this,
  //         &DeviceActivityFetcher::StartFetchingListDevices);
  //     return;
  //   } else {
  //     observer_->OnFetchDeviceActivityFailure();
  //     return;
  //   }
  // }

  std::vector<DeviceActivity> devices;
  // TODO(mlerman): Fill |devices| from the proto in |source|.

  // Call this last as OnFetchDeviceActivitySuccess will delete |this|.
  observer_->OnFetchDeviceActivitySuccess(devices);
}

void DeviceActivityFetcher::OnListIdpSessionsSuccess(
    const std::string& login_hint) {
  fetcher_backoff_.InformOfRequest(true);
  login_hint_ = login_hint;
  access_token_ = std::string();
  StartFetchingGetTokenResponse();
}

void DeviceActivityFetcher::OnListIdpSessionsError(
    const GoogleServiceAuthError& error) {
  if (++fetcher_retries_ < kMaxFetcherRetries && error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
                         this,
                         &DeviceActivityFetcher::StartFetchingListIdpSessions);
    return;
  }
  observer_->OnFetchDeviceActivityFailure();
}

void DeviceActivityFetcher::OnGetTokenResponseSuccess(
    const ClientOAuthResult& result) {
  fetcher_backoff_.InformOfRequest(true);
  access_token_ = result.access_token;
  StartFetchingListDevices();
}

void DeviceActivityFetcher::OnGetTokenResponseError(
    const GoogleServiceAuthError& error) {
  if (++fetcher_retries_ < kMaxFetcherRetries && error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
                         this,
                         &DeviceActivityFetcher::StartFetchingGetTokenResponse);
    return;
  }
  observer_->OnFetchDeviceActivityFailure();
}
