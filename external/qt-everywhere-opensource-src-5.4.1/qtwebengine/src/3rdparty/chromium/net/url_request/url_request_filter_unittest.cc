// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

URLRequestTestJob* job_a;

URLRequestJob* FactoryA(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_a = new URLRequestTestJob(request, network_delegate);
  return job_a;
}

URLRequestTestJob* job_b;

URLRequestJob* FactoryB(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_b = new URLRequestTestJob(request, network_delegate);
  return job_b;
}

URLRequestTestJob* job_c;

class TestURLRequestInterceptor : public URLRequestInterceptor {
 public:
  virtual ~TestURLRequestInterceptor() {}

  virtual URLRequestJob* MaybeInterceptRequest(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    job_c = new URLRequestTestJob(request, network_delegate);
    return job_c;
  }
};

TEST(URLRequestFilter, BasicMatching) {
  TestDelegate delegate;
  TestURLRequestContext request_context;
  URLRequestFilter* filter = URLRequestFilter::GetInstance();

  GURL url_1("http://foo.com/");
  TestURLRequest request_1(
      url_1, DEFAULT_PRIORITY, &delegate, &request_context);

  GURL url_2("http://bar.com/");
  TestURLRequest request_2(
      url_2, DEFAULT_PRIORITY, &delegate, &request_context);

  // Check AddUrlHandler checks for invalid URLs.
  EXPECT_FALSE(filter->AddUrlHandler(GURL(), &FactoryA));

  // Check URL matching.
  filter->ClearHandlers();
  EXPECT_TRUE(filter->AddUrlHandler(url_1, &FactoryA));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(&request_1, NULL);
    EXPECT_EQ(job_a, found);
    EXPECT_TRUE(job_a != NULL);
    job_a = NULL;
  }
  EXPECT_EQ(filter->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_TRUE(filter->MaybeInterceptRequest(&request_2, NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove URL matching.
  filter->RemoveUrlHandler(url_1);
  EXPECT_TRUE(filter->MaybeInterceptRequest(&request_1, NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddHostnameHandler(url_1.scheme(), url_1.host(), &FactoryB);
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(&request_1, NULL);
    EXPECT_EQ(job_b, found);
    EXPECT_TRUE(job_b != NULL);
    job_b = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check we don't match other hostnames.
  EXPECT_TRUE(filter->MaybeInterceptRequest(&request_2, NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove hostname matching.
  filter->RemoveHostnameHandler(url_1.scheme(), url_1.host());
  EXPECT_TRUE(filter->MaybeInterceptRequest(&request_1, NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check URLRequestInterceptor hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddHostnameInterceptor(
      url_1.scheme(), url_1.host(),
      scoped_ptr<net::URLRequestInterceptor>(new TestURLRequestInterceptor()));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(&request_1, NULL);
    EXPECT_EQ(job_c, found);
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check URLRequestInterceptor URL matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddUrlInterceptor(
      url_2,
      scoped_ptr<net::URLRequestInterceptor>(new TestURLRequestInterceptor()));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(&request_2, NULL);
    EXPECT_EQ(job_c, found);
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  filter->ClearHandlers();
}

}  // namespace

}  // namespace net
