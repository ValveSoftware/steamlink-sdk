// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class NetMetricsLogUploaderTest : public testing::Test {
 public:
  NetMetricsLogUploaderTest() : on_upload_complete_count_(0) {
  }

  void CreateAndOnUploadCompleteReuseUploader() {
    uploader_.reset(new NetMetricsLogUploader(
        NULL, "http://dummy_server", "dummy_mime",
        base::Bind(&NetMetricsLogUploaderTest::OnUploadCompleteReuseUploader,
                   base::Unretained(this))));
    uploader_->UploadLog("initial_dummy_data", "initial_dummy_hash");
  }

  void OnUploadCompleteReuseUploader(int response_code) {
    ++on_upload_complete_count_;
    if (on_upload_complete_count_ == 1)
      uploader_->UploadLog("dummy_data", "dummy_hash");
  }

  int on_upload_complete_count() const {
    return on_upload_complete_count_;
  }

 private:
  std::unique_ptr<NetMetricsLogUploader> uploader_;
  int on_upload_complete_count_;

  DISALLOW_COPY_AND_ASSIGN(NetMetricsLogUploaderTest);
};

TEST_F(NetMetricsLogUploaderTest, OnUploadCompleteReuseUploader) {
  net::TestURLFetcherFactory factory;
  CreateAndOnUploadCompleteReuseUploader();

  // Mimic the initial fetcher callback.
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Mimic the second fetcher callback.
  fetcher = factory.GetFetcherByID(0);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_EQ(on_upload_complete_count(), 2);
}

}  // namespace metrics
