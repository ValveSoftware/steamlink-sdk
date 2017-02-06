// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_revalidation_driver.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

namespace content {

namespace {
// This matches the maximum allocation size of AsyncResourceHandler.
const int kReadBufSize = 32 * 1024;

// The time to wait for a response. Since this includes the time taken to
// connect, this has been set to match the connect timeout
// kTransportConnectJobTimeoutInSeconds.
const int kResponseTimeoutInSeconds = 240;  // 4 minutes.

// This value should not be too large, as this request may be tying up a socket
// that could be used for something better. However, if it is too small, the
// cache entry will be truncated for no good reason.
// TODO(ricea): Find a more scientific way to set this timeout.
const int kReadTimeoutInSeconds = 30;
}  // namespace

AsyncRevalidationDriver::AsyncRevalidationDriver(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceThrottle> throttle,
    const base::Closure& completion_callback)
    : request_(std::move(request)),
      throttle_(std::move(throttle)),
      completion_callback_(completion_callback),
      weak_ptr_factory_(this) {
  request_->set_delegate(this);
  throttle_->set_controller(this);
}

AsyncRevalidationDriver::~AsyncRevalidationDriver() {}

void AsyncRevalidationDriver::StartRequest() {
  // Give the handler a chance to delay the URLRequest from being started.
  bool defer_start = false;
  throttle_->WillStartRequest(&defer_start);

  if (defer_start) {
    RecordDefer();
  } else {
    StartRequestInternal();
  }
}

void AsyncRevalidationDriver::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  DCHECK_EQ(request_.get(), request);

  // The async revalidation should not follow redirects, because caching is
  // a property of an individual HTTP resource.
  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  CancelRequestInternal(net::ERR_ABORTED, RESULT_GOT_REDIRECT);
}

void AsyncRevalidationDriver::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request_.get(), request);
  // This error code doesn't have exactly the right semantics, but it should
  // be sufficient to narrow down the problem in net logs.
  CancelRequestInternal(net::ERR_ACCESS_DENIED, RESULT_AUTH_FAILED);
}

void AsyncRevalidationDriver::OnBeforeNetworkStart(net::URLRequest* request,
                                                   bool* defer) {
  DCHECK_EQ(request_.get(), request);

  // Verify that the ResourceScheduler does not defer here.
  throttle_->WillStartUsingNetwork(defer);
  DCHECK(!*defer);

  // Start the response timer. This use of base::Unretained() is guaranteed safe
  // by the semantics of base::OneShotTimer.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kResponseTimeoutInSeconds),
               base::Bind(&AsyncRevalidationDriver::OnTimeout,
                          base::Unretained(this), RESULT_RESPONSE_TIMEOUT));
}

void AsyncRevalidationDriver::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request_.get(), request);

  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  // We have the response. No need to wait any longer.
  timer_.Stop();

  if (!request_->status().is_success()) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.AsyncRevalidation.ResponseError",
                                -request_->status().ToNetError());
    ResponseCompleted(RESULT_NET_ERROR);
    // |this| may be deleted after this point.
    return;
  }

  const net::HttpResponseInfo& response_info = request_->response_info();
  if (!response_info.response_time.is_null() && response_info.was_cached) {
    // The cached entry was revalidated. No need to read it in.
    ResponseCompleted(RESULT_REVALIDATED);
    // |this| may be deleted after this point.
    return;
  }

  bool defer = false;
  throttle_->WillProcessResponse(&defer);
  DCHECK(!defer);

  // Set up the timer for reading the body. This use of base::Unretained() is
  // guaranteed safe by the semantics of base::OneShotTimer.
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kReadTimeoutInSeconds),
               base::Bind(&AsyncRevalidationDriver::OnTimeout,
                          base::Unretained(this), RESULT_BODY_TIMEOUT));
  StartReading(false);  // Read the first chunk.
}

void AsyncRevalidationDriver::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  // request_ could be NULL if a timeout happened while OnReadCompleted() was
  // queued to run asynchronously.
  if (!request_)
    return;
  DCHECK_EQ(request_.get(), request);
  DCHECK(!is_deferred_);
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  // bytes_read == 0 is EOF.
  if (bytes_read == 0) {
    ResponseCompleted(RESULT_LOADED);
    return;
  }
  // bytes_read == -1 is an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.AsyncRevalidation.ReadError",
                                -request_->status().ToNetError());
    ResponseCompleted(RESULT_READ_ERROR);
    // |this| may be deleted after this point.
    return;
  }

  DCHECK_GT(bytes_read, 0);
  StartReading(true);  // Read the next chunk.
}

void AsyncRevalidationDriver::Resume() {
  DCHECK(is_deferred_);
  DCHECK(request_);
  is_deferred_ = false;
  request_->LogUnblocked();
  StartRequestInternal();
}

void AsyncRevalidationDriver::Cancel() {
  NOTREACHED();
}

void AsyncRevalidationDriver::CancelAndIgnore() {
  NOTREACHED();
}

void AsyncRevalidationDriver::CancelWithError(int error_code) {
  NOTREACHED();
}

void AsyncRevalidationDriver::StartRequestInternal() {
  DCHECK(request_);
  DCHECK(!request_->is_pending());

  request_->Start();
}

void AsyncRevalidationDriver::CancelRequestInternal(
    int error,
    AsyncRevalidationResult result) {
  DVLOG(1) << "CancelRequestInternal: " << request_->url().spec();

  // Set the error code since this will be reported to the NetworkDelegate and
  // recorded in the NetLog.
  request_->CancelWithError(error);

  // The ResourceScheduler needs to be able to examine the request when the
  // ResourceThrottle is destroyed, so delete it first.
  throttle_.reset();

  // Destroy the request so that it doesn't try to send an asynchronous
  // notification of completion.
  request_.reset();

  // Cancel timer to prevent OnTimeout() being called.
  timer_.Stop();

  ResponseCompleted(result);
  // |this| may deleted after this point.
}

void AsyncRevalidationDriver::StartReading(bool is_continuation) {
  int bytes_read = 0;
  ReadMore(&bytes_read);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (request_->status().is_io_pending())
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    scoped_refptr<base::SingleThreadTaskRunner> single_thread_task_runner =
        base::ThreadTaskRunnerHandle::Get();
    single_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&AsyncRevalidationDriver::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(), request_.get(), bytes_read));
  }
}

void AsyncRevalidationDriver::ReadMore(int* bytes_read) {
  DCHECK(!is_deferred_);

  if (!read_buffer_)
    read_buffer_ = new net::IOBuffer(kReadBufSize);

  timer_.Reset();
  request_->Read(read_buffer_.get(), kReadBufSize, bytes_read);

  // No need to check the return value here as we'll detect errors by
  // inspecting the URLRequest's status.
}

void AsyncRevalidationDriver::ResponseCompleted(
    AsyncRevalidationResult result) {
  DVLOG(1) << "ResponseCompleted: "
           << (request_ ? request_->url().spec() : "(request deleted)")
           << "result = " << result;
  UMA_HISTOGRAM_ENUMERATION("Net.AsyncRevalidation.Result", result, RESULT_MAX);
  DCHECK(!completion_callback_.is_null());
  base::ResetAndReturn(&completion_callback_).Run();
  // |this| may be deleted after this point.
}

void AsyncRevalidationDriver::OnTimeout(AsyncRevalidationResult result) {
  CancelRequestInternal(net::ERR_TIMED_OUT, result);
}

void AsyncRevalidationDriver::RecordDefer() {
  request_->LogBlockedBy(throttle_->GetNameForLogging());
  DCHECK(!is_deferred_);
  is_deferred_ = true;
}

}  // namespace content
