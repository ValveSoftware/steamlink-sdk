// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_TCP_CLIENT_TRANSPORT_H_
#define BLIMP_NET_TCP_CLIENT_TRANSPORT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blimp_transport.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

namespace net {
class ClientSocketFactory;
class NetLog;
class StreamSocket;
}  // namespace net

namespace blimp {

class BlimpConnection;
class BlimpConnectionStatistics;

// BlimpTransport which creates a TCP connection to one of the specified
// |addresses| on each call to Connect().
class BLIMP_NET_EXPORT TCPClientTransport : public BlimpTransport {
 public:
  TCPClientTransport(const net::IPEndPoint& ip_endpoint,
                     BlimpConnectionStatistics* statistics,
                     net::NetLog* net_log);
  ~TCPClientTransport() override;

  void SetClientSocketFactoryForTest(net::ClientSocketFactory* factory);

  // BlimpTransport implementation.
  void Connect(const net::CompletionCallback& callback) override;
  std::unique_ptr<BlimpConnection> TakeConnection() override;
  const char* GetName() const override;

 protected:
  // Called when the TCP connection completed.
  virtual void OnTCPConnectComplete(int result);

  // Called when the connection attempt completed or failed.
  // Resets |socket_| if |result| indicates a failure (!= net::OK).
  void OnConnectComplete(int result);

  // Methods for taking and setting |socket_|. Can be used by subclasses to
  // swap out a socket for an upgraded one, e.g. adding SSL encryption.
  std::unique_ptr<net::StreamSocket> TakeSocket();
  void SetSocket(std::unique_ptr<net::StreamSocket> socket);

  // Gets the socket factory instance.
  net::ClientSocketFactory* socket_factory() const;

 private:
  net::IPEndPoint ip_endpoint_;
  BlimpConnectionStatistics* blimp_connection_statistics_;
  net::NetLog* net_log_;
  net::CompletionCallback connect_callback_;
  net::ClientSocketFactory* socket_factory_ = nullptr;
  std::unique_ptr<net::StreamSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientTransport);
};

}  // namespace blimp

#endif  // BLIMP_NET_TCP_CLIENT_TRANSPORT_H_
