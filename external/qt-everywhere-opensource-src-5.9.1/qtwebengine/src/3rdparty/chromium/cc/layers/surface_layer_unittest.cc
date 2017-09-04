// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <set>
#include <vector>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class SurfaceLayerTest : public testing::Test {
 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_ = layer_tree_host_->GetLayerTree();
    layer_tree_->SetViewportSize(gfx::Size(10, 10));
  }

  void TearDown() override {
    if (layer_tree_host_) {
      layer_tree_->SetRootLayer(nullptr);
      layer_tree_host_ = nullptr;
    }
  }

  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  LayerTree* layer_tree_;
};

void SatisfyCallback(SurfaceSequence* out, const SurfaceSequence& in) {
  *out = in;
}

void RequireCallback(SurfaceId* out_id,
                     std::set<SurfaceSequence>* out,
                     const SurfaceId& in_id,
                     const SurfaceSequence& in) {
  *out_id = in_id;
  out->insert(in);
}

// Check that one surface can be referenced by multiple LayerTreeHosts, and
// each will create its own SurfaceSequence that's satisfied on destruction.
TEST_F(SurfaceLayerTest, MultipleFramesOneSurface) {
  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
  SurfaceSequence blank_change;  // Receives sequence if commit doesn't happen.

  SurfaceId required_id;
  std::set<SurfaceSequence> required_seq;
  scoped_refptr<SurfaceLayer> layer(SurfaceLayer::Create(
      base::Bind(&SatisfyCallback, &blank_change),
      base::Bind(&RequireCallback, &required_id, &required_seq)));
  layer->SetSurfaceId(
      SurfaceId(kArbitraryFrameSinkId, LocalFrameId(1, kArbitraryToken)), 1.f,
      gfx::Size(1, 1));
  layer_tree_host_->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      FrameSinkId(1, 1));
  layer_tree_->SetRootLayer(layer);

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host2 =
      FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_,
                                animation_host2.get());
  scoped_refptr<SurfaceLayer> layer2(SurfaceLayer::Create(
      base::Bind(&SatisfyCallback, &blank_change),
      base::Bind(&RequireCallback, &required_id, &required_seq)));
  layer2->SetSurfaceId(
      SurfaceId(kArbitraryFrameSinkId, LocalFrameId(1, kArbitraryToken)), 1.f,
      gfx::Size(1, 1));
  layer_tree_host2->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      FrameSinkId(2, 2));
  layer_tree_host2->SetRootLayer(layer2);

  // Layers haven't been removed, so no sequence should be satisfied.
  EXPECT_FALSE(blank_change.is_valid());

  SurfaceSequence expected1(FrameSinkId(1, 1), 1u);
  SurfaceSequence expected2(FrameSinkId(2, 2), 1u);

  layer_tree_host2->SetRootLayer(nullptr);
  layer_tree_host2.reset();
  animation_host2 = nullptr;

  // Layer was removed so sequence from second LayerTreeHost should be
  // satisfied.
  EXPECT_TRUE(blank_change == expected2);

  // Set of sequences that need to be satisfied should include sequences from
  // both trees.
  EXPECT_TRUE(required_id == SurfaceId(kArbitraryFrameSinkId,
                                       LocalFrameId(1, kArbitraryToken)));
  EXPECT_EQ(2u, required_seq.size());
  EXPECT_TRUE(required_seq.count(expected1));
  EXPECT_TRUE(required_seq.count(expected2));

  layer_tree_->SetRootLayer(nullptr);
  layer_tree_host_.reset();

  // Layer was removed so sequence from first LayerTreeHost should be
  // satisfied.
  EXPECT_TRUE(blank_change == expected1);

  // No more SurfaceSequences should have been generated that need to have be
  // satisfied.
  EXPECT_EQ(2u, required_seq.size());
}

// Check that SurfaceSequence is sent through swap promise.
class SurfaceLayerSwapPromise : public LayerTreeTest {
 public:
  SurfaceLayerSwapPromise()
      : commit_count_(0), sequence_was_satisfied_(false) {}

  void BeginTest() override {
    layer_tree_host()->GetSurfaceSequenceGenerator()->set_frame_sink_id(
        FrameSinkId(1, 1));
    layer_ = SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, &satisfied_sequence_),
        base::Bind(&RequireCallback, &required_id_, &required_set_));
    layer_->SetSurfaceId(
        SurfaceId(kArbitraryFrameSinkId, LocalFrameId(1, kArbitraryToken)), 1.f,
        gfx::Size(1, 1));

    // Layer hasn't been added to tree so no SurfaceSequence generated yet.
    EXPECT_EQ(0u, required_set_.size());

    layer_tree()->SetRootLayer(layer_);

    // Should have SurfaceSequence from first tree.
    SurfaceSequence expected(kArbitraryFrameSinkId, 1u);
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalFrameId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    EXPECT_TRUE(required_set_.count(expected));

    gfx::Size bounds(100, 100);
    layer_tree()->SetViewportSize(bounds);

    blank_layer_ = SolidColorLayer::Create();
    blank_layer_->SetIsDrawable(true);
    blank_layer_->SetBounds(gfx::Size(10, 10));

    PostSetNeedsCommitToMainThread();
  }

  virtual void ChangeTree() = 0;

  void DidCommitAndDrawFrame() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SurfaceLayerSwapPromise::ChangeTree,
                              base::Unretained(this)));
  }

 protected:
  int commit_count_;
  bool sequence_was_satisfied_;
  scoped_refptr<SurfaceLayer> layer_;
  scoped_refptr<Layer> blank_layer_;
  SurfaceSequence satisfied_sequence_;

  SurfaceId required_id_;
  std::set<SurfaceSequence> required_set_;
  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
};

// Check that SurfaceSequence is sent through swap promise.
class SurfaceLayerSwapPromiseWithDraw : public SurfaceLayerSwapPromise {
 public:
  void ChangeTree() override {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Remove SurfaceLayer from tree to cause SwapPromise to be created.
        layer_tree()->SetRootLayer(blank_layer_);
        break;
      case 2:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const CompositorFrame& frame) override {
    const std::vector<uint32_t>& satisfied = frame.metadata.satisfies_sequences;
    EXPECT_LE(satisfied.size(), 1u);
    if (satisfied.size() == 1) {
      // Eventually the one SurfaceSequence should be satisfied, but only
      // after the layer was removed from the tree, and only once.
      EXPECT_EQ(1u, satisfied[0]);
      EXPECT_LE(1, commit_count_);
      EXPECT_FALSE(sequence_was_satisfied_);
      sequence_was_satisfied_ = true;
      EndTest();
    }
  }

  void AfterTest() override {
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalFrameId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    // Sequence should have been satisfied through Swap, not with the
    // callback.
    EXPECT_FALSE(satisfied_sequence_.is_valid());
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithDraw);

// Check that SurfaceSequence is sent through swap promise and resolved when
// swap fails.
class SurfaceLayerSwapPromiseWithoutDraw : public SurfaceLayerSwapPromise {
 public:
  SurfaceLayerSwapPromiseWithoutDraw() : SurfaceLayerSwapPromise() {}

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    return DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
  }

  void ChangeTree() override {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Remove SurfaceLayer from tree to cause SwapPromise to be created.
        layer_tree()->SetRootLayer(blank_layer_);
        break;
      case 2:
        layer_tree_host()->SetNeedsCommit();
        break;
      default:
        EndTest();
        break;
    }
  }

  void AfterTest() override {
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalFrameId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    // Sequence should have been satisfied with the callback.
    EXPECT_TRUE(satisfied_sequence_ ==
                SurfaceSequence(kArbitraryFrameSinkId, 1u));
  }
};

MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithoutDraw);

}  // namespace
}  // namespace cc
