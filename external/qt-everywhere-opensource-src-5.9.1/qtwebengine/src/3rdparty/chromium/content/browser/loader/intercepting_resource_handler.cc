// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/intercepting_resource_handler.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"

namespace content {

InterceptingResourceHandler::InterceptingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, std::move(next_handler)) {
  next_handler_->SetController(this);
}

InterceptingResourceHandler::~InterceptingResourceHandler() {}

void InterceptingResourceHandler::SetController(
    ResourceController* controller) {
  if (state_ == State::PASS_THROUGH)
    return LayeredResourceHandler::SetController(controller);
  ResourceHandler::SetController(controller);
}

bool InterceptingResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                    bool* defer) {
  // If there's no need to switch handlers, just start acting as a blind
  // pass-through ResourceHandler.
  if (!new_handler_) {
    state_ = State::PASS_THROUGH;
    next_handler_->SetController(controller());
    return next_handler_->OnResponseStarted(response, defer);
  }

  DCHECK_EQ(state_, State::STARTING);
  // Otherwise, switch handlers. First, inform the original ResourceHandler
  // that this will be handled entirely by the new ResourceHandler.
  bool defer_ignored = false;
  if (!next_handler_->OnResponseStarted(response, &defer_ignored))
    return false;

  // Although deferring OnResponseStarted is legal, the only downstream handler
  // which does so is CrossSiteResourceHandler. Cross-site transitions should
  // not trigger when switching handlers.
  DCHECK(!defer_ignored);

  // TODO(yhirano): Retaining ownership from a raw pointer is bad.
  response_ = response;
  state_ = State::SENDING_PAYLOAD_TO_OLD_HANDLER;
  return DoLoop(defer);
}

bool InterceptingResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                             int* buf_size,
                                             int min_size) {
  if (state_ == State::PASS_THROUGH)
    return next_handler_->OnWillRead(buf, buf_size, min_size);

  DCHECK_EQ(State::STARTING, state_);
  DCHECK_EQ(-1, min_size);

  if (!next_handler_->OnWillRead(buf, buf_size, min_size))
    return false;

  first_read_buffer_ = *buf;
  first_read_buffer_size_ = *buf_size;
  first_read_buffer_double_ = new net::IOBuffer(static_cast<size_t>(*buf_size));
  *buf = first_read_buffer_double_;
  return true;
}

bool InterceptingResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_GE(bytes_read, 0);
  if (state_ == State::PASS_THROUGH) {
    if (first_read_buffer_double_) {
      // |first_read_buffer_double_| was allocated and the user wrote data to
      // the buffer, but switching has not been done after all.
      memcpy(first_read_buffer_->data(), first_read_buffer_double_->data(),
             bytes_read);
      first_read_buffer_ = nullptr;
      first_read_buffer_double_ = nullptr;
    }
    return next_handler_->OnReadCompleted(bytes_read, defer);
  }

  DCHECK_EQ(State::WAITING_FOR_ON_READ_COMPLETED, state_);
  first_read_buffer_bytes_read_ = bytes_read;
  state_ = State::SENDING_BUFFER_TO_NEW_HANDLER;
  return DoLoop(defer);
}

void InterceptingResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    bool* defer) {
  if (state_ == State::PASS_THROUGH) {
    LayeredResourceHandler::OnResponseCompleted(status, defer);
    return;
  }
  if (!new_handler_) {
    // Therer is only one ResourceHandler in this InterceptingResourceHandler.
    state_ = State::PASS_THROUGH;
    first_read_buffer_double_ = nullptr;
    next_handler_->SetController(controller());
    next_handler_->OnResponseCompleted(status, defer);
    return;
  }

  // There are two ResourceHandlers in this InterceptingResourceHandler.
  // |next_handler_| is the old handler and |new_handler_| is the new handler.
  // As written in the class comment, this class assumes that the old handler
  // will not set |*defer| in OnResponseCompleted.
  next_handler_->SetController(controller());
  next_handler_->OnResponseCompleted(status, defer);
  DCHECK(!*defer);

  state_ = State::PASS_THROUGH;
  first_read_buffer_double_ = nullptr;
  new_handler_->SetController(controller());
  next_handler_ = std::move(new_handler_);
  next_handler_->OnResponseCompleted(status, defer);
}

void InterceptingResourceHandler::Cancel() {
  DCHECK_NE(State::PASS_THROUGH, state_);
  controller()->Cancel();
}

void InterceptingResourceHandler::CancelAndIgnore() {
  DCHECK_NE(State::PASS_THROUGH, state_);
  controller()->CancelAndIgnore();
}

void InterceptingResourceHandler::CancelWithError(int error_code) {
  DCHECK_NE(State::PASS_THROUGH, state_);
  controller()->CancelWithError(error_code);
}

void InterceptingResourceHandler::Resume() {
  DCHECK_NE(State::PASS_THROUGH, state_);
  if (state_ == State::STARTING ||
      state_ == State::WAITING_FOR_ON_READ_COMPLETED) {
    // Uninteresting Resume: just delegate to the original resource controller.
    controller()->Resume();
    return;
  }
  bool defer = false;
  if (!DoLoop(&defer)) {
    controller()->Cancel();
    return;
  }

  if (!defer)
    controller()->Resume();
}

void InterceptingResourceHandler::UseNewHandler(
    std::unique_ptr<ResourceHandler> new_handler,
    const std::string& payload_for_old_handler) {
  new_handler_ = std::move(new_handler);
  new_handler_->SetController(this);
  payload_for_old_handler_ = payload_for_old_handler;
}

bool InterceptingResourceHandler::DoLoop(bool* defer) {
  bool result = true;
  do {
    switch (state_) {
      case State::STARTING:
      case State::WAITING_FOR_ON_READ_COMPLETED:
      case State::PASS_THROUGH:
        NOTREACHED();
        break;
      case State::SENDING_ON_WILL_START_TO_NEW_HANDLER:
        result = SendOnResponseStartedToNewHandler(defer);
        break;
      case State::SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER:
        if (first_read_buffer_double_) {
          // OnWillRead has been called, so copying the data from
          // |first_read_buffer_double_| to |first_read_buffer_| will be needed
          // when OnReadCompleted is called.
          state_ = State::WAITING_FOR_ON_READ_COMPLETED;
        } else {
          // OnWillRead has not been called, so no special handling will be
          // needed from now on.
          state_ = State::PASS_THROUGH;
          next_handler_->SetController(controller());
        }
        break;
      case State::SENDING_PAYLOAD_TO_OLD_HANDLER:
        result = SendPayloadToOldHandler(defer);
        break;
      case State::SENDING_BUFFER_TO_NEW_HANDLER:
        result = SendFirstReadBufferToNewHandler(defer);
        break;
    }
  } while (result && !*defer &&
           state_ != State::WAITING_FOR_ON_READ_COMPLETED &&
           state_ != State::PASS_THROUGH);
  return result;
}

bool InterceptingResourceHandler::SendPayloadToOldHandler(bool* defer) {
  DCHECK_EQ(State::SENDING_PAYLOAD_TO_OLD_HANDLER, state_);
  while (payload_bytes_written_ < payload_for_old_handler_.size()) {
    scoped_refptr<net::IOBuffer> buffer;
    int size = 0;
    if (first_read_buffer_) {
      // |first_read_buffer_| is a buffer gotten from |next_handler_| via
      // OnWillRead. Use the buffer.
      buffer = first_read_buffer_;
      size = first_read_buffer_size_;

      first_read_buffer_ = nullptr;
      first_read_buffer_size_ = 0;
    } else {
      if (!next_handler_->OnWillRead(&buffer, &size, -1))
        return false;
    }

    size = std::min(size, static_cast<int>(payload_for_old_handler_.size() -
                                           payload_bytes_written_));
    memcpy(buffer->data(),
           payload_for_old_handler_.data() + payload_bytes_written_, size);
    if (!next_handler_->OnReadCompleted(size, defer))
      return false;
    payload_bytes_written_ += size;
    if (*defer)
      return true;
  }

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  if (payload_for_old_handler_.empty()) {
    // If there is no payload, just finalize the request on the old handler.
    status = net::URLRequestStatus::FromError(net::ERR_ABORTED);
  }
  next_handler_->OnResponseCompleted(status, defer);
  DCHECK(!*defer);

  next_handler_ = std::move(new_handler_);
  state_ = State::SENDING_ON_WILL_START_TO_NEW_HANDLER;
  return next_handler_->OnWillStart(request()->url(), defer);
}

bool InterceptingResourceHandler::SendOnResponseStartedToNewHandler(
    bool* defer) {
  state_ = State::SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER;
  return next_handler_->OnResponseStarted(response_.get(), defer);
}

bool InterceptingResourceHandler::SendFirstReadBufferToNewHandler(bool* defer) {
  DCHECK_EQ(state_, State::SENDING_BUFFER_TO_NEW_HANDLER);

  while (first_read_buffer_bytes_written_ < first_read_buffer_bytes_read_) {
    scoped_refptr<net::IOBuffer> buf;
    int size = 0;
    if (!next_handler_->OnWillRead(&buf, &size, -1))
      return false;
    size = std::min(size, static_cast<int>(first_read_buffer_bytes_read_ -
                                           first_read_buffer_bytes_written_));
    memcpy(buf->data(),
           first_read_buffer_double_->data() + first_read_buffer_bytes_written_,
           size);
    if (!next_handler_->OnReadCompleted(size, defer))
      return false;
    first_read_buffer_bytes_written_ += size;
    if (*defer)
      return true;
  }

  state_ = State::PASS_THROUGH;
  first_read_buffer_double_ = nullptr;
  next_handler_->SetController(controller());
  return true;
}

}  // namespace content
