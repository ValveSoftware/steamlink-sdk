// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

MetricsLogUploader::MetricsLogUploader(
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete)
    : server_url_(server_url),
      mime_type_(mime_type),
      on_upload_complete_(on_upload_complete) {
}

MetricsLogUploader::~MetricsLogUploader() {
}

}  // namespace metrics
