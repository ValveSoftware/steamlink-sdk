// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_header_adapter.h"

#include "net/http/http_response_headers.h"

namespace net {

URLRequestThrottlerHeaderAdapter::URLRequestThrottlerHeaderAdapter(
    HttpResponseHeaders* headers)
    : response_header_(headers) {
}

URLRequestThrottlerHeaderAdapter::~URLRequestThrottlerHeaderAdapter() {}

std::string URLRequestThrottlerHeaderAdapter::GetNormalizedValue(
    const std::string& key) const {
  std::string return_value;
  response_header_->GetNormalizedHeader(key, &return_value);
  return return_value;
}

int URLRequestThrottlerHeaderAdapter::GetResponseCode() const {
  return response_header_->response_code();
}

}  // namespace net
