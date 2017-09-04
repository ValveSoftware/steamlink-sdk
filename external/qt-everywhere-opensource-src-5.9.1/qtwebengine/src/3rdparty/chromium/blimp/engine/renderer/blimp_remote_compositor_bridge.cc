// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_remote_compositor_bridge.h"

#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/remote_compositor_bridge_client.h"
#include "cc/output/swap_promise.h"
#include "cc/proto/compositor_message.pb.h"

namespace blimp {
namespace engine {

BlimpRemoteCompositorBridge::BlimpRemoteCompositorBridge(
    content::RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
    : RemoteCompositorBridge(std::move(compositor_main_task_runner)),
      remote_proto_channel_(remote_proto_channel),
      scheduler_(compositor_main_task_runner_.get(), this) {
  remote_proto_channel_->SetProtoReceiver(this);
}

BlimpRemoteCompositorBridge::~BlimpRemoteCompositorBridge() {
  remote_proto_channel_->SetProtoReceiver(nullptr);
}

void BlimpRemoteCompositorBridge::BindToClient(
    cc::RemoteCompositorBridgeClient* client) {
  DCHECK(!client_);
  client_ = client;
}

void BlimpRemoteCompositorBridge::ScheduleMainFrame() {
  scheduler_.ScheduleFrameUpdate();
}

void BlimpRemoteCompositorBridge::ProcessCompositorStateUpdate(
    std::unique_ptr<cc::CompositorProtoState> compositor_proto_state) {
  compositor_proto_state->compositor_message->set_client_state_update_ack(
      client_state_update_ack_pending_);
  client_state_update_ack_pending_ = false;

  remote_proto_channel_->SendCompositorProto(
      *compositor_proto_state->compositor_message);
  scheduler_.DidSendFrameUpdateToClient();

  // Activate the swap promises after the frame is queued.
  for (const auto& swap_promise : compositor_proto_state->swap_promises)
    swap_promise->DidActivate();
}

void BlimpRemoteCompositorBridge::OnProtoReceived(
    std::unique_ptr<cc::proto::CompositorMessage> proto) {
  if (proto->frame_ack())
    scheduler_.DidReceiveFrameUpdateAck();

  if (proto->has_client_state_update()) {
    DCHECK(!client_state_update_ack_pending_);

    client_->ApplyStateUpdateFromClient(proto->client_state_update());

    // If applying the delta resulted in a frame request, run the main frame
    // first so the ack sent to the client includes the frame with the deltas
    // applied.
    if (scheduler_.needs_frame_update()) {
      client_state_update_ack_pending_ = true;
    } else {
      cc::proto::CompositorMessage message;
      message.set_client_state_update_ack(true);
      remote_proto_channel_->SendCompositorProto(message);
    }
  }
}

void BlimpRemoteCompositorBridge::StartFrameUpdate() {
  client_->BeginMainFrame();

  // If the frame resulted in an update to the client, the ack should have gone
  // with it. If it is still pending, this means the main frame was aborted so
  // send the ack now.
  if (client_state_update_ack_pending_) {
    client_state_update_ack_pending_ = false;
    cc::proto::CompositorMessage message;
    message.set_client_state_update_ack(true);
    remote_proto_channel_->SendCompositorProto(message);
  }
}

}  // namespace engine
}  // namespace blimp
