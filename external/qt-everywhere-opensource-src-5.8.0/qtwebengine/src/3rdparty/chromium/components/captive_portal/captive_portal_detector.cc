// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/captive_portal_detector.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_status.h"

namespace captive_portal {

const char CaptivePortalDetector::kDefaultURL[] =
    "http://www.gstatic.com/generate_204";

CaptivePortalDetector::CaptivePortalDetector(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : request_context_(request_context) {
}

CaptivePortalDetector::~CaptivePortalDetector() {
}

void CaptivePortalDetector::DetectCaptivePortal(
    const GURL& url,
    const DetectionCallback& detection_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!FetchingURL());
  DCHECK(detection_callback_.is_null());

  detection_callback_ = detection_callback;

  // The first 0 means this can use a TestURLFetcherFactory in unit tests.
  url_fetcher_ = net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetRequestContext(request_context_.get());

  // Can't safely use net::LOAD_DISABLE_CERT_REVOCATION_CHECKING here,
  // since then the connection may be reused without checking the cert.
  url_fetcher_->SetLoadFlags(
      net::LOAD_BYPASS_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

void CaptivePortalDetector::Cancel() {
  url_fetcher_.reset();
  detection_callback_.Reset();
}

void CaptivePortalDetector::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DCHECK(FetchingURL());
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(!detection_callback_.is_null());

  Results results;
  GetCaptivePortalResultFromResponse(url_fetcher_.get(), &results);
  DetectionCallback callback = detection_callback_;
  url_fetcher_.reset();
  detection_callback_.Reset();
  callback.Run(results);
}

// Takes a net::URLFetcher that has finished trying to retrieve the test
// URL, and returns a CaptivePortalService::Result based on its result.
void CaptivePortalDetector::GetCaptivePortalResultFromResponse(
    const net::URLFetcher* url_fetcher,
    Results* results) const {
  DCHECK(results);
  DCHECK(!url_fetcher->GetStatus().is_io_pending());

  results->result = captive_portal::RESULT_NO_RESPONSE;
  results->response_code = url_fetcher->GetResponseCode();
  results->retry_after_delta = base::TimeDelta();
  results->landing_url = url_fetcher->GetURL();

  VLOG(1) << "Getting captive portal result"
          << " response code: " << results->response_code
          << " landing_url: " << results->landing_url;

  // If there's a network error of some sort when fetching a file via HTTP,
  // there may be a networking problem, rather than a captive portal.
  // TODO(mmenke):  Consider special handling for redirects that end up at
  //                errors, especially SSL certificate errors.
  if (url_fetcher->GetStatus().status() != net::URLRequestStatus::SUCCESS)
    return;

  // In the case of 503 errors, look for the Retry-After header.
  if (results->response_code == 503) {
    net::HttpResponseHeaders* headers = url_fetcher->GetResponseHeaders();
    std::string retry_after_string;

    // If there's no Retry-After header, nothing else to do.
    if (!headers->EnumerateHeader(NULL, "Retry-After", &retry_after_string))
      return;

    base::TimeDelta retry_after_delta;
    if (net::HttpUtil::ParseRetryAfterHeader(retry_after_string,
                                             GetCurrentTime(),
                                             &retry_after_delta)) {
      results->retry_after_delta = retry_after_delta;
    }

    return;
  }

  // A 511 response (Network Authentication Required) means that the user needs
  // to login to whatever server issued the response.
  // See:  http://tools.ietf.org/html/rfc6585
  if (results->response_code == 511) {
    results->result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
    return;
  }

  // Other non-2xx/3xx HTTP responses may indicate server errors.
  if (results->response_code >= 400 || results->response_code < 200)
    return;

  // A 204 response code indicates there's no captive portal.
  if (results->response_code == 204) {
    results->result = captive_portal::RESULT_INTERNET_CONNECTED;
    return;
  }

  // Otherwise, assume it's a captive portal.
  results->result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
}

base::Time CaptivePortalDetector::GetCurrentTime() const {
  if (time_for_testing_.is_null())
    return base::Time::Now();
  else
    return time_for_testing_;
}

bool CaptivePortalDetector::FetchingURL() const {
  return url_fetcher_.get() != NULL;
}

}  // namespace captive_portal
