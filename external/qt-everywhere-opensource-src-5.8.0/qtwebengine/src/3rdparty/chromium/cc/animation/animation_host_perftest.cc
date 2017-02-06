// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

class AnimationHostPerfTest : public testing::Test {
 public:
  AnimationHostPerfTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    LayerTreeSettings settings;
    layer_tree_host_ =
        FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_, settings);
    layer_tree_host_->InitializeSingleThreaded(
        &fake_client_, base::ThreadTaskRunnerHandle::Get(), nullptr);

    root_layer_ = Layer::Create();
    layer_tree_host_->SetRootLayer(root_layer_);

    root_layer_impl_ = layer_tree_host_->CommitAndCreateLayerImplTree();
  }

  void TearDown() override {
    root_layer_ = nullptr;
    root_layer_impl_ = nullptr;

    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  AnimationHost* host() const { return layer_tree_host_->animation_host(); }
  AnimationHost* host_impl() const {
    return layer_tree_host_->host_impl()->animation_host();
  }

  void CreatePlayers(const int num_players) {
    scoped_refptr<AnimationTimeline> timeline =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    host()->AddAnimationTimeline(timeline);

    const int first_player_id = AnimationIdProvider::NextPlayerId();
    int last_player_id = first_player_id;

    for (int i = 0; i < num_players; ++i) {
      scoped_refptr<Layer> layer = Layer::Create();
      root_layer_->AddChild(layer);
      layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));

      scoped_refptr<AnimationPlayer> player =
          AnimationPlayer::Create(last_player_id);
      last_player_id = AnimationIdProvider::NextPlayerId();

      timeline->AttachPlayer(player);
      player->AttachElement(layer->element_id());
      EXPECT_TRUE(player->element_animations());
    }

    // Create impl players.
    layer_tree_host_->CommitAndCreateLayerImplTree();

    // Check impl instances created.
    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl()->GetTimelineById(timeline->id());
    EXPECT_TRUE(timeline_impl);
    for (int i = first_player_id; i < last_player_id; ++i)
      EXPECT_TRUE(timeline_impl->GetPlayerById(i));
  }

  void CreateTimelines(const int num_timelines) {
    const int first_timeline_id = AnimationIdProvider::NextTimelineId();
    int last_timeline_id = first_timeline_id;

    for (int i = 0; i < num_timelines; ++i) {
      scoped_refptr<AnimationTimeline> timeline =
          AnimationTimeline::Create(last_timeline_id);
      last_timeline_id = AnimationIdProvider::NextTimelineId();
      host()->AddAnimationTimeline(timeline);
    }

    // Create impl timelines.
    layer_tree_host_->CommitAndCreateLayerImplTree();

    // Check impl instances created.
    for (int i = first_timeline_id; i < last_timeline_id; ++i)
      EXPECT_TRUE(host_impl()->GetTimelineById(i));
  }

  void DoTest() {
    timer_.Reset();
    do {
      host()->PushPropertiesTo(host_impl());
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("push_properties_to", "", "", timer_.LapsPerSecond(),
                           "runs/s", true);
  }

 protected:
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  scoped_refptr<Layer> root_layer_;
  LayerImpl* root_layer_impl_;

  LapTimer timer_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(AnimationHostPerfTest, Push1000PlayersPropertiesTo) {
  CreatePlayers(1000);
  DoTest();
}

TEST_F(AnimationHostPerfTest, Push10TimelinesPropertiesTo) {
  CreateTimelines(10);
  DoTest();
}

TEST_F(AnimationHostPerfTest, Push1000TimelinesPropertiesTo) {
  CreateTimelines(1000);
  DoTest();
}

}  // namespace cc
