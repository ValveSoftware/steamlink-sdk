// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<MockLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int layer_id) {
    return base::WrapUnique(new MockLayerImpl(tree_impl, layer_id));
  }
  ~MockLayerImpl() override {
    if (layer_impl_destruction_list_)
      layer_impl_destruction_list_->push_back(id());
  }

  void SetLayerImplDestructionList(std::vector<int>* list) {
    layer_impl_destruction_list_ = list;
  }

 private:
  MockLayerImpl(LayerTreeImpl* tree_impl, int layer_id)
      : LayerImpl(tree_impl, layer_id), layer_impl_destruction_list_(NULL) {}

  std::vector<int>* layer_impl_destruction_list_;
};

class MockLayer : public Layer {
 public:
  static scoped_refptr<MockLayer> Create(
      std::vector<int>* layer_impl_destruction_list) {
    return make_scoped_refptr(new MockLayer(layer_impl_destruction_list));
  }

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) override {
    return MockLayerImpl::Create(tree_impl, layer_id_);
  }

  void PushPropertiesTo(LayerImpl* layer_impl) override {
    Layer::PushPropertiesTo(layer_impl);

    MockLayerImpl* mock_layer_impl = static_cast<MockLayerImpl*>(layer_impl);
    mock_layer_impl->SetLayerImplDestructionList(layer_impl_destruction_list_);
  }

 private:
  explicit MockLayer(std::vector<int>* layer_impl_destruction_list)
      : layer_impl_destruction_list_(layer_impl_destruction_list) {}
  ~MockLayer() override {}

  std::vector<int>* layer_impl_destruction_list_;
};

void ExpectTreesAreIdentical(Layer* root_layer,
                             LayerImpl* root_layer_impl,
                             LayerTreeImpl* tree_impl) {
  auto layer_iter = root_layer->layer_tree_host()->begin();
  auto layer_impl_iter = tree_impl->begin();
  for (; layer_iter != root_layer->layer_tree_host()->end();
       ++layer_iter, ++layer_impl_iter) {
    Layer* layer = *layer_iter;
    LayerImpl* layer_impl = *layer_impl_iter;
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layer_impl);

    EXPECT_EQ(layer->id(), layer_impl->id());
    EXPECT_EQ(layer_impl->layer_tree_impl(), tree_impl);

    EXPECT_EQ(layer->non_fast_scrollable_region(),
              layer_impl->non_fast_scrollable_region());

    const EffectTree& effect_tree = tree_impl->property_trees()->effect_tree;
    if (layer->mask_layer()) {
      SCOPED_TRACE("mask_layer");
      int mask_layer_id = layer->mask_layer()->id();
      EXPECT_TRUE(tree_impl->LayerById(mask_layer_id));
      EXPECT_EQ(mask_layer_id,
                effect_tree.Node(layer_impl->effect_tree_index())
                    ->data.mask_layer_id);
    }

    if (layer->replica_layer()) {
      SCOPED_TRACE("replica_layer");
      int replica_layer_id = layer->replica_layer()->id();
      EXPECT_TRUE(tree_impl->LayerById(layer->replica_layer()->id()));
      EXPECT_EQ(replica_layer_id,
                effect_tree.Node(layer_impl->effect_tree_index())
                    ->data.replica_layer_id);
      if (layer->replica_layer()->mask_layer()) {
        SCOPED_TRACE("replica_mask_layer");
        int replica_mask_layer_id = layer->replica_layer()->mask_layer()->id();
        EXPECT_TRUE(tree_impl->LayerById(replica_mask_layer_id));
        EXPECT_EQ(replica_mask_layer_id,
                  effect_tree.Node(layer_impl->effect_tree_index())
                      ->data.replica_mask_layer_id);
      }
    }

    const Layer* layer_scroll_parent = layer->scroll_parent();

    if (layer_scroll_parent) {
      ASSERT_TRUE(layer_scroll_parent->scroll_children()->find(layer) !=
                  layer_scroll_parent->scroll_children()->end());
    }

    const Layer* layer_clip_parent = layer->clip_parent();

    if (layer_clip_parent) {
      const std::set<Layer*>* clip_children =
          layer_clip_parent->clip_children();
      ASSERT_TRUE(clip_children->find(layer) != clip_children->end());
    }
  }
}

class TreeSynchronizerTest : public testing::Test {
 public:
  TreeSynchronizerTest()
      : client_(FakeLayerTreeHostClient::DIRECT_3D),
        host_(FakeLayerTreeHost::Create(&client_, &task_graph_runner_)) {}

 protected:
  FakeLayerTreeHostClient client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<FakeLayerTreeHost> host_;

  bool is_equal(ScrollTree::ScrollOffsetMap map,
                ScrollTree::ScrollOffsetMap other) {
    if (map.size() != other.size())
      return false;
    for (auto& map_entry : map) {
      if (other.find(map_entry.first) == other.end())
        return false;
      SyncedScrollOffset& from_map = *map_entry.second.get();
      SyncedScrollOffset& from_other = *other[map_entry.first].get();
      if (from_map.PendingBase() != from_other.PendingBase() ||
          from_map.ActiveBase() != from_other.ActiveBase() ||
          from_map.Delta() != from_other.Delta() ||
          from_map.PendingDelta().get() != from_other.PendingDelta().get())
        return false;
    }
    return true;
  }
};

// Attempts to synchronizes a null tree. This should not crash, and should
// return a null tree.
TEST_F(TreeSynchronizerTest, SyncNullTree) {
  TreeSynchronizer::SynchronizeTrees(static_cast<Layer*>(NULL),
                                     host_->active_tree());
  EXPECT_TRUE(!host_->active_tree()->root_layer_for_testing());
}

// Constructs a very simple tree and synchronizes it without trying to reuse any
// preexisting layers.
TEST_F(TreeSynchronizerTest, SyncSimpleTreeFromEmpty) {
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  layer_tree_root->AddChild(Layer::Create());
  layer_tree_root->AddChild(Layer::Create());

  host_->SetRootLayer(layer_tree_root);

  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());

  ExpectTreesAreIdentical(layer_tree_root.get(),
                          host_->active_tree()->root_layer_for_testing(),
                          host_->active_tree());
}

// Constructs a very simple tree and synchronizes it attempting to reuse some
// layers
TEST_F(TreeSynchronizerTest, SyncSimpleTreeReusingLayers) {
  std::vector<int> layer_impl_destruction_list;

  scoped_refptr<Layer> layer_tree_root =
      MockLayer::Create(&layer_impl_destruction_list);
  layer_tree_root->AddChild(MockLayer::Create(&layer_impl_destruction_list));
  layer_tree_root->AddChild(MockLayer::Create(&layer_impl_destruction_list));
  int second_layer_impl_id = layer_tree_root->children()[1]->id();

  host_->SetRootLayer(layer_tree_root);

  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // We have to push properties to pick up the destruction list pointer.
  TreeSynchronizer::PushLayerProperties(layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  // Add a new layer to the Layer side
  layer_tree_root->children()[0]->AddChild(
      MockLayer::Create(&layer_impl_destruction_list));
  // Remove one.
  layer_tree_root->children()[1]->RemoveFromParent();

  // Synchronize again. After the sync the trees should be equivalent and we
  // should have created and destroyed one LayerImpl.
  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();

  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  ASSERT_EQ(1u, layer_impl_destruction_list.size());
  EXPECT_EQ(second_layer_impl_id, layer_impl_destruction_list[0]);

  host_->active_tree()->DetachLayers();
}

// Constructs a very simple tree and checks that a stacking-order change is
// tracked properly.
TEST_F(TreeSynchronizerTest, SyncSimpleTreeAndTrackStackingOrderChange) {
  std::vector<int> layer_impl_destruction_list;

  // Set up the tree and sync once. child2 needs to be synced here, too, even
  // though we remove it to set up the intended scenario.
  scoped_refptr<Layer> layer_tree_root =
      MockLayer::Create(&layer_impl_destruction_list);
  scoped_refptr<Layer> child2 = MockLayer::Create(&layer_impl_destruction_list);
  layer_tree_root->AddChild(MockLayer::Create(&layer_impl_destruction_list));
  layer_tree_root->AddChild(child2);
  int child1_id = layer_tree_root->children()[0]->id();
  int child2_id = layer_tree_root->children()[1]->id();

  host_->SetRootLayer(layer_tree_root);

  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // We have to push properties to pick up the destruction list pointer.
  TreeSynchronizer::PushLayerProperties(layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  host_->active_tree()->ResetAllChangeTracking();

  // re-insert the layer and sync again.
  child2->RemoveFromParent();
  layer_tree_root->AddChild(child2);
  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  TreeSynchronizer::PushLayerProperties(layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  // Check that the impl thread properly tracked the change.
  EXPECT_FALSE(layer_impl_tree_root->LayerPropertyChanged());
  EXPECT_FALSE(
      host_->active_tree()->LayerById(child1_id)->LayerPropertyChanged());
  EXPECT_TRUE(
      host_->active_tree()->LayerById(child2_id)->LayerPropertyChanged());
  host_->active_tree()->DetachLayers();
}

TEST_F(TreeSynchronizerTest, SyncSimpleTreeAndProperties) {
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  layer_tree_root->AddChild(Layer::Create());
  layer_tree_root->AddChild(Layer::Create());

  host_->SetRootLayer(layer_tree_root);

  // Pick some random properties to set. The values are not important, we're
  // just testing that at least some properties are making it through.
  gfx::PointF root_position = gfx::PointF(2.3f, 7.4f);
  layer_tree_root->SetPosition(root_position);

  gfx::Size second_child_bounds = gfx::Size(25, 53);
  layer_tree_root->children()[1]->SetBounds(second_child_bounds);
  layer_tree_root->children()[1]->SavePaintProperties();
  int second_child_id = layer_tree_root->children()[1]->id();

  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  TreeSynchronizer::PushLayerProperties(layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  // Check that the property values we set on the Layer tree are reflected in
  // the LayerImpl tree.
  gfx::PointF root_layer_impl_position = layer_impl_tree_root->position();
  EXPECT_EQ(root_position.x(), root_layer_impl_position.x());
  EXPECT_EQ(root_position.y(), root_layer_impl_position.y());

  gfx::Size second_layer_impl_child_bounds =
      layer_impl_tree_root->layer_tree_impl()
          ->LayerById(second_child_id)
          ->bounds();
  EXPECT_EQ(second_child_bounds.width(),
            second_layer_impl_child_bounds.width());
  EXPECT_EQ(second_child_bounds.height(),
            second_layer_impl_child_bounds.height());
}

TEST_F(TreeSynchronizerTest, ReuseLayerImplsAfterStructuralChange) {
  std::vector<int> layer_impl_destruction_list;

  // Set up a tree with this sort of structure:
  // root --- A --- B ---+--- C
  //                     |
  //                     +--- D
  scoped_refptr<Layer> layer_tree_root =
      MockLayer::Create(&layer_impl_destruction_list);
  layer_tree_root->AddChild(MockLayer::Create(&layer_impl_destruction_list));

  scoped_refptr<Layer> layer_a = layer_tree_root->children()[0];
  layer_a->AddChild(MockLayer::Create(&layer_impl_destruction_list));

  scoped_refptr<Layer> layer_b = layer_a->children()[0];
  layer_b->AddChild(MockLayer::Create(&layer_impl_destruction_list));

  scoped_refptr<Layer> layer_c = layer_b->children()[0];
  layer_b->AddChild(MockLayer::Create(&layer_impl_destruction_list));
  scoped_refptr<Layer> layer_d = layer_b->children()[1];

  host_->SetRootLayer(layer_tree_root);

  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // We have to push properties to pick up the destruction list pointer.
  TreeSynchronizer::PushLayerProperties(layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  // Now restructure the tree to look like this:
  // root --- D ---+--- A
  //               |
  //               +--- C --- B
  layer_tree_root->RemoveAllChildren();
  layer_d->RemoveAllChildren();
  layer_tree_root->AddChild(layer_d);
  layer_a->RemoveAllChildren();
  layer_d->AddChild(layer_a);
  layer_c->RemoveAllChildren();
  layer_d->AddChild(layer_c);
  layer_b->RemoveAllChildren();
  layer_c->AddChild(layer_b);

  // After another synchronize our trees should match and we should not have
  // destroyed any LayerImpls
  TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                     host_->active_tree());
  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  EXPECT_EQ(0u, layer_impl_destruction_list.size());

  host_->active_tree()->DetachLayers();
}

// Constructs a very simple tree, synchronizes it, then synchronizes to a
// totally new tree. All layers from the old tree should be deleted.
TEST_F(TreeSynchronizerTest, SyncSimpleTreeThenDestroy) {
  std::vector<int> layer_impl_destruction_list;

  scoped_refptr<Layer> old_layer_tree_root =
      MockLayer::Create(&layer_impl_destruction_list);
  old_layer_tree_root->AddChild(
      MockLayer::Create(&layer_impl_destruction_list));
  old_layer_tree_root->AddChild(
      MockLayer::Create(&layer_impl_destruction_list));

  host_->SetRootLayer(old_layer_tree_root);

  int old_tree_root_layer_id = old_layer_tree_root->id();
  int old_tree_first_child_layer_id = old_layer_tree_root->children()[0]->id();
  int old_tree_second_child_layer_id = old_layer_tree_root->children()[1]->id();

  TreeSynchronizer::SynchronizeTrees(old_layer_tree_root.get(),
                                     host_->active_tree());
  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(old_layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // We have to push properties to pick up the destruction list pointer.
  TreeSynchronizer::PushLayerProperties(old_layer_tree_root->layer_tree_host(),
                                        host_->active_tree());

  // Remove all children on the Layer side.
  old_layer_tree_root->RemoveAllChildren();

  // Synchronize again. After the sync all LayerImpls from the old tree should
  // be deleted.
  scoped_refptr<Layer> new_layer_tree_root = Layer::Create();
  host_->SetRootLayer(new_layer_tree_root);

  TreeSynchronizer::SynchronizeTrees(new_layer_tree_root.get(),
                                     host_->active_tree());
  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(new_layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  ASSERT_EQ(3u, layer_impl_destruction_list.size());

  EXPECT_TRUE(std::find(layer_impl_destruction_list.begin(),
                        layer_impl_destruction_list.end(),
                        old_tree_root_layer_id) !=
              layer_impl_destruction_list.end());
  EXPECT_TRUE(std::find(layer_impl_destruction_list.begin(),
                        layer_impl_destruction_list.end(),
                        old_tree_first_child_layer_id) !=
              layer_impl_destruction_list.end());
  EXPECT_TRUE(std::find(layer_impl_destruction_list.begin(),
                        layer_impl_destruction_list.end(),
                        old_tree_second_child_layer_id) !=
              layer_impl_destruction_list.end());
}

// Constructs+syncs a tree with mask, replica, and replica mask layers.
TEST_F(TreeSynchronizerTest, SyncMaskReplicaAndReplicaMaskLayers) {
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  layer_tree_root->AddChild(Layer::Create());
  layer_tree_root->AddChild(Layer::Create());
  layer_tree_root->AddChild(Layer::Create());

  // First child gets a mask layer.
  scoped_refptr<Layer> mask_layer = Layer::Create();
  layer_tree_root->children()[0]->SetMaskLayer(mask_layer.get());

  // Second child gets a replica layer.
  scoped_refptr<Layer> replica_layer = Layer::Create();
  layer_tree_root->children()[1]->SetReplicaLayer(replica_layer.get());

  // Third child gets a replica layer with a mask layer.
  scoped_refptr<Layer> replica_layer_with_mask = Layer::Create();
  scoped_refptr<Layer> replica_mask_layer = Layer::Create();
  replica_layer_with_mask->SetMaskLayer(replica_mask_layer.get());
  layer_tree_root->children()[2]->SetReplicaLayer(
      replica_layer_with_mask.get());

  host_->SetRootLayer(layer_tree_root);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreateLayerImplTree();

  LayerImpl* layer_impl_tree_root =
      host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // Remove the mask layer.
  layer_tree_root->children()[0]->SetMaskLayer(NULL);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreateLayerImplTree();

  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // Remove the replica layer.
  layer_tree_root->children()[1]->SetReplicaLayer(NULL);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreateLayerImplTree();

  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  // Remove the replica mask.
  replica_layer_with_mask->SetMaskLayer(NULL);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreateLayerImplTree();

  layer_impl_tree_root = host_->active_tree()->root_layer_for_testing();
  ExpectTreesAreIdentical(layer_tree_root.get(), layer_impl_tree_root,
                          host_->active_tree());

  host_->active_tree()->DetachLayers();
}

TEST_F(TreeSynchronizerTest, SynchronizeCurrentlyScrollingNode) {
  LayerTreeSettings settings;
  FakeLayerTreeHostImplClient client;
  FakeImplTaskRunnerProvider task_runner_provider;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl* host_impl = host_->host_impl();
  host_impl->CreatePendingTree();

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_clip_layer = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scoped_refptr<Layer> transient_scroll_clip_layer = Layer::Create();
  scoped_refptr<Layer> transient_scroll_layer = Layer::Create();

  layer_tree_root->AddChild(transient_scroll_clip_layer);
  transient_scroll_clip_layer->AddChild(transient_scroll_layer);
  transient_scroll_layer->AddChild(scroll_clip_layer);
  scroll_clip_layer->AddChild(scroll_layer);

  transient_scroll_layer->SetScrollClipLayerId(
      transient_scroll_clip_layer->id());
  scroll_layer->SetScrollClipLayerId(scroll_clip_layer->id());
  host_->SetRootLayer(layer_tree_root);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  ExpectTreesAreIdentical(layer_tree_root.get(),
                          host_impl->active_tree()->root_layer_for_testing(),
                          host_impl->active_tree());

  host_impl->active_tree()->SetCurrentlyScrollingLayer(
      host_impl->active_tree()->LayerById(scroll_layer->id()));
  transient_scroll_layer->SetScrollClipLayerId(Layer::INVALID_ID);
  host_->BuildPropertyTreesForTesting();

  host_impl->CreatePendingTree();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  EXPECT_EQ(scroll_layer->id(),
            host_impl->active_tree()->CurrentlyScrollingLayer()->id());
}

TEST_F(TreeSynchronizerTest, SynchronizeScrollTreeScrollOffsetMap) {
  host_->InitializeSingleThreaded(&client_, base::ThreadTaskRunnerHandle::Get(),
                                  nullptr);
  LayerTreeSettings settings;
  FakeLayerTreeHostImplClient client;
  FakeImplTaskRunnerProvider task_runner_provider;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl* host_impl = host_->host_impl();
  host_impl->CreatePendingTree();

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_clip_layer = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scoped_refptr<Layer> transient_scroll_clip_layer = Layer::Create();
  scoped_refptr<Layer> transient_scroll_layer = Layer::Create();

  layer_tree_root->AddChild(transient_scroll_clip_layer);
  transient_scroll_clip_layer->AddChild(transient_scroll_layer);
  transient_scroll_layer->AddChild(scroll_clip_layer);
  scroll_clip_layer->AddChild(scroll_layer);

  transient_scroll_layer->SetScrollClipLayerId(
      transient_scroll_clip_layer->id());
  scroll_layer->SetScrollClipLayerId(scroll_clip_layer->id());
  transient_scroll_layer->SetScrollOffset(gfx::ScrollOffset(1, 2));
  scroll_layer->SetScrollOffset(gfx::ScrollOffset(10, 20));

  host_->SetRootLayer(layer_tree_root);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  ExpectTreesAreIdentical(layer_tree_root.get(),
                          host_impl->active_tree()->root_layer_for_testing(),
                          host_impl->active_tree());

  // After the initial commit, scroll_offset_map in scroll_tree is expected to
  // have one entry for scroll_layer and one entry for transient_scroll_layer,
  // the pending base and active base must be the same at this stage.
  ScrollTree::ScrollOffsetMap scroll_offset_map;
  scroll_offset_map[scroll_layer->id()] = new SyncedScrollOffset;
  scroll_offset_map[transient_scroll_layer->id()] = new SyncedScrollOffset;
  scroll_offset_map[scroll_layer->id()]->PushFromMainThread(
      scroll_layer->scroll_offset());
  scroll_offset_map[scroll_layer->id()]->PushPendingToActive();
  scroll_offset_map[transient_scroll_layer->id()]->PushFromMainThread(
      transient_scroll_layer->scroll_offset());
  scroll_offset_map[transient_scroll_layer->id()]->PushPendingToActive();
  EXPECT_TRUE(
      is_equal(scroll_offset_map, host_impl->active_tree()
                                      ->property_trees()
                                      ->scroll_tree.scroll_offset_map()));

  // Set ScrollOffset active delta: gfx::ScrollOffset(10, 10)
  LayerImpl* scroll_layer_impl =
      host_impl->active_tree()->LayerById(scroll_layer->id());
  ScrollTree& scroll_tree =
      host_impl->active_tree()->property_trees()->scroll_tree;
  scroll_tree.SetScrollOffset(scroll_layer_impl->id(),
                              gfx::ScrollOffset(20, 30));

  // Pull ScrollOffset delta for main thread, and change offset on main thread
  std::unique_ptr<ScrollAndScaleSet> scroll_info(new ScrollAndScaleSet());
  scroll_tree.CollectScrollDeltas(scroll_info.get());
  host_->proxy()->SetNeedsCommit();
  host_->ApplyScrollAndScale(scroll_info.get());
  EXPECT_EQ(gfx::ScrollOffset(20, 30), scroll_layer->scroll_offset());
  scroll_layer->SetScrollOffset(gfx::ScrollOffset(100, 100));

  // More update to ScrollOffset active delta: gfx::ScrollOffset(20, 20)
  scroll_tree.SetScrollOffset(scroll_layer_impl->id(),
                              gfx::ScrollOffset(40, 50));
  host_impl->active_tree()->SetCurrentlyScrollingLayer(scroll_layer_impl);

  // Make one layer unscrollable so that scroll tree topology changes
  transient_scroll_layer->SetScrollClipLayerId(Layer::INVALID_ID);
  host_->BuildPropertyTreesForTesting();

  host_impl->CreatePendingTree();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  EXPECT_EQ(scroll_layer->id(),
            host_impl->active_tree()->CurrentlyScrollingLayer()->id());
  scroll_offset_map.erase(transient_scroll_layer->id());
  scroll_offset_map[scroll_layer->id()]->SetCurrent(gfx::ScrollOffset(20, 30));
  scroll_offset_map[scroll_layer->id()]->PullDeltaForMainThread();
  scroll_offset_map[scroll_layer->id()]->SetCurrent(gfx::ScrollOffset(40, 50));
  scroll_offset_map[scroll_layer->id()]->PushFromMainThread(
      gfx::ScrollOffset(100, 100));
  scroll_offset_map[scroll_layer->id()]->PushPendingToActive();
  EXPECT_TRUE(is_equal(scroll_offset_map, scroll_tree.scroll_offset_map()));
}

TEST_F(TreeSynchronizerTest, RefreshPropertyTreesCachedData) {
  host_->InitializeSingleThreaded(&client_, base::ThreadTaskRunnerHandle::Get(),
                                  nullptr);
  LayerTreeSettings settings;
  FakeLayerTreeHostImplClient client;
  FakeImplTaskRunnerProvider task_runner_provider;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl* host_impl = host_->host_impl();
  host_impl->CreatePendingTree();

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> transform_layer = Layer::Create();

  gfx::Transform scale_transform;
  scale_transform.Scale3d(2.f, 2.f, 2.f);
  // Force adding a transform node for the layer.
  transform_layer->SetTransform(scale_transform);

  layer_tree_root->AddChild(transform_layer);

  host_->SetRootLayer(layer_tree_root);
  host_->BuildPropertyTreesForTesting();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  // This arbitrarily set the animation scale for transform_layer and see if it
  // is
  // refreshed when pushing layer trees.
  host_impl->active_tree()->property_trees()->SetAnimationScalesForTesting(
      transform_layer->transform_tree_index(), 10.f, 10.f);
  EXPECT_EQ(
      CombinedAnimationScale(10.f, 10.f),
      host_impl->active_tree()->property_trees()->GetAnimationScales(
          transform_layer->transform_tree_index(), host_impl->active_tree()));

  host_impl->CreatePendingTree();
  host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();
  EXPECT_EQ(
      CombinedAnimationScale(0.f, 0.f),
      host_impl->active_tree()->property_trees()->GetAnimationScales(
          transform_layer->transform_tree_index(), host_impl->active_tree()));
}

}  // namespace
}  // namespace cc
