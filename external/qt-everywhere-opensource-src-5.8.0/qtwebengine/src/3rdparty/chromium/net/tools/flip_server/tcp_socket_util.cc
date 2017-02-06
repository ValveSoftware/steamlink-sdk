// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/tcp_socket_util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "net/socket/tcp_socket.h"

namespace net {

namespace {

// Used to ensure we delete the addrinfo structure alloc'd by getaddrinfo().
class AddrinfoGuard {
 public:
  explicit AddrinfoGuard(struct addrinfo* addrinfo_ptr)
      : addrinfo_ptr_(addrinfo_ptr) {}

  ~AddrinfoGuard() { freeaddrinfo(addrinfo_ptr_); }

 private:
  struct addrinfo* addrinfo_ptr_;
};

// Summary:
//   Closes a socket, with option to attempt it multiple times.
//   Why do this? Well, if the system-call gets interrupted, close
//   can fail with EINTR. In that case you should just retry.. Unfortunately,
//   we can't be sure that errno is properly set since we're using a
//   multithreaded approach in the filter proxy, so we should just retry.
// Args:
//   fd - the socket to close
//   tries - the number of tries to close the socket.
// Returns:
//   true - if socket was closed
//   false - if socket was NOT closed.
// Side-effects:
//   sets *fd to -1 if socket was closed.
//
bool CloseSocket(int* fd, int tries) {
  for (int i = 0; i < tries; ++i) {
    if (!close(*fd)) {
      *fd = -1;
      return true;
    }
  }
  return false;
}

}  // namespace

int CreateTCPServerSocket(const std::string& host,
                          const std::string& port,
                          bool is_numeric_host_address,
                          int backlog,
                          bool reuseaddr,
                          bool reuseport,
                          bool wait_for_iface,
                          bool disable_nagle,
                          int* listen_fd) {
  // start out by assuming things will fail.
  *listen_fd = -1;

  const char* node = NULL;
  const char* service = NULL;

  if (!host.empty())
    node = host.c_str();
  if (!port.empty())
    service = port.c_str();

  struct addrinfo* results = 0;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  if (is_numeric_host_address) {
    hints.ai_flags = AI_NUMERICHOST;  // iff you know the name is numeric.
  }
  hints.ai_flags |= AI_PASSIVE;

  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int err = 0;
  if ((err = getaddrinfo(node, service, &hints, &results))) {
    // gai_strerror -is- threadsafe, so we get to use it here.
    LOG(ERROR) << "getaddrinfo "
               << " for (" << host << ":" << port << ") " << gai_strerror(err)
               << "\n";
    return -1;
  }
  // this will delete the addrinfo memory when we return from this function.
  AddrinfoGuard addrinfo_guard(results);

  int sock =
      socket(results->ai_family, results->ai_socktype, results->ai_protocol);
  if (sock == -1) {
    LOG(ERROR) << "Unable to create socket for (" << host << ":" << port
               << "): " << strerror(errno) << "\n";
    return -1;
  }

  if (reuseaddr) {
    // set SO_REUSEADDR on the listening socket.
    int on = 1;
    int rc;
    rc = setsockopt(sock,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    reinterpret_cast<char*>(&on),
                    sizeof(on));
    if (rc < 0) {
      close(sock);
      LOG(FATAL) << "setsockopt() failed fd=" << listen_fd << "\n";
    }
  }
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
  if (reuseport) {
    // set SO_REUSEPORT on the listening socket.
    int on = 1;
    int rc;
    rc = setsockopt(sock,
                    SOL_SOCKET,
                    SO_REUSEPORT,
                    reinterpret_cast<char*>(&on),
                    sizeof(on));
    if (rc < 0) {
      close(sock);
      LOG(FATAL) << "setsockopt() failed fd=" << listen_fd << "\n";
    }
  }

  if (bind(sock, results->ai_addr, results->ai_addrlen)) {
    // If we are waiting for the interface to be raised, such as in an
    // HA environment, ignore reporting any errors.
    int saved_errno = errno;
    if (!wait_for_iface || errno != EADDRNOTAVAIL) {
      LOG(ERROR) << "Bind was unsuccessful for (" << host << ":" << port
                 << "): " << strerror(errno) << "\n";
    }
    // if we knew that we were not multithreaded, we could do the following:
    // " : " << strerror(errno) << "\n";
    if (CloseSocket(&sock, 100)) {
      if (saved_errno == EADDRNOTAVAIL) {
        return -3;
      }
      return -2;
    } else {
      // couldn't even close the dang socket?!
      LOG(ERROR) << "Unable to close the socket.. Considering this a fatal "
                    "error, and exiting\n";
      exit(EXIT_FAILURE);
      return -1;
    }
  }

  if (disable_nagle) {
    if (!SetTCPNoDelay(sock, /*no_delay=*/true)) {
      close(sock);
      LOG(FATAL) << "SetTCPNoDelay() failed on fd: " << sock;
      return -1;
    }
  }

  if (listen(sock, backlog)) {
    // listen was unsuccessful.
    LOG(ERROR) << "Listen was unsuccessful for (" << host << ":" << port
               << "): " << strerror(errno) << "\n";
    // if we knew that we were not multithreaded, we could do the following:
    // " : " << strerror(errno) << "\n";

    if (CloseSocket(&sock, 100)) {
      sock = -1;
      return -1;
    } else {
      // couldn't even close the dang socket?!
      LOG(FATAL) << "Unable to close the socket.. Considering this a fatal "
                    "error, and exiting\n";
    }
  }

  // If we've gotten to here, Yeay! Success!
  *listen_fd = sock;

  return 0;
}

int CreateTCPClientSocket(const std::string& host,
                          const std::string& port,
                          bool is_numeric_host_address,
                          bool disable_nagle,
                          int* connect_fd) {
  const char* node = NULL;
  const char* service = NULL;

  *connect_fd = -1;
  if (!host.empty())
    node = host.c_str();
  if (!port.empty())
    service = port.c_str();

  struct addrinfo* results = 0;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  if (is_numeric_host_address)
    hints.ai_flags = AI_NUMERICHOST;  // iff you know the name is numeric.
  hints.ai_flags |= AI_PASSIVE;

  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int err = 0;
  if ((err = getaddrinfo(node, service, &hints, &results))) {
    // gai_strerror -is- threadsafe, so we get to use it here.
    LOG(ERROR) << "getaddrinfo for (" << node << ":" << service
               << "): " << gai_strerror(err);
    return -1;
  }
  // this will delete the addrinfo memory when we return from this function.
  AddrinfoGuard addrinfo_guard(results);

  int sock =
      socket(results->ai_family, results->ai_socktype, results->ai_protocol);
  if (sock == -1) {
    LOG(ERROR) << "Unable to create socket for (" << node << ":" << service
               << "): " << strerror(errno);
    return -1;
  }

  if (!base::SetNonBlocking(sock)) {
    LOG(FATAL) << "base::SetNonBlocking failed: " << sock;
  }

  if (disable_nagle) {
    if (!SetTCPNoDelay(sock, /*no_delay=*/true)) {
      close(sock);
      LOG(FATAL) << "SetTCPNoDelay() failed on fd: " << sock;
      return -1;
    }
  }

  int ret_val = 0;
  if (connect(sock, results->ai_addr, results->ai_addrlen)) {
    if (errno != EINPROGRESS) {
      LOG(ERROR) << "Connect was unsuccessful for (" << node << ":" << service
                 << "): " << strerror(errno);
      close(sock);
      return -1;
    }
  } else {
    ret_val = 1;
  }

  // If we've gotten to here, Yeay! Success!
  *connect_fd = sock;

  return ret_val;
}

}  // namespace net
