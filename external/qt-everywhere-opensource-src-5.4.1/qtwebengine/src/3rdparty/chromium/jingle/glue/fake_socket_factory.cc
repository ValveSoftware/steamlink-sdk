// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/fake_socket_factory.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "jingle/glue/utils.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"
#include "third_party/libjingle/source/talk/base/asyncsocket.h"

namespace jingle_glue {

FakeUDPPacketSocket::FakeUDPPacketSocket(FakeSocketManager* fake_socket_manager,
                                         const net::IPEndPoint& address)
    : fake_socket_manager_(fake_socket_manager),
      endpoint_(address), state_(IS_OPEN), error_(0) {
  CHECK(IPEndPointToSocketAddress(endpoint_, &local_address_));
  fake_socket_manager_->AddSocket(this);
}

FakeUDPPacketSocket::~FakeUDPPacketSocket() {
  fake_socket_manager_->RemoveSocket(this);
}

talk_base::SocketAddress FakeUDPPacketSocket::GetLocalAddress() const {
  DCHECK(CalledOnValidThread());
  return local_address_;
}

talk_base::SocketAddress FakeUDPPacketSocket::GetRemoteAddress() const {
  DCHECK(CalledOnValidThread());
  return remote_address_;
}

int FakeUDPPacketSocket::Send(const void *data, size_t data_size,
                              const talk_base::PacketOptions& options) {
  DCHECK(CalledOnValidThread());
  return SendTo(data, data_size, remote_address_, options);
}

int FakeUDPPacketSocket::SendTo(const void *data, size_t data_size,
                                const talk_base::SocketAddress& address,
                                const talk_base::PacketOptions& options) {
  DCHECK(CalledOnValidThread());

  if (state_ == IS_CLOSED) {
    return ENOTCONN;
  }

  net::IPEndPoint destination;
  if (!SocketAddressToIPEndPoint(address, &destination)) {
    return EINVAL;
  }

  const char* data_char = reinterpret_cast<const char*>(data);
  std::vector<char> data_vector(data_char, data_char + data_size);

  fake_socket_manager_->SendPacket(endpoint_, destination, data_vector);

  return data_size;
}

int FakeUDPPacketSocket::Close() {
  DCHECK(CalledOnValidThread());
  state_ = IS_CLOSED;
  return 0;
}

talk_base::AsyncPacketSocket::State FakeUDPPacketSocket::GetState() const {
  DCHECK(CalledOnValidThread());

  switch (state_) {
    case IS_OPEN:
      return STATE_BOUND;
    case IS_CLOSED:
      return STATE_CLOSED;
  }

  NOTREACHED();
  return STATE_CLOSED;
}

int FakeUDPPacketSocket::GetOption(talk_base::Socket::Option opt, int* value) {
  DCHECK(CalledOnValidThread());
  return -1;
}

int FakeUDPPacketSocket::SetOption(talk_base::Socket::Option opt, int value) {
  DCHECK(CalledOnValidThread());
  return -1;
}

int FakeUDPPacketSocket::GetError() const {
  DCHECK(CalledOnValidThread());
  return error_;
}

void FakeUDPPacketSocket::SetError(int error) {
  DCHECK(CalledOnValidThread());
  error_ = error;
}

void FakeUDPPacketSocket::DeliverPacket(const net::IPEndPoint& from,
                                        const std::vector<char>& data) {
  DCHECK(CalledOnValidThread());

  talk_base::SocketAddress address;
  if (!jingle_glue::IPEndPointToSocketAddress(from, &address)) {
    // We should always be able to convert address here because we
    // don't expect IPv6 address on IPv4 connections.
    NOTREACHED();
    return;
  }

  SignalReadPacket(this, &data[0], data.size(), address,
                   talk_base::CreatePacketTime(0));
}

FakeSocketManager::FakeSocketManager()
    : message_loop_(base::MessageLoop::current()) {}

FakeSocketManager::~FakeSocketManager() { }

void FakeSocketManager::SendPacket(const net::IPEndPoint& from,
                                   const net::IPEndPoint& to,
                                   const std::vector<char>& data) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FakeSocketManager::DeliverPacket, this, from, to, data));
}

void FakeSocketManager::DeliverPacket(const net::IPEndPoint& from,
                                      const net::IPEndPoint& to,
                                      const std::vector<char>& data) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  std::map<net::IPEndPoint, FakeUDPPacketSocket*>::iterator it =
      endpoints_.find(to);
  if (it == endpoints_.end()) {
    LOG(WARNING) << "Dropping packet with unknown destination: "
                 << to.ToString();
    return;
  }
  it->second->DeliverPacket(from, data);
}

void FakeSocketManager::AddSocket(FakeUDPPacketSocket* socket_factory) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  endpoints_[socket_factory->endpoint()] = socket_factory;
}

void FakeSocketManager::RemoveSocket(FakeUDPPacketSocket* socket_factory) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  endpoints_.erase(socket_factory->endpoint());
}

FakeSocketFactory::FakeSocketFactory(FakeSocketManager* socket_manager,
                                     const net::IPAddressNumber& address)
    : socket_manager_(socket_manager),
      address_(address),
      last_allocated_port_(0) {
}

FakeSocketFactory::~FakeSocketFactory() {
}

talk_base::AsyncPacketSocket* FakeSocketFactory::CreateUdpSocket(
    const talk_base::SocketAddress& local_address, int min_port, int max_port) {
  CHECK_EQ(min_port, 0);
  CHECK_EQ(max_port, 0);
  return new FakeUDPPacketSocket(
      socket_manager_.get(), net::IPEndPoint(address_, ++last_allocated_port_));
}

talk_base::AsyncPacketSocket* FakeSocketFactory::CreateServerTcpSocket(
    const talk_base::SocketAddress& local_address, int min_port, int max_port,
    int opts) {
  // TODO(sergeyu): Implement fake TCP sockets.
  NOTIMPLEMENTED();
  return NULL;
}

talk_base::AsyncPacketSocket* FakeSocketFactory::CreateClientTcpSocket(
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address,
    const talk_base::ProxyInfo& proxy_info, const std::string& user_agent,
    int opts) {
  // TODO(sergeyu): Implement fake TCP sockets.
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace jingle_glue
