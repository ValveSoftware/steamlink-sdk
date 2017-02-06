// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace IPC {

namespace {

// MojoBootstrap for the server process. You should create the instance
// using MojoBootstrap::Create().
class MojoServerBootstrap : public MojoBootstrap {
 public:
  MojoServerBootstrap();

 private:
  // MojoBootstrap implementation.
  void Connect() override;

  void OnInitDone(int32_t peer_pid);

  mojom::BootstrapPtr bootstrap_;
  IPC::mojom::ChannelAssociatedPtrInfo send_channel_;
  IPC::mojom::ChannelAssociatedRequest receive_channel_request_;

  DISALLOW_COPY_AND_ASSIGN(MojoServerBootstrap);
};

MojoServerBootstrap::MojoServerBootstrap() = default;

void MojoServerBootstrap::Connect() {
  DCHECK_EQ(state(), STATE_INITIALIZED);

  bootstrap_.Bind(mojom::BootstrapPtrInfo(TakeHandle(), 0));
  bootstrap_.set_connection_error_handler(
      base::Bind(&MojoServerBootstrap::Fail, base::Unretained(this)));

  IPC::mojom::ChannelAssociatedRequest send_channel_request;
  IPC::mojom::ChannelAssociatedPtrInfo receive_channel;

  bootstrap_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_REQUEST, &send_channel_,
      &send_channel_request);
  bootstrap_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &receive_channel,
      &receive_channel_request_);

  bootstrap_->Init(
      std::move(send_channel_request), std::move(receive_channel),
      GetSelfPID(),
      base::Bind(&MojoServerBootstrap::OnInitDone, base::Unretained(this)));

  set_state(STATE_WAITING_ACK);
}

void MojoServerBootstrap::OnInitDone(int32_t peer_pid) {
  if (state() != STATE_WAITING_ACK) {
    set_state(STATE_ERROR);
    LOG(ERROR) << "Got inconsistent message from client.";
    return;
  }

  set_state(STATE_READY);
  bootstrap_.set_connection_error_handler(base::Closure());
  delegate()->OnPipesAvailable(std::move(send_channel_),
                               std::move(receive_channel_request_), peer_pid);
}

// MojoBootstrap for client processes. You should create the instance
// using MojoBootstrap::Create().
class MojoClientBootstrap : public MojoBootstrap, public mojom::Bootstrap {
 public:
  MojoClientBootstrap();

 private:
  // MojoBootstrap implementation.
  void Connect() override;

  // mojom::Bootstrap implementation.
  void Init(mojom::ChannelAssociatedRequest receive_channel,
            mojom::ChannelAssociatedPtrInfo send_channel,
            int32_t peer_pid,
            const InitCallback& callback) override;

  mojo::Binding<mojom::Bootstrap> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoClientBootstrap);
};

MojoClientBootstrap::MojoClientBootstrap() : binding_(this) {}

void MojoClientBootstrap::Connect() {
  binding_.Bind(TakeHandle());
  binding_.set_connection_error_handler(
      base::Bind(&MojoClientBootstrap::Fail, base::Unretained(this)));
}

void MojoClientBootstrap::Init(mojom::ChannelAssociatedRequest receive_channel,
                               mojom::ChannelAssociatedPtrInfo send_channel,
                               int32_t peer_pid,
                               const InitCallback& callback) {
  callback.Run(GetSelfPID());
  set_state(STATE_READY);
  binding_.set_connection_error_handler(base::Closure());
  delegate()->OnPipesAvailable(std::move(send_channel),
                               std::move(receive_channel), peer_pid);
}

}  // namespace

// MojoBootstrap

// static
std::unique_ptr<MojoBootstrap> MojoBootstrap::Create(
    mojo::ScopedMessagePipeHandle handle,
    Channel::Mode mode,
    Delegate* delegate) {
  CHECK(mode == Channel::MODE_CLIENT || mode == Channel::MODE_SERVER);
  std::unique_ptr<MojoBootstrap> self =
      mode == Channel::MODE_CLIENT
          ? std::unique_ptr<MojoBootstrap>(new MojoClientBootstrap)
          : std::unique_ptr<MojoBootstrap>(new MojoServerBootstrap);

  self->Init(std::move(handle), delegate);
  return self;
}

MojoBootstrap::MojoBootstrap() : delegate_(NULL), state_(STATE_INITIALIZED) {
}

MojoBootstrap::~MojoBootstrap() {}

void MojoBootstrap::Init(mojo::ScopedMessagePipeHandle handle,
                         Delegate* delegate) {
  handle_ = std::move(handle);
  delegate_ = delegate;
}

base::ProcessId MojoBootstrap::GetSelfPID() const {
#if defined(OS_LINUX)
  if (int global_pid = Channel::GetGlobalPid())
    return global_pid;
#endif  // OS_LINUX
#if defined(OS_NACL)
  return -1;
#else
  return base::GetCurrentProcId();
#endif  // defined(OS_NACL)
}

void MojoBootstrap::Fail() {
  set_state(STATE_ERROR);
  delegate()->OnBootstrapError();
}

bool MojoBootstrap::HasFailed() const {
  return state() == STATE_ERROR;
}

mojo::ScopedMessagePipeHandle MojoBootstrap::TakeHandle() {
  return std::move(handle_);
}

}  // namespace IPC
