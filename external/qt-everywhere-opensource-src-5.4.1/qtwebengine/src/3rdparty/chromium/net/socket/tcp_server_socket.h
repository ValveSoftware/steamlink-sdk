// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/socket/server_socket.h"
#include "net/socket/tcp_socket.h"

namespace net {

class NET_EXPORT_PRIVATE TCPServerSocket : public ServerSocket {
 public:
  TCPServerSocket(NetLog* net_log, const NetLog::Source& source);
  virtual ~TCPServerSocket();

  // net::ServerSocket implementation.
  virtual int Listen(const IPEndPoint& address, int backlog) OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual int Accept(scoped_ptr<StreamSocket>* socket,
                     const CompletionCallback& callback) OVERRIDE;

 private:
  // Converts |accepted_socket_| and stores the result in
  // |output_accepted_socket|.
  // |output_accepted_socket| is untouched on failure. But |accepted_socket_| is
  // set to NULL in any case.
  int ConvertAcceptedSocket(int result,
                            scoped_ptr<StreamSocket>* output_accepted_socket);
  // Completion callback for calling TCPSocket::Accept().
  void OnAcceptCompleted(scoped_ptr<StreamSocket>* output_accepted_socket,
                         const CompletionCallback& forward_callback,
                         int result);

  TCPSocket socket_;

  scoped_ptr<TCPSocket> accepted_socket_;
  IPEndPoint accepted_address_;
  bool pending_accept_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_H_
