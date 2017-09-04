// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/layer_tree_host_remote.h"

#include <memory>
#include <unordered_set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/proto/client_state_update.pb.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_remote_compositor_bridge.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::Mock;
using testing::StrictMock;

#define EXPECT_BEGIN_MAIN_FRAME(client, num)                  \
  EXPECT_CALL(client, WillBeginMainFrame()).Times(num);       \
  EXPECT_CALL(client, DidReceiveBeginMainFrame()).Times(num); \
  EXPECT_CALL(client, DidUpdateLayerTreeHost()).Times(num);   \
  EXPECT_CALL(client, WillCommit()).Times(num);               \
  EXPECT_CALL(client, DidCommit()).Times(num);                \
  EXPECT_CALL(client, DidBeginMainFrame()).Times(num);

#define EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(client, num)    \
  EXPECT_BEGIN_MAIN_FRAME(client, num)                     \
  EXPECT_CALL(client, DidCommitAndDrawFrame()).Times(num); \
  EXPECT_CALL(client, DidReceiveCompositorFrameAck()).Times(num);

namespace cc {
namespace {

gfx::Vector2dF SerializeScrollUpdate(
    proto::ClientStateUpdate* client_state_update,
    Layer* layer,
    const gfx::ScrollOffset& offset_after_update) {
  proto::ScrollUpdate* scroll_update =
      client_state_update->add_scroll_updates();
  scroll_update->set_layer_id(layer->id());
  gfx::ScrollOffset scroll_delta = offset_after_update - layer->scroll_offset();
  gfx::Vector2dF scroll_delta_vector =
      gfx::ScrollOffsetToVector2dF(scroll_delta);
  Vector2dFToProto(scroll_delta_vector, scroll_update->mutable_scroll_delta());
  return scroll_delta_vector;
}

class UpdateTrackingRemoteCompositorBridge : public FakeRemoteCompositorBridge {
 public:
  UpdateTrackingRemoteCompositorBridge(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
      : FakeRemoteCompositorBridge(std::move(compositor_main_task_runner)) {}

  ~UpdateTrackingRemoteCompositorBridge() override = default;

  void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state) override {
    num_updates_received_++;
    compositor_proto_state_ = std::move(compositor_proto_state);
  };

  void SendUpdates(const proto::ClientStateUpdate& client_state_update) {
    client_->ApplyStateUpdateFromClient(client_state_update);
  }

  int num_updates_received() const { return num_updates_received_; }

  CompositorProtoState* compositor_proto_state() {
    return compositor_proto_state_.get();
  }

 private:
  int num_updates_received_ = 0;
  std::unique_ptr<CompositorProtoState> compositor_proto_state_;
};

class MockLayerTreeHostClient : public StubLayerTreeHostClient {
 public:
  MockLayerTreeHostClient() = default;
  ~MockLayerTreeHostClient() override = default;

  void set_update_host_callback(base::Closure callback) {
    update_host_callback_ = callback;
  }

  void UpdateLayerTreeHost() override {
    update_host_callback_.Run();
    DidUpdateLayerTreeHost();
  }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    DidReceiveBeginMainFrame();
  }

  // LayerTreeHostClient implementation.
  MOCK_METHOD0(WillBeginMainFrame, void());
  MOCK_METHOD0(DidBeginMainFrame, void());
  MOCK_METHOD0(DidReceiveBeginMainFrame, void());
  MOCK_METHOD0(DidUpdateLayerTreeHost, void());
  MOCK_METHOD5(ApplyViewportDeltas,
               void(const gfx::Vector2dF&,
                    const gfx::Vector2dF&,
                    const gfx::Vector2dF&,
                    float,
                    float));
  MOCK_METHOD0(WillCommit, void());
  MOCK_METHOD0(DidCommit, void());
  MOCK_METHOD0(DidCommitAndDrawFrame, void());
  MOCK_METHOD0(DidReceiveCompositorFrameAck, void());

 private:
  base::Closure update_host_callback_;
};

class MockLayer : public Layer {
 public:
  explicit MockLayer(bool update) : update_(update) {}

  bool Update() override {
    did_update_ = true;
    return update_;
  }

  bool did_update() const { return did_update_; }

 private:
  ~MockLayer() override {}

  bool update_;
  bool did_update_ = false;
};

class MockLayerTree : public LayerTree {
 public:
  MockLayerTree(MutatorHost* mutator_host, LayerTreeHost* layer_tree_host)
      : LayerTree(mutator_host, layer_tree_host) {}
  ~MockLayerTree() override {}

  // We don't want tree sync requests to trigger commits.
  void SetNeedsFullTreeSync() override {}
};

class LayerTreeHostRemoteForTesting : public LayerTreeHostRemote {
 public:
  explicit LayerTreeHostRemoteForTesting(InitParams* params)
      : LayerTreeHostRemote(
            params,
            base::MakeUnique<MockLayerTree>(params->mutator_host, this)) {}
  ~LayerTreeHostRemoteForTesting() override {}
};

class LayerTreeHostRemoteTest : public testing::Test {
 public:
  LayerTreeHostRemoteTest() {
    mock_layer_tree_host_client_.set_update_host_callback(base::Bind(
        &LayerTreeHostRemoteTest::UpdateLayerTreeHost, base::Unretained(this)));
  }
  ~LayerTreeHostRemoteTest() override {}

  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

    LayerTreeHostRemote::InitParams params;
    params.client = &mock_layer_tree_host_client_;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
        base::ThreadTaskRunnerHandle::Get();
    params.main_task_runner = main_task_runner;
    std::unique_ptr<UpdateTrackingRemoteCompositorBridge>
        remote_compositor_bridge =
            base::MakeUnique<UpdateTrackingRemoteCompositorBridge>(
                main_task_runner);
    remote_compositor_bridge_ = remote_compositor_bridge.get();
    params.remote_compositor_bridge = std::move(remote_compositor_bridge);
    params.engine_picture_cache =
        image_serialization_processor_.CreateEnginePictureCache();
    LayerTreeSettings settings;
    params.settings = &settings;
    params.mutator_host = animation_host_.get();
    layer_tree_host_ = base::MakeUnique<LayerTreeHostRemoteForTesting>(&params);

    // Make sure the root layer always updates.
    root_layer_ = make_scoped_refptr(new MockLayer(false));
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(5, 10));
    layer_tree_host_->GetLayerTree()->SetRootLayer(root_layer_);
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&mock_layer_tree_host_client_);
    layer_tree_host_ = nullptr;
    root_layer_ = nullptr;
    remote_compositor_bridge_ = nullptr;
  }

  void UpdateLayerTreeHost() {
    if (needs_animate_during_main_frame_) {
      layer_tree_host_->SetNeedsAnimate();
      needs_animate_during_main_frame_ = false;
    }

    if (needs_commit_during_main_frame_) {
      layer_tree_host_->SetNeedsCommit();
      needs_commit_during_main_frame_ = false;
    }
  }

  void set_needs_animate_during_main_frame(bool needs) {
    needs_animate_during_main_frame_ = needs;
  }

  void set_needs_commit_during_main_frame(bool needs) {
    needs_commit_during_main_frame_ = needs;
  }

 protected:
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<LayerTreeHostRemote> layer_tree_host_;
  StrictMock<MockLayerTreeHostClient> mock_layer_tree_host_client_;
  UpdateTrackingRemoteCompositorBridge* remote_compositor_bridge_ = nullptr;
  scoped_refptr<MockLayer> root_layer_;
  FakeImageSerializationProcessor image_serialization_processor_;

  bool needs_animate_during_main_frame_ = false;
  bool needs_commit_during_main_frame_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostRemoteTest);
};

TEST_F(LayerTreeHostRemoteTest, BeginMainFrameAnimateOnly) {
  // The main frame should run until the animate step only.
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME(mock_layer_tree_host_client_, num_of_frames);

  int previous_source_frame = layer_tree_host_->SourceFrameNumber();
  layer_tree_host_->SetNeedsAnimate();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(root_layer_->did_update());
  EXPECT_EQ(0, remote_compositor_bridge_->num_updates_received());
  EXPECT_EQ(++previous_source_frame, layer_tree_host_->SourceFrameNumber());
}

TEST_F(LayerTreeHostRemoteTest, BeginMainFrameUpdateLayers) {
  // The main frame should run until the update layers step only.
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME(mock_layer_tree_host_client_, num_of_frames);

  int previous_source_frame = layer_tree_host_->SourceFrameNumber();
  layer_tree_host_->SetNeedsUpdateLayers();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(0, remote_compositor_bridge_->num_updates_received());
  EXPECT_EQ(++previous_source_frame, layer_tree_host_->SourceFrameNumber());
}

TEST_F(LayerTreeHostRemoteTest, BeginMainFrameCommit) {
  // The main frame should run until the commit step.
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_,
                                     num_of_frames);

  int previous_source_frame = layer_tree_host_->SourceFrameNumber();
  layer_tree_host_->SetNeedsCommit();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
  EXPECT_EQ(++previous_source_frame, layer_tree_host_->SourceFrameNumber());
}

TEST_F(LayerTreeHostRemoteTest, BeginMainFrameMultipleRequests) {
  // Multiple BeginMainFrame requests should result in a single main frame
  // update.
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_,
                                     num_of_frames);

  layer_tree_host_->SetNeedsAnimate();
  layer_tree_host_->SetNeedsUpdateLayers();
  layer_tree_host_->SetNeedsCommit();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, CommitRequestThenDeferCommits) {
  // Make a commit request, followed by a request to defer commits.
  layer_tree_host_->SetNeedsCommit();
  layer_tree_host_->SetDeferCommits(true);

  // We should not have seen any BeginMainFrames.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_layer_tree_host_client_);
  EXPECT_FALSE(root_layer_->did_update());
  EXPECT_EQ(0, remote_compositor_bridge_->num_updates_received());

  // Now enable commits and ensure we see a BeginMainFrame.
  layer_tree_host_->SetDeferCommits(false);
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_,
                                     num_of_frames);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, DeferCommitsThenCommitRequest) {
  // Defer commits followed by a commit request.
  layer_tree_host_->SetDeferCommits(true);
  layer_tree_host_->SetNeedsCommit();

  // We should not have seen any BeginMainFrames.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_layer_tree_host_client_);
  EXPECT_FALSE(root_layer_->did_update());
  EXPECT_EQ(0, remote_compositor_bridge_->num_updates_received());

  // Now enable commits and ensure we see a BeginMainFrame.
  layer_tree_host_->SetDeferCommits(false);
  InSequence s;
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_,
                                     num_of_frames);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, RequestAnimateDuringMainFrame) {
  // An animate request during BeginMainFrame should result in a second main
  // frame being scheduled.
  set_needs_animate_during_main_frame(true);
  int num_of_frames = 2;
  EXPECT_BEGIN_MAIN_FRAME(mock_layer_tree_host_client_, num_of_frames);

  layer_tree_host_->SetNeedsAnimate();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(root_layer_->did_update());
  EXPECT_EQ(0, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, RequestCommitDuringMainFrame) {
  // A commit request during a BeginMainFrame scheduled for an animate request
  // should go till the commit stage.
  set_needs_commit_during_main_frame(true);
  int num_of_frames = 1;
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_,
                                     num_of_frames);

  layer_tree_host_->SetNeedsAnimate();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, RequestCommitDuringLayerUpdates) {
  // A layer update during a main frame should result in a commit.
  scoped_refptr<Layer> child_layer = make_scoped_refptr(new MockLayer(true));
  child_layer->SetIsDrawable(true);
  child_layer->SetBounds(gfx::Size(5, 10));
  root_layer_->AddChild(child_layer);
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_, 1);

  layer_tree_host_->SetNeedsUpdateLayers();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(root_layer_->did_update());
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
}

TEST_F(LayerTreeHostRemoteTest, ScrollAndScaleSync) {
  scoped_refptr<Layer> child_layer1 = make_scoped_refptr(new MockLayer(false));
  root_layer_->AddChild(child_layer1);

  scoped_refptr<Layer> child_layer2 = make_scoped_refptr(new MockLayer(false));
  child_layer1->AddChild(child_layer2);
  child_layer2->SetScrollOffset(gfx::ScrollOffset(3, 9));

  scoped_refptr<Layer> inner_viewport_layer =
      make_scoped_refptr(new MockLayer(false));
  inner_viewport_layer->SetScrollOffset(gfx::ScrollOffset(5, 10));
  root_layer_->AddChild(inner_viewport_layer);
  layer_tree_host_->GetLayerTree()->RegisterViewportLayers(
      nullptr, nullptr, inner_viewport_layer, nullptr);

  // First test case.
  gfx::ScrollOffset expected_child1_offset(4, 5);
  gfx::ScrollOffset expected_child2_offset(3, 10);
  gfx::ScrollOffset expected_inner_viewport_offset(-2, 5);
  float expected_page_scale = 2.0f;

  proto::ClientStateUpdate client_state_update;
  SerializeScrollUpdate(&client_state_update, child_layer1.get(),
                        expected_child1_offset);
  SerializeScrollUpdate(&client_state_update, child_layer2.get(),
                        expected_child2_offset);
  gfx::Vector2dF inner_viewport_delta =
      SerializeScrollUpdate(&client_state_update, inner_viewport_layer.get(),
                            expected_inner_viewport_offset);
  float page_scale_delta =
      expected_page_scale /
      layer_tree_host_->GetLayerTree()->page_scale_factor();
  client_state_update.set_page_scale_delta(page_scale_delta);

  EXPECT_CALL(mock_layer_tree_host_client_,
              ApplyViewportDeltas(inner_viewport_delta, gfx::Vector2dF(),
                                  gfx::Vector2dF(), page_scale_delta, 0.0f))
      .Times(1);

  remote_compositor_bridge_->SendUpdates(client_state_update);

  // The host should have pre-emtively applied the changes.
  EXPECT_EQ(expected_page_scale,
            layer_tree_host_->GetLayerTree()->page_scale_factor());
  EXPECT_EQ(expected_child1_offset, child_layer1->scroll_offset());
  EXPECT_EQ(expected_child2_offset, child_layer2->scroll_offset());
  EXPECT_EQ(expected_inner_viewport_offset,
            inner_viewport_layer->scroll_offset());

  // Remove a layer from the tree and send a scroll update for it.
  child_layer1->RemoveAllChildren();
  proto::ClientStateUpdate client_state_update2;
  SerializeScrollUpdate(&client_state_update2, child_layer2.get(),
                        gfx::ScrollOffset(9, 10));
  remote_compositor_bridge_->SendUpdates(client_state_update2);
}

TEST_F(LayerTreeHostRemoteTest, IdentifiedLayersToSkipUpdates) {
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_, 1);

  scoped_refptr<MockLayer> non_drawable_layer = new MockLayer(true);
  non_drawable_layer->SetIsDrawable(false);
  non_drawable_layer->SetBounds(gfx::Size(5, 5));

  scoped_refptr<MockLayer> empty_bound_layer = new MockLayer(true);
  empty_bound_layer->SetIsDrawable(true);
  empty_bound_layer->SetBounds(gfx::Size());

  scoped_refptr<MockLayer> transparent_layer = new MockLayer(true);
  transparent_layer->SetIsDrawable(true);
  transparent_layer->SetBounds(gfx::Size(5, 5));
  transparent_layer->SetOpacity(0.f);

  scoped_refptr<MockLayer> updated_layer = new MockLayer(true);
  updated_layer->SetIsDrawable(true);
  updated_layer->SetBounds(gfx::Size(5, 5));

  root_layer_->AddChild(non_drawable_layer);
  root_layer_->AddChild(empty_bound_layer);
  empty_bound_layer->SetMaskLayer(transparent_layer.get());
  empty_bound_layer->AddChild(updated_layer);
  DCHECK(updated_layer->GetLayerTreeHostForTesting());

  // Commit and serialize.
  layer_tree_host_->SetNeedsCommit();
  base::RunLoop().RunUntilIdle();

  // Only the updated_layer should have been updated.
  EXPECT_FALSE(non_drawable_layer->did_update());
  EXPECT_FALSE(empty_bound_layer->did_update());
  EXPECT_FALSE(transparent_layer->did_update());
  EXPECT_TRUE(updated_layer->did_update());
}

TEST_F(LayerTreeHostRemoteTest, OnlyPushPropertiesOnChangingLayers) {
  EXPECT_BEGIN_MAIN_FRAME_AND_COMMIT(mock_layer_tree_host_client_, 2);

  // Setup a tree with 3 nodes.
  scoped_refptr<Layer> child_layer = Layer::Create();
  scoped_refptr<Layer> grandchild_layer = Layer::Create();
  root_layer_->AddChild(child_layer);
  child_layer->AddChild(grandchild_layer);
  layer_tree_host_->SetNeedsCommit();

  base::RunLoop().RunUntilIdle();

  // Ensure the first proto contains all layer updates.
  EXPECT_EQ(1, remote_compositor_bridge_->num_updates_received());
  CompositorProtoState* compositor_proto_state =
      remote_compositor_bridge_->compositor_proto_state();
  const proto::LayerUpdate& layer_updates_first_commit =
      compositor_proto_state->compositor_message->layer_tree_host()
          .layer_updates();

  std::unordered_set<int> layer_updates_id_set;
  for (int i = 0; i < layer_updates_first_commit.layers_size(); ++i) {
    layer_updates_id_set.insert(layer_updates_first_commit.layers(i).id());
  }

  EXPECT_TRUE(layer_updates_id_set.find(root_layer_->id()) !=
              layer_updates_id_set.end());
  EXPECT_TRUE(layer_updates_id_set.find(child_layer->id()) !=
              layer_updates_id_set.end());
  EXPECT_TRUE(layer_updates_id_set.find(grandchild_layer->id()) !=
              layer_updates_id_set.end());
  EXPECT_EQ(3, layer_updates_first_commit.layers_size());

  // Modify the |child_layer|, and run the second frame.
  child_layer->SetNeedsPushProperties();
  layer_tree_host_->SetNeedsCommit();
  base::RunLoop().RunUntilIdle();

  // Ensure the second proto only contains |child_layer|.
  EXPECT_EQ(2, remote_compositor_bridge_->num_updates_received());
  compositor_proto_state = remote_compositor_bridge_->compositor_proto_state();
  DCHECK(compositor_proto_state);
  const proto::LayerUpdate& layer_updates_second_commit =
      compositor_proto_state->compositor_message->layer_tree_host()
          .layer_updates();
  EXPECT_EQ(1, layer_updates_second_commit.layers_size());
  EXPECT_EQ(child_layer->id(), layer_updates_second_commit.layers(0).id());
}

}  // namespace
}  // namespace cc
