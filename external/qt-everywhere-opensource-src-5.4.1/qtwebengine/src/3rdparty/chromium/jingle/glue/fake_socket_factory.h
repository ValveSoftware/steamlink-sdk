// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_FAKE_SOCKET_FACTORY_H_
#define JINGLE_GLUE_FAKE_SOCKET_FACTORY_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/ip_endpoint.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"
#include "third_party/libjingle/source/talk/p2p/base/packetsocketfactory.h"

namespace base {
class MessageLoop;
}

namespace jingle_glue {

class FakeSocketManager;

class FakeUDPPacketSocket : public talk_base::AsyncPacketSocket,
                            public base::NonThreadSafe {
 public:
  FakeUDPPacketSocket(FakeSocketManager* fake_socket_manager,
                      const net::IPEndPoint& address);
  virtual ~FakeUDPPacketSocket();

  const net::IPEndPoint& endpoint() const { return endpoint_; }
  void DeliverPacket(const net::IPEndPoint& from,
                     const std::vector<char>& data);

  // talk_base::AsyncPacketSocket implementation.
  virtual talk_base::SocketAddress GetLocalAddress() const OVERRIDE;
  virtual talk_base::SocketAddress GetRemoteAddress() const OVERRIDE;
  virtual int Send(const void *pv, size_t cb,
                   const talk_base::PacketOptions& options) OVERRIDE;
  virtual int SendTo(const void *pv, size_t cb,
                     const talk_base::SocketAddress& addr,
                     const talk_base::PacketOptions& options) OVERRIDE;
  virtual int Close() OVERRIDE;
  virtual State GetState() const OVERRIDE;
  virtual int GetOption(talk_base::Socket::Option opt, int* value) OVERRIDE;
  virtual int SetOption(talk_base::Socket::Option opt, int value) OVERRIDE;
  virtual int GetError() const OVERRIDE;
  virtual void SetError(int error) OVERRIDE;

 private:
  enum InternalState {
    IS_OPEN,
    IS_CLOSED,
  };

  scoped_refptr<FakeSocketManager> fake_socket_manager_;
  net::IPEndPoint endpoint_;
  talk_base::SocketAddress local_address_;
  talk_base::SocketAddress remote_address_;
  InternalState state_;
  int error_;

  DISALLOW_COPY_AND_ASSIGN(FakeUDPPacketSocket);
};

class FakeSocketManager : public base::RefCountedThreadSafe<FakeSocketManager> {
 public:
  FakeSocketManager();

  void SendPacket(const net::IPEndPoint& from,
                  const net::IPEndPoint& to,
                  const std::vector<char>& data);

  void AddSocket(FakeUDPPacketSocket* socket_factory);
  void RemoveSocket(FakeUDPPacketSocket* socket_factory);

 private:
  friend class base::RefCountedThreadSafe<FakeSocketManager>;

  ~FakeSocketManager();

  void DeliverPacket(const net::IPEndPoint& from,
                     const net::IPEndPoint& to,
                     const std::vector<char>& data);

  base::MessageLoop* message_loop_;
  std::map<net::IPEndPoint, FakeUDPPacketSocket*> endpoints_;

  DISALLOW_COPY_AND_ASSIGN(FakeSocketManager);
};

class FakeSocketFactory : public talk_base::PacketSocketFactory {
 public:
  FakeSocketFactory(FakeSocketManager* socket_manager,
                    const net::IPAddressNumber& address);
  virtual ~FakeSocketFactory();

  // talk_base::PacketSocketFactory implementation.
  virtual talk_base::AsyncPacketSocket* CreateUdpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port, int max_port) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateServerTcpSocket(
      const talk_base::SocketAddress& local_address, int min_port, int max_port,
      int opts) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateClientTcpSocket(
      const talk_base::SocketAddress& local_address,
      const talk_base::SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent,
      int opts) OVERRIDE;

 private:
  scoped_refptr<FakeSocketManager> socket_manager_;
  net::IPAddressNumber address_;
  int last_allocated_port_;

  DISALLOW_COPY_AND_ASSIGN(FakeSocketFactory);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_FAKE_SOCKET_FACTORY_H_
