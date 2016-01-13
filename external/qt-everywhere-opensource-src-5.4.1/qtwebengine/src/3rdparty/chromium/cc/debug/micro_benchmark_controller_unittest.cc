// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/layers/layer.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MicroBenchmarkControllerTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    impl_proxy_ = make_scoped_ptr(new FakeImplProxy);
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    layer_tree_host_impl_ = make_scoped_ptr(new FakeLayerTreeHostImpl(
        impl_proxy_.get(), shared_bitmap_manager_.get()));

    layer_tree_host_ = FakeLayerTreeHost::Create();
    layer_tree_host_->SetRootLayer(Layer::Create());
    layer_tree_host_->InitializeForTesting(scoped_ptr<Proxy>(new FakeProxy));
  }

  virtual void TearDown() OVERRIDE {
    layer_tree_host_impl_.reset();
    layer_tree_host_.reset();
    impl_proxy_.reset();
  }

  scoped_ptr<FakeLayerTreeHost> layer_tree_host_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<FakeLayerTreeHostImpl> layer_tree_host_impl_;
  scoped_ptr<FakeImplProxy> impl_proxy_;
};

void Noop(scoped_ptr<base::Value> value) {
}

void IncrementCallCount(int* count, scoped_ptr<base::Value> value) {
  ++(*count);
}

TEST_F(MicroBenchmarkControllerTest, ScheduleFail) {
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "non_existant_benchmark", scoped_ptr<base::Value>(), base::Bind(&Noop));
  EXPECT_EQ(id, 0);
}

TEST_F(MicroBenchmarkControllerTest, CommitScheduled) {
  EXPECT_FALSE(layer_tree_host_->needs_commit());
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", scoped_ptr<base::Value>(), base::Bind(&Noop));
  EXPECT_GT(id, 0);
  EXPECT_TRUE(layer_tree_host_->needs_commit());
}

TEST_F(MicroBenchmarkControllerTest, BenchmarkRan) {
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer_tree_host_->SetOutputSurfaceLostForTesting(false);
  layer_tree_host_->UpdateLayers(queue.get());

  EXPECT_EQ(1, run_count);
}

TEST_F(MicroBenchmarkControllerTest, MultipleBenchmarkRan) {
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);
  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer_tree_host_->SetOutputSurfaceLostForTesting(false);
  layer_tree_host_->UpdateLayers(queue.get());

  EXPECT_EQ(2, run_count);

  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);
  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  layer_tree_host_->UpdateLayers(queue.get());
  EXPECT_EQ(4, run_count);

  layer_tree_host_->UpdateLayers(queue.get());
  EXPECT_EQ(4, run_count);
}

TEST_F(MicroBenchmarkControllerTest, BenchmarkImplRan) {
  int run_count = 0;
  scoped_ptr<base::DictionaryValue> settings(new base::DictionaryValue);
  settings->SetBoolean("run_benchmark_impl", true);

  // Schedule a main thread benchmark.
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      settings.PassAs<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  // Schedule impl benchmarks. In production code, this is run in commit.
  layer_tree_host_->GetMicroBenchmarkController()->ScheduleImplBenchmarks(
      layer_tree_host_impl_.get());

  // Now complete the commit (as if on the impl thread).
  layer_tree_host_impl_->CommitComplete();

  // Make sure all posted messages run.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(1, run_count);
}

TEST_F(MicroBenchmarkControllerTest, SendMessage) {
  // Send valid message to invalid benchmark (id = 0)
  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue);
  message->SetBoolean("can_handle", true);
  bool message_handled = layer_tree_host_->SendMessageToMicroBenchmark(
      0, message.PassAs<base::Value>());
  EXPECT_FALSE(message_handled);

  // Schedule a benchmark
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  // Send valid message to valid benchmark
  message = scoped_ptr<base::DictionaryValue>(new base::DictionaryValue);
  message->SetBoolean("can_handle", true);
  message_handled = layer_tree_host_->SendMessageToMicroBenchmark(
      id, message.PassAs<base::Value>());
  EXPECT_TRUE(message_handled);

  // Send invalid message to valid benchmark
  message = scoped_ptr<base::DictionaryValue>(new base::DictionaryValue);
  message->SetBoolean("can_handle", false);
  message_handled = layer_tree_host_->SendMessageToMicroBenchmark(
      id, message.PassAs<base::Value>());
  EXPECT_FALSE(message_handled);
}

}  // namespace
}  // namespace cc
