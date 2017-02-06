// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MOJO_BOOTSTRAP_H_
#define IPC_IPC_MOJO_BOOTSTRAP_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

// MojoBootstrap establishes a pair of associated interfaces between two
// processes in Chrome.
//
// Clients should implement MojoBootstrap::Delegate to get the associated pipes
// from MojoBootstrap object.
//
// This lives on IO thread other than Create(), which can be called from
// UI thread as Channel::Create() can be.
class IPC_EXPORT MojoBootstrap {
 public:
  class Delegate {
   public:
    virtual void OnPipesAvailable(
        mojom::ChannelAssociatedPtrInfo send_channel,
        mojom::ChannelAssociatedRequest receive_channel,
        int32_t peer_pid) = 0;
    virtual void OnBootstrapError() = 0;
  };

  // Create the MojoBootstrap instance, using |handle| as the message pipe, in
  // mode as specified by |mode|. The result is passed to |delegate|.
  static std::unique_ptr<MojoBootstrap> Create(
      mojo::ScopedMessagePipeHandle handle,
      Channel::Mode mode,
      Delegate* delegate);

  MojoBootstrap();
  virtual ~MojoBootstrap();

  // Start the handshake over the underlying message pipe.
  virtual void Connect() = 0;

  // GetSelfPID returns our PID.
  base::ProcessId GetSelfPID() const;

 protected:
  // On MojoServerBootstrap: INITIALIZED -> WAITING_ACK -> READY
  // On MojoClientBootstrap: INITIALIZED -> READY
  // STATE_ERROR is a catch-all state that captures any observed error.
  enum State { STATE_INITIALIZED, STATE_WAITING_ACK, STATE_READY, STATE_ERROR };

  Delegate* delegate() const { return delegate_; }
  void Fail();
  bool HasFailed() const;

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  mojo::ScopedMessagePipeHandle TakeHandle();

 private:
  void Init(mojo::ScopedMessagePipeHandle, Delegate* delegate);

  mojo::ScopedMessagePipeHandle handle_;
  Delegate* delegate_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(MojoBootstrap);
};

}  // namespace IPC

#endif  // IPC_IPC_MOJO_BOOTSTRAP_H_
