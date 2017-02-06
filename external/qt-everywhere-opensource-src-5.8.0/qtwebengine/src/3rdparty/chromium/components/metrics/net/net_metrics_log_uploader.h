// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/metrics/metrics_log_uploader.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace metrics {

// Implementation of MetricsLogUploader using the Chrome network stack.
class NetMetricsLogUploader : public MetricsLogUploader,
                              public net::URLFetcherDelegate {
 public:
  // Constructs a NetMetricsLogUploader with the specified request context and
  // other params (see comments on MetricsLogUploader for details). The caller
  // must ensure that |request_context_getter| remains valid for the lifetime
  // of this class.
  NetMetricsLogUploader(net::URLRequestContextGetter* request_context_getter,
                        const std::string& server_url,
                        const std::string& mime_type,
                        const base::Callback<void(int)>& on_upload_complete);
  ~NetMetricsLogUploader() override;

  // MetricsLogUploader:
  void UploadLog(const std::string& compressed_log_data,
                 const std::string& log_hash) override;

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // The request context for fetches done using the network stack.
  net::URLRequestContextGetter* const request_context_getter_;

  // The outstanding transmission appears as a URL Fetch operation.
  std::unique_ptr<net::URLFetcher> current_fetch_;

  DISALLOW_COPY_AND_ASSIGN(NetMetricsLogUploader);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_
