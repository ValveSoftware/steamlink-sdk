// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_
#define MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/ports/port_ref.h"

namespace mojo {
namespace edk {

class NodeController;

// This is used by Core to negotiate a new cross-process message pipe given
// an arbitrary platform channel. Both ends of the channel must be passed to
// RemoteMessagePipeBootstrap::Create() in their respective processes.
//
// The bootstrapping procedure the same on either end:
//
//   1. Select a local port P to be merged with a remote port.
//   2. Write the local node name and P's name to the bootstrap pipe.
//   3. When a message is read from the pipe:
//      - If it's the first message read, extract the remote node+port name and
//        and send an empty ACK message on the pipe.
//      - If it's the second message read, close the channel, and delete |this|.
//   4. When an error occus on the pipe, delete |this|.
//
// Excluding irrecoverable error conditions such as either process dying,
// armageddon, etc., this ensures neither end closes the channel until both ends
// are aware of each other's port-to-merge.
//
// At step 3, one side of the channel is chosen to issue a message at the Ports
// layer which eventually merges the two ports.
class RemoteMessagePipeBootstrap
    : public Channel::Delegate,
      public base::MessageLoop::DestructionObserver {
 public:
  ~RemoteMessagePipeBootstrap() override;

  // |port| must be a reference to an uninitialized local port.
  static void Create(NodeController* node_controller,
                     ScopedPlatformHandle platform_handle,
                     const ports::PortRef& port);

 protected:
  explicit RemoteMessagePipeBootstrap(NodeController* node_controller,
                                      ScopedPlatformHandle platform_handle,
                                      const ports::PortRef& port);

  void ShutDown();

  bool shutting_down_ = false;
  NodeController* node_controller_;
  const ports::PortRef local_port_;

  scoped_refptr<base::TaskRunner> io_task_runner_;
  scoped_refptr<Channel> channel_;

  bool peer_info_received_ = false;

 private:
  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override;
  void OnChannelError() override;

  void ShutDownNow();

  DISALLOW_COPY_AND_ASSIGN(RemoteMessagePipeBootstrap);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_
