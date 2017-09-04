// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_HOST_H_
#define MOJO_EDK_SYSTEM_BROKER_HOST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

// The BrokerHost is a channel to the child process, which services synchronous
// IPCs.
class BrokerHost : public Channel::Delegate,
                   public base::MessageLoop::DestructionObserver {
 public:
  BrokerHost(base::ProcessHandle client_process, ScopedPlatformHandle handle);

  // Send |handle| to the child, to be used to establish a NodeChannel to us.
  bool SendChannel(ScopedPlatformHandle handle);

#if defined(OS_WIN)
  // Sends a named channel to the child. Like above, but for named pipes.
  void SendNamedChannel(const base::StringPiece16& pipe_name);
#endif

 private:
  ~BrokerHost() override;

  bool PrepareHandlesForClient(PlatformHandleVector* handles);

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override;
  void OnChannelError() override;

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  void OnBufferRequest(uint32_t num_bytes);

#if defined(OS_WIN)
  base::ProcessHandle client_process_;
#endif

  scoped_refptr<Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(BrokerHost);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_HOST_H_
