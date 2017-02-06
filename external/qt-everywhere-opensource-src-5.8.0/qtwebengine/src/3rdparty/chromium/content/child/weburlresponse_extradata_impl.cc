// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/weburlresponse_extradata_impl.h"

namespace content {

WebURLResponseExtraDataImpl::WebURLResponseExtraDataImpl(
    const std::string& npn_negotiated_protocol)
    : npn_negotiated_protocol_(npn_negotiated_protocol),
      is_ftp_directory_listing_(false),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      effective_connection_type_(
          net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

WebURLResponseExtraDataImpl::~WebURLResponseExtraDataImpl() {
}

}  // namespace content
