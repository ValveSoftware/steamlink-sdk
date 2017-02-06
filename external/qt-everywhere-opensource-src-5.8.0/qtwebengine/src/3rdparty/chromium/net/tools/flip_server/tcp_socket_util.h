// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_TCP_SOCKET_UTIL_H_
#define NET_TOOLS_FLIP_SERVER_TCP_SOCKET_UTIL_H_

#include <string>

namespace net {

// Summary:
//   creates a socket for listening, and bind()s and listen()s it.
// Args:
//   host - hostname or numeric address, or empty-string if you want
//          to bind to listen on all addresses
//   port - a port number or service name. By service name I mean a
//          -real- service name, not a Google service name. I'd suggest
//          you just stick to a numeric representation like "80"
//   is_numeric_host_address -
//           if you know that the host address has already been looked-up,
//           and will be provided in numeric form like "130.207.244.244",
//           then you can set this to true, and it will save you the time
//           of a DNS lookup.
//   backlog - passed into listen. This is the number of pending incoming
//             connections a socket which is listening may have acquired before
//             the OS starts rejecting new incoming connections.
//   reuseaddr - if true sets SO_REUSEADDR on the listening socket
//   reuseport - if true sets SO_REUSEPORT on the listening socket
//   wait_for_iface - A boolean indicating that CreateTCPServerSocket should
//                    block until the interface that it will bind to has been
//                    raised. This is intended for HA environments.
//   disable_nagle - if true sets TCP_NODELAY on the listening socket.
//   listen_fd - this will be assigned a positive value if the socket is
//               successfully created, else it will be assigned -1.
int CreateTCPServerSocket(const std::string& host,
                          const std::string& port,
                          bool is_numeric_host_address,
                          int backlog,
                          bool reuseaddr,
                          bool reuseport,
                          bool wait_for_iface,
                          bool disable_nagle,
                          int* listen_fd);

int CreateTCPClientSocket(const std::string& host,
                          const std::string& port,
                          bool is_numeric_host_address,
                          bool disable_nagle,
                          int* connect_fd);

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_TCP_SOCKET_UTIL_H_
