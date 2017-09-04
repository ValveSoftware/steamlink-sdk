// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/udp_socket.h"

#include <algorithm>

#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "extensions/browser/api/api_resource.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/udp_client_socket.h"

namespace extensions {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableUDPSocket> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableUDPSocket> >*
ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

UDPSocket::UDPSocket(const std::string& owner_extension_id)
    : Socket(owner_extension_id),
      socket_(net::DatagramSocket::DEFAULT_BIND,
              net::RandIntCallback(),
              NULL,
              net::NetLogSource()) {}

UDPSocket::~UDPSocket() {
  Disconnect(true /* socket_destroying */);
}

void UDPSocket::Connect(const net::AddressList& address,
                        const CompletionCallback& callback) {
  int result = net::ERR_CONNECTION_FAILED;
  do {
    if (is_connected_)
      break;

    // UDP API only connects to the first address received from DNS so
    // connection may not work even if other addresses are reachable.
    const net::IPEndPoint& ip_end_point = address.front();
    result = socket_.Open(ip_end_point.GetFamily());
    if (result != net::OK)
      break;

    result = socket_.Connect(ip_end_point);
    if (result != net::OK) {
      socket_.Close();
      break;
    }
    is_connected_ = true;
  } while (false);

  callback.Run(result);
}

int UDPSocket::Bind(const std::string& address, uint16_t port) {
  if (IsBound())
    return net::ERR_CONNECTION_FAILED;

  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point))
    return net::ERR_INVALID_ARGUMENT;

  int result = socket_.Open(ip_end_point.GetFamily());
  if (result != net::OK)
    return result;

  result = socket_.Bind(ip_end_point);
  if (result != net::OK)
    socket_.Close();
  return result;
}

void UDPSocket::Disconnect(bool socket_destroying) {
  is_connected_ = false;
  socket_.Close();
  read_callback_.Reset();
  // TODO(devlin): Should we do this for all callbacks?
  if (!recv_from_callback_.is_null()) {
    base::ResetAndReturn(&recv_from_callback_)
        .Run(net::ERR_CONNECTION_CLOSED, nullptr, true /* socket_destroying */,
             std::string(), 0);
  }
  send_to_callback_.Reset();
  multicast_groups_.clear();
}

void UDPSocket::Read(int count, const ReadCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!read_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, nullptr, false /* socket_destroying */);
    return;
  } else {
    read_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  scoped_refptr<net::IOBuffer> io_buffer;
  do {
    if (count < 0) {
      result = net::ERR_INVALID_ARGUMENT;
      break;
    }

    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    io_buffer = new net::IOBuffer(count);
    result = socket_.Read(
        io_buffer.get(),
        count,
        base::Bind(
            &UDPSocket::OnReadComplete, base::Unretained(this), io_buffer));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnReadComplete(io_buffer, result);
}

int UDPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         const net::CompletionCallback& callback) {
  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;
  else
    return socket_.Write(io_buffer, io_buffer_size, callback);
}

void UDPSocket::RecvFrom(int count,
                         const RecvFromCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!recv_from_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, nullptr, false /* socket_destroying */,
                 std::string(), 0);
    return;
  } else {
    recv_from_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  scoped_refptr<net::IOBuffer> io_buffer;
  scoped_refptr<IPEndPoint> address;
  do {
    if (count < 0) {
      result = net::ERR_INVALID_ARGUMENT;
      break;
    }

    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    io_buffer = new net::IOBuffer(count);
    address = new IPEndPoint();
    result = socket_.RecvFrom(io_buffer.get(),
                              count,
                              &address->data,
                              base::Bind(&UDPSocket::OnRecvFromComplete,
                                         base::Unretained(this),
                                         io_buffer,
                                         address));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnRecvFromComplete(io_buffer, address, result);
}

void UDPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                       int byte_count,
                       const net::IPEndPoint& address,
                       const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!send_to_callback_.is_null()) {
    // TODO(penghuang): Put requests in a pending queue to support multiple
    // sendTo calls.
    callback.Run(net::ERR_IO_PENDING);
    return;
  } else {
    send_to_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  do {
    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    result = socket_.SendTo(
        io_buffer.get(), byte_count, address,
        base::Bind(&UDPSocket::OnSendToComplete, base::Unretained(this)));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnSendToComplete(result);
}

bool UDPSocket::IsConnected() { return is_connected_; }

bool UDPSocket::GetPeerAddress(net::IPEndPoint* address) {
  return !socket_.GetPeerAddress(address);
}

bool UDPSocket::GetLocalAddress(net::IPEndPoint* address) {
  return !socket_.GetLocalAddress(address);
}

Socket::SocketType UDPSocket::GetSocketType() const { return Socket::TYPE_UDP; }

void UDPSocket::OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer,
                               int result) {
  DCHECK(!read_callback_.is_null());
  read_callback_.Run(result, io_buffer, false /* socket_destroying */);
  read_callback_.Reset();
}

void UDPSocket::OnRecvFromComplete(scoped_refptr<net::IOBuffer> io_buffer,
                                   scoped_refptr<IPEndPoint> address,
                                   int result) {
  DCHECK(!recv_from_callback_.is_null());
  std::string ip;
  uint16_t port = 0;
  if (result > 0 && address.get()) {
    IPEndPointToStringAndPort(address->data, &ip, &port);
  }
  recv_from_callback_.Run(result, io_buffer, false /* socket_destroying */, ip,
                          port);
  recv_from_callback_.Reset();
}

void UDPSocket::OnSendToComplete(int result) {
  DCHECK(!send_to_callback_.is_null());
  send_to_callback_.Run(result);
  send_to_callback_.Reset();
}

bool UDPSocket::IsBound() { return socket_.is_connected(); }

int UDPSocket::JoinGroup(const std::string& address) {
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(address))
    return net::ERR_ADDRESS_INVALID;

  std::string normalized_address = ip.ToString();
  std::vector<std::string>::iterator find_result = std::find(
      multicast_groups_.begin(), multicast_groups_.end(), normalized_address);
  if (find_result != multicast_groups_.end())
    return net::ERR_ADDRESS_INVALID;

  int rv = socket_.JoinGroup(ip);
  if (rv == 0)
    multicast_groups_.push_back(normalized_address);
  return rv;
}

int UDPSocket::LeaveGroup(const std::string& address) {
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(address))
    return net::ERR_ADDRESS_INVALID;

  std::string normalized_address = ip.ToString();
  std::vector<std::string>::iterator find_result = std::find(
      multicast_groups_.begin(), multicast_groups_.end(), normalized_address);
  if (find_result == multicast_groups_.end())
    return net::ERR_ADDRESS_INVALID;

  int rv = socket_.LeaveGroup(ip);
  if (rv == 0)
    multicast_groups_.erase(find_result);
  return rv;
}

int UDPSocket::SetMulticastTimeToLive(int ttl) {
  return socket_.SetMulticastTimeToLive(ttl);
}

int UDPSocket::SetMulticastLoopbackMode(bool loopback) {
  return socket_.SetMulticastLoopbackMode(loopback);
}

int UDPSocket::SetBroadcast(bool enabled) {
  if (!socket_.is_connected()) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return socket_.SetBroadcast(enabled);
}

const std::vector<std::string>& UDPSocket::GetJoinedGroups() const {
  return multicast_groups_;
}

ResumableUDPSocket::ResumableUDPSocket(const std::string& owner_extension_id)
    : UDPSocket(owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

bool ResumableUDPSocket::IsPersistent() const { return persistent(); }

}  // namespace extensions
