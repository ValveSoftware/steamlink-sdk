// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_request_info.h"

namespace content {

SSLRequestInfo::SSLRequestInfo(const GURL& url,
                               ResourceType resource_type,
                               int ssl_cert_id,
                               net::CertStatus ssl_cert_status)
    : url_(url),
      resource_type_(resource_type),
      ssl_cert_id_(ssl_cert_id),
      ssl_cert_status_(ssl_cert_status) {
}

SSLRequestInfo::~SSLRequestInfo() {}

}  // namespace content
