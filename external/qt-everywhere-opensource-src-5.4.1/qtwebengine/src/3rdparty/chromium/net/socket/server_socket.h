// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SERVER_SOCKET_H_
#define NET_SOCKET_SERVER_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class IPEndPoint;
class StreamSocket;

class NET_EXPORT ServerSocket {
 public:
  ServerSocket() { }
  virtual ~ServerSocket() { }

  // Bind the socket and start listening. Destroy the socket to stop
  // listening.
  virtual int Listen(const net::IPEndPoint& address, int backlog) = 0;

  // Gets current address the socket is bound to.
  virtual int GetLocalAddress(IPEndPoint* address) const = 0;

  // Accept connection. Callback is called when new connection is
  // accepted.
  virtual int Accept(scoped_ptr<StreamSocket>* socket,
                     const CompletionCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_SERVER_SOCKET_H_
