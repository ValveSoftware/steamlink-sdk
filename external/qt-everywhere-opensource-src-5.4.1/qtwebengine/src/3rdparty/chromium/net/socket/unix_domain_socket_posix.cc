// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_socket_posix.h"

#include <cstring>
#include <string>

#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket_descriptor.h"

namespace net {

namespace {

bool NoAuthenticationCallback(uid_t, gid_t) {
  return true;
}

bool GetPeerIds(int socket, uid_t* user_id, gid_t* group_id) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  struct ucred user_cred;
  socklen_t len = sizeof(user_cred);
  if (getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &user_cred, &len) == -1)
    return false;
  *user_id = user_cred.uid;
  *group_id = user_cred.gid;
#else
  if (getpeereid(socket, user_id, group_id) == -1)
    return false;
#endif
  return true;
}

}  // namespace

// static
UnixDomainSocket::AuthCallback UnixDomainSocket::NoAuthentication() {
  return base::Bind(NoAuthenticationCallback);
}

// static
scoped_ptr<UnixDomainSocket> UnixDomainSocket::CreateAndListenInternal(
    const std::string& path,
    const std::string& fallback_path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback,
    bool use_abstract_namespace) {
  SocketDescriptor s = CreateAndBind(path, use_abstract_namespace);
  if (s == kInvalidSocket && !fallback_path.empty())
    s = CreateAndBind(fallback_path, use_abstract_namespace);
  if (s == kInvalidSocket)
    return scoped_ptr<UnixDomainSocket>();
  scoped_ptr<UnixDomainSocket> sock(
      new UnixDomainSocket(s, del, auth_callback));
  sock->Listen();
  return sock.Pass();
}

// static
scoped_ptr<UnixDomainSocket> UnixDomainSocket::CreateAndListen(
    const std::string& path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback) {
  return CreateAndListenInternal(path, "", del, auth_callback, false);
}

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
// static
scoped_ptr<UnixDomainSocket>
UnixDomainSocket::CreateAndListenWithAbstractNamespace(
    const std::string& path,
    const std::string& fallback_path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback) {
  return
      CreateAndListenInternal(path, fallback_path, del, auth_callback, true);
}
#endif

UnixDomainSocket::UnixDomainSocket(
    SocketDescriptor s,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback)
    : StreamListenSocket(s, del),
      auth_callback_(auth_callback) {}

UnixDomainSocket::~UnixDomainSocket() {}

// static
SocketDescriptor UnixDomainSocket::CreateAndBind(const std::string& path,
                                                 bool use_abstract_namespace) {
  sockaddr_un addr;
  static const size_t kPathMax = sizeof(addr.sun_path);
  if (use_abstract_namespace + path.size() + 1 /* '\0' */ > kPathMax)
    return kInvalidSocket;
  const SocketDescriptor s = CreatePlatformSocket(PF_UNIX, SOCK_STREAM, 0);
  if (s == kInvalidSocket)
    return kInvalidSocket;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  socklen_t addr_len;
  if (use_abstract_namespace) {
    // Convert the path given into abstract socket name. It must start with
    // the '\0' character, so we are adding it. |addr_len| must specify the
    // length of the structure exactly, as potentially the socket name may
    // have '\0' characters embedded (although we don't support this).
    // Note that addr.sun_path is already zero initialized.
    memcpy(addr.sun_path + 1, path.c_str(), path.size());
    addr_len = path.size() + offsetof(struct sockaddr_un, sun_path) + 1;
  } else {
    memcpy(addr.sun_path, path.c_str(), path.size());
    addr_len = sizeof(sockaddr_un);
  }
  if (bind(s, reinterpret_cast<sockaddr*>(&addr), addr_len)) {
    LOG(ERROR) << "Could not bind unix domain socket to " << path;
    if (use_abstract_namespace)
      LOG(ERROR) << " (with abstract namespace enabled)";
    if (IGNORE_EINTR(close(s)) < 0)
      LOG(ERROR) << "close() error";
    return kInvalidSocket;
  }
  return s;
}

void UnixDomainSocket::Accept() {
  SocketDescriptor conn = StreamListenSocket::AcceptSocket();
  if (conn == kInvalidSocket)
    return;
  uid_t user_id;
  gid_t group_id;
  if (!GetPeerIds(conn, &user_id, &group_id) ||
      !auth_callback_.Run(user_id, group_id)) {
    if (IGNORE_EINTR(close(conn)) < 0)
      LOG(ERROR) << "close() error";
    return;
  }
  scoped_ptr<UnixDomainSocket> sock(
      new UnixDomainSocket(conn, socket_delegate_, auth_callback_));
  // It's up to the delegate to AddRef if it wants to keep it around.
  sock->WatchSocket(WAITING_READ);
  socket_delegate_->DidAccept(this, sock.PassAs<StreamListenSocket>());
}

UnixDomainSocketFactory::UnixDomainSocketFactory(
    const std::string& path,
    const UnixDomainSocket::AuthCallback& auth_callback)
    : path_(path),
      auth_callback_(auth_callback) {}

UnixDomainSocketFactory::~UnixDomainSocketFactory() {}

scoped_ptr<StreamListenSocket> UnixDomainSocketFactory::CreateAndListen(
    StreamListenSocket::Delegate* delegate) const {
  return UnixDomainSocket::CreateAndListen(
      path_, delegate, auth_callback_).PassAs<StreamListenSocket>();
}

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)

UnixDomainSocketWithAbstractNamespaceFactory::
UnixDomainSocketWithAbstractNamespaceFactory(
    const std::string& path,
    const std::string& fallback_path,
    const UnixDomainSocket::AuthCallback& auth_callback)
    : UnixDomainSocketFactory(path, auth_callback),
      fallback_path_(fallback_path) {}

UnixDomainSocketWithAbstractNamespaceFactory::
~UnixDomainSocketWithAbstractNamespaceFactory() {}

scoped_ptr<StreamListenSocket>
UnixDomainSocketWithAbstractNamespaceFactory::CreateAndListen(
    StreamListenSocket::Delegate* delegate) const {
  return UnixDomainSocket::CreateAndListenWithAbstractNamespace(
             path_, fallback_path_, delegate, auth_callback_)
         .PassAs<StreamListenSocket>();
}

#endif

}  // namespace net
