// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame_sink.h"

#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/test/fake_compositor_frame_sink_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestCompositorFrameSink : public CompositorFrameSink {
 public:
  explicit TestCompositorFrameSink(
      scoped_refptr<TestContextProvider> context_provider,
      scoped_refptr<TestContextProvider> worker_context_provider)
      : CompositorFrameSink(std::move(context_provider),
                            std::move(worker_context_provider),
                            nullptr,
                            nullptr) {}

  void SubmitCompositorFrame(CompositorFrame frame) override {
    client_->DidReceiveCompositorFrameAck();
  }
};

TEST(CompositorFrameSinkTest, ContextLossInformsClient) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();
  TestCompositorFrameSink compositor_frame_sink(provider, worker_provider);
  EXPECT_FALSE(compositor_frame_sink.HasClient());

  FakeCompositorFrameSinkClient client;
  EXPECT_TRUE(compositor_frame_sink.BindToClient(&client));
  EXPECT_TRUE(compositor_frame_sink.HasClient());

  // Verify DidLoseCompositorFrameSink callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_compositor_frame_sink_called());
  compositor_frame_sink.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  compositor_frame_sink.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_compositor_frame_sink_called());
}

// TODO(danakj): Add a test for worker context failure as well when
// CompositorFrameSink creates/binds it.
TEST(CompositorFrameSinkTest, ContextLossFailsBind) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestContext3d()->set_context_lost(true);

  TestCompositorFrameSink compositor_frame_sink(context_provider,
                                                worker_provider);
  EXPECT_FALSE(compositor_frame_sink.HasClient());

  FakeCompositorFrameSinkClient client;
  EXPECT_FALSE(compositor_frame_sink.BindToClient(&client));
  EXPECT_FALSE(compositor_frame_sink.HasClient());
}

}  // namespace
}  // namespace cc
