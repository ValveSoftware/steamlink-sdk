// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/test/compositor/blimp_compositor_with_fake_host.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "cc/layers/layer.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

class BlimpCompositorTest : public testing::Test, public BlimpCompositorClient {
 public:
  BlimpCompositorTest() = default;
  ~BlimpCompositorTest() override = default;

  void SetUp() override {
    compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
        base::MakeUnique<MockCompositorDependencies>());
    compositor_ = BlimpCompositorWithFakeHost::Create(
        compositor_dependencies_.get(), this);
  }

  void TearDown() override {
    compositor_.reset();
    compositor_dependencies_.reset();
  }

  void SendFrameUpdate(scoped_refptr<cc::Layer> root_layer) {
    compositor_->OnCompositorMessageReceived(
        compositor_->CreateFakeUpdate(root_layer));
  }

  void SendClientStateUpdateAck() {
    std::unique_ptr<cc::proto::CompositorMessage> message =
        base::MakeUnique<cc::proto::CompositorMessage>();
    message->set_client_state_update_ack(true);
    compositor_->OnCompositorMessageReceived(std::move(message));
  }

  // BlimpCompositorClient implementation.
  void SendCompositorMessage(
      const cc::proto::CompositorMessage& message) override {
    if (message.has_client_state_update())
      client_state_updates_++;
    if (message.has_frame_ack())
      frame_acks_++;
  }

  int client_state_updates() const { return client_state_updates_; }
  int frame_acks() const { return frame_acks_; }

 protected:
  base::MessageLoop loop_;
  std::unique_ptr<BlimpCompositorDependencies> compositor_dependencies_;
  std::unique_ptr<BlimpCompositorWithFakeHost> compositor_;
  BlobImageSerializationProcessor blob_image_serialization_processor_;

 private:
  int client_state_updates_ = 0;
  int frame_acks_ = 0;
};

TEST_F(BlimpCompositorTest, FrameAck) {
  // Send a frame and run a local frame update.
  scoped_refptr<cc::Layer> engine_root_layer = cc::Layer::Create();
  SendFrameUpdate(engine_root_layer);
  compositor_->UpdateLayerTreeHost();

  // There should be a local root layer for this engine root layer.
  cc::Layer* client_root_layer =
      compositor_->compositor_state_deserializer_for_testing()
          ->GetLayerForEngineId(engine_root_layer->id());
  EXPECT_EQ(client_root_layer,
            compositor_->host()->GetLayerTree()->root_layer());

  // We should have a received an ack for this frame.
  EXPECT_EQ(1, frame_acks());
}

TEST_F(BlimpCompositorTest, WaitsForClientAckToFlushState) {
  EXPECT_EQ(compositor_->host()->reflected_main_frame_state_for_testing(),
            nullptr);

  // Tell the compositor that the local state was modified and run a frame
  // update. This should trigger a client state flush.
  compositor_->DidUpdateLocalState();
  compositor_->UpdateLayerTreeHost();
  EXPECT_EQ(1, client_state_updates());
  // On each update, the reflected main frame state should be sent to the impl
  // thread.
  EXPECT_NE(compositor_->host()->reflected_main_frame_state_for_testing(),
            nullptr);

  // Now do this again while the ack for the previous state update is pending,
  // the compositor should wait for an ack.
  compositor_->DidUpdateLocalState();
  compositor_->UpdateLayerTreeHost();
  EXPECT_EQ(1, client_state_updates());

  // Send a state ack, this should cause the state to be flushed.
  SendClientStateUpdateAck();
  EXPECT_EQ(2, client_state_updates());
}

TEST_F(BlimpCompositorTest, Visible) {
  compositor_->SetVisible(true);
  EXPECT_TRUE(compositor_->host()->IsVisible());

  compositor_->SetVisible(false);
  EXPECT_FALSE(compositor_->host()->IsVisible());
}

}  // namespace
}  // namespace client
}  // namespace blimp
