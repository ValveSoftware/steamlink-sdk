// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/p2p/quic_p2p_session.h"

#include <utility>

#include "base/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/quic/p2p/quic_p2p_crypto_stream.h"
#include "net/quic/quic_connection.h"
#include "net/socket/socket.h"

namespace net {

QuicP2PSession::QuicP2PSession(const QuicConfig& config,
                               const QuicP2PCryptoConfig& crypto_config,
                               std::unique_ptr<QuicConnection> connection,
                               std::unique_ptr<Socket> socket)
    : QuicSession(connection.release(), config),
      socket_(std::move(socket)),
      crypto_stream_(new QuicP2PCryptoStream(this, crypto_config)),
      read_buffer_(new IOBuffer(static_cast<size_t>(kMaxPacketSize))) {
  DCHECK(config.negotiated());
}

QuicP2PSession::~QuicP2PSession() {}

void QuicP2PSession::Initialize() {
  QuicSession::Initialize();
  crypto_stream_->Connect();
  DoReadLoop(OK);
}

void QuicP2PSession::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

QuicCryptoStream* QuicP2PSession::GetCryptoStream() {
  return crypto_stream_.get();
}

QuicP2PStream* QuicP2PSession::CreateIncomingDynamicStream(QuicStreamId id) {
  QuicP2PStream* stream = new QuicP2PStream(id, this);
  if (delegate_) {
    delegate_->OnIncomingStream(stream);
  }
  ActivateStream(stream);
  return stream;
}

QuicP2PStream* QuicP2PSession::CreateOutgoingDynamicStream(
    SpdyPriority priority) {
  QuicP2PStream* stream = new QuicP2PStream(GetNextOutgoingStreamId(), this);
  if (stream) {
    ActivateStream(stream);
  }
  return stream;
}

void QuicP2PSession::OnConnectionClosed(QuicErrorCode error,
                                        const std::string& error_details,
                                        ConnectionCloseSource source) {
  QuicSession::OnConnectionClosed(error, error_details, source);

  socket_.reset();

  if (delegate_) {
    Delegate* delegate = delegate_;
    delegate_ = nullptr;
    delegate->OnConnectionClosed(error);
  }
}

void QuicP2PSession::DoReadLoop(int result) {
  while (error() == QUIC_NO_ERROR) {
    switch (read_state_) {
      case READ_STATE_DO_READ:
        CHECK_EQ(result, OK);
        result = DoRead();
        break;
      case READ_STATE_DO_READ_COMPLETE:
        result = DoReadComplete(result);
        break;
      default:
        NOTREACHED() << "read_state_: " << read_state_;
        break;
    }

    if (result < 0)
      break;
  }
}

int QuicP2PSession::DoRead() {
  DCHECK_EQ(read_state_, READ_STATE_DO_READ);
  read_state_ = READ_STATE_DO_READ_COMPLETE;

  if (!socket_) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  return socket_->Read(
      read_buffer_.get(), kMaxPacketSize,
      base::Bind(&QuicP2PSession::DoReadLoop, base::Unretained(this)));
}

int QuicP2PSession::DoReadComplete(int result) {
  DCHECK_EQ(read_state_, READ_STATE_DO_READ_COMPLETE);
  read_state_ = READ_STATE_DO_READ;

  if (result <= 0) {
    connection()->CloseConnection(QUIC_PACKET_READ_ERROR, "packet read error",
                                  ConnectionCloseBehavior::SILENT_CLOSE);
    return result;
  }

  QuicReceivedPacket packet(read_buffer_->data(), result, clock_.Now());
  connection()->ProcessUdpPacket(connection()->self_address(),
                                 connection()->peer_address(), packet);
  return OK;
}

}  // namespace net
