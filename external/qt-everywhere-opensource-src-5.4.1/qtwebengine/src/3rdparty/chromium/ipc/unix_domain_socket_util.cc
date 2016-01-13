// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/unix_domain_socket_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace IPC {

// Verify that kMaxSocketNameLength is a decent size.
COMPILE_ASSERT(sizeof(((sockaddr_un*)0)->sun_path) >= kMaxSocketNameLength,
               BAD_SUN_PATH_LENGTH);

namespace {

// Returns fd (>= 0) on success, -1 on failure. If successful, fills in
// |unix_addr| with the appropriate data for the socket, and sets
// |unix_addr_len| to the length of the data therein.
int MakeUnixAddrForPath(const std::string& socket_name,
                        struct sockaddr_un* unix_addr,
                        size_t* unix_addr_len) {
  DCHECK(unix_addr);
  DCHECK(unix_addr_len);

  if (socket_name.length() == 0) {
    LOG(ERROR) << "Empty socket name provided for unix socket address.";
    return -1;
  }
  // We reject socket_name.length() == kMaxSocketNameLength to make room for
  // the NUL terminator at the end of the string.
  if (socket_name.length() >= kMaxSocketNameLength) {
    LOG(ERROR) << "Socket name too long: " << socket_name;
    return -1;
  }

  // Create socket.
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "socket";
    return -1;
  }

  // Make socket non-blocking
  if (HANDLE_EINTR(fcntl(fd.get(), F_SETFL, O_NONBLOCK)) < 0) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    return -1;
  }

  // Create unix_addr structure.
  memset(unix_addr, 0, sizeof(struct sockaddr_un));
  unix_addr->sun_family = AF_UNIX;
  strncpy(unix_addr->sun_path, socket_name.c_str(), kMaxSocketNameLength);
  *unix_addr_len =
      offsetof(struct sockaddr_un, sun_path) + socket_name.length();
  return fd.release();
}

}  // namespace

bool CreateServerUnixDomainSocket(const base::FilePath& socket_path,
                                  int* server_listen_fd) {
  DCHECK(server_listen_fd);

  std::string socket_name = socket_path.value();
  base::FilePath socket_dir = socket_path.DirName();

  struct sockaddr_un unix_addr;
  size_t unix_addr_len;
  base::ScopedFD fd(
      MakeUnixAddrForPath(socket_name, &unix_addr, &unix_addr_len));
  if (!fd.is_valid())
    return false;

  // Make sure the path we need exists.
  if (!base::CreateDirectory(socket_dir)) {
    LOG(ERROR) << "Couldn't create directory: " << socket_dir.value();
    return false;
  }

  // Delete any old FS instances.
  if (unlink(socket_name.c_str()) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "unlink " << socket_name;
    return false;
  }

  // Bind the socket.
  if (bind(fd.get(), reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) < 0) {
    PLOG(ERROR) << "bind " << socket_path.value();
    return false;
  }

  // Start listening on the socket.
  if (listen(fd.get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen " << socket_path.value();
    unlink(socket_name.c_str());
    return false;
  }

  *server_listen_fd = fd.release();
  return true;
}

bool CreateClientUnixDomainSocket(const base::FilePath& socket_path,
                                  int* client_socket) {
  DCHECK(client_socket);

  std::string socket_name = socket_path.value();
  base::FilePath socket_dir = socket_path.DirName();

  struct sockaddr_un unix_addr;
  size_t unix_addr_len;
  base::ScopedFD fd(
      MakeUnixAddrForPath(socket_name, &unix_addr, &unix_addr_len));
  if (!fd.is_valid())
    return false;

  if (HANDLE_EINTR(connect(fd.get(), reinterpret_cast<sockaddr*>(&unix_addr),
                           unix_addr_len)) < 0) {
    PLOG(ERROR) << "connect " << socket_path.value();
    return false;
  }

  *client_socket = fd.release();
  return true;
}

bool GetPeerEuid(int fd, uid_t* peer_euid) {
  DCHECK(peer_euid);
#if defined(OS_MACOSX) || defined(OS_OPENBSD) || defined(OS_FREEBSD)
  uid_t socket_euid;
  gid_t socket_gid;
  if (getpeereid(fd, &socket_euid, &socket_gid) < 0) {
    PLOG(ERROR) << "getpeereid " << fd;
    return false;
  }
  *peer_euid = socket_euid;
  return true;
#else
  struct ucred cred;
  socklen_t cred_len = sizeof(cred);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) < 0) {
    PLOG(ERROR) << "getsockopt " << fd;
    return false;
  }
  if (static_cast<unsigned>(cred_len) < sizeof(cred)) {
    NOTREACHED() << "Truncated ucred from SO_PEERCRED?";
    return false;
  }
  *peer_euid = cred.uid;
  return true;
#endif
}

bool IsPeerAuthorized(int peer_fd) {
  uid_t peer_euid;
  if (!GetPeerEuid(peer_fd, &peer_euid))
    return false;
  if (peer_euid != geteuid()) {
    DLOG(ERROR) << "Client euid is not authorised";
    return false;
  }
  return true;
}

bool IsRecoverableError(int err) {
  return errno == ECONNABORTED || errno == EMFILE || errno == ENFILE ||
         errno == ENOMEM || errno == ENOBUFS;
}

bool ServerAcceptConnection(int server_listen_fd, int* server_socket) {
  DCHECK(server_socket);
  *server_socket = -1;

  base::ScopedFD accept_fd(HANDLE_EINTR(accept(server_listen_fd, NULL, 0)));
  if (!accept_fd.is_valid())
    return IsRecoverableError(errno);
  if (HANDLE_EINTR(fcntl(accept_fd.get(), F_SETFL, O_NONBLOCK)) < 0) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK) " << accept_fd.get();
    // It's safe to keep listening on |server_listen_fd| even if the attempt to
    // set O_NONBLOCK failed on the client fd.
    return true;
  }

  *server_socket = accept_fd.release();
  return true;
}

}  // namespace IPC
