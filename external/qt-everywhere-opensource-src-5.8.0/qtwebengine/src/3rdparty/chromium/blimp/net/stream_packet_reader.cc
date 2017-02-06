// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/stream_packet_reader.h"

#include <iostream>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/common.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace blimp {

std::ostream& operator<<(std::ostream& out,
                         const StreamPacketReader::ReadState state) {
  switch (state) {
    case StreamPacketReader::ReadState::HEADER:
      out << "HEADER";
      break;
    case StreamPacketReader::ReadState::PAYLOAD:
      out << "PAYLOAD";
      break;
    case StreamPacketReader::ReadState::IDLE:
      out << "IDLE";
      break;
  }
  return out;
}

StreamPacketReader::StreamPacketReader(net::StreamSocket* socket,
                                       BlimpConnectionStatistics* statistics)
    : read_state_(ReadState::IDLE),
      socket_(socket),
      statistics_(statistics),
      weak_factory_(this) {
  DCHECK(socket_);
  header_buffer_ = new net::GrowableIOBuffer;
  header_buffer_->SetCapacity(kPacketHeaderSizeBytes);
}

StreamPacketReader::~StreamPacketReader() {}

void StreamPacketReader::ReadPacket(
    const scoped_refptr<net::GrowableIOBuffer>& buf,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(ReadState::IDLE, read_state_);
  if (static_cast<size_t>(buf->capacity()) < kPacketHeaderSizeBytes) {
    buf->SetCapacity(kPacketHeaderSizeBytes);
  }

  header_buffer_->set_offset(0);
  payload_buffer_ = buf;
  payload_buffer_->set_offset(0);
  read_state_ = ReadState::HEADER;

  int result = DoReadLoop(net::OK);
  if (result != net::ERR_IO_PENDING) {
    // Release the payload buffer, since the read operation has completed
    // synchronously.
    payload_buffer_ = nullptr;

    // Adapt synchronous completion to an asynchronous style.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, result == net::OK ? payload_size_ : result));
  } else {
    callback_ = callback;
  }
}

int StreamPacketReader::DoReadLoop(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DCHECK_GE(result, 0);
  DCHECK_NE(ReadState::IDLE, read_state_);

  while (result >= 0 && read_state_ != ReadState::IDLE) {
    VLOG(2) << "DoReadLoop (state=" << read_state_ << ", result=" << result
            << ")";

    switch (read_state_) {
      case ReadState::HEADER:
        result = DoReadHeader(result);
        break;
      case ReadState::PAYLOAD:
        result = DoReadPayload(result);
        break;
      case ReadState::IDLE:
        NOTREACHED();
        result = net::ERR_UNEXPECTED;
        break;
    }
  }

  return result;
}

int StreamPacketReader::DoReadHeader(int result) {
  DCHECK_EQ(ReadState::HEADER, read_state_);
  DCHECK_GT(kPacketHeaderSizeBytes,
            static_cast<size_t>(header_buffer_->offset()));
  DCHECK_GE(result, 0);

  header_buffer_->set_offset(header_buffer_->offset() + result);
  if (static_cast<size_t>(header_buffer_->offset()) < kPacketHeaderSizeBytes) {
    // There is more header to read.
    return DoRead(header_buffer_.get(),
                  kPacketHeaderSizeBytes - header_buffer_->offset());
  }

  // Finished reading the header. Parse the size and prepare for payload read.
  payload_size_ = base::NetToHost32(
      *reinterpret_cast<uint32_t*>(header_buffer_->StartOfBuffer()));
  if (payload_size_ == 0 || payload_size_ > kMaxPacketPayloadSizeBytes) {
    DLOG(ERROR) << "Illegal payload size: " << payload_size_;
    return net::ERR_INVALID_RESPONSE;
  }
  if (static_cast<size_t>(payload_buffer_->capacity()) < payload_size_) {
    payload_buffer_->SetCapacity(payload_size_);
  }
  read_state_ = ReadState::PAYLOAD;
  return net::OK;
}

int StreamPacketReader::DoReadPayload(int result) {
  DCHECK_EQ(ReadState::PAYLOAD, read_state_);
  DCHECK_GE(result, 0);

  payload_buffer_->set_offset(payload_buffer_->offset() + result);
  if (static_cast<size_t>(payload_buffer_->offset()) < payload_size_) {
    return DoRead(payload_buffer_.get(),
                  payload_size_ - payload_buffer_->offset());
  }
  statistics_->Add(BlimpConnectionStatistics::BYTES_RECEIVED, payload_size_);

  // Finished reading the payload.
  read_state_ = ReadState::IDLE;
  payload_buffer_->set_offset(0);
  return payload_size_;
}

void StreamPacketReader::OnReadComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result == 0 /* EOF */) {
    payload_buffer_ = nullptr;
    base::ResetAndReturn(&callback_).Run(net::ERR_CONNECTION_CLOSED);
    return;
  }

  // If the read was succesful, then process the result.
  if (result > 0) {
    result = DoReadLoop(result);
  }

  // If all reading completed, either successfully or by error, inform the
  // caller.
  if (result != net::ERR_IO_PENDING) {
    payload_buffer_ = nullptr;
    base::ResetAndReturn(&callback_).Run(result);
  }
}

int StreamPacketReader::DoRead(net::IOBuffer* buf, int buf_len) {
  int result = socket_->Read(buf, buf_len,
                             base::Bind(&StreamPacketReader::OnReadComplete,
                                        weak_factory_.GetWeakPtr()));
  return (result != 0 ? result : net::ERR_CONNECTION_CLOSED);
}

}  // namespace blimp
