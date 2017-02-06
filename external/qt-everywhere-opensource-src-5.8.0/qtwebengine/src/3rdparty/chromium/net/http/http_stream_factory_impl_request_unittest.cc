// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include <memory>

#include "base/run_loop.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/http/http_stream_factory_impl_job_controller.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {

class HttpStreamFactoryImplRequestTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NextProto> {};

INSTANTIATE_TEST_CASE_P(NextProto,
                        HttpStreamFactoryImplRequestTest,
                        testing::Values(kProtoSPDY31,
                                        kProtoHTTP2));

// Make sure that Request passes on its priority updates to its jobs.
TEST_P(HttpStreamFactoryImplRequestTest, SetPriority) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  HttpStreamFactoryImpl* factory =
      static_cast<HttpStreamFactoryImpl*>(session->http_stream_factory());
  MockHttpStreamRequestDelegate request_delegate;
  TestJobFactory job_factory;
  HttpStreamFactoryImpl::JobController* job_controller =
      new HttpStreamFactoryImpl::JobController(factory, &request_delegate,
                                               session.get(), &job_factory);
  factory->job_controller_set_.insert(base::WrapUnique(job_controller));

  HttpRequestInfo request_info;
  std::unique_ptr<HttpStreamFactoryImpl::Request> request(
      job_controller->Start(request_info, &request_delegate, nullptr,
                            BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                            DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller->main_job());
  EXPECT_EQ(DEFAULT_PRIORITY, job_controller->main_job()->priority());

  request->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job_controller->main_job()->priority());

  EXPECT_CALL(request_delegate, OnStreamFailed(_, _)).Times(1);
  job_controller->OnStreamFailed(job_factory.main_job(), ERR_FAILED,
                                 SSLConfig());

  request->SetPriority(IDLE);
  EXPECT_EQ(IDLE, job_controller->main_job()->priority());
}

TEST_P(HttpStreamFactoryImplRequestTest, DelayMainJob) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  std::unique_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  HttpStreamFactoryImpl* factory =
      static_cast<HttpStreamFactoryImpl*>(session->http_stream_factory());
  MockHttpStreamRequestDelegate request_delegate;
  HttpStreamFactoryImpl::JobFactory* job_factory =
      HttpStreamFactoryImplPeer::GetDefaultJobFactory(factory);
  HttpStreamFactoryImpl::JobController* job_controller =
      new HttpStreamFactoryImpl::JobController(factory, &request_delegate,
                                               session.get(), job_factory);
  factory->job_controller_set_.insert(base::WrapUnique(job_controller));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  HttpStreamFactoryImpl::Request request(
      request_info.url, job_controller, &request_delegate, nullptr,
      BoundNetLog(), HttpStreamFactoryImpl::Request::HTTP_STREAM);
  job_controller->request_ = &request;

  HostPortPair server = HostPortPair::FromURL(request_info.url);
  GURL original_url =
      job_controller->ApplyHostMappingRules(request_info.url, &server);

  HttpStreamFactoryImpl::Job* job = new HttpStreamFactoryImpl::Job(
      job_controller, HttpStreamFactoryImpl::MAIN, session.get(), request_info,
      DEFAULT_PRIORITY, SSLConfig(), SSLConfig(), server, original_url,
      nullptr);
  job_controller->main_job_.reset(job);
  job_controller->AttachJob(job);
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  AlternativeService alternative_service(net::NPN_HTTP_2, server);
  HttpStreamFactoryImpl::Job* alternative_job = new HttpStreamFactoryImpl::Job(
      job_controller, HttpStreamFactoryImpl::ALTERNATIVE, session.get(),
      request_info, DEFAULT_PRIORITY, SSLConfig(), SSLConfig(), server,
      original_url, alternative_service, nullptr);
  job_controller->alternative_job_.reset(alternative_job);
  job_controller->AttachJob(alternative_job);

  job->WaitFor(alternative_job);
  EXPECT_EQ(HttpStreamFactoryImpl::Job::STATE_NONE, job->next_state_);

  // Test |alternative_job| resuming the |job| after delay.
  int wait_time = 1;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(wait_time);
  job->Resume(alternative_job, delay);

  // Verify |job| has |wait_time_| and there is no |blocking_job_|
  EXPECT_EQ(delay, job->wait_time_);
  EXPECT_TRUE(!job->blocking_job_);

  // Start the |job| and verify |job|'s |wait_time_| is cleared.
  job->Start(request.stream_type());

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(wait_time + 1));
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(delay, job->wait_time_);
  EXPECT_TRUE(job->wait_time_.is_zero());
  EXPECT_EQ(HttpStreamFactoryImpl::Job::STATE_INIT_CONNECTION_COMPLETE,
            job->next_state_);
}

}  // namespace net
