// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kTestData[] = "Huge data array";
const int kRangeFirstPosition = 5;
const int kRangeLastPosition = 8;
COMPILE_ASSERT(kRangeFirstPosition > 0 &&
    kRangeFirstPosition < kRangeLastPosition &&
    kRangeLastPosition < static_cast<int>(arraysize(kTestData) - 1),
    invalid_range);

class MockSimpleJob : public URLRequestSimpleJob {
 public:
  MockSimpleJob(URLRequest* request, NetworkDelegate* network_delegate)
      : URLRequestSimpleJob(request, network_delegate) {
  }

 protected:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/plain");
    charset->assign("US-ASCII");
    data->assign(kTestData);
    return OK;
  }

 private:
  virtual ~MockSimpleJob() {}

  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(MockSimpleJob);
};

class SimpleJobProtocolHandler :
    public URLRequestJobFactory::ProtocolHandler {
 public:
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request,
      NetworkDelegate* network_delegate) const OVERRIDE {
    return new MockSimpleJob(request, network_delegate);
  }
};

class URLRequestSimpleJobTest : public ::testing::Test {
 public:
  URLRequestSimpleJobTest() : context_(true) {
    job_factory_.SetProtocolHandler("data", new SimpleJobProtocolHandler());
    context_.set_job_factory(&job_factory_);
    context_.Init();

    request_.reset(new URLRequest(
        GURL("data:test"), DEFAULT_PRIORITY, &delegate_, &context_));
  }

  void StartRequest(const HttpRequestHeaders* headers) {
    if (headers)
      request_->SetExtraRequestHeaders(*headers);
    request_->Start();

    EXPECT_TRUE(request_->is_pending());
    base::RunLoop().Run();
    EXPECT_FALSE(request_->is_pending());
  }

 protected:
  URLRequestJobFactoryImpl job_factory_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  scoped_ptr<URLRequest> request_;
};

}  // namespace

TEST_F(URLRequestSimpleJobTest, SimpleRequest) {
  StartRequest(NULL);
  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kTestData, delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, RangeRequest) {
  const std::string kExpectedBody = std::string(
      kTestData + kRangeFirstPosition, kTestData + kRangeLastPosition + 1);
  HttpRequestHeaders headers;
  headers.SetHeader(
      HttpRequestHeaders::kRange,
      HttpByteRange::Bounded(kRangeFirstPosition, kRangeLastPosition)
          .GetHeaderValue());

  StartRequest(&headers);

  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kExpectedBody, delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, MultipleRangeRequest) {
  HttpRequestHeaders headers;
  int middle_pos = (kRangeFirstPosition + kRangeLastPosition)/2;
  std::string range = base::StringPrintf("bytes=%d-%d,%d-%d",
                                         kRangeFirstPosition,
                                         middle_pos,
                                         middle_pos + 1,
                                         kRangeLastPosition);
  headers.SetHeader(HttpRequestHeaders::kRange, range);

  StartRequest(&headers);

  EXPECT_TRUE(delegate_.request_failed());
  EXPECT_EQ(ERR_REQUEST_RANGE_NOT_SATISFIABLE, request_->status().error());
}

TEST_F(URLRequestSimpleJobTest, InvalidRangeRequest) {
  HttpRequestHeaders headers;
  std::string range = base::StringPrintf(
      "bytes=%d-%d", kRangeLastPosition, kRangeFirstPosition);
  headers.SetHeader(HttpRequestHeaders::kRange, range);

  StartRequest(&headers);

  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kTestData, delegate_.data_received());
}

}  // namespace net
