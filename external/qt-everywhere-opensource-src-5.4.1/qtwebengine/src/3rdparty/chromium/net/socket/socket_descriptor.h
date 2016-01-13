// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_DESCRIPTOR_H_
#define NET_SOCKET_SOCKET_DESCRIPTOR_H_

#include "build/build_config.h"
#include "net/base/net_export.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif  // OS_WIN

namespace net {

#if defined(OS_POSIX)
typedef int SocketDescriptor;
const SocketDescriptor kInvalidSocket = -1;
#elif defined(OS_WIN)
typedef SOCKET SocketDescriptor;
const SocketDescriptor kInvalidSocket = INVALID_SOCKET;
#endif

// Interface to create native socket.
// Usually such factories are used for testing purposes, which is not true in
// this case. This interface is used to substitute WSASocket/socket to make
// possible execution of some network code in sandbox.
class NET_EXPORT PlatformSocketFactory {
 public:
  PlatformSocketFactory();
  virtual ~PlatformSocketFactory();

  // Replace WSASocket/socket with given factory. The factory will be used by
  // CreatePlatformSocket.
  static void SetInstance(PlatformSocketFactory* factory);

  // Creates  socket. See WSASocket/socket documentation of parameters.
  virtual SocketDescriptor CreateSocket(int family, int type, int protocol) = 0;
};

// Creates  socket. See WSASocket/socket documentation of parameters.
SocketDescriptor NET_EXPORT CreatePlatformSocket(int family,
                                                 int type,
                                                 int protocol);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_DESCRIPTOR_H_
