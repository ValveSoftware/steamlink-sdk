// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_data_savings.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/test/histogram_tester.h"
#include "components/data_reduction_proxy/core/common/data_savings_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

class TestDataSavingsRecorder
    : public data_reduction_proxy::DataSavingsRecorder {
 public:
  TestDataSavingsRecorder()
      : data_saver_enabled_(false), data_used_(0), original_size_(0) {}
  ~TestDataSavingsRecorder() {}

  // data_reduction_proxy::DataSavingsRecorder implementation:
  bool UpdateDataSavings(const std::string& host,
                         int64_t data_used,
                         int64_t original_size) override {
    if (!data_saver_enabled_) {
      return false;
    }
    last_host_ = host;
    data_used_ += data_used;
    original_size_ += original_size;
    return true;
  }

  void set_data_saver_enabled(bool data_saver_enabled) {
    data_saver_enabled_ = data_saver_enabled;
  }

  std::string last_host() const { return last_host_; }

  int64_t data_used() const { return data_used_; }

  int64_t original_size() const { return original_size_; }

 private:
  bool data_saver_enabled_;
  std::string last_host_;
  int64_t data_used_;
  int64_t original_size_;
};

}  // namespace

class PreviewsDataSavingsTest : public testing::Test {
 public:
  PreviewsDataSavingsTest()
      : test_data_savings_recorder_(new TestDataSavingsRecorder()),
        data_savings_(
            new PreviewsDataSavings(test_data_savings_recorder_.get())) {}
  ~PreviewsDataSavingsTest() override {}

  PreviewsDataSavings* data_savings() const { return data_savings_.get(); }

  TestDataSavingsRecorder* test_data_savings_recorder() const {
    return test_data_savings_recorder_.get();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;

  std::unique_ptr<TestDataSavingsRecorder> test_data_savings_recorder_;
  std::unique_ptr<PreviewsDataSavings> data_savings_;
};

TEST_F(PreviewsDataSavingsTest, RecordDataSavings) {
  int64_t original_size = 200;
  int64_t data_used = 100;
  std::string host = "host";

  EXPECT_EQ(0, test_data_savings_recorder()->data_used());
  EXPECT_EQ(0, test_data_savings_recorder()->original_size());
  test_data_savings_recorder()->set_data_saver_enabled(false);
  data_savings()->RecordDataSavings(host, data_used, original_size);

  EXPECT_EQ(0, test_data_savings_recorder()->data_used());
  EXPECT_EQ(0, test_data_savings_recorder()->original_size());
  EXPECT_EQ(std::string(), test_data_savings_recorder()->last_host());

  test_data_savings_recorder()->set_data_saver_enabled(true);
  data_savings()->RecordDataSavings(host, data_used, original_size);

  EXPECT_EQ(data_used, test_data_savings_recorder()->data_used());
  EXPECT_EQ(original_size, test_data_savings_recorder()->original_size());
  EXPECT_EQ(host, test_data_savings_recorder()->last_host());
}

TEST_F(PreviewsDataSavingsTest, RecordDataSavingsHistograms) {
  int64_t twenty_kb = 20480;
  int64_t twenty = twenty_kb >> 10;
  int64_t ten_kb = 10240;
  int64_t ten = ten_kb >> 10;
  std::string host = "host";

  test_data_savings_recorder()->set_data_saver_enabled(false);
  data_savings()->RecordDataSavings(host, ten_kb, twenty_kb);

  // The histograms record bytes in KB.
  histogram_tester()->ExpectUniqueSample(
      "Previews.OriginalContentLength.DataSaverDisabled", twenty, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.ContentLength.DataSaverDisabled", ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataSavings.DataSaverDisabled", twenty - ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataSavingsPercent.DataSaverDisabled",
      (twenty_kb - ten_kb) * 100 / twenty_kb, 1);

  data_savings()->RecordDataSavings(host, twenty_kb, ten_kb);

  // The histograms record bytes in KB.
  histogram_tester()->ExpectBucketCount(
      "Previews.OriginalContentLength.DataSaverDisabled", ten, 1);
  histogram_tester()->ExpectBucketCount(
      "Previews.ContentLength.DataSaverDisabled", twenty, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataInflation.DataSaverDisabled", twenty - ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataInflationPercent.DataSaverDisabled",
      (twenty_kb - ten_kb) * 100 / twenty_kb, 1);

  test_data_savings_recorder()->set_data_saver_enabled(true);
  data_savings()->RecordDataSavings(host, ten_kb, twenty_kb);

  // The histograms record bytes in KB.
  histogram_tester()->ExpectUniqueSample(
      "Previews.OriginalContentLength.DataSaverEnabled", twenty, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.ContentLength.DataSaverEnabled", ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataSavings.DataSaverEnabled", twenty - ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataSavingsPercent.DataSaverEnabled",
      (twenty_kb - ten_kb) * 100 / twenty_kb, 1);

  data_savings()->RecordDataSavings(host, twenty_kb, ten_kb);

  // The histograms record bytes in KB.
  histogram_tester()->ExpectBucketCount(
      "Previews.OriginalContentLength.DataSaverEnabled", ten, 1);
  histogram_tester()->ExpectBucketCount(
      "Previews.ContentLength.DataSaverEnabled", twenty, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataInflation.DataSaverEnabled", twenty - ten, 1);
  histogram_tester()->ExpectUniqueSample(
      "Previews.DataInflationPercent.DataSaverEnabled",
      (twenty_kb - ten_kb) * 100 / twenty_kb, 1);
}

}  // namespace previews
