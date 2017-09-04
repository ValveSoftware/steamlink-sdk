// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_remote_compositor_bridge.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/remote_compositor_bridge_client.h"
#include "cc/proto/compositor_message.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

class BlimpRemoteCompositorBridgeTest : public testing::Test,
                                        public cc::RemoteCompositorBridgeClient,
                                        public content::RemoteProtoChannel {
 public:
  BlimpRemoteCompositorBridgeTest() = default;
  ~BlimpRemoteCompositorBridgeTest() override = default;

  void SetUp() override {
    remote_compositor_bridge_ = base::MakeUnique<BlimpRemoteCompositorBridge>(
        this, base::ThreadTaskRunnerHandle::Get());
    DCHECK(proto_receiver_);

    remote_compositor_bridge_->BindToClient(this);
    remote_compositor_bridge_->scheduler_for_testing()
        ->set_frame_delay_for_testing(base::TimeDelta::FromSeconds(0));
  }
  void TearDown() override {
    remote_compositor_bridge_ = nullptr;
    DCHECK(!proto_receiver_);
  }

  // RemoteProtoChannel implementation.
  void SetProtoReceiver(ProtoReceiver* receiver) override {
    DCHECK(!proto_receiver_ || !receiver);
    proto_receiver_ = receiver;
  }
  void SendCompositorProto(const cc::proto::CompositorMessage& proto) override {
    if (proto.has_layer_tree_host())
      num_of_frames_sent_++;
    if (proto.has_client_state_update_ack())
      num_of_client_state_acks_sent_++;
  }

  // cc::RemoteCompositorBridgeClient implementation.
  void BeginMainFrame() override {
    if (send_frame_) {
      std::unique_ptr<cc::CompositorProtoState> proto_state =
          base::MakeUnique<cc::CompositorProtoState>();
      proto_state->compositor_message =
          base::MakeUnique<cc::proto::CompositorMessage>();
      proto_state->compositor_message->mutable_layer_tree_host()
          ->mutable_layer_tree()
          ->set_page_scale_factor(1.f);

      send_frame_ = false;
      remote_compositor_bridge_->ProcessCompositorStateUpdate(
          std::move(proto_state));
    }
  }
  void ApplyStateUpdateFromClient(
      const cc::proto::ClientStateUpdate& client_state_update) override {
    num_of_client_updates_++;
  }

  int num_of_frames_sent() const { return num_of_frames_sent_; }
  int num_of_client_state_acks_sent() const {
    return num_of_client_state_acks_sent_;
  }
  int num_of_client_updates() const { return num_of_client_updates_; }

  void set_send_frame(bool send_frame) { send_frame_ = send_frame; }

  void SendFrameAck() {
    std::unique_ptr<cc::proto::CompositorMessage> message =
        base::MakeUnique<cc::proto::CompositorMessage>();
    message->set_frame_ack(true);
    proto_receiver_->OnProtoReceived(std::move(message));
  }

  void SendClientStateUpdate() {
    std::unique_ptr<cc::proto::CompositorMessage> message =
        base::MakeUnique<cc::proto::CompositorMessage>();
    message->mutable_client_state_update()->set_page_scale_delta(0.2f);
    proto_receiver_->OnProtoReceived(std::move(message));
  }

 protected:
  base::MessageLoop loop_;
  std::unique_ptr<BlimpRemoteCompositorBridge> remote_compositor_bridge_;
  content::RemoteProtoChannel::ProtoReceiver* proto_receiver_ = nullptr;

 private:
  int num_of_frames_sent_ = 0;
  int num_of_client_state_acks_sent_ = 0;
  int num_of_client_updates_ = 0;
  bool send_frame_ = false;
};

TEST_F(BlimpRemoteCompositorBridgeTest, FrameAndFrameAck) {
  // Request a frame with an update.
  set_send_frame(true);
  remote_compositor_bridge_->ScheduleMainFrame();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_of_frames_sent());

  // Request another frame while the previous one has not been acked.
  set_send_frame(true);
  remote_compositor_bridge_->ScheduleMainFrame();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_of_frames_sent());

  // Ack the previous frame and ensure that the next frame is run.
  SendFrameAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_of_frames_sent());
}

TEST_F(BlimpRemoteCompositorBridgeTest, ClientStateUpdateAck) {
  // Send a client state update with no frame requests present. We should see
  // an ack immediately
  SendClientStateUpdate();
  EXPECT_EQ(1, num_of_client_updates());
  EXPECT_EQ(1, num_of_client_state_acks_sent());

  // Request a frame with no update, the ack should come after the frame runs.
  remote_compositor_bridge_->ScheduleMainFrame();
  SendClientStateUpdate();
  EXPECT_EQ(2, num_of_client_updates());
  EXPECT_EQ(1, num_of_client_state_acks_sent());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_of_client_state_acks_sent());

  // Request a frame with an update, the ack should be bundled with the frame.
  set_send_frame(true);
  remote_compositor_bridge_->ScheduleMainFrame();
  SendClientStateUpdate();
  EXPECT_EQ(3, num_of_client_updates());
  EXPECT_EQ(2, num_of_client_state_acks_sent());
  EXPECT_EQ(0, num_of_frames_sent());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, num_of_client_state_acks_sent());
  EXPECT_EQ(1, num_of_frames_sent());
}

}  // namespace
}  // namespace engine
}  // namespace blimp
