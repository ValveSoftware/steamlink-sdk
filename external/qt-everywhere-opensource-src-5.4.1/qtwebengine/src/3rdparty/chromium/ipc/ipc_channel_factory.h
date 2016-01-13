// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_FACTORY_H_
#define IPC_IPC_CHANNEL_FACTORY_H_

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_export.h"

namespace IPC {

// A ChannelFactory listens on a UNIX domain socket. When a client connects to
// the socket, it accept()s the connection and passes the new FD to the
// delegate. The delegate is then responsible for creating a new IPC::Channel
// for the FD.
class IPC_EXPORT ChannelFactory : public base::MessageLoopForIO::Watcher {
 public:
  class Delegate {
   public:
    // Called when a client connects to the factory. It is the delegate's
    // responsibility to create an IPC::Channel for the handle, or else close
    // the file descriptor contained therein.
    virtual void OnClientConnected(const ChannelHandle& handle) = 0;

    // Called when an error occurs and the channel is closed.
    virtual void OnListenError() = 0;
  };

  ChannelFactory(const base::FilePath& path, Delegate* delegate);

  virtual ~ChannelFactory();

  // Call this to start listening on the socket.
  bool Listen();

  // Close and unlink the socket, and stop accepting connections.
  void Close();

 private:
  bool CreateSocket();
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  base::MessageLoopForIO::FileDescriptorWatcher
  server_listen_connection_watcher_;
  base::FilePath path_;
  Delegate* delegate_;
  int listen_fd_;

  DISALLOW_COPY_AND_ASSIGN(ChannelFactory);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_FACTORY_H_
