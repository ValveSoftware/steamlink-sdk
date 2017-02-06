// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor.h"

#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/feature/compositor/blimp_gpu_memory_buffer_manager.h"
#include "blimp/client/feature/compositor/blob_image_serialization_processor.h"
#include "blimp/common/compositor/blimp_task_graph_runner.h"
#include "cc/proto/compositor_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {

class MockBlimpCompositorClient : public BlimpCompositorClient {
 public:
  MockBlimpCompositorClient() : compositor_thread_("Compositor") {
    compositor_thread_.Start();
  }
  ~MockBlimpCompositorClient() override { compositor_thread_.Stop(); }

  cc::LayerTreeSettings* GetLayerTreeSettings() override { return &settings_; }
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner() override
      { return compositor_thread_.task_runner(); }
  cc::TaskGraphRunner* GetTaskGraphRunner() override {
    return &task_graph_runner_; }
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &gpu_memory_buffer_manager_;
  }
  cc::ImageSerializationProcessor* GetImageSerializationProcessor() override {
    return BlobImageSerializationProcessor::current();
  }
  void DidCompleteSwapBuffers() override {}
  void DidCommitAndDrawFrame() override {}

  void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) override {
    MockableSendWebGestureEvent(render_widget_id);
  }
  void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) override {
    MockableSendCompositorMessage(render_widget_id);
  }

  MOCK_METHOD1(MockableSendWebGestureEvent, void(int));
  MOCK_METHOD1(MockableSendCompositorMessage, void(int));

  cc::LayerTreeSettings settings_;
  base::Thread compositor_thread_;
  BlimpTaskGraphRunner task_graph_runner_;
  BlimpGpuMemoryBufferManager gpu_memory_buffer_manager_;
  BlobImageSerializationProcessor serialization_processor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlimpCompositorClient);
};

class BlimpCompositorForTesting : public BlimpCompositor {
 public:
  BlimpCompositorForTesting(int render_widget_id,
                            BlimpCompositorClient* client)
  : BlimpCompositor(render_widget_id, client) {}

  void SendProto(const cc::proto::CompositorMessage& proto) {
    SendCompositorProto(proto);
  }

  void SendGestureEvent(const blink::WebGestureEvent& gesture_event) {
    SendWebGestureEvent(gesture_event);
  }

  cc::LayerTreeHost* host() const { return host_.get(); }
};

class BlimpCompositorTest : public testing::Test {
 public:
  BlimpCompositorTest():
    render_widget_id_(1),
    loop_(new base::MessageLoop),
    window_(42u) {}

  void SetUp() override {
    compositor_.reset(new BlimpCompositorForTesting(render_widget_id_,
                                                    &compositor_client_));
  }

  void TearDown() override {
    compositor_.reset();
  }

  ~BlimpCompositorTest() override {}

  void SendInitializeMessage() {
    std::unique_ptr<cc::proto::CompositorMessage> message;
    message.reset(new cc::proto::CompositorMessage);
    cc::proto::CompositorMessageToImpl* to_impl =
        message->mutable_to_impl();
    to_impl->set_message_type(
        cc::proto::CompositorMessageToImpl::INITIALIZE_IMPL);
    cc::proto::InitializeImpl* initialize_message =
        to_impl->mutable_initialize_impl_message();
    cc::LayerTreeSettings settings;
    settings.ToProtobuf(initialize_message->mutable_layer_tree_settings());
    compositor_->OnCompositorMessageReceived(std::move(message));
  }

  void SendShutdownMessage() {
    std::unique_ptr<cc::proto::CompositorMessage> message;
    message.reset(new cc::proto::CompositorMessage);
    cc::proto::CompositorMessageToImpl* to_impl =
        message->mutable_to_impl();
    to_impl->set_message_type(cc::proto::CompositorMessageToImpl::CLOSE_IMPL);
    compositor_->OnCompositorMessageReceived(std::move(message));
  }

  int render_widget_id_;
  std::unique_ptr<base::MessageLoop> loop_;
  MockBlimpCompositorClient compositor_client_;
  std::unique_ptr<BlimpCompositorForTesting> compositor_;
  gfx::AcceleratedWidget window_;
};

TEST_F(BlimpCompositorTest, ToggleVisibilityAndWidgetWithHost) {
  // Make the compositor visible and give it a widget when we don't have a host.
  compositor_->SetVisible(true);
  compositor_->SetAcceleratedWidget(window_);
  SendInitializeMessage();

  // Check that the visibility is set correctly on the host.
  EXPECT_NE(compositor_->host(), nullptr);
  EXPECT_TRUE(compositor_->host()->visible());

  // Make the compositor invisible. This should drop the output surface and
  // make the |host_| invisible.
  compositor_->SetVisible(false);
  EXPECT_FALSE(compositor_->host()->visible());

  // Make the compositor visible and release the widget. This should make the
  // |host_| invisible.
  compositor_->SetVisible(true);
  EXPECT_TRUE(compositor_->host()->visible());
  compositor_->ReleaseAcceleratedWidget();
  EXPECT_FALSE(compositor_->host()->visible());

  SendShutdownMessage();
  EXPECT_EQ(compositor_->host(), nullptr);
}

TEST_F(BlimpCompositorTest, DestroyAndRecreateHost) {
  // Create the host and make it visible.
  SendInitializeMessage();
  compositor_->SetVisible(true);
  compositor_->SetAcceleratedWidget(window_);

  // Destroy this host and recreate a new one. Make sure that the visibility is
  // set correctly on this host.
  SendShutdownMessage();
  SendInitializeMessage();
  EXPECT_NE(compositor_->host(), nullptr);
  EXPECT_TRUE(compositor_->host()->visible());
}

TEST_F(BlimpCompositorTest, MessagesHaveCorrectId) {
  EXPECT_CALL(compositor_client_,
              MockableSendCompositorMessage(render_widget_id_)).Times(1);
  EXPECT_CALL(compositor_client_,
              MockableSendWebGestureEvent(render_widget_id_)).Times(1);

  compositor_->SendProto(cc::proto::CompositorMessage());
  compositor_->SendGestureEvent(blink::WebGestureEvent());
}

}  // namespace client
}  // namespace blimp
