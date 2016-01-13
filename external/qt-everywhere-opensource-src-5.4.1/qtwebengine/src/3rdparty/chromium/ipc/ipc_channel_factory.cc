// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_factory.h"

#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "ipc/unix_domain_socket_util.h"

namespace IPC {

ChannelFactory::ChannelFactory(const base::FilePath& path, Delegate* delegate)
    : path_(path), delegate_(delegate), listen_fd_(-1) {
  DCHECK(delegate_);
  CreateSocket();
}

ChannelFactory::~ChannelFactory() {
  Close();
}

bool ChannelFactory::CreateSocket() {
  DCHECK(listen_fd_ < 0);

  // Create the socket.
  return CreateServerUnixDomainSocket(path_, &listen_fd_);
}

bool ChannelFactory::Listen() {
  if (listen_fd_ < 0)
    return false;

  // Watch the fd for connections, and turn any connections into
  // active sockets.
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      listen_fd_,
      true,
      base::MessageLoopForIO::WATCH_READ,
      &server_listen_connection_watcher_,
      this);
  return true;
}

// Called by libevent when we can read from the fd without blocking.
void ChannelFactory::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(fd == listen_fd_);
  int new_fd = -1;
  if (!ServerAcceptConnection(listen_fd_, &new_fd)) {
    Close();
    delegate_->OnListenError();
    return;
  }
  base::ScopedFD scoped_fd(new_fd);

  if (!scoped_fd.is_valid()) {
    // The accept() failed, but not in such a way that the factory needs to be
    // shut down.
    return;
  }

  // Verify that the IPC channel peer is running as the same user.
  if (!IsPeerAuthorized(scoped_fd.get()))
    return;

  ChannelHandle handle(std::string(),
                       base::FileDescriptor(scoped_fd.release(), true));
  delegate_->OnClientConnected(handle);
}

void ChannelFactory::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Listen fd should never be writable.";
}

void ChannelFactory::Close() {
  if (listen_fd_ < 0)
    return;
  if (IGNORE_EINTR(close(listen_fd_)) < 0)
    PLOG(ERROR) << "close";
  listen_fd_ = -1;
  if (unlink(path_.value().c_str()) < 0)
    PLOG(ERROR) << "unlink";

  // Unregister libevent for the listening socket and close it.
  server_listen_connection_watcher_.StopWatchingFileDescriptor();
}

}  // namespace IPC
