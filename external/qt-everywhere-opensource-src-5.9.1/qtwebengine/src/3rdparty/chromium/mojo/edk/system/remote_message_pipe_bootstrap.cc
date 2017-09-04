// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/remote_message_pipe_bootstrap.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {

namespace {

struct BootstrapData {
  // The node name of the sender.
  ports::NodeName node_name;

  // The port name of the sender's local bootstrap port.
  ports::PortName port_name;
};

}  // namespace

// static
void RemoteMessagePipeBootstrap::Create(
    NodeController* node_controller,
    ScopedPlatformHandle platform_handle,
    const ports::PortRef& port) {
  CHECK(node_controller);
  CHECK(node_controller->io_task_runner());
  if (node_controller->io_task_runner()->RunsTasksOnCurrentThread()) {
    // Owns itself.
    new RemoteMessagePipeBootstrap(
        node_controller, std::move(platform_handle), port);
  } else {
    node_controller->io_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeBootstrap::Create,
                   base::Unretained(node_controller),
                   base::Passed(&platform_handle), port));
  }
}

RemoteMessagePipeBootstrap::~RemoteMessagePipeBootstrap() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  if (channel_)
    channel_->ShutDown();
}

RemoteMessagePipeBootstrap::RemoteMessagePipeBootstrap(
    NodeController* node_controller,
    ScopedPlatformHandle platform_handle,
    const ports::PortRef& port)
    : node_controller_(node_controller),
      local_port_(port),
      io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      channel_(Channel::Create(this, std::move(platform_handle),
                               io_task_runner_)) {
  base::MessageLoop::current()->AddDestructionObserver(this);
  channel_->Start();

  Channel::MessagePtr message(new Channel::Message(sizeof(BootstrapData), 0));
  BootstrapData* data = static_cast<BootstrapData*>(message->mutable_payload());
  data->node_name = node_controller_->name();
  data->port_name = local_port_.name();
  channel_->Write(std::move(message));
}

void RemoteMessagePipeBootstrap::ShutDown() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  if (shutting_down_)
    return;

  shutting_down_ = true;

  // Shut down asynchronously so ShutDown() can be called from within
  // OnChannelMessage and OnChannelError.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RemoteMessagePipeBootstrap::ShutDownNow,
                 base::Unretained(this)));
}

void RemoteMessagePipeBootstrap::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ShutDownNow();
}

void RemoteMessagePipeBootstrap::OnChannelMessage(
    const void* payload,
    size_t payload_size,
    ScopedPlatformHandleVectorPtr handles) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!handles || !handles->size());

  const BootstrapData* data = static_cast<const BootstrapData*>(payload);
  if (peer_info_received_) {
    // This should only be a confirmation from the other end, which tells us
    // it's now safe to shut down.
    if (payload_size != 0)
      DLOG(ERROR) << "Unexpected message received in message pipe bootstrap.";
    ShutDown();
    return;
  }

  if (payload_size != sizeof(BootstrapData)) {
    DLOG(ERROR) << "Invalid bootstrap payload.";
    ShutDown();
    return;
  }

  peer_info_received_ = true;

  // We need to choose one side to initiate the port merge. It doesn't matter
  // who does it as long as they don't both try. Simple solution: pick the one
  // with the "smaller" port name.
  if (local_port_.name() < data->port_name) {
    node_controller_->node()->MergePorts(local_port_, data->node_name,
                                         data->port_name);
  }

  // Send another ping to the other end to trigger shutdown. This may race with
  // the other end sending its own ping, but it doesn't matter. Whoever wins
  // will cause the other end to tear down, and the ensuing channel error will
  // in turn clean up the remaining end.

  Channel::MessagePtr message(new Channel::Message(0, 0));
  channel_->Write(std::move(message));
}

void RemoteMessagePipeBootstrap::OnChannelError() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ShutDown();
}

void RemoteMessagePipeBootstrap::ShutDownNow() {
  delete this;
}

}  // namespace edk
}  // namespace mojo
