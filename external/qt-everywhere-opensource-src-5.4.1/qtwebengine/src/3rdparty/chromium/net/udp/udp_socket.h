// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_UDP_UDP_SOCKET_H_
#define NET_UDP_UDP_SOCKET_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "net/udp/udp_socket_win.h"
#elif defined(OS_POSIX)
#include "net/udp/udp_socket_libevent.h"
#endif

namespace net {

// UDPSocket
// Accessor API for a UDP socket in either client or server form.
//
// Client form:
// In this case, we're connecting to a specific server, so the client will
// usually use:
//       Connect(address)    // Connect to a UDP server
//       Read/Write          // Reads/Writes all go to a single destination
//
// Server form:
// In this case, we want to read/write to many clients which are connecting
// to this server.  First the server 'binds' to an addres, then we read from
// clients and write responses to them.
// Example:
//       Bind(address/port)  // Binds to port for reading from clients
//       RecvFrom/SendTo     // Each read can come from a different client
//                           // Writes need to be directed to a specific
//                           // address.
#if defined(OS_WIN)
typedef UDPSocketWin UDPSocket;
#elif defined(OS_POSIX)
typedef UDPSocketLibevent UDPSocket;
#endif

}  // namespace net

#endif  // NET_UDP_UDP_SOCKET_H_
