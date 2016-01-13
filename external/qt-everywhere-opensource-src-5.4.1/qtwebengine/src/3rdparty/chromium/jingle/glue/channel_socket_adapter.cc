// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/channel_socket_adapter.h"

#include <limits>

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "third_party/libjingle/source/talk/p2p/base/transportchannel.h"

namespace jingle_glue {

TransportChannelSocketAdapter::TransportChannelSocketAdapter(
    cricket::TransportChannel* channel)
    : message_loop_(base::MessageLoop::current()),
      channel_(channel),
      closed_error_code_(net::OK) {
  DCHECK(channel_);

  channel_->SignalReadPacket.connect(
      this, &TransportChannelSocketAdapter::OnNewPacket);
  channel_->SignalWritableState.connect(
      this, &TransportChannelSocketAdapter::OnWritableState);
  channel_->SignalDestroyed.connect(
      this, &TransportChannelSocketAdapter::OnChannelDestroyed);
}

TransportChannelSocketAdapter::~TransportChannelSocketAdapter() {
  if (!destruction_callback_.is_null())
    destruction_callback_.Run();
}

void TransportChannelSocketAdapter::SetOnDestroyedCallback(
    const base::Closure& callback) {
  destruction_callback_ = callback;
}

int TransportChannelSocketAdapter::Read(
    net::IOBuffer* buf,
    int buffer_size,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK(buf);
  DCHECK(!callback.is_null());
  CHECK(read_callback_.is_null());

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  read_callback_ = callback;
  read_buffer_ = buf;
  read_buffer_size_ = buffer_size;

  return net::ERR_IO_PENDING;
}

int TransportChannelSocketAdapter::Write(
    net::IOBuffer* buffer,
    int buffer_size,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK(buffer);
  DCHECK(!callback.is_null());
  CHECK(write_callback_.is_null());

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  int result;
  talk_base::PacketOptions options;
  if (channel_->writable()) {
    result = channel_->SendPacket(buffer->data(), buffer_size, options);
    if (result < 0) {
      result = net::MapSystemError(channel_->GetError());

      // If the underlying socket returns IO pending where it shouldn't we
      // pretend the packet is dropped and return as succeeded because no
      // writeable callback will happen.
      if (result == net::ERR_IO_PENDING)
        result = net::OK;
    }
  } else {
    // Channel is not writable yet.
    result = net::ERR_IO_PENDING;
    write_callback_ = callback;
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
  }

  return result;
}

int TransportChannelSocketAdapter::SetReceiveBufferSize(int32 size) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return (channel_->SetOption(talk_base::Socket::OPT_RCVBUF, size) == 0) ?
      net::OK : net::ERR_SOCKET_SET_RECEIVE_BUFFER_SIZE_ERROR;
}

int TransportChannelSocketAdapter::SetSendBufferSize(int32 size) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return (channel_->SetOption(talk_base::Socket::OPT_SNDBUF, size) == 0) ?
      net::OK : net::ERR_SOCKET_SET_SEND_BUFFER_SIZE_ERROR;
}

void TransportChannelSocketAdapter::Close(int error_code) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  if (!channel_)  // Already closed.
    return;

  DCHECK(error_code != net::OK);
  closed_error_code_ = error_code;
  channel_->SignalReadPacket.disconnect(this);
  channel_->SignalDestroyed.disconnect(this);
  channel_ = NULL;

  if (!read_callback_.is_null()) {
    net::CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_ = NULL;
    callback.Run(error_code);
  }

  if (!write_callback_.is_null()) {
    net::CompletionCallback callback = write_callback_;
    write_callback_.Reset();
    write_buffer_ = NULL;
    callback.Run(error_code);
  }
}

void TransportChannelSocketAdapter::OnNewPacket(
    cricket::TransportChannel* channel,
    const char* data,
    size_t data_size,
    const talk_base::PacketTime& packet_time,
    int flags) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK_EQ(channel, channel_);
  if (!read_callback_.is_null()) {
    DCHECK(read_buffer_.get());
    CHECK_LT(data_size, static_cast<size_t>(std::numeric_limits<int>::max()));

    if (read_buffer_size_ < static_cast<int>(data_size)) {
      LOG(WARNING) << "Data buffer is smaller than the received packet. "
                   << "Dropping the data that doesn't fit.";
      data_size = read_buffer_size_;
    }

    memcpy(read_buffer_->data(), data, data_size);

    net::CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_ = NULL;

    callback.Run(data_size);
  } else {
    LOG(WARNING)
        << "Data was received without a callback. Dropping the packet.";
  }
}

void TransportChannelSocketAdapter::OnWritableState(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  // Try to send the packet if there is a pending write.
  if (!write_callback_.is_null()) {
    talk_base::PacketOptions options;
    int result = channel_->SendPacket(write_buffer_->data(),
                                      write_buffer_size_,
                                      options);
    if (result < 0)
      result = net::MapSystemError(channel_->GetError());

    if (result != net::ERR_IO_PENDING) {
      net::CompletionCallback callback = write_callback_;
      write_callback_.Reset();
      write_buffer_ = NULL;
      callback.Run(result);
    }
  }
}

void TransportChannelSocketAdapter::OnChannelDestroyed(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK_EQ(channel, channel_);
  Close(net::ERR_CONNECTION_ABORTED);
}

}  // namespace jingle_glue
