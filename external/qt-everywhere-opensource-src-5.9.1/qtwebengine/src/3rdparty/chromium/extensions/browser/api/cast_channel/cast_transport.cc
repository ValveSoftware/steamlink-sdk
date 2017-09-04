// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_transport.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/api/cast_channel/cast_framer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/browser/api/cast_channel/logger_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

#define VLOG_WITH_CONNECTION(level)                                           \
  VLOG(level) << "[" << ip_endpoint_.ToString() << ", auth=" << channel_auth_ \
              << "] "

namespace extensions {
namespace api {
namespace cast_channel {

CastTransportImpl::CastTransportImpl(net::Socket* socket,
                                     int channel_id,
                                     const net::IPEndPoint& ip_endpoint,
                                     ChannelAuthType channel_auth,
                                     scoped_refptr<Logger> logger)
    : started_(false),
      socket_(socket),
      write_state_(WRITE_STATE_IDLE),
      read_state_(READ_STATE_READ),
      error_state_(CHANNEL_ERROR_NONE),
      channel_id_(channel_id),
      ip_endpoint_(ip_endpoint),
      channel_auth_(channel_auth),
      logger_(logger) {
  DCHECK(socket);

  // Buffer is reused across messages to minimize unnecessary buffer
  // [re]allocations.
  read_buffer_ = new net::GrowableIOBuffer();
  read_buffer_->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  framer_.reset(new MessageFramer(read_buffer_));
}

CastTransportImpl::~CastTransportImpl() {
  DCHECK(CalledOnValidThread());
  FlushWriteQueue();
}

bool CastTransportImpl::IsTerminalWriteState(
    CastTransportImpl::WriteState write_state) {
  return write_state == WRITE_STATE_ERROR || write_state == WRITE_STATE_IDLE;
}

bool CastTransportImpl::IsTerminalReadState(
    CastTransportImpl::ReadState read_state) {
  return read_state == READ_STATE_ERROR;
}

// static
proto::ReadState CastTransportImpl::ReadStateToProto(
    CastTransportImpl::ReadState state) {
  switch (state) {
    case CastTransportImpl::READ_STATE_UNKNOWN:
      return proto::READ_STATE_UNKNOWN;
    case CastTransportImpl::READ_STATE_READ:
      return proto::READ_STATE_READ;
    case CastTransportImpl::READ_STATE_READ_COMPLETE:
      return proto::READ_STATE_READ_COMPLETE;
    case CastTransportImpl::READ_STATE_DO_CALLBACK:
      return proto::READ_STATE_DO_CALLBACK;
    case CastTransportImpl::READ_STATE_HANDLE_ERROR:
      return proto::READ_STATE_HANDLE_ERROR;
    case CastTransportImpl::READ_STATE_ERROR:
      return proto::READ_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::READ_STATE_UNKNOWN;
  }
}

// static
proto::WriteState CastTransportImpl::WriteStateToProto(
    CastTransportImpl::WriteState state) {
  switch (state) {
    case CastTransportImpl::WRITE_STATE_IDLE:
      return proto::WRITE_STATE_IDLE;
    case CastTransportImpl::WRITE_STATE_UNKNOWN:
      return proto::WRITE_STATE_UNKNOWN;
    case CastTransportImpl::WRITE_STATE_WRITE:
      return proto::WRITE_STATE_WRITE;
    case CastTransportImpl::WRITE_STATE_WRITE_COMPLETE:
      return proto::WRITE_STATE_WRITE_COMPLETE;
    case CastTransportImpl::WRITE_STATE_DO_CALLBACK:
      return proto::WRITE_STATE_DO_CALLBACK;
    case CastTransportImpl::WRITE_STATE_HANDLE_ERROR:
      return proto::WRITE_STATE_HANDLE_ERROR;
    case CastTransportImpl::WRITE_STATE_ERROR:
      return proto::WRITE_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::WRITE_STATE_UNKNOWN;
  }
}

// static
proto::ErrorState CastTransportImpl::ErrorStateToProto(ChannelError state) {
  switch (state) {
    case CHANNEL_ERROR_NONE:
      return proto::CHANNEL_ERROR_NONE;
    case CHANNEL_ERROR_CHANNEL_NOT_OPEN:
      return proto::CHANNEL_ERROR_CHANNEL_NOT_OPEN;
    case CHANNEL_ERROR_AUTHENTICATION_ERROR:
      return proto::CHANNEL_ERROR_AUTHENTICATION_ERROR;
    case CHANNEL_ERROR_CONNECT_ERROR:
      return proto::CHANNEL_ERROR_CONNECT_ERROR;
    case CHANNEL_ERROR_SOCKET_ERROR:
      return proto::CHANNEL_ERROR_SOCKET_ERROR;
    case CHANNEL_ERROR_TRANSPORT_ERROR:
      return proto::CHANNEL_ERROR_TRANSPORT_ERROR;
    case CHANNEL_ERROR_INVALID_MESSAGE:
      return proto::CHANNEL_ERROR_INVALID_MESSAGE;
    case CHANNEL_ERROR_INVALID_CHANNEL_ID:
      return proto::CHANNEL_ERROR_INVALID_CHANNEL_ID;
    case CHANNEL_ERROR_CONNECT_TIMEOUT:
      return proto::CHANNEL_ERROR_CONNECT_TIMEOUT;
    case CHANNEL_ERROR_UNKNOWN:
      return proto::CHANNEL_ERROR_UNKNOWN;
    default:
      NOTREACHED();
      return proto::CHANNEL_ERROR_NONE;
  }
}

void CastTransportImpl::SetReadDelegate(std::unique_ptr<Delegate> delegate) {
  DCHECK(CalledOnValidThread());
  DCHECK(delegate);
  delegate_ = std::move(delegate);
  if (started_) {
    delegate_->Start();
  }
}

void CastTransportImpl::FlushWriteQueue() {
  for (; !write_queue_.empty(); write_queue_.pop()) {
    net::CompletionCallback& callback = write_queue_.front().callback;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, net::ERR_FAILED));
    callback.Reset();
  }
}

void CastTransportImpl::SendMessage(const CastMessage& message,
                                    const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string serialized_message;
  if (!MessageFramer::Serialize(message, &serialized_message)) {
    logger_->LogSocketEventForMessage(channel_id_, proto::SEND_MESSAGE_FAILED,
                                      message.namespace_(),
                                      "Error when serializing message.");
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, net::ERR_FAILED));
    return;
  }
  WriteRequest write_request(
      message.namespace_(), serialized_message, callback);

  write_queue_.push(write_request);
  logger_->LogSocketEventForMessage(
      channel_id_, proto::MESSAGE_ENQUEUED, message.namespace_(),
      base::StringPrintf("Queue size: %" PRIuS, write_queue_.size()));
  if (write_state_ == WRITE_STATE_IDLE) {
    SetWriteState(WRITE_STATE_WRITE);
    OnWriteResult(net::OK);
  }
}

CastTransportImpl::WriteRequest::WriteRequest(
    const std::string& namespace_,
    const std::string& payload,
    const net::CompletionCallback& callback)
    : message_namespace(namespace_), callback(callback) {
  VLOG(2) << "WriteRequest size: " << payload.size();
  io_buffer = new net::DrainableIOBuffer(new net::StringIOBuffer(payload),
                                         payload.size());
}

CastTransportImpl::WriteRequest::WriteRequest(const WriteRequest& other) =
    default;

CastTransportImpl::WriteRequest::~WriteRequest() {
}

void CastTransportImpl::SetReadState(ReadState read_state) {
  if (read_state_ != read_state) {
    read_state_ = read_state;
    logger_->LogSocketReadState(channel_id_, ReadStateToProto(read_state_));
  }
}

void CastTransportImpl::SetWriteState(WriteState write_state) {
  if (write_state_ != write_state) {
    write_state_ = write_state;
    logger_->LogSocketWriteState(channel_id_, WriteStateToProto(write_state_));
  }
}

void CastTransportImpl::SetErrorState(ChannelError error_state) {
  VLOG_WITH_CONNECTION(2) << "SetErrorState: " << error_state;
  error_state_ = error_state;
}

void CastTransportImpl::OnWriteResult(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(WRITE_STATE_IDLE, write_state_);
  if (write_queue_.empty()) {
    SetWriteState(WRITE_STATE_IDLE);
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2) << "OnWriteResult (state=" << write_state_ << ", "
                            << "result=" << rv << ", "
                            << "queue size=" << write_queue_.size() << ")";

    WriteState state = write_state_;
    write_state_ = WRITE_STATE_UNKNOWN;
    switch (state) {
      case WRITE_STATE_WRITE:
        rv = DoWrite();
        break;
      case WRITE_STATE_WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case WRITE_STATE_DO_CALLBACK:
        rv = DoWriteCallback();
        break;
      case WRITE_STATE_HANDLE_ERROR:
        rv = DoWriteHandleError(rv);
        DCHECK_EQ(WRITE_STATE_ERROR, write_state_);
        break;
      default:
        NOTREACHED() << "Unknown state in write state machine: " << state;
        SetWriteState(WRITE_STATE_ERROR);
        SetErrorState(CHANNEL_ERROR_UNKNOWN);
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalWriteState(write_state_));

  if (IsTerminalWriteState(write_state_)) {
    logger_->LogSocketWriteState(channel_id_, WriteStateToProto(write_state_));

    if (write_state_ == WRITE_STATE_ERROR) {
      FlushWriteQueue();
      DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
      VLOG_WITH_CONNECTION(2) << "Sending OnError().";
      delegate_->OnError(error_state_);
    }
  }
}

int CastTransportImpl::DoWrite() {
  DCHECK(!write_queue_.empty());
  WriteRequest& request = write_queue_.front();

  VLOG_WITH_CONNECTION(2) << "WriteData byte_count = "
                          << request.io_buffer->size() << " bytes_written "
                          << request.io_buffer->BytesConsumed();

  SetWriteState(WRITE_STATE_WRITE_COMPLETE);

  int rv = socket_->Write(
      request.io_buffer.get(), request.io_buffer->BytesRemaining(),
      base::Bind(&CastTransportImpl::OnWriteResult, base::Unretained(this)));
  return rv;
}

int CastTransportImpl::DoWriteComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteComplete result=" << result;
  DCHECK(!write_queue_.empty());
  logger_->LogSocketEventWithRv(channel_id_, proto::SOCKET_WRITE, result);
  if (result <= 0) {  // NOTE that 0 also indicates an error
    SetErrorState(CHANNEL_ERROR_SOCKET_ERROR);
    SetWriteState(WRITE_STATE_HANDLE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  // Some bytes were successfully written
  WriteRequest& request = write_queue_.front();
  scoped_refptr<net::DrainableIOBuffer> io_buffer = request.io_buffer;
  io_buffer->DidConsume(result);
  if (io_buffer->BytesRemaining() == 0) {  // Message fully sent
    SetWriteState(WRITE_STATE_DO_CALLBACK);
  } else {
    SetWriteState(WRITE_STATE_WRITE);
  }

  return net::OK;
}

int CastTransportImpl::DoWriteCallback() {
  VLOG_WITH_CONNECTION(2) << "DoWriteCallback";
  DCHECK(!write_queue_.empty());

  WriteRequest& request = write_queue_.front();
  int bytes_consumed = request.io_buffer->BytesConsumed();
  logger_->LogSocketEventForMessage(
      channel_id_, proto::MESSAGE_WRITTEN, request.message_namespace,
      base::StringPrintf("Bytes: %d", bytes_consumed));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(request.callback, net::OK));

  write_queue_.pop();
  if (write_queue_.empty()) {
    SetWriteState(WRITE_STATE_IDLE);
  } else {
    SetWriteState(WRITE_STATE_WRITE);
  }

  return net::OK;
}

int CastTransportImpl::DoWriteHandleError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteHandleError result=" << result;
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LT(result, 0);
  SetWriteState(WRITE_STATE_ERROR);
  return net::ERR_FAILED;
}

void CastTransportImpl::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK(!started_);
  DCHECK_EQ(READ_STATE_READ, read_state_);
  DCHECK(delegate_) << "Read delegate must be set prior to calling Start()";
  started_ = true;
  delegate_->Start();
  SetReadState(READ_STATE_READ);

  // Start the read state machine.
  OnReadResult(net::OK);
}

void CastTransportImpl::OnReadResult(int result) {
  DCHECK(CalledOnValidThread());
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2) << "OnReadResult(state=" << read_state_
                            << ", result=" << rv << ")";
    ReadState state = read_state_;
    read_state_ = READ_STATE_UNKNOWN;

    switch (state) {
      case READ_STATE_READ:
        rv = DoRead();
        break;
      case READ_STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case READ_STATE_DO_CALLBACK:
        rv = DoReadCallback();
        break;
      case READ_STATE_HANDLE_ERROR:
        rv = DoReadHandleError(rv);
        DCHECK_EQ(read_state_, READ_STATE_ERROR);
        break;
      default:
        NOTREACHED() << "Unknown state in read state machine: " << state;
        SetReadState(READ_STATE_ERROR);
        SetErrorState(CHANNEL_ERROR_UNKNOWN);
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalReadState(read_state_));

  if (IsTerminalReadState(read_state_)) {
    DCHECK_EQ(READ_STATE_ERROR, read_state_);
    logger_->LogSocketReadState(channel_id_, ReadStateToProto(read_state_));
    VLOG_WITH_CONNECTION(2) << "Sending OnError().";
    delegate_->OnError(error_state_);
  }
}

int CastTransportImpl::DoRead() {
  VLOG_WITH_CONNECTION(2) << "DoRead";
  SetReadState(READ_STATE_READ_COMPLETE);

  // Determine how many bytes need to be read.
  size_t num_bytes_to_read = framer_->BytesRequested();
  DCHECK_GT(num_bytes_to_read, 0u);

  // Read up to num_bytes_to_read into |current_read_buffer_|.
  return socket_->Read(
      read_buffer_.get(), base::checked_cast<uint32_t>(num_bytes_to_read),
      base::Bind(&CastTransportImpl::OnReadResult, base::Unretained(this)));
}

int CastTransportImpl::DoReadComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadComplete result = " << result;
  logger_->LogSocketEventWithRv(channel_id_, proto::SOCKET_READ, result);
  if (result <= 0) {
    VLOG_WITH_CONNECTION(1) << "Read error, peer closed the socket.";
    SetErrorState(CHANNEL_ERROR_SOCKET_ERROR);
    SetReadState(READ_STATE_HANDLE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  size_t message_size;
  DCHECK(!current_message_);
  ChannelError framing_error;
  current_message_ = framer_->Ingest(result, &message_size, &framing_error);
  if (current_message_.get() && (framing_error == CHANNEL_ERROR_NONE)) {
    DCHECK_GT(message_size, static_cast<size_t>(0));
    logger_->LogSocketEventForMessage(
        channel_id_, proto::MESSAGE_READ, current_message_->namespace_(),
        base::StringPrintf("Message size: %u",
                           static_cast<uint32_t>(message_size)));
    SetReadState(READ_STATE_DO_CALLBACK);
  } else if (framing_error != CHANNEL_ERROR_NONE) {
    DCHECK(!current_message_);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    SetReadState(READ_STATE_HANDLE_ERROR);
  } else {
    DCHECK(!current_message_);
    SetReadState(READ_STATE_READ);
  }
  return net::OK;
}

int CastTransportImpl::DoReadCallback() {
  VLOG_WITH_CONNECTION(2) << "DoReadCallback";
  if (!IsCastMessageValid(*current_message_)) {
    SetReadState(READ_STATE_HANDLE_ERROR);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    return net::ERR_INVALID_RESPONSE;
  }
  SetReadState(READ_STATE_READ);
  delegate_->OnMessage(*current_message_);
  current_message_.reset();
  return net::OK;
}

int CastTransportImpl::DoReadHandleError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadHandleError";
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LE(result, 0);
  SetReadState(READ_STATE_ERROR);
  return net::ERR_FAILED;
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
