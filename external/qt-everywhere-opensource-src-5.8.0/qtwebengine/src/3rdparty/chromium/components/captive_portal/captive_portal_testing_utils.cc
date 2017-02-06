// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/captive_portal_testing_utils.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace {

scoped_refptr<net::HttpResponseHeaders> CreateResponseHeaders(
    const std::string& response_headers) {
  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      response_headers.c_str(), static_cast<int>(response_headers.length()));
  return new net::HttpResponseHeaders(raw_headers);
}

}  // namespace

namespace captive_portal {

CaptivePortalDetectorTestBase::CaptivePortalDetectorTestBase()
    : detector_(NULL) {
}

CaptivePortalDetectorTestBase::~CaptivePortalDetectorTestBase() {
}

void CaptivePortalDetectorTestBase::SetTime(const base::Time& time) {
  detector()->set_time_for_testing(time);
}

void CaptivePortalDetectorTestBase::AdvanceTime(const base::TimeDelta& delta) {
  detector()->advance_time_for_testing(delta);
}

bool CaptivePortalDetectorTestBase::FetchingURL() {
  return detector()->FetchingURL();
}

void CaptivePortalDetectorTestBase::CompleteURLFetch(
    int net_error,
    int status_code,
    const char* response_headers) {
  if (net_error != net::OK) {
    DCHECK(!response_headers);
    fetcher()->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                net_error));
  } else {
    fetcher()->set_response_code(status_code);
    if (response_headers) {
      scoped_refptr<net::HttpResponseHeaders> headers(
          CreateResponseHeaders(response_headers));
      DCHECK_EQ(status_code, headers->response_code());
      fetcher()->set_response_headers(headers);
    }
  }
  detector()->OnURLFetchComplete(fetcher());
}

}  // namespace captive_portal
