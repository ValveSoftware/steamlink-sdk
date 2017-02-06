// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

static const char kHistogramSucceeded[] =
    "DataReductionProxy.Pingback.Succeeded";
static const char kHistogramAttempted[] =
    "DataReductionProxy.Pingback.Attempted";
static const char kSessionKey[] = "fake-session";
static const char kFakeURL[] = "http://www.google.com/";

}  // namespace

// Controls whether a pingback is sent or not.
class TestDataReductionProxyPingbackClient
    : public DataReductionProxyPingbackClient {
 public:
  TestDataReductionProxyPingbackClient(
      net::URLRequestContextGetter* url_request_context_getter)
      : DataReductionProxyPingbackClient(url_request_context_getter),
        should_override_random_(false),
        override_value_(0.0f) {}

  void OverrideRandom(bool should_override_random, float override_value) {
    should_override_random_ = should_override_random;
    override_value_ = override_value;
  }

 private:
  float GenerateRandomFloat() const override {
    if (should_override_random_)
      return override_value_;
    return DataReductionProxyPingbackClient::GenerateRandomFloat();
  }

  bool should_override_random_;
  float override_value_;
};

class DataReductionProxyPingbackClientTest : public testing::Test {
 public:
  DataReductionProxyPingbackClientTest()
      : timing_(base::Time::FromJsTime(1500),
                base::TimeDelta::FromMilliseconds(1600),
                base::TimeDelta::FromMilliseconds(1700),
                base::TimeDelta::FromMilliseconds(1800),
                base::TimeDelta::FromMilliseconds(1900)) {}

  TestDataReductionProxyPingbackClient* pingback_client() const {
    return pingback_client_.get();
  }

  void Init() {
    request_context_getter_ =
        new net::TestURLRequestContextGetter(message_loop_.task_runner());
    pingback_client_ = base::WrapUnique<TestDataReductionProxyPingbackClient>(
        new TestDataReductionProxyPingbackClient(
            request_context_getter_.get()));
  }

  void CreateAndSendPingback() {
    DataReductionProxyData request_data;
    request_data.set_session_key(kSessionKey);
    request_data.set_original_request_url(GURL(kFakeURL));
    factory()->set_remove_fetcher_on_delete(true);
    pingback_client()->SendPingback(request_data, timing_);
  }

  net::TestURLFetcherFactory* factory() { return &factory_; }

  const DataReductionProxyPageLoadTiming& timing() { return timing_; }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::unique_ptr<TestDataReductionProxyPingbackClient> pingback_client_;
  net::TestURLFetcherFactory factory_;
  DataReductionProxyPageLoadTiming timing_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DataReductionProxyPingbackClientTest, VerifyPingbackContent) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->OverrideRandom(true, 0.5f);
  pingback_client()->SetPingbackReportingFraction(1.0f);
  CreateAndSendPingback();
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_EQ(test_fetcher->upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(test_fetcher->upload_data());
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(
      timing().navigation_start,
      protobuf_parser::TimestampToTime(pageload_metrics.first_request_time()));
  EXPECT_EQ(timing().response_start,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_byte()));
  EXPECT_EQ(timing().load_event_start, protobuf_parser::DurationToTimeDelta(
                                           pageload_metrics.page_load_time()));
  EXPECT_EQ(timing().first_image_paint,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_image_paint()));
  EXPECT_EQ(timing().first_contentful_paint,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_contentful_paint()));

  EXPECT_EQ(kSessionKey, pageload_metrics.session_key());
  EXPECT_EQ(kFakeURL, pageload_metrics.first_request_url());
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
  EXPECT_FALSE(factory()->GetFetcherByID(0));
}

TEST_F(DataReductionProxyPingbackClientTest, SendTwoPingbacks) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->OverrideRandom(true, 0.5f);
  pingback_client()->SetPingbackReportingFraction(1.0f);
  CreateAndSendPingback();
  CreateAndSendPingback();

  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 2);
  EXPECT_TRUE(factory()->GetFetcherByID(0));
  test_fetcher = factory()->GetFetcherByID(0);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 2);
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  histogram_tester().ExpectTotalCount(kHistogramAttempted, 2);
}

TEST_F(DataReductionProxyPingbackClientTest, NoPingbackSent) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->OverrideRandom(true, 0.5f);
  pingback_client()->SetPingbackReportingFraction(0.0f);
  CreateAndSendPingback();
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, false, 1);
  histogram_tester().ExpectTotalCount(kHistogramSucceeded, 0);
  EXPECT_FALSE(factory()->GetFetcherByID(0));
}

TEST_F(DataReductionProxyPingbackClientTest, VerifyReportingBehvaior) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));

  // Verify that if the random number is less than the reporting fraction, the
  // pingback is created.
  pingback_client()->SetPingbackReportingFraction(0.5f);
  pingback_client()->OverrideRandom(true, 0.4f);
  CreateAndSendPingback();
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_TRUE(test_fetcher);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);

  // Verify that if the random number is greater than the reporting fraction,
  // the pingback is not created.
  pingback_client()->OverrideRandom(true, 0.6f);
  CreateAndSendPingback();
  histogram_tester().ExpectBucketCount(kHistogramAttempted, false, 1);
  test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_FALSE(test_fetcher);

  // Verify that if the random number is equal to the reporting fraction, the
  // pingback is not created. Specifically, if the reporting fraction is zero,
  // and the random number is zero, no pingback is sent.
  pingback_client()->SetPingbackReportingFraction(0.0f);
  pingback_client()->OverrideRandom(true, 0.0f);
  CreateAndSendPingback();
  histogram_tester().ExpectBucketCount(kHistogramAttempted, false, 2);
  test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_FALSE(test_fetcher);

  // Verify that the command line flag forces a pingback.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyForcePingback);
  pingback_client()->SetPingbackReportingFraction(0.0f);
  pingback_client()->OverrideRandom(true, 1.0f);
  CreateAndSendPingback();
  histogram_tester().ExpectBucketCount(kHistogramAttempted, true, 2);
  test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_TRUE(test_fetcher);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 2);
}

TEST_F(DataReductionProxyPingbackClientTest, FailedPingback) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->OverrideRandom(true, 0.5f);
  pingback_client()->SetPingbackReportingFraction(1.0f);
  CreateAndSendPingback();
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_TRUE(test_fetcher);
  // Simulate a network error.
  test_fetcher->set_status(net::URLRequestStatus(
      net::URLRequestStatus::FAILED, net::ERR_INVALID_AUTH_CREDENTIALS));
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, false, 1);
}

}  // namespace data_reduction_proxy
