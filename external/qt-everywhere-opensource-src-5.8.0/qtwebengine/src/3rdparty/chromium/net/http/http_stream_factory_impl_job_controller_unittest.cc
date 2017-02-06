// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_job_controller.h"

#include <memory>

#include "net/http/http_basic_stream.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace net {

namespace {

void DeleteHttpStreamPointer(const SSLConfig& used_ssl_config,
                             const ProxyInfo& used_proxy_info,
                             HttpStream* stream) {
  delete stream;
}

}  // anonymous namespace

class HttpStreamFactoryImplJobControllerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NextProto> {
 public:
  HttpStreamFactoryImplJobControllerTest()
      : session_deps_(GetParam(), ProxyService::CreateDirect()) {
    session_deps_.enable_quic = true;
    session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    factory_ =
        static_cast<HttpStreamFactoryImpl*>(session_->http_stream_factory());
    job_controller_ = new HttpStreamFactoryImpl::JobController(
        factory_, &request_delegate_, session_.get(), &job_factory_);
    HttpStreamFactoryImplPeer::AddJobController(factory_, job_controller_);
  }

  ~HttpStreamFactoryImplJobControllerTest() {}

  void SetAlternativeService(const HttpRequestInfo& request_info,
                             AlternativeService alternative_service) {
    HostPortPair host_port_pair = HostPortPair::FromURL(request_info.url);
    url::SchemeHostPort server(request_info.url);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    session_->http_server_properties()->SetAlternativeService(
        server, alternative_service, expiration);
  }

  TestJobFactory job_factory_;
  MockHttpStreamRequestDelegate request_delegate_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  HttpStreamFactoryImpl* factory_;
  HttpStreamFactoryImpl::JobController* job_controller_;
  std::unique_ptr<HttpStreamFactoryImpl::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactoryImplJobControllerTest);
};

INSTANTIATE_TEST_CASE_P(NextProto,
                        HttpStreamFactoryImplJobControllerTest,
                        testing::Values(kProtoSPDY31, kProtoHTTP2));

TEST_P(HttpStreamFactoryImplJobControllerTest,
       OnStreamFailedWithNoAlternativeJob) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));

  EXPECT_TRUE(job_controller_->main_job());

  // There's no other alternative job. Thus when stream failed, it should
  // notify Request of the stream failure.
  EXPECT_CALL(request_delegate_, OnStreamFailed(ERR_FAILED, _)).Times(1);
  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED,
                                  SSLConfig());
}

TEST_P(HttpStreamFactoryImplJobControllerTest,
       OnStreamReadyWithNoAlternativeJob) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());

  // There's no other alternative job. Thus when stream is ready, it should
  // notify Request.
  HttpStream* http_stream =
      new HttpBasicStream(new ClientSocketHandle(), false);
  job_factory_.main_job()->SetStream(http_stream);

  EXPECT_CALL(request_delegate_, OnStreamReady(_, _, http_stream))
      .WillOnce(Invoke(DeleteHttpStreamPointer));
  job_controller_->OnStreamReady(job_factory_.main_job(), SSLConfig(),
                                 ProxyInfo());
}

// Test we cancel Jobs correctly when the Request is explicitly canceled
// before any Job is bound to Request.
TEST_P(HttpStreamFactoryImplJobControllerTest, CancelJobsBeforeBinding) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(QUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Reset the Request will cancel all the Jobs since there's no Job determined
  // to serve Request yet and JobController will notify the factory to delete
  // itself upon completion.
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryImplPeer::IsJobControllerDeleted(factory_));
}

TEST_P(HttpStreamFactoryImplJobControllerTest, OnStreamFailedForBothJobs) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(QUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // We have the main job with unknown status when the alternative job is failed
  // thus should not notify Request of the alternative job's failure. But should
  // notify the main job to mark the alternative job failed.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _)).Times(0);
  EXPECT_CALL(*job_factory_.main_job(), MarkOtherJobComplete(_)).Times(1);
  job_controller_->OnStreamFailed(job_factory_.alternative_job(), ERR_FAILED,
                                  SSLConfig());
  EXPECT_TRUE(!job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job());

  // The failure of second Job should be reported to Request as there's no more
  // pending Job to serve the Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _)).Times(1);
  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED,
                                  SSLConfig());
}

TEST_P(HttpStreamFactoryImplJobControllerTest,
       SecondJobFailsAfterFirstJobSucceeds) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(QUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Main job succeeds, starts serving Request and it should report status
  // to Request. The alternative job will mark the main job complete and gets
  // orphaned.
  HttpStream* http_stream =
      new HttpBasicStream(new ClientSocketHandle(), false);
  job_factory_.main_job()->SetStream(http_stream);

  EXPECT_CALL(request_delegate_, OnStreamReady(_, _, http_stream))
      .WillOnce(Invoke(DeleteHttpStreamPointer));
  EXPECT_CALL(*job_factory_.alternative_job(), MarkOtherJobComplete(_))
      .Times(1);
  job_controller_->OnStreamReady(job_factory_.main_job(), SSLConfig(),
                                 ProxyInfo());

  // JobController shouldn't report the status of second job as request
  // is already successfully served.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _)).Times(0);
  job_controller_->OnStreamFailed(job_factory_.alternative_job(), ERR_FAILED,
                                  SSLConfig());

  // Reset the request as it's been successfully served.
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryImplPeer::IsJobControllerDeleted(factory_));
}

TEST_P(HttpStreamFactoryImplJobControllerTest,
       SecondJobSucceedsAfterFirstJobFailed) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(QUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // |main_job| fails but should not report status to Request.
  // The alternative job will mark the main job complete.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _)).Times(0);
  EXPECT_CALL(*job_factory_.alternative_job(), MarkOtherJobComplete(_))
      .Times(1);

  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED,
                                  SSLConfig());

  // |alternative_job| succeeds and should report status to Request.
  HttpStream* http_stream =
      new HttpBasicStream(new ClientSocketHandle(), false);
  job_factory_.alternative_job()->SetStream(http_stream);

  EXPECT_CALL(request_delegate_, OnStreamReady(_, _, http_stream))
      .WillOnce(Invoke(DeleteHttpStreamPointer));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig(),
                                 ProxyInfo());
}

// Regression test for crbug/621069.
// Get load state after main job fails and before alternative job succeeds.
TEST_P(HttpStreamFactoryImplJobControllerTest, GetLoadStateAfterMainJobFailed) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(QUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_.reset(
      job_controller_->Start(request_info, &request_delegate_, nullptr,
                             BoundNetLog(), HttpStreamRequest::HTTP_STREAM,
                             DEFAULT_PRIORITY, SSLConfig(), SSLConfig()));
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // |main_job| fails but should not report status to Request.
  // The alternative job will mark the main job complete.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _)).Times(0);
  EXPECT_CALL(*job_factory_.alternative_job(), MarkOtherJobComplete(_))
      .Times(1);

  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED,
                                  SSLConfig());

  // Controller should use alternative job to get load state.
  job_controller_->GetLoadState();

  // |alternative_job| succeeds and should report status to Request.
  HttpStream* http_stream =
      new HttpBasicStream(new ClientSocketHandle(), false);
  job_factory_.alternative_job()->SetStream(http_stream);

  EXPECT_CALL(request_delegate_, OnStreamReady(_, _, http_stream))
      .WillOnce(Invoke(DeleteHttpStreamPointer));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig(),
                                 ProxyInfo());
}

}  // namespace net
