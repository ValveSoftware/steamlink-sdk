// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple implementation of a data: protocol handler.

#include "net/url_request/url_request_data_job.h"

#include "net/base/data_url.h"
#include "net/base/net_errors.h"

namespace net {

URLRequestDataJob::URLRequestDataJob(
    URLRequest* request, NetworkDelegate* network_delegate)
    : URLRequestSimpleJob(request, network_delegate) {
}

int URLRequestDataJob::GetData(std::string* mime_type,
                               std::string* charset,
                               std::string* data,
                               const CompletionCallback& callback) const {
  // Check if data URL is valid. If not, don't bother to try to extract data.
  // Otherwise, parse the data from the data URL.
  const GURL& url = request_->url();
  if (!url.is_valid())
    return ERR_INVALID_URL;
  return DataURL::Parse(url, mime_type, charset, data)? OK: ERR_INVALID_URL;
}

URLRequestDataJob::~URLRequestDataJob() {
}

}  // namespace net
