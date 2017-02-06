// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_blob_sender.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "content/browser/renderer_host/websocket_dispatcher_host.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_frame.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

using storage::BlobReader;
using storage::BlobDataHandle;
using storage::BlobStorageContext;

// This must be smaller than the send quota high water mark or this class will
// never send anything.
const int kMinimumNonFinalFrameSize = 8 * 1024;

// The IOBuffer has a fixed size for simplicity.
const size_t kBufferSize = 128 * 1024;

}  // namespace

// This is needed to make DCHECK_EQ(), etc. compile.
std::ostream& operator<<(std::ostream& os, WebSocketBlobSender::State state) {
  static const char* const kStateStrings[] = {
      "NONE",
      "READ_SIZE",
      "READ_SIZE_COMPLETE",
      "WAIT_FOR_QUOTA",
      "WAIT_FOR_QUOTA_COMPLETE",
      "READ",
      "READ_COMPLETE",
  };
  if (state < WebSocketBlobSender::State::NONE ||
      state > WebSocketBlobSender::State::READ_COMPLETE) {
    return os << "Bad State (" << static_cast<int>(state) << ")";
  }
  return os << kStateStrings[static_cast<int>(state)];
}

WebSocketBlobSender::WebSocketBlobSender(std::unique_ptr<Channel> channel)
    : channel_(std::move(channel)) {}

WebSocketBlobSender::~WebSocketBlobSender() {}

int WebSocketBlobSender::Start(
    const std::string& uuid,
    uint64_t expected_size,
    BlobStorageContext* context,
    storage::FileSystemContext* file_system_context,
    base::SingleThreadTaskRunner* file_task_runner,
    net::WebSocketEventInterface::ChannelState* channel_state,
    const net::CompletionCallback& callback) {
  DCHECK(context);
  DCHECK(channel_state);
  DCHECK(!reader_);
  std::unique_ptr<storage::BlobDataHandle> data_handle(
      context->GetBlobDataFromUUID(uuid));
  if (!data_handle)
    return net::ERR_INVALID_HANDLE;
  reader_ = data_handle->CreateReader(file_system_context, file_task_runner);
  expected_size_ = expected_size;
  next_state_ = State::READ_SIZE;
  int rv = DoLoop(net::OK, channel_state);
  if (*channel_state == net::WebSocketEventInterface::CHANNEL_ALIVE &&
      rv == net::ERR_IO_PENDING) {
    callback_ = callback;
  }
  return rv;
}

void WebSocketBlobSender::OnNewSendQuota() {
  if (next_state_ == State::WAIT_FOR_QUOTA)
    DoLoopAsync(net::OK);
  // |this| may be deleted.
}

uint64_t WebSocketBlobSender::ActualSize() const {
  return reader_->total_size();
}

void WebSocketBlobSender::OnReadComplete(int rv) {
  DCHECK_EQ(State::READ_COMPLETE, next_state_);
  DoLoopAsync(rv);
  // |this| may be deleted.
}

void WebSocketBlobSender::OnSizeCalculated(int rv) {
  DCHECK_EQ(State::READ_SIZE_COMPLETE, next_state_);
  DoLoopAsync(rv);
  // |this| may be deleted.
}

int WebSocketBlobSender::DoLoop(int result,
                                Channel::ChannelState* channel_state) {
  DCHECK_NE(State::NONE, next_state_);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = State::NONE;
    switch (state) {
      case State::READ_SIZE:
        DCHECK_EQ(net::OK, rv);
        rv = DoReadSize();
        break;

      case State::READ_SIZE_COMPLETE:
        rv = DoReadSizeComplete(rv);
        break;

      case State::WAIT_FOR_QUOTA:
        DCHECK_EQ(net::OK, rv);
        rv = DoWaitForQuota();
        break;

      case State::WAIT_FOR_QUOTA_COMPLETE:
        DCHECK_EQ(net::OK, rv);
        rv = DoWaitForQuotaComplete();
        break;

      case State::READ:
        DCHECK_EQ(net::OK, rv);
        rv = DoRead();
        break;

      case State::READ_COMPLETE:
        rv = DoReadComplete(rv, channel_state);
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (*channel_state != net::WebSocketEventInterface::CHANNEL_DELETED &&
           rv != net::ERR_IO_PENDING && next_state_ != State::NONE);
  return rv;
}

void WebSocketBlobSender::DoLoopAsync(int result) {
  Channel::ChannelState channel_state =
      net::WebSocketEventInterface::CHANNEL_ALIVE;
  int rv = DoLoop(result, &channel_state);
  if (channel_state == net::WebSocketEventInterface::CHANNEL_ALIVE &&
      rv != net::ERR_IO_PENDING) {
    ResetAndReturn(&callback_).Run(rv);
  }
  // |this| may be deleted.
}

int WebSocketBlobSender::DoReadSize() {
  next_state_ = State::READ_SIZE_COMPLETE;
  // This use of base::Unretained() is safe because BlobReader cannot call the
  // callback after it has been destroyed, and it is owned by this object.
  BlobReader::Status status = reader_->CalculateSize(base::Bind(
      &WebSocketBlobSender::OnSizeCalculated, base::Unretained(this)));
  switch (status) {
    case BlobReader::Status::NET_ERROR:
      return reader_->net_error();

    case BlobReader::Status::IO_PENDING:
      return net::ERR_IO_PENDING;

    case BlobReader::Status::DONE:
      return net::OK;
  }
  NOTREACHED();
  return net::ERR_UNEXPECTED;
}

int WebSocketBlobSender::DoReadSizeComplete(int result) {
  if (result < 0)
    return result;
  if (reader_->total_size() != expected_size_)
    return net::ERR_UPLOAD_FILE_CHANGED;
  bytes_left_ = expected_size_;
  // The result of the call to std::min() must fit inside a size_t because
  // kBufferSize is type size_t.
  size_t buffer_size = static_cast<size_t>(
      std::min(bytes_left_, base::strict_cast<uint64_t>(kBufferSize)));
  buffer_ = new net::IOBuffer(buffer_size);
  next_state_ = State::WAIT_FOR_QUOTA;
  return net::OK;
}

// The WAIT_FOR_QUOTA state has a self-edge; it will wait in this state until
// there is enough quota to send some data.
int WebSocketBlobSender::DoWaitForQuota() {
  size_t quota = channel_->GetSendQuota();
  if (kMinimumNonFinalFrameSize <= quota || bytes_left_ <= quota) {
    next_state_ = State::WAIT_FOR_QUOTA_COMPLETE;
    return net::OK;
  }
  next_state_ = State::WAIT_FOR_QUOTA;
  return net::ERR_IO_PENDING;
}

// State::WAIT_FOR_QUOTA_COMPLETE exists just to give the state machine the
// expected shape. It should be mostly optimised out.
int WebSocketBlobSender::DoWaitForQuotaComplete() {
  next_state_ = State::READ;
  return net::OK;
}

int WebSocketBlobSender::DoRead() {
  next_state_ = State::READ_COMPLETE;
  size_t quota = channel_->GetSendQuota();
  // |desired_bytes| must fit in a size_t because |quota| is of type
  // size_t and so cannot be larger than its maximum value.
  size_t desired_bytes =
      static_cast<size_t>(std::min(bytes_left_, static_cast<uint64_t>(quota)));

  // For simplicity this method only reads as many bytes as are currently
  // needed.
  size_t bytes_to_read = std::min(desired_bytes, kBufferSize);
  int bytes_read = 0;
  DCHECK(reader_);
  DCHECK(buffer_);

  // This use of base::Unretained is safe because the BlobReader object won't
  // call the callback after it has been destroyed, and it belongs to this
  // object.
  BlobReader::Status status = reader_->Read(
      buffer_.get(), bytes_to_read, &bytes_read,
      base::Bind(&WebSocketBlobSender::OnReadComplete, base::Unretained(this)));

  switch (status) {
    case BlobReader::Status::NET_ERROR:
      return reader_->net_error();

    case BlobReader::Status::IO_PENDING:
      return net::ERR_IO_PENDING;

    case BlobReader::Status::DONE:
      return bytes_read;
  }
  NOTREACHED();
  return net::ERR_UNEXPECTED;
}

int WebSocketBlobSender::DoReadComplete(int result,
                                        Channel::ChannelState* channel_state) {
  if (result < 0)
    return result;
  DCHECK_GE(channel_->GetSendQuota(), static_cast<size_t>(result));
  uint64_t bytes_read = static_cast<uint64_t>(result);
  DCHECK_GE(bytes_left_, bytes_read);
  bytes_left_ -= bytes_read;
  bool fin = bytes_left_ == 0;
  std::vector<char> data(buffer_->data(), buffer_->data() + bytes_read);
  DCHECK(fin || data.size() > 0u) << "Non-final frames should be non-empty";
  *channel_state = channel_->SendFrame(fin, data);
  if (*channel_state == net::WebSocketEventInterface::CHANNEL_DELETED) {
    // |this| is deleted.
    return net::ERR_CONNECTION_RESET;
  }

  // It is important not to set next_state_ until after the call to SendFrame()
  // because SendFrame() will sometimes call OnNewSendQuota() synchronously.
  if (!fin)
    next_state_ = State::WAIT_FOR_QUOTA;
  return net::OK;
}

}  // namespace content
