// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_UNIX_DOMAIN_SOCKET_UTIL_H_
#define IPC_UNIX_DOMAIN_SOCKET_UTIL_H_

#include <sys/types.h>

#include <string>

#include "ipc/ipc_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace IPC {

// Creates a UNIX-domain socket at |socket_name| and bind()s it, then listen()s
// on it. If successful, |server_listen_fd| will be set to the new file
// descriptor, and the function will return true. Otherwise returns false.
//
// This function also effectively performs `mkdir -p` on the dirname of
// |socket_name| to ensure that all the directories up to |socket_name| exist.
// As a result of which this function must be run on a thread that allows
// blocking I/O, e.g. the FILE thread in Chrome's browser process.
IPC_EXPORT bool CreateServerUnixDomainSocket(const base::FilePath& socket_name,
                                             int* server_listen_fd);

// Opens a UNIX-domain socket at |socket_name| and connect()s to it. If
// successful, |client_socket| will be set to the new file descriptor, and the
// function will return true. Otherwise returns false.
IPC_EXPORT bool CreateClientUnixDomainSocket(const base::FilePath& socket_name,
                                             int* client_socket);

// Gets the effective user ID of the other end of the UNIX-domain socket
// specified by |fd|. If successful, sets |peer_euid| to the uid, and returns
// true. Otherwise returns false.
IPC_EXPORT bool GetPeerEuid(int fd, uid_t* peer_euid);

// Checks that the process on the other end of the UNIX domain socket
// represented by |peer_fd| shares the same EUID as this process.
IPC_EXPORT bool IsPeerAuthorized(int peer_fd);

// Accepts a client attempting to connect to |server_listen_fd|, storing the
// new file descriptor for the connection in |server_socket|.
//
// Returns false if |server_listen_fd| encounters an unrecoverable error.
// Returns true if it's valid to keep listening on |server_listen_fd|. In this
// case, it's possible that a connection wasn't successfully established; then,
// |server_socket| will be set to -1.
IPC_EXPORT bool ServerAcceptConnection(int server_listen_fd,
                                       int* server_socket);

// The maximum length of the name of a socket for MODE_NAMED_SERVER or
// MODE_NAMED_CLIENT if you want to pass in your own socket.
// The standard size on linux is 108, mac is 104. To maintain consistency
// across platforms we standardize on the smaller value.
static const size_t kMaxSocketNameLength = 104;

}  // namespace IPC

#endif  // IPC_UNIX_DOMAIN_SOCKET_UTIL_H_
