// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"

namespace metrics {

// MetricsLogUploader is an abstract base class for uploading UMA logs on behalf
// of MetricsService.
class MetricsLogUploader {
 public:
  // Constructs the uploader that will upload logs to the specified |server_url|
  // with the given |mime_type|. The |on_upload_complete| callback will be
  // called with the HTTP response code of the upload or with -1 on an error.
  MetricsLogUploader(const std::string& server_url,
                     const std::string& mime_type,
                     const base::Callback<void(int)>& on_upload_complete);
  virtual ~MetricsLogUploader();

  // Uploads a log with the specified |compressed_log_data| and |log_hash|.
  // |log_hash| is expected to be the hex-encoded SHA1 hash of the log data
  // before compression.
  virtual void UploadLog(const std::string& compressed_log_data,
                         const std::string& log_hash) = 0;

 protected:
  const std::string server_url_;
  const std::string mime_type_;
  const base::Callback<void(int)> on_upload_complete_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsLogUploader);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_
