// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/refresh_token_annotation_request.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

void RecordRequestStatusHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Signin.RefreshTokenAnnotationRequest", success);
}
}

RefreshTokenAnnotationRequest::RefreshTokenAnnotationRequest(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const std::string& product_version,
    const std::string& device_id,
    const std::string& client_id,
    const base::Closure& request_callback)
    : OAuth2TokenService::Consumer("refresh_token_annotation"),
      request_context_getter_(request_context_getter),
      product_version_(product_version),
      device_id_(device_id),
      client_id_(client_id),
      request_callback_(request_callback) {
}

RefreshTokenAnnotationRequest::~RefreshTokenAnnotationRequest() {
  DCHECK(CalledOnValidThread());
}

// static
std::unique_ptr<RefreshTokenAnnotationRequest>
RefreshTokenAnnotationRequest::SendIfNeeded(
    PrefService* pref_service,
    OAuth2TokenService* token_service,
    SigninClient* signin_client,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const std::string& account_id,
    const base::Closure& request_callback) {
  std::unique_ptr<RefreshTokenAnnotationRequest> request;

  if (!ShouldSendNow(pref_service))
    return request;

  // Don't send request if device_id is disabled.
  std::string device_id = signin_client->GetSigninScopedDeviceId();
  if (device_id.empty())
    return request;

  request.reset(new RefreshTokenAnnotationRequest(
      request_context_getter, signin_client->GetProductVersion(), device_id,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), request_callback));
  request->RequestAccessToken(token_service, account_id);
  return request;
}

// static
bool RefreshTokenAnnotationRequest::ShouldSendNow(PrefService* pref_service) {
  bool should_send_now = false;
  // Read scheduled time from prefs.
  base::Time scheduled_time =
      base::Time::FromInternalValue(pref_service->GetInt64(
          prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime));

  if (!scheduled_time.is_null() && scheduled_time <= base::Time::Now()) {
    DVLOG(2) << "Sending RefreshTokenAnnotationRequest";
    should_send_now = true;
    // Reset scheduled_time so that it gets rescheduled later in the function.
    scheduled_time = base::Time();
  }

  if (scheduled_time.is_null()) {
    // Random delay in interval [0, 20] days.
    double delay_in_sec = base::RandDouble() * 20.0 * 24.0 * 3600.0;
    scheduled_time =
        base::Time::Now() + base::TimeDelta::FromSecondsD(delay_in_sec);
    DVLOG(2) << "RefreshTokenAnnotationRequest scheduled for "
             << scheduled_time;
    pref_service->SetInt64(
        prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime,
        scheduled_time.ToInternalValue());
  }
  return should_send_now;
}

void RefreshTokenAnnotationRequest::RequestAccessToken(
    OAuth2TokenService* token_service,
    const std::string& account_id) {
  DCHECK(CalledOnValidThread());
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  access_token_request_ = token_service->StartRequest(account_id, scopes, this);
}

void RefreshTokenAnnotationRequest::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Got access token";
  Start(request_context_getter_.get(), access_token);
}

void RefreshTokenAnnotationRequest::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Failed to get access token";
  RecordRequestStatusHistogram(false);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, request_callback_);
  request_callback_.Reset();
}

GURL RefreshTokenAnnotationRequest::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_issue_token_url();
}

std::string RefreshTokenAnnotationRequest::CreateApiCallBody() {
  // response_type=none means we don't want any token back, just record that
  // this request was sent.
  const char kIssueTokenBodyFormat[] =
      "force=true"
      "&response_type=none"
      "&scope=%s"
      "&client_id=%s"
      "&device_id=%s"
      "&device_type=chrome"
      "&lib_ver=%s";

  // It doesn't matter which scope to use for IssueToken request, any common
  // scope would do. In this case I'm using "userinfo.email".
  return base::StringPrintf(
      kIssueTokenBodyFormat,
      net::EscapeUrlEncodedData(GaiaConstants::kGoogleUserInfoEmail, true)
          .c_str(),
      net::EscapeUrlEncodedData(client_id_, true).c_str(),
      net::EscapeUrlEncodedData(device_id_, true).c_str(),
      net::EscapeUrlEncodedData(product_version_, true).c_str());
}

void RefreshTokenAnnotationRequest::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Request succeeded";
  RecordRequestStatusHistogram(true);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, request_callback_);
  request_callback_.Reset();
}

void RefreshTokenAnnotationRequest::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Request failed";
  RecordRequestStatusHistogram(false);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, request_callback_);
  request_callback_.Reset();
}
