// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_internals_for_test.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutable_properties.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::_;

#define EXPECT_SET_NEEDS_FULL_TREE_SYNC(expect, code_to_test)          \
  do {                                                                 \
    EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times((expect)); \
    code_to_test;                                                      \
    Mock::VerifyAndClearExpectations(layer_tree_);                     \
  } while (false)

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                       \
  code_to_test;                                                                \
  root->GetLayerTree()->BuildPropertyTreesForTesting();                        \
  EXPECT_TRUE(root->subtree_property_changed());                               \
  EXPECT_TRUE(                                                                 \
      root->GetLayerTree()->LayerNeedsPushPropertiesForTesting(root.get()));   \
  EXPECT_TRUE(child->subtree_property_changed());                              \
  EXPECT_TRUE(                                                                 \
      child->GetLayerTree()->LayerNeedsPushPropertiesForTesting(child.get())); \
  EXPECT_TRUE(grand_child->subtree_property_changed());                        \
  EXPECT_TRUE(grand_child->GetLayerTree()->LayerNeedsPushPropertiesForTesting( \
      grand_child.get()));

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(code_to_test)                 \
  code_to_test;                                                                \
  EXPECT_FALSE(root->subtree_property_changed());                              \
  EXPECT_FALSE(                                                                \
      root->GetLayerTree()->LayerNeedsPushPropertiesForTesting(root.get()));   \
  EXPECT_FALSE(child->subtree_property_changed());                             \
  EXPECT_FALSE(                                                                \
      child->GetLayerTree()->LayerNeedsPushPropertiesForTesting(child.get())); \
  EXPECT_FALSE(grand_child->subtree_property_changed());                       \
  EXPECT_FALSE(                                                                \
      grand_child->GetLayerTree()->LayerNeedsPushPropertiesForTesting(         \
          grand_child.get()));

namespace cc {

namespace {

class MockLayerTree : public LayerTree {
 public:
  MockLayerTree(LayerTreeHostInProcess::InitParams* params,
                LayerTreeHost* layer_tree_host)
      : LayerTree(params->mutator_host, layer_tree_host) {}
  ~MockLayerTree() override {}

  MOCK_METHOD0(SetNeedsFullTreeSync, void());
};

class MockLayerTreeHost : public LayerTreeHostInProcess {
 public:
  MockLayerTreeHost(LayerTreeHostSingleThreadClient* single_thread_client,
                    LayerTreeHostInProcess::InitParams* params)
      : LayerTreeHostInProcess(
            params,
            CompositorMode::SINGLE_THREADED,
            base::MakeUnique<StrictMock<MockLayerTree>>(params, this)) {
    InitializeSingleThreaded(single_thread_client,
                             base::ThreadTaskRunnerHandle::Get());
  }

  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(SetNeedsUpdateLayers, void());
};

class LayerTest : public testing::Test {
 public:
  LayerTest()
      : host_impl_(LayerTreeSettings(),
                   &task_runner_provider_,
                   &task_graph_runner_) {
    timeline_impl_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    timeline_impl_->set_is_impl_only(true);
    host_impl_.animation_host()->AddAnimationTimeline(timeline_impl_);
  }

  const LayerTreeSettings& settings() { return settings_; }
  scoped_refptr<AnimationTimeline> timeline_impl() { return timeline_impl_; }

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

    LayerTreeHostInProcess::InitParams params;
    params.client = &fake_client_;
    params.settings = &settings_;
    params.task_graph_runner = &task_graph_runner_;
    params.mutator_host = animation_host_.get();

    layer_tree_host_.reset(
        new StrictMock<MockLayerTreeHost>(&single_thread_client_, &params));
    layer_tree_ = static_cast<StrictMock<MockLayerTree>*>(
        layer_tree_host_->GetLayerTree());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    Mock::VerifyAndClearExpectations(layer_tree_);
    EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(AnyNumber());
    parent_ = nullptr;
    child1_ = nullptr;
    child2_ = nullptr;
    child3_ = nullptr;
    grand_child1_ = nullptr;
    grand_child2_ = nullptr;
    grand_child3_ = nullptr;

    layer_tree_->SetRootLayer(nullptr);
    animation_host_->SetMutatorHostClient(nullptr);
    layer_tree_host_ = nullptr;
    animation_host_ = nullptr;
    layer_tree_ = nullptr;
  }

  void VerifyTestTreeInitialState() const {
    ASSERT_EQ(3U, parent_->children().size());
    EXPECT_EQ(child1_, parent_->children()[0]);
    EXPECT_EQ(child2_, parent_->children()[1]);
    EXPECT_EQ(child3_, parent_->children()[2]);
    EXPECT_EQ(parent_.get(), child1_->parent());
    EXPECT_EQ(parent_.get(), child2_->parent());
    EXPECT_EQ(parent_.get(), child3_->parent());

    ASSERT_EQ(2U, child1_->children().size());
    EXPECT_EQ(grand_child1_, child1_->children()[0]);
    EXPECT_EQ(grand_child2_, child1_->children()[1]);
    EXPECT_EQ(child1_.get(), grand_child1_->parent());
    EXPECT_EQ(child1_.get(), grand_child2_->parent());

    ASSERT_EQ(1U, child2_->children().size());
    EXPECT_EQ(grand_child3_, child2_->children()[0]);
    EXPECT_EQ(child2_.get(), grand_child3_->parent());

    ASSERT_EQ(0U, child3_->children().size());
  }

  void CreateSimpleTestTree() {
    parent_ = Layer::Create();
    child1_ = Layer::Create();
    child2_ = Layer::Create();
    child3_ = Layer::Create();
    grand_child1_ = Layer::Create();
    grand_child2_ = Layer::Create();
    grand_child3_ = Layer::Create();

    EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(AnyNumber());
    layer_tree_->SetRootLayer(parent_);

    parent_->AddChild(child1_);
    parent_->AddChild(child2_);
    parent_->AddChild(child3_);
    child1_->AddChild(grand_child1_);
    child1_->AddChild(grand_child2_);
    child2_->AddChild(grand_child3_);

    Mock::VerifyAndClearExpectations(layer_tree_);

    VerifyTestTreeInitialState();
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;

  StubLayerTreeHostSingleThreadClient single_thread_client_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<StrictMock<MockLayerTreeHost>> layer_tree_host_;
  std::unique_ptr<AnimationHost> animation_host_;
  StrictMock<MockLayerTree>* layer_tree_;
  scoped_refptr<Layer> parent_;
  scoped_refptr<Layer> child1_;
  scoped_refptr<Layer> child2_;
  scoped_refptr<Layer> child3_;
  scoped_refptr<Layer> grand_child1_;
  scoped_refptr<Layer> grand_child2_;
  scoped_refptr<Layer> grand_child3_;

  scoped_refptr<AnimationTimeline> timeline_impl_;

  LayerTreeSettings settings_;
};

TEST_F(LayerTest, BasicCreateAndDestroy) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->SetLayerTreeHost(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, LayerPropertyChangedForSubtree) {
  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(AtLeast(1));
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<Layer> dummy_layer1 = Layer::Create();

  layer_tree_->SetRootLayer(root);
  root->AddChild(child);
  root->AddChild(child2);
  child->AddChild(grand_child);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  child->SetForceRenderSurfaceForTesting(true);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  child2->SetScrollParent(grand_child.get());
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;
  std::unique_ptr<LayerImpl> root_impl =
      LayerImpl::Create(host_impl_.active_tree(), root->id());
  std::unique_ptr<LayerImpl> child_impl =
      LayerImpl::Create(host_impl_.active_tree(), child->id());
  std::unique_ptr<LayerImpl> child2_impl =
      LayerImpl::Create(host_impl_.active_tree(), child2->id());
  std::unique_ptr<LayerImpl> grand_child_impl =
      LayerImpl::Create(host_impl_.active_tree(), grand_child->id());
  std::unique_ptr<LayerImpl> dummy_layer1_impl =
      LayerImpl::Create(host_impl_.active_tree(), dummy_layer1->id());

  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMaskLayer(dummy_layer1.get()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      dummy_layer1->PushPropertiesTo(dummy_layer1_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetShouldFlattenTransform(false));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->Set3dSortingContextId(1));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetDoubleSided(false));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetHideLayerAndSubtree(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  // Should be a different size than previous call, to ensure it marks tree
  // changed.
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetBackgroundFilters(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get()));

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetPosition(arbitrary_point_f);
  TransformNode* node = layer_tree_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_->property_trees()->ResetAllChangeTracking());
  EXPECT_FALSE(node->transform_changed);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  child->SetPosition(arbitrary_point_f);
  node = layer_tree_->property_trees()->transform_tree.Node(
      child->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  // child2 is not in the subtree of child, but its scroll parent is. So, its
  // to_screen will be effected by change in position of child2.
  layer_tree_->property_trees()->transform_tree.UpdateTransforms(
      child2->transform_tree_index());
  node = layer_tree_->property_trees()->transform_tree.Node(
      child2->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      child->PushPropertiesTo(child_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_->property_trees()->ResetAllChangeTracking());
  node = layer_tree_->property_trees()->transform_tree.Node(
      child->transform_tree_index());
  EXPECT_FALSE(node->transform_changed);

  gfx::Point3F arbitrary_point_3f = gfx::Point3F(0.125f, 0.25f, 0.f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetTransformOrigin(arbitrary_point_3f);
  node = layer_tree_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_->property_trees()->ResetAllChangeTracking());

  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetTransform(arbitrary_transform);
  node = layer_tree_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
}

TEST_F(LayerTest, AddAndRemoveChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  // Upon creation, layers should not have children or parent.
  ASSERT_EQ(0U, parent->children().size());
  EXPECT_FALSE(child->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(parent));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->AddChild(child));

  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child.get(), parent->children()[0]);
  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(parent.get(), child->RootLayer());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), child->RemoveFromParent());
}

TEST_F(LayerTest, AddSameChildTwice) {
  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(AtLeast(1));

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  layer_tree_->SetRootLayer(parent);

  ASSERT_EQ(0u, parent->children().size());

  parent->AddChild(child);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), child->parent());

  parent->AddChild(child);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), child->parent());
}

TEST_F(LayerTest, InsertChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> child3 = Layer::Create();
  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  // Case 1: inserting to empty list.
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child3, 0));
  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child3, parent->children()[0]);
  EXPECT_EQ(parent.get(), child3->parent());

  // Case 2: inserting to beginning of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child3, parent->children()[1]);
  EXPECT_EQ(parent.get(), child1->parent());

  // Case 3: inserting to middle of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));
  ASSERT_EQ(3U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
  EXPECT_EQ(child3, parent->children()[2]);
  EXPECT_EQ(parent.get(), child2->parent());

  // Case 4: inserting to end of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child4, 3));

  ASSERT_EQ(4U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
  EXPECT_EQ(child3, parent->children()[2]);
  EXPECT_EQ(child4, parent->children()[3]);
  EXPECT_EQ(parent.get(), child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, InsertChildPastEndOfList) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  ASSERT_EQ(0U, parent->children().size());

  // insert to an out-of-bounds index
  parent->InsertChild(child1, 53);

  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);

  // insert another child to out-of-bounds, when list is not already empty.
  parent->InsertChild(child2, 2459);

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerTest, InsertSameChildTwice) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);

  // Inserting the same child again should cause the child to be removed and
  // re-inserted at the new location.
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent->InsertChild(child1, 1));

  // child1 should now be at the end of the list.
  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child2, parent->children()[0]);
  EXPECT_EQ(child1, parent->children()[1]);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, ReplaceChildWithNewChild) {
  CreateSimpleTestTree();
  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_FALSE(child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      AtLeast(1), parent_->ReplaceChild(child2_.get(), child4));
  EXPECT_FALSE(parent_->NeedsDisplayForTesting());
  EXPECT_FALSE(child1_->NeedsDisplayForTesting());
  EXPECT_FALSE(child2_->NeedsDisplayForTesting());
  EXPECT_FALSE(child3_->NeedsDisplayForTesting());
  EXPECT_FALSE(child4->NeedsDisplayForTesting());

  ASSERT_EQ(static_cast<size_t>(3), parent_->children().size());
  EXPECT_EQ(child1_, parent_->children()[0]);
  EXPECT_EQ(child4, parent_->children()[1]);
  EXPECT_EQ(child3_, parent_->children()[2]);
  EXPECT_EQ(parent_.get(), child4->parent());

  EXPECT_FALSE(child2_->parent());
}

TEST_F(LayerTest, ReplaceChildWithNewChildThatHasOtherParent) {
  CreateSimpleTestTree();

  // create another simple tree with test_layer and child4.
  scoped_refptr<Layer> test_layer = Layer::Create();
  scoped_refptr<Layer> child4 = Layer::Create();
  test_layer->AddChild(child4);
  ASSERT_EQ(1U, test_layer->children().size());
  EXPECT_EQ(child4, test_layer->children()[0]);
  EXPECT_EQ(test_layer.get(), child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      AtLeast(1), parent_->ReplaceChild(child2_.get(), child4));

  ASSERT_EQ(3U, parent_->children().size());
  EXPECT_EQ(child1_, parent_->children()[0]);
  EXPECT_EQ(child4, parent_->children()[1]);
  EXPECT_EQ(child3_, parent_->children()[2]);
  EXPECT_EQ(parent_.get(), child4->parent());

  // test_layer should no longer have child4,
  // and child2 should no longer have a parent.
  ASSERT_EQ(0U, test_layer->children().size());
  EXPECT_FALSE(child2_->parent());
}

TEST_F(LayerTest, DeleteRemovedScrollParent) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);

  EXPECT_SET_NEEDS_COMMIT(2, child1->SetScrollParent(child2.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, child2->RemoveFromParent());

  child1->ResetNeedsPushPropertiesForTesting();

  EXPECT_SET_NEEDS_COMMIT(1, child2 = nullptr);

  EXPECT_TRUE(layer_tree_->LayerNeedsPushPropertiesForTesting(child1.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, DeleteRemovedScrollChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);

  EXPECT_SET_NEEDS_COMMIT(2, child1->SetScrollParent(child2.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, child1->RemoveFromParent());

  child2->ResetNeedsPushPropertiesForTesting();

  EXPECT_SET_NEEDS_COMMIT(1, child1 = nullptr);

  EXPECT_TRUE(layer_tree_->LayerNeedsPushPropertiesForTesting(child2.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, ReplaceChildWithSameChild) {
  CreateSimpleTestTree();

  // SetNeedsFullTreeSync / SetNeedsCommit should not be called because its the
  // same child.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(0);
  parent_->ReplaceChild(child2_.get(), child2_);

  VerifyTestTreeInitialState();
}

TEST_F(LayerTest, RemoveAllChildren) {
  CreateSimpleTestTree();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(3), parent_->RemoveAllChildren());

  ASSERT_EQ(0U, parent_->children().size());
  EXPECT_FALSE(child1_->parent());
  EXPECT_FALSE(child2_->parent());
  EXPECT_FALSE(child3_->parent());
}

TEST_F(LayerTest, SetChildren) {
  scoped_refptr<Layer> old_parent = Layer::Create();
  scoped_refptr<Layer> new_parent = Layer::Create();

  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  LayerList new_children;
  new_children.push_back(child1);
  new_children.push_back(child2);

  // Set up and verify initial test conditions: child1 has a parent, child2 has
  // no parent.
  old_parent->AddChild(child1);
  ASSERT_EQ(0U, new_parent->children().size());
  EXPECT_EQ(old_parent.get(), child1->parent());
  EXPECT_FALSE(child2->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(new_parent));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      AtLeast(1), new_parent->SetChildren(new_children));

  ASSERT_EQ(2U, new_parent->children().size());
  EXPECT_EQ(new_parent.get(), child1->parent());
  EXPECT_EQ(new_parent.get(), child2->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, HasAncestor) {
  scoped_refptr<Layer> parent = Layer::Create();
  EXPECT_FALSE(parent->HasAncestor(parent.get()));

  scoped_refptr<Layer> child = Layer::Create();
  parent->AddChild(child);

  EXPECT_FALSE(child->HasAncestor(child.get()));
  EXPECT_TRUE(child->HasAncestor(parent.get()));
  EXPECT_FALSE(parent->HasAncestor(child.get()));

  scoped_refptr<Layer> child_child = Layer::Create();
  child->AddChild(child_child);

  EXPECT_FALSE(child_child->HasAncestor(child_child.get()));
  EXPECT_TRUE(child_child->HasAncestor(parent.get()));
  EXPECT_TRUE(child_child->HasAncestor(child.get()));
  EXPECT_FALSE(parent->HasAncestor(child.get()));
  EXPECT_FALSE(parent->HasAncestor(child_child.get()));
}

TEST_F(LayerTest, GetRootLayerAfterTreeManipulations) {
  CreateSimpleTestTree();

  // For this test we don't care about SetNeedsFullTreeSync calls.
  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times(AnyNumber());

  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(parent_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(child4.get(),   child4->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  child1_->RemoveFromParent();

  // |child1| and its children, grand_child1 and grand_child2 are now on a
  // separate subtree.
  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(child1_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(child4.get(), child4->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  grand_child3_->AddChild(child4);

  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(child1_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(parent_.get(), child4->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  child2_->ReplaceChild(grand_child3_.get(), child1_);

  // |grand_child3| gets orphaned and the child1 subtree gets planted back into
  // the tree under child2.
  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(parent_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(grand_child3_.get(), child4->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(grand_child3_.get(), grand_child3_->RootLayer());
}

TEST_F(LayerTest, CheckSetNeedsDisplayCausesCorrectBehavior) {
  // The semantics for SetNeedsDisplay which are tested here:
  //   1. sets NeedsDisplay flag appropriately.
  //   2. indirectly calls SetNeedsUpdate, exactly once for each call to
  //      SetNeedsDisplay.

  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(true));

  gfx::Size test_bounds = gfx::Size(501, 508);

  gfx::Rect dirty_rect = gfx::Rect(10, 15, 1, 2);
  gfx::Rect out_of_bounds_dirty_rect = gfx::Rect(400, 405, 500, 502);

  // Before anything, test_layer should not be dirty.
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // This is just initialization, but SetNeedsCommit behavior is verified anyway
  // to avoid warnings.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBounds(test_bounds));
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // The real test begins here.
  test_layer->ResetNeedsDisplayForTesting();
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // Case 1: Layer should accept dirty rects that go beyond its bounds.
  test_layer->ResetNeedsDisplayForTesting();
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());
  EXPECT_SET_NEEDS_UPDATE(
      1, test_layer->SetNeedsDisplayRect(out_of_bounds_dirty_rect));
  EXPECT_TRUE(test_layer->NeedsDisplayForTesting());
  test_layer->ResetNeedsDisplayForTesting();

  // Case 2: SetNeedsDisplay() without the dirty rect arg.
  test_layer->ResetNeedsDisplayForTesting();
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());
  EXPECT_SET_NEEDS_UPDATE(1, test_layer->SetNeedsDisplay());
  EXPECT_TRUE(test_layer->NeedsDisplayForTesting());
  test_layer->ResetNeedsDisplayForTesting();

  // Case 3: SetNeedsDisplay() with an empty rect.
  test_layer->ResetNeedsDisplayForTesting();
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());
  EXPECT_SET_NEEDS_COMMIT(0, test_layer->SetNeedsDisplayRect(gfx::Rect()));
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // Case 4: SetNeedsDisplay() with a non-drawable layer
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(false));
  test_layer->ResetNeedsDisplayForTesting();
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());
  EXPECT_SET_NEEDS_UPDATE(0, test_layer->SetNeedsDisplayRect(dirty_rect));
  EXPECT_TRUE(test_layer->NeedsDisplayForTesting());
}

TEST_F(LayerTest, TestSettingMainThreadScrollingReason) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(true));

  // sanity check of initial test condition
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  uint32_t reasons = 0, reasons_to_clear = 0, reasons_after_clearing = 0;
  reasons |= MainThreadScrollingReason::kNonFastScrollableRegion;
  reasons |= MainThreadScrollingReason::kContinuingMainThreadScroll;
  reasons |= MainThreadScrollingReason::kScrollbarScrolling;

  reasons_to_clear |= MainThreadScrollingReason::kContinuingMainThreadScroll;
  reasons_to_clear |= MainThreadScrollingReason::kThreadedScrollingDisabled;

  reasons_after_clearing |= MainThreadScrollingReason::kNonFastScrollableRegion;
  reasons_after_clearing |= MainThreadScrollingReason::kScrollbarScrolling;

  // Check that the reasons are added correctly.
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->AddMainThreadScrollingReasons(
             MainThreadScrollingReason::kNonFastScrollableRegion));
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->AddMainThreadScrollingReasons(
             MainThreadScrollingReason::kContinuingMainThreadScroll));
  EXPECT_SET_NEEDS_COMMIT(1,
                          test_layer->AddMainThreadScrollingReasons(
                              MainThreadScrollingReason::kScrollbarScrolling));
  EXPECT_EQ(reasons, test_layer->main_thread_scrolling_reasons());

  // Check that the reasons can be selectively cleared.
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->ClearMainThreadScrollingReasons(reasons_to_clear));
  EXPECT_EQ(reasons_after_clearing,
            test_layer->main_thread_scrolling_reasons());

  // Check that clearing non-set reasons doesn't set needs commit.
  reasons_to_clear = 0;
  reasons_to_clear |= MainThreadScrollingReason::kThreadedScrollingDisabled;
  reasons_to_clear |= MainThreadScrollingReason::kNoScrollingLayer;
  EXPECT_SET_NEEDS_COMMIT(
      0, test_layer->ClearMainThreadScrollingReasons(reasons_to_clear));
  EXPECT_EQ(reasons_after_clearing,
            test_layer->main_thread_scrolling_reasons());

  // Check that adding an existing condition doesn't set needs commit.
  EXPECT_SET_NEEDS_COMMIT(
      0, test_layer->AddMainThreadScrollingReasons(
             MainThreadScrollingReason::kNonFastScrollableRegion));
}

TEST_F(LayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(true));

  scoped_refptr<Layer> dummy_layer1 = Layer::Create();

  // sanity check of initial test condition
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // Next, test properties that should call SetNeedsCommit (but not
  // SetNeedsDisplay). All properties need to be set to new values in order for
  // SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->SetTransformOrigin(gfx::Point3F(1.23f, 4.56f, 0.f)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBackgroundColor(SK_ColorLTGRAY));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetMasksToBounds(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetOpacity(0.5f));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBlendMode(SkXfermode::kHue_Mode));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsRootForIsolatedGroup(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetContentsOpaque(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetPosition(gfx::PointF(4.f, 9.f)));
  // We can use any layer pointer here since we aren't syncing for real.
  EXPECT_SET_NEEDS_COMMIT(1,
                          test_layer->SetScrollClipLayerId(test_layer->id()));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetUserScrollable(true, false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetScrollOffset(
      gfx::ScrollOffset(10, 10)));
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->AddMainThreadScrollingReasons(
             MainThreadScrollingReason::kNonFastScrollableRegion));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetNonFastScrollableRegion(
      Region(gfx::Rect(1, 1, 2, 2))));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetTransform(
      gfx::Transform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetDoubleSided(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetTouchEventHandlerRegion(
      gfx::Rect(10, 10)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetForceRenderSurfaceForTesting(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetHideLayerAndSubtree(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetElementId(ElementId(2, 0)));
  EXPECT_SET_NEEDS_COMMIT(
      1, test_layer->SetMutableProperties(MutableProperty::kTransform));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, test_layer->SetMaskLayer(
      dummy_layer1.get()));

  // The above tests should not have caused a change to the needs_display flag.
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // As layers are removed from the tree, they will cause a tree sync.
  EXPECT_CALL(*layer_tree_, SetNeedsFullTreeSync()).Times((AnyNumber()));
}

TEST_F(LayerTest, PushPropertiesAccumulatesUpdateRect) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));

  host_impl_.active_tree()->SetRootLayerForTesting(std::move(impl_layer));
  host_impl_.active_tree()->BuildLayerListForTesting();
  LayerImpl* impl_layer_ptr = host_impl_.active_tree()->LayerById(1);
  test_layer->SetNeedsDisplayRect(gfx::Rect(5, 5));
  test_layer->PushPropertiesTo(impl_layer_ptr);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 5.f, 5.f),
                       impl_layer_ptr->update_rect());

  // The LayerImpl's update_rect() should be accumulated here, since we did not
  // do anything to clear it.
  test_layer->SetNeedsDisplayRect(gfx::Rect(10, 10, 5, 5));
  test_layer->PushPropertiesTo(impl_layer_ptr);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 15.f, 15.f),
                       impl_layer_ptr->update_rect());

  // If we do clear the LayerImpl side, then the next update_rect() should be
  // fresh without accumulation.
  host_impl_.active_tree()->ResetAllChangeTracking();
  test_layer->SetNeedsDisplayRect(gfx::Rect(10, 10, 5, 5));
  test_layer->PushPropertiesTo(impl_layer_ptr);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10.f, 10.f, 5.f, 5.f),
                       impl_layer_ptr->update_rect());
}

TEST_F(LayerTest, PushPropertiesCausesLayerPropertyChangedForTransform) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));

  gfx::Transform transform;
  transform.Rotate(45.0);
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetTransform(transform));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  test_layer->PushPropertiesTo(impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
}

TEST_F(LayerTest, PushPropertiesCausesLayerPropertyChangedForOpacity) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));

  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetOpacity(0.5f));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  test_layer->PushPropertiesTo(impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
}

TEST_F(LayerTest, MaskHasParent) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> mask_replacement = Layer::Create();

  parent->AddChild(child);
  child->SetMaskLayer(mask.get());

  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(child.get(), mask->parent());

  child->SetMaskLayer(mask_replacement.get());
  EXPECT_EQ(nullptr, mask->parent());
  EXPECT_EQ(child.get(), mask_replacement->parent());
}

class LayerTreeHostFactory {
 public:
  std::unique_ptr<LayerTreeHost> Create(MutatorHost* mutator_host) {
    return Create(LayerTreeSettings(), mutator_host);
  }

  std::unique_ptr<LayerTreeHost> Create(LayerTreeSettings settings,
                                        MutatorHost* mutator_host) {
    LayerTreeHostInProcess::InitParams params;
    params.client = &client_;
    params.task_graph_runner = &task_graph_runner_;
    params.settings = &settings;
    params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    params.mutator_host = mutator_host;

    return LayerTreeHostInProcess::CreateSingleThreaded(&single_thread_client_,
                                                        &params);
  }

 private:
  FakeLayerTreeHostClient client_;
  StubLayerTreeHostSingleThreadClient single_thread_client_;
  TestTaskGraphRunner task_graph_runner_;
};

void AssertLayerTreeHostMatchesForSubtree(Layer* layer, LayerTreeHost* host) {
  EXPECT_EQ(host, layer->GetLayerTreeHostForTesting());

  for (size_t i = 0; i < layer->children().size(); ++i)
    AssertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

  if (layer->mask_layer())
    AssertLayerTreeHostMatchesForSubtree(layer->mask_layer(), host);
}

class LayerLayerTreeHostTest : public testing::Test {};

TEST_F(LayerLayerTreeHostTest, EnteringTree) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();

  // Set up a detached tree of layers. The host pointer should be nil for these
  // layers.
  parent->AddChild(child);
  child->SetMaskLayer(mask.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);

  LayerTreeHostFactory factory;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  LayerTree* layer_tree = layer_tree_host->GetLayerTree();
  // Setting the root layer should set the host pointer for all layers in the
  // tree.
  layer_tree->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Clearing the root layer should also clear out the host pointers for all
  // layers in the tree.
  layer_tree->SetRootLayer(nullptr);

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);
}

TEST_F(LayerLayerTreeHostTest, AddingLayerSubtree) {
  scoped_refptr<Layer> parent = Layer::Create();
  LayerTreeHostFactory factory;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  LayerTree* layer_tree = layer_tree_host->GetLayerTree();

  layer_tree->SetRootLayer(parent.get());

  EXPECT_EQ(parent->GetLayerTreeHostForTesting(), layer_tree_host.get());

  // Adding a subtree to a layer already associated with a host should set the
  // host pointer on all layers in that subtree.
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  child->AddChild(grand_child);

  // Masks should pick up the new host too.
  scoped_refptr<Layer> child_mask = Layer::Create();
  child->SetMaskLayer(child_mask.get());

  parent->AddChild(child);
  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  layer_tree->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ChangeHost) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();

  // Same setup as the previous test.
  parent->AddChild(child);
  child->SetMaskLayer(mask.get());

  LayerTreeHostFactory factory;
  auto animation_host1 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> first_layer_tree_host =
      factory.Create(animation_host1.get());
  first_layer_tree_host->GetLayerTree()->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       first_layer_tree_host.get());

  // Now re-root the tree to a new host (simulating what we do on a context lost
  // event). This should update the host pointers for all layers in the tree.
  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> second_layer_tree_host =
      factory.Create(animation_host2.get());
  second_layer_tree_host->GetLayerTree()->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       second_layer_tree_host.get());

  second_layer_tree_host->GetLayerTree()->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ChangeHostInSubtree) {
  scoped_refptr<Layer> first_parent = Layer::Create();
  scoped_refptr<Layer> first_child = Layer::Create();
  scoped_refptr<Layer> second_parent = Layer::Create();
  scoped_refptr<Layer> second_child = Layer::Create();
  scoped_refptr<Layer> second_grand_child = Layer::Create();

  // First put all children under the first parent and set the first host.
  first_parent->AddChild(first_child);
  second_child->AddChild(second_grand_child);
  first_parent->AddChild(second_child);

  LayerTreeHostFactory factory;
  auto animation_host1 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> first_layer_tree_host =
      factory.Create(animation_host1.get());
  first_layer_tree_host->GetLayerTree()->SetRootLayer(first_parent.get());

  AssertLayerTreeHostMatchesForSubtree(first_parent.get(),
                                       first_layer_tree_host.get());

  // Now reparent the subtree starting at second_child to a layer in a different
  // tree.
  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> second_layer_tree_host =
      factory.Create(animation_host2.get());
  second_layer_tree_host->GetLayerTree()->SetRootLayer(second_parent.get());

  second_parent->AddChild(second_child);

  // The moved layer and its children should point to the new host.
  EXPECT_EQ(second_layer_tree_host.get(),
            second_child->GetLayerTreeHostForTesting());
  EXPECT_EQ(second_layer_tree_host.get(),
            second_grand_child->GetLayerTreeHostForTesting());

  // Test over, cleanup time.
  first_layer_tree_host->GetLayerTree()->SetRootLayer(nullptr);
  second_layer_tree_host->GetLayerTree()->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ReplaceMaskLayer) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> mask_child = Layer::Create();
  scoped_refptr<Layer> mask_replacement = Layer::Create();

  parent->SetMaskLayer(mask.get());
  mask->AddChild(mask_child);

  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  layer_tree_host->GetLayerTree()->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Replacing the mask should clear out the old mask's subtree's host pointers.
  parent->SetMaskLayer(mask_replacement.get());
  EXPECT_EQ(nullptr, mask->GetLayerTreeHostForTesting());
  EXPECT_EQ(nullptr, mask_child->GetLayerTreeHostForTesting());

  // Test over, cleanup time.
  layer_tree_host->GetLayerTree()->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, DestroyHostWithNonNullRootLayer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  root->AddChild(child);
  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  layer_tree_host->GetLayerTree()->SetRootLayer(root);
}

TEST_F(LayerTest, SafeOpaqueBackgroundColor) {
  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  LayerTree* layer_tree = layer_tree_host->GetLayerTree();

  scoped_refptr<Layer> layer = Layer::Create();
  layer_tree->SetRootLayer(layer);

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SK_ColorRED
                                               : SK_ColorTRANSPARENT);
        layer_tree->set_background_color(host_opaque ? SK_ColorRED
                                                     : SK_ColorTRANSPARENT);

        layer_tree->property_trees()->needs_rebuild = true;
        layer_tree_host->GetLayerTree()->BuildPropertyTreesForTesting();
        SkColor safe_color = layer->SafeOpaqueBackgroundColor();
        if (contents_opaque) {
          EXPECT_EQ(SkColorGetA(safe_color), 255u)
              << "Flags: " << contents_opaque << ", " << layer_opaque << ", "
              << host_opaque << "\n";
        } else {
          EXPECT_NE(SkColorGetA(safe_color), 255u)
              << "Flags: " << contents_opaque << ", " << layer_opaque << ", "
              << host_opaque << "\n";
        }
      }
    }
  }
}

class DrawsContentChangeLayer : public Layer {
 public:
  static scoped_refptr<DrawsContentChangeLayer> Create() {
    return make_scoped_refptr(new DrawsContentChangeLayer());
  }

  void SetLayerTreeHost(LayerTreeHost* host) override {
    Layer::SetLayerTreeHost(host);
    SetFakeDrawsContent(!fake_draws_content_);
  }

  bool HasDrawableContent() const override {
    return fake_draws_content_ && Layer::HasDrawableContent();
  }

  void SetFakeDrawsContent(bool fake_draws_content) {
    fake_draws_content_ = fake_draws_content;
    UpdateDrawsContent(HasDrawableContent());
  }

 private:
  DrawsContentChangeLayer() : fake_draws_content_(false) {}
  ~DrawsContentChangeLayer() override {}

  bool fake_draws_content_;
};

TEST_F(LayerTest, DrawsContentChangedInSetLayerTreeHost) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  scoped_refptr<DrawsContentChangeLayer> becomes_not_draws_content =
      DrawsContentChangeLayer::Create();
  scoped_refptr<DrawsContentChangeLayer> becomes_draws_content =
      DrawsContentChangeLayer::Create();
  root_layer->SetIsDrawable(true);
  becomes_not_draws_content->SetIsDrawable(true);
  becomes_not_draws_content->SetFakeDrawsContent(true);
  EXPECT_EQ(0, root_layer->NumDescendantsThatDrawContent());
  root_layer->AddChild(becomes_not_draws_content);
  EXPECT_EQ(0, root_layer->NumDescendantsThatDrawContent());

  becomes_draws_content->SetIsDrawable(true);
  root_layer->AddChild(becomes_draws_content);
  EXPECT_EQ(1, root_layer->NumDescendantsThatDrawContent());
}

void ReceiveCopyOutputResult(int* result_count,
                             std::unique_ptr<CopyOutputResult> result) {
  ++(*result_count);
}

TEST_F(LayerTest, DedupesCopyOutputRequestsBySource) {
  scoped_refptr<Layer> layer = Layer::Create();
  int result_count = 0;

  // Create identical requests without the source being set, and expect the
  // layer does not abort either one.
  std::unique_ptr<CopyOutputRequest> request = CopyOutputRequest::CreateRequest(
      base::Bind(&ReceiveCopyOutputResult, &result_count));
  layer->RequestCopyOfOutput(std::move(request));
  EXPECT_EQ(0, result_count);
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&ReceiveCopyOutputResult, &result_count));
  layer->RequestCopyOfOutput(std::move(request));
  EXPECT_EQ(0, result_count);

  // When the layer is destroyed, expect both requests to be aborted.
  layer = nullptr;
  EXPECT_EQ(2, result_count);

  layer = Layer::Create();
  result_count = 0;

  // Create identical requests, but this time the source is being set.  Expect
  // the first request from |this| source aborts immediately when the second
  // request from |this| source is made.
  int did_receive_first_result_from_this_source = 0;
  request = CopyOutputRequest::CreateRequest(base::Bind(
      &ReceiveCopyOutputResult, &did_receive_first_result_from_this_source));
  request->set_source(this);
  layer->RequestCopyOfOutput(std::move(request));
  EXPECT_EQ(0, did_receive_first_result_from_this_source);
  // Make a request from a different source.
  int did_receive_result_from_different_source = 0;
  request = CopyOutputRequest::CreateRequest(base::Bind(
      &ReceiveCopyOutputResult, &did_receive_result_from_different_source));
  request->set_source(reinterpret_cast<void*>(0xdeadbee0));
  layer->RequestCopyOfOutput(std::move(request));
  EXPECT_EQ(0, did_receive_result_from_different_source);
  // Make a request without specifying the source.
  int did_receive_result_from_anonymous_source = 0;
  request = CopyOutputRequest::CreateRequest(base::Bind(
      &ReceiveCopyOutputResult, &did_receive_result_from_anonymous_source));
  layer->RequestCopyOfOutput(std::move(request));
  EXPECT_EQ(0, did_receive_result_from_anonymous_source);
  // Make the second request from |this| source.
  int did_receive_second_result_from_this_source = 0;
  request = CopyOutputRequest::CreateRequest(base::Bind(
      &ReceiveCopyOutputResult, &did_receive_second_result_from_this_source));
  request->set_source(this);
  layer->RequestCopyOfOutput(
      std::move(request));  // First request to be aborted.
  EXPECT_EQ(1, did_receive_first_result_from_this_source);
  EXPECT_EQ(0, did_receive_result_from_different_source);
  EXPECT_EQ(0, did_receive_result_from_anonymous_source);
  EXPECT_EQ(0, did_receive_second_result_from_this_source);

  // When the layer is destroyed, the other three requests should be aborted.
  layer = nullptr;
  EXPECT_EQ(1, did_receive_first_result_from_this_source);
  EXPECT_EQ(1, did_receive_result_from_different_source);
  EXPECT_EQ(1, did_receive_result_from_anonymous_source);
  EXPECT_EQ(1, did_receive_second_result_from_this_source);
}

TEST_F(LayerTest, AnimationSchedulesLayerUpdate) {
  scoped_refptr<Layer> layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(layer));

  LayerInternalsForTest layer_internals(layer.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  layer_internals.OnOpacityAnimated(0.5f);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  gfx::Transform transform;
  transform.Rotate(45.0);
  layer_internals.OnTransformAnimated(transform);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Scroll offset animation should not schedule a layer update since it is
  // handled similarly to normal compositor scroll updates.
  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(0);
  layer_internals.OnScrollOffsetAnimated(gfx::ScrollOffset(10, 10));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(LayerTest, ElementIdAndMutablePropertiesArePushed) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_->SetRootLayer(test_layer));

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(2);

  test_layer->SetElementId(ElementId(2, 0));
  test_layer->SetMutableProperties(MutableProperty::kTransform);

  EXPECT_FALSE(impl_layer->element_id());
  EXPECT_EQ(MutableProperty::kNone, impl_layer->mutable_properties());

  test_layer->PushPropertiesTo(impl_layer.get());

  EXPECT_EQ(ElementId(2, 0), impl_layer->element_id());
  EXPECT_EQ(MutableProperty::kTransform, impl_layer->mutable_properties());
}

}  // namespace
}  // namespace cc
