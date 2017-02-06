// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_channel_impl.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/proxy_impl_for_test.h"
#include "cc/test/test_hooks.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ProxyImplTest : public testing::Test, public TestHooks {
 public:
  ~ProxyImplTest() override {
    DebugScopedSetImplThreadAndMainThreadBlocked impl_and_main_blocked(
        task_runner_provider_);
    proxy_impl_.reset();
  }

  void RequestNewOutputSurface() override {}

 protected:
  ProxyImplTest()
      : host_client_(FakeLayerTreeHostClient::DIRECT_3D),
        task_runner_provider_(nullptr) {}

  void Initialize(CompositorMode mode) {
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &host_client_, &task_graph_runner_, settings_, mode);
    task_runner_provider_ = layer_tree_host_->task_runner_provider();
    {
      DebugScopedSetImplThreadAndMainThreadBlocked impl_and_main_blocked(
          task_runner_provider_);
      proxy_impl_ =
          ProxyImplForTest::Create(this, &channel_impl_, layer_tree_host_.get(),
                                   task_runner_provider_, nullptr);
    }
  }

  TestTaskGraphRunner task_graph_runner_;
  LayerTreeSettings settings_;
  FakeLayerTreeHostClient host_client_;
  FakeChannelImpl channel_impl_;
  TaskRunnerProvider* task_runner_provider_;
  std::unique_ptr<ProxyImplForTest> proxy_impl_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
};

// This is a regression test. See crbug/568120.
TEST_F(ProxyImplTest, NonZeroSmoothnessPriorityExpiration) {
  Initialize(CompositorMode::THREADED);
  DebugScopedSetImplThread impl_thread(task_runner_provider_);
  EXPECT_FALSE(
      proxy_impl_->smoothness_priority_expiration_notifier().delay().is_zero());
}

}  // namespace cc
