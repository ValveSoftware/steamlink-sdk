// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockURLRequestJob : public URLRequestJob {
 public:
  MockURLRequestJob(URLRequest* request,
                    NetworkDelegate* network_delegate,
                    const URLRequestStatus& status)
      : URLRequestJob(request, network_delegate),
        status_(status),
        weak_factory_(this) {}

  virtual void Start() OVERRIDE {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockURLRequestJob::StartAsync, weak_factory_.GetWeakPtr()));
  }

 protected:
  virtual ~MockURLRequestJob() {}

 private:
  void StartAsync() {
    SetStatus(status_);
    NotifyHeadersComplete();
  }

  URLRequestStatus status_;
  base::WeakPtrFactory<MockURLRequestJob> weak_factory_;
};

class DummyProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    return new MockURLRequestJob(
        request,
        network_delegate,
        URLRequestStatus(URLRequestStatus::SUCCESS, OK));
  }
};

TEST(URLRequestJobFactoryTest, NoProtocolHandler) {
  TestDelegate delegate;
  TestURLRequestContext request_context;
  TestURLRequest request(
      GURL("foo://bar"), DEFAULT_PRIORITY, &delegate, &request_context);
  request.Start();

  base::MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, request.status().error());
}

TEST(URLRequestJobFactoryTest, BasicProtocolHandler) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo", new DummyProtocolHandler);
  TestURLRequest request(
      GURL("foo://bar"), DEFAULT_PRIORITY, &delegate, &request_context);
  request.Start();

  base::MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, request.status().status());
  EXPECT_EQ(OK, request.status().error());
}

TEST(URLRequestJobFactoryTest, DeleteProtocolHandler) {
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo", new DummyProtocolHandler);
  job_factory.SetProtocolHandler("foo", NULL);
}

}  // namespace

}  // namespace net
