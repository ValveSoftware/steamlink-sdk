// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SERVER_SOCKET_H_
#define NET_SOCKET_UDP_SERVER_SOCKET_H_

#include "net/base/completion_callback.h"
#include "net/base/net_util.h"
#include "net/udp/datagram_server_socket.h"
#include "net/udp/udp_socket.h"

namespace net {

class IPEndPoint;
class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class NET_EXPORT UDPServerSocket : public DatagramServerSocket {
 public:
  UDPServerSocket(net::NetLog* net_log, const net::NetLog::Source& source);
  virtual ~UDPServerSocket();

  // Implement DatagramServerSocket:
  virtual int Listen(const IPEndPoint& address) OVERRIDE;
  virtual int RecvFrom(IOBuffer* buf,
                       int buf_len,
                       IPEndPoint* address,
                       const CompletionCallback& callback) OVERRIDE;
  virtual int SendTo(IOBuffer* buf,
                     int buf_len,
                     const IPEndPoint& address,
                     const CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void AllowAddressReuse() OVERRIDE;
  virtual void AllowBroadcast() OVERRIDE;
  virtual int JoinGroup(const IPAddressNumber& group_address) const OVERRIDE;
  virtual int LeaveGroup(const IPAddressNumber& group_address) const OVERRIDE;
  virtual int SetMulticastInterface(uint32 interface_index) OVERRIDE;
  virtual int SetMulticastTimeToLive(int time_to_live) OVERRIDE;
  virtual int SetMulticastLoopbackMode(bool loopback) OVERRIDE;
  virtual int SetDiffServCodePoint(DiffServCodePoint dscp) OVERRIDE;
  virtual void DetachFromThread() OVERRIDE;

 private:
  UDPSocket socket_;
  DISALLOW_COPY_AND_ASSIGN(UDPServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SERVER_SOCKET_H_
