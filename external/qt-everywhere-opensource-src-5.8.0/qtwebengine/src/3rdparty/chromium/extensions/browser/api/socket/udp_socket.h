// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_UDP_SOCKET_H_
#define EXTENSIONS_BROWSER_API_SOCKET_UDP_SOCKET_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "extensions/browser/api/socket/socket.h"
#include "net/udp/udp_socket.h"

namespace extensions {

class UDPSocket : public Socket {
 public:
  explicit UDPSocket(const std::string& owner_extension_id);
  ~UDPSocket() override;

  void Connect(const net::AddressList& address,
               const CompletionCallback& callback) override;
  void Disconnect() override;
  int Bind(const std::string& address, uint16_t port) override;
  void Read(int count, const ReadCompletionCallback& callback) override;
  void RecvFrom(int count, const RecvFromCompletionCallback& callback) override;
  void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
              int byte_count,
              const net::IPEndPoint& address,
              const CompletionCallback& callback) override;

  bool IsConnected() override;

  bool GetPeerAddress(net::IPEndPoint* address) override;
  bool GetLocalAddress(net::IPEndPoint* address) override;
  Socket::SocketType GetSocketType() const override;

  bool IsBound();

  int JoinGroup(const std::string& address);
  int LeaveGroup(const std::string& address);

  int SetMulticastTimeToLive(int ttl);
  int SetMulticastLoopbackMode(bool loopback);

  int SetBroadcast(bool enabled);

  const std::vector<std::string>& GetJoinedGroups() const;

 protected:
  int WriteImpl(net::IOBuffer* io_buffer,
                int io_buffer_size,
                const net::CompletionCallback& callback) override;

 private:
  // Make net::IPEndPoint can be refcounted
  typedef base::RefCountedData<net::IPEndPoint> IPEndPoint;

  void OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer, int result);
  void OnRecvFromComplete(scoped_refptr<net::IOBuffer> io_buffer,
                          scoped_refptr<IPEndPoint> address,
                          int result);
  void OnSendToComplete(int result);

  net::UDPSocket socket_;

  ReadCompletionCallback read_callback_;

  RecvFromCompletionCallback recv_from_callback_;

  CompletionCallback send_to_callback_;

  std::vector<std::string> multicast_groups_;
};

// UDP Socket instances from the "sockets.udp" namespace. These are regular
// socket objects with additional properties related to the behavior defined in
// the "sockets.udp" namespace.
class ResumableUDPSocket : public UDPSocket {
 public:
  explicit ResumableUDPSocket(const std::string& owner_extension_id);

  // Overriden from ApiResource
  bool IsPersistent() const override;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  int buffer_size() const { return buffer_size_; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class ApiResourceManager<ResumableUDPSocket>;
  static const char* service_name() { return "ResumableUDPSocketManager"; }

  // Application-defined string - see sockets_udp.idl.
  std::string name_;
  // Flag indicating whether the socket is left open when the application is
  // suspended - see sockets_udp.idl.
  bool persistent_;
  // The size of the buffer used to receive data - see sockets_udp.idl.
  int buffer_size_;
  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see sockets_udp.idl.
  bool paused_;
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_UDP_SOCKET_H_
