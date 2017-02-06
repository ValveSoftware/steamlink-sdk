// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/tcp_client_transport.h"

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/stream_socket_connection.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"

namespace blimp {

TCPClientTransport::TCPClientTransport(const net::IPEndPoint& ip_endpoint,
                                       BlimpConnectionStatistics* statistics,
                                       net::NetLog* net_log)
    : ip_endpoint_(ip_endpoint),
      blimp_connection_statistics_(statistics),
      net_log_(net_log),
      socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
  DCHECK(blimp_connection_statistics_);
}

TCPClientTransport::~TCPClientTransport() {}

void TCPClientTransport::SetClientSocketFactoryForTest(
    net::ClientSocketFactory* factory) {
  DCHECK(factory);
  socket_factory_ = factory;
}

void TCPClientTransport::Connect(const net::CompletionCallback& callback) {
  DCHECK(!socket_);
  DCHECK(!callback.is_null());

  connect_callback_ = callback;
  socket_ = socket_factory_->CreateTransportClientSocket(
      net::AddressList(ip_endpoint_), nullptr, net_log_, net::NetLog::Source());
  net::CompletionCallback completion_callback = base::Bind(
      &TCPClientTransport::OnTCPConnectComplete, base::Unretained(this));

  int result = socket_->Connect(completion_callback);
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  OnTCPConnectComplete(result);
}

std::unique_ptr<BlimpConnection> TCPClientTransport::TakeConnection() {
  DCHECK(connect_callback_.is_null());
  DCHECK(socket_);
  return base::WrapUnique(new StreamSocketConnection(
      std::move(socket_), blimp_connection_statistics_));
}

const char* TCPClientTransport::GetName() const {
  return "TCP";
}

void TCPClientTransport::OnTCPConnectComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  OnConnectComplete(result);
}

void TCPClientTransport::OnConnectComplete(int result) {
  if (result != net::OK) {
    socket_ = nullptr;
  }
  base::ResetAndReturn(&connect_callback_).Run(result);
}

std::unique_ptr<net::StreamSocket> TCPClientTransport::TakeSocket() {
  return std::move(socket_);
}

void TCPClientTransport::SetSocket(std::unique_ptr<net::StreamSocket> socket) {
  DCHECK(socket);
  socket_ = std::move(socket);
}

net::ClientSocketFactory* TCPClientTransport::socket_factory() const {
  return socket_factory_;
}

}  // namespace blimp
