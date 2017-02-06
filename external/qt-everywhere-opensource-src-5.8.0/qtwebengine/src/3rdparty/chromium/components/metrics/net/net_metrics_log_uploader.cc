// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/metrics/histogram_macros.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    net::URLRequestContextGetter* request_context_getter,
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete)
    : MetricsLogUploader(server_url, mime_type, on_upload_complete),
      request_context_getter_(request_context_getter) {
}

NetMetricsLogUploader::~NetMetricsLogUploader() {
}

void NetMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                      const std::string& log_hash) {
  current_fetch_ =
      net::URLFetcher::Create(GURL(server_url_), net::URLFetcher::POST, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      current_fetch_.get(), data_use_measurement::DataUseUserData::UMA);
  current_fetch_->SetRequestContext(request_context_getter_);
  current_fetch_->SetUploadData(mime_type_, compressed_log_data);

  // Tell the server that we're uploading gzipped protobufs.
  current_fetch_->SetExtraRequestHeaders("content-encoding: gzip");

  DCHECK(!log_hash.empty());
  current_fetch_->AddExtraRequestHeader("X-Chrome-UMA-Log-SHA1: " + log_hash);

  // We already drop cookies server-side, but we might as well strip them out
  // client-side as well.
  current_fetch_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_COOKIES);
  current_fetch_->Start();
}

void NetMetricsLogUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  // We're not allowed to re-use the existing |URLFetcher|s, so free them here.
  // Note however that |source| is aliased to the fetcher, so we should be
  // careful not to delete it too early.
  DCHECK_EQ(current_fetch_.get(), source);

  int response_code = source->GetResponseCode();
  if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID)
    response_code = -1;
  current_fetch_.reset();
  on_upload_complete_.Run(response_code);
}

}  // namespace metrics
