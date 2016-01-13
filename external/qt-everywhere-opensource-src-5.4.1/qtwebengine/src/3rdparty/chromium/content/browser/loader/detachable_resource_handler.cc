// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/detachable_resource_handler.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

namespace {
// This matches the maximum allocation size of AsyncResourceHandler.
const int kReadBufSize = 32 * 1024;
}

namespace content {

DetachableResourceHandler::DetachableResourceHandler(
    net::URLRequest* request,
    base::TimeDelta cancel_delay,
    scoped_ptr<ResourceHandler> next_handler)
    : ResourceHandler(request),
      next_handler_(next_handler.Pass()),
      cancel_delay_(cancel_delay),
      is_deferred_(false),
      is_finished_(false) {
  GetRequestInfo()->set_detachable_handler(this);
}

DetachableResourceHandler::~DetachableResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  GetRequestInfo()->set_detachable_handler(NULL);
}

void DetachableResourceHandler::Detach() {
  if (is_detached())
    return;

  if (!is_finished_) {
    // Simulate a cancel on the next handler before destroying it.
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    bool defer_ignored = false;
    next_handler_->OnResponseCompleted(status, std::string(), &defer_ignored);
    DCHECK(!defer_ignored);
    // If |next_handler_| were to defer its shutdown in OnResponseCompleted,
    // this would destroy it anyway. Fortunately, AsyncResourceHandler never
    // does this anyway, so DCHECK it. BufferedResourceHandler and RVH shutdown
    // already ignore deferred ResourceHandler shutdown, but
    // DetachableResourceHandler and the detach-on-renderer-cancel logic
    // introduces a case where this occurs when the renderer cancels a resource.
  }
  // A OnWillRead / OnReadCompleted pair may still be in progress, but
  // OnWillRead passes back a scoped_refptr, so downstream handler's buffer will
  // survive long enough to complete that read. From there, future reads will
  // drain into |read_buffer_|. (If |next_handler_| is an AsyncResourceHandler,
  // the net::IOBuffer takes a reference to the ResourceBuffer which owns the
  // shared memory.)
  next_handler_.reset();

  // Time the request out if it takes too long.
  detached_timer_.reset(new base::OneShotTimer<DetachableResourceHandler>());
  detached_timer_->Start(
      FROM_HERE, cancel_delay_, this, &DetachableResourceHandler::Cancel);

  // Resume if necessary. The request may have been deferred, say, waiting on a
  // full buffer in AsyncResourceHandler. Now that it has been detached, resume
  // and drain it.
  if (is_deferred_)
    Resume();
}

void DetachableResourceHandler::SetController(ResourceController* controller) {
  ResourceHandler::SetController(controller);

  // Intercept the ResourceController for downstream handlers to keep track of
  // whether the request is deferred.
  if (next_handler_)
    next_handler_->SetController(this);
}

bool DetachableResourceHandler::OnUploadProgress(uint64 position, uint64 size) {
  if (!next_handler_)
    return true;

  return next_handler_->OnUploadProgress(position, size);
}

bool DetachableResourceHandler::OnRequestRedirected(const GURL& url,
                                                    ResourceResponse* response,
                                                    bool* defer) {
  DCHECK(!is_deferred_);

  if (!next_handler_)
    return true;

  bool ret = next_handler_->OnRequestRedirected(url, response, &is_deferred_);
  *defer = is_deferred_;
  return ret;
}

bool DetachableResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                  bool* defer) {
  DCHECK(!is_deferred_);

  if (!next_handler_)
    return true;

  bool ret =
      next_handler_->OnResponseStarted(response, &is_deferred_);
  *defer = is_deferred_;
  return ret;
}

bool DetachableResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  DCHECK(!is_deferred_);

  if (!next_handler_)
    return true;

  bool ret = next_handler_->OnWillStart(url, &is_deferred_);
  *defer = is_deferred_;
  return ret;
}

bool DetachableResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                     bool* defer) {
  DCHECK(!is_deferred_);

  if (!next_handler_)
    return true;

  bool ret =
      next_handler_->OnBeforeNetworkStart(url, &is_deferred_);
  *defer = is_deferred_;
  return ret;
}

bool DetachableResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                           int* buf_size,
                                           int min_size) {
  if (!next_handler_) {
    DCHECK_EQ(-1, min_size);
    if (!read_buffer_)
      read_buffer_ = new net::IOBuffer(kReadBufSize);
    *buf = read_buffer_;
    *buf_size = kReadBufSize;
    return true;
  }

  return next_handler_->OnWillRead(buf, buf_size, min_size);
}

bool DetachableResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK(!is_deferred_);

  if (!next_handler_)
    return true;

  bool ret =
      next_handler_->OnReadCompleted(bytes_read, &is_deferred_);
  *defer = is_deferred_;
  return ret;
}

void DetachableResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  // No DCHECK(!is_deferred_) as the request may have been cancelled while
  // deferred.

  if (!next_handler_)
    return;

  is_finished_ = true;

  next_handler_->OnResponseCompleted(status, security_info, &is_deferred_);
  *defer = is_deferred_;
}

void DetachableResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  if (!next_handler_)
    return;

  next_handler_->OnDataDownloaded(bytes_downloaded);
}

void DetachableResourceHandler::Resume() {
  DCHECK(is_deferred_);
  is_deferred_ = false;
  controller()->Resume();
}

void DetachableResourceHandler::Cancel() {
  controller()->Cancel();
}

void DetachableResourceHandler::CancelAndIgnore() {
  controller()->CancelAndIgnore();
}

void DetachableResourceHandler::CancelWithError(int error_code) {
  controller()->CancelWithError(error_code);
}

}  // namespace content
