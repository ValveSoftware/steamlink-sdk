// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/mutable_properties.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/proto/layer.pb.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/single_thread_proxy.h"
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

#define EXPECT_SET_NEEDS_FULL_TREE_SYNC(expect, code_to_test)               \
  do {                                                                      \
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times((expect)); \
    code_to_test;                                                           \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());               \
  } while (false)

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                    \
  code_to_test;                                                             \
  root->layer_tree_host()->BuildPropertyTreesForTesting();                  \
  EXPECT_TRUE(root->subtree_property_changed());                            \
  EXPECT_TRUE(root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(  \
      root.get()));                                                         \
  EXPECT_TRUE(child->subtree_property_changed());                           \
  EXPECT_TRUE(child->layer_tree_host()->LayerNeedsPushPropertiesForTesting( \
      child.get()));                                                        \
  EXPECT_TRUE(grand_child->subtree_property_changed());                     \
  EXPECT_TRUE(                                                              \
      grand_child->layer_tree_host()->LayerNeedsPushPropertiesForTesting(   \
          grand_child.get()));

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(code_to_test)               \
  code_to_test;                                                              \
  EXPECT_FALSE(root->subtree_property_changed());                            \
  EXPECT_FALSE(root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(  \
      root.get()));                                                          \
  EXPECT_FALSE(child->subtree_property_changed());                           \
  EXPECT_FALSE(child->layer_tree_host()->LayerNeedsPushPropertiesForTesting( \
      child.get()));                                                         \
  EXPECT_FALSE(grand_child->subtree_property_changed());                     \
  EXPECT_FALSE(                                                              \
      grand_child->layer_tree_host()->LayerNeedsPushPropertiesForTesting(    \
          grand_child.get()));

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)                  \
  code_to_test;                                                              \
  root->layer_tree_host()->BuildPropertyTreesForTesting();                   \
  EXPECT_TRUE(root->layer_property_changed());                               \
  EXPECT_FALSE(root->subtree_property_changed());                            \
  EXPECT_TRUE(root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(   \
      root.get()));                                                          \
  EXPECT_FALSE(child->layer_property_changed());                             \
  EXPECT_FALSE(child->subtree_property_changed());                           \
  EXPECT_FALSE(child->layer_tree_host()->LayerNeedsPushPropertiesForTesting( \
      child.get()));                                                         \
  EXPECT_FALSE(grand_child->layer_property_changed());                       \
  EXPECT_FALSE(grand_child->subtree_property_changed());                     \
  EXPECT_FALSE(                                                              \
      grand_child->layer_tree_host()->LayerNeedsPushPropertiesForTesting(    \
          grand_child.get()));

namespace cc {

// This class is a friend of Layer, and is used as a wrapper for all the tests
// related to proto serialization. This is done so that it is unnecessary to
// add FRIEND_TEST_ALL_PREFIXES in //cc/layers/layer.h for all the tests.
// It is in the cc namespace so that it can be a friend of Layer.
// The tests still have helpful names, and a test with the name FooBar would
// have a wrapper method in this class called RunFooBarTest.
class LayerSerializationTest : public testing::Test {
 public:
  LayerSerializationTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    layer_tree_host_ =
        FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_);
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  // Serializes |src| to proto and back again to a Layer, then verifies that
  // the two Layers are equal for serialization purposes.
  void VerifyBaseLayerPropertiesSerializationAndDeserialization(Layer* src) {
    // This is required to ensure that properties are serialized.
    src->SetNeedsPushProperties();
    src->SetLayerTreeHost(layer_tree_host_.get());

    // The following member is reset during serialization, so store the original
    // values.
    gfx::Rect update_rect = src->update_rect_;

    // Serialize |src| to protobuf and read the first entry in the
    // LayerUpdate. There are no descendants, so the serialization
    // of |src| is the only entry.
    proto::LayerUpdate layer_update;
    src->ToLayerPropertiesProto(&layer_update);
    ASSERT_EQ(1, layer_update.layers_size());
    proto::LayerProperties props = layer_update.layers(0);

    // The |dest| layer needs to be able to lookup the scroll and clip parents.
    if (src->scroll_parent_)
      layer_tree_host_->RegisterLayer(src->scroll_parent_);
    if (src->scroll_children_) {
      for (auto* child : *(src->scroll_children_))
        layer_tree_host_->RegisterLayer(child);
    }
    if (src->clip_parent_)
      layer_tree_host_->RegisterLayer(src->clip_parent_);
    if (src->clip_children_) {
      for (auto* child : *(src->clip_children_))
        layer_tree_host_->RegisterLayer(child);
    }
    // Reset the LayerTreeHost registration for the |src| layer so
    // it can be re-used for the |dest| layer.
    src->SetLayerTreeHost(nullptr);

    scoped_refptr<Layer> dest = Layer::Create();
    dest->layer_id_ = src->layer_id_;
    dest->SetLayerTreeHost(layer_tree_host_.get());
    dest->FromLayerPropertiesProto(props);

    // Verify that both layers are equal.
    EXPECT_EQ(src->transform_origin_, dest->transform_origin_);
    EXPECT_EQ(src->background_color_, dest->background_color_);
    EXPECT_EQ(src->bounds_, dest->bounds_);
    EXPECT_EQ(src->transform_tree_index_, dest->transform_tree_index_);
    EXPECT_EQ(src->effect_tree_index_, dest->effect_tree_index_);
    EXPECT_EQ(src->clip_tree_index_, dest->clip_tree_index_);
    EXPECT_EQ(src->offset_to_transform_parent_,
              dest->offset_to_transform_parent_);
    EXPECT_EQ(src->double_sided_, dest->double_sided_);
    EXPECT_EQ(src->draws_content_, dest->draws_content_);
    EXPECT_EQ(src->hide_layer_and_subtree_, dest->hide_layer_and_subtree_);
    EXPECT_EQ(src->masks_to_bounds_, dest->masks_to_bounds_);
    EXPECT_EQ(src->main_thread_scrolling_reasons_,
              dest->main_thread_scrolling_reasons_);
    EXPECT_EQ(src->non_fast_scrollable_region_,
              dest->non_fast_scrollable_region_);
    EXPECT_EQ(src->touch_event_handler_region_,
              dest->touch_event_handler_region_);
    EXPECT_EQ(src->contents_opaque_, dest->contents_opaque_);
    EXPECT_EQ(src->opacity_, dest->opacity_);
    EXPECT_EQ(src->blend_mode_, dest->blend_mode_);
    EXPECT_EQ(src->is_root_for_isolated_group_,
              dest->is_root_for_isolated_group_);
    EXPECT_EQ(src->position_, dest->position_);
    EXPECT_EQ(src->is_container_for_fixed_position_layers_,
              dest->is_container_for_fixed_position_layers_);
    EXPECT_EQ(src->position_constraint_, dest->position_constraint_);
    EXPECT_EQ(src->should_flatten_transform_, dest->should_flatten_transform_);
    EXPECT_EQ(src->should_flatten_transform_from_property_tree_,
              dest->should_flatten_transform_from_property_tree_);
    EXPECT_EQ(src->draw_blend_mode_, dest->draw_blend_mode_);
    EXPECT_EQ(src->use_parent_backface_visibility_,
              dest->use_parent_backface_visibility_);
    EXPECT_EQ(src->transform_, dest->transform_);
    EXPECT_EQ(src->sorting_context_id_, dest->sorting_context_id_);
    EXPECT_EQ(src->num_descendants_that_draw_content_,
              dest->num_descendants_that_draw_content_);
    EXPECT_EQ(src->scroll_clip_layer_id_, dest->scroll_clip_layer_id_);
    EXPECT_EQ(src->user_scrollable_horizontal_,
              dest->user_scrollable_horizontal_);
    EXPECT_EQ(src->user_scrollable_vertical_, dest->user_scrollable_vertical_);
    EXPECT_EQ(src->scroll_offset_, dest->scroll_offset_);
    EXPECT_EQ(update_rect, dest->update_rect_);

    if (src->scroll_parent_) {
      ASSERT_TRUE(dest->scroll_parent_);
      EXPECT_EQ(src->scroll_parent_->id(), dest->scroll_parent_->id());
    } else {
      EXPECT_FALSE(dest->scroll_parent_);
    }
    if (src->scroll_children_) {
      ASSERT_TRUE(dest->scroll_children_);
      EXPECT_EQ(*(src->scroll_children_), *(dest->scroll_children_));
    } else {
      EXPECT_FALSE(dest->scroll_children_);
    }

    if (src->clip_parent_) {
      ASSERT_TRUE(dest->clip_parent_);
      EXPECT_EQ(src->clip_parent_->id(), dest->clip_parent_->id());
    } else {
      ASSERT_FALSE(dest->clip_parent_);
    }
    if (src->clip_children_) {
      ASSERT_TRUE(dest->clip_children_);
      EXPECT_EQ(*(src->clip_children_), *(dest->clip_children_));
    } else {
      EXPECT_FALSE(dest->clip_children_);
    }

    // The following member should have been reset during serialization.
    EXPECT_EQ(gfx::Rect(), src->update_rect_);

    // Before deleting |dest|, the LayerTreeHost must be unset.
    dest->SetLayerTreeHost(nullptr);

    // Cleanup scroll tree.
    if (src->scroll_parent_)
      layer_tree_host_->UnregisterLayer(src->scroll_parent_);
    src->scroll_parent_ = nullptr;
    dest->scroll_parent_ = nullptr;
    if (src->scroll_children_) {
      for (auto* child : *(src->scroll_children_))
        layer_tree_host_->UnregisterLayer(child);
      src->scroll_children_.reset();
      dest->scroll_children_.reset();
    }

    // Cleanup clip tree.
    if (src->clip_parent_)
      layer_tree_host_->UnregisterLayer(src->clip_parent_);
    src->clip_parent_ = nullptr;
    dest->clip_parent_ = nullptr;
    if (src->clip_children_) {
      for (auto* child : *(src->clip_children_))
        layer_tree_host_->UnregisterLayer(child);
      src->clip_children_.reset();
      dest->clip_children_.reset();
    }
  }

  void RunNoMembersChangedTest() {
    scoped_refptr<Layer> layer = Layer::Create();
    VerifyBaseLayerPropertiesSerializationAndDeserialization(layer.get());
  }

  void RunArbitraryMembersChangedTest() {
    scoped_refptr<Layer> layer = Layer::Create();
    layer->transform_origin_ = gfx::Point3F(3.0f, 1.0f, 4.0f);
    layer->background_color_ = SK_ColorRED;
    layer->bounds_ = gfx::Size(3, 14);
    layer->transform_tree_index_ = -1;
    layer->effect_tree_index_ = -1;
    layer->clip_tree_index_ = 71;
    layer->offset_to_transform_parent_ = gfx::Vector2dF(3.14f, 1.618f);
    layer->double_sided_ = true;
    layer->draws_content_ = true;
    layer->hide_layer_and_subtree_ = false;
    layer->masks_to_bounds_ = true;
    layer->main_thread_scrolling_reasons_ =
        MainThreadScrollingReason::kNotScrollingOnMain;
    layer->non_fast_scrollable_region_ = Region(gfx::Rect(5, 1, 14, 3));
    layer->touch_event_handler_region_ = Region(gfx::Rect(3, 14, 1, 5));
    layer->contents_opaque_ = true;
    layer->opacity_ = 1.f;
    layer->blend_mode_ = SkXfermode::kSrcOver_Mode;
    layer->is_root_for_isolated_group_ = true;
    layer->position_ = gfx::PointF(3.14f, 6.28f);
    layer->is_container_for_fixed_position_layers_ = true;
    LayerPositionConstraint pos_con;
    pos_con.set_is_fixed_to_bottom_edge(true);
    layer->position_constraint_ = pos_con;
    layer->should_flatten_transform_ = true;
    layer->should_flatten_transform_from_property_tree_ = true;
    layer->draw_blend_mode_ = SkXfermode::kSrcOut_Mode;
    layer->use_parent_backface_visibility_ = true;
    gfx::Transform transform;
    transform.Rotate(90);
    layer->transform_ = transform;
    layer->sorting_context_id_ = 0;
    layer->num_descendants_that_draw_content_ = 5;
    layer->scroll_clip_layer_id_ = Layer::INVALID_ID;
    layer->user_scrollable_horizontal_ = false;
    layer->user_scrollable_vertical_ = true;
    layer->scroll_offset_ = gfx::ScrollOffset(3, 14);
    layer->update_rect_ = gfx::Rect(14, 15);

    VerifyBaseLayerPropertiesSerializationAndDeserialization(layer.get());
  }

  void RunAllMembersChangedTest() {
    scoped_refptr<Layer> layer = Layer::Create();
    layer->transform_origin_ = gfx::Point3F(3.0f, 1.0f, 4.0f);
    layer->background_color_ = SK_ColorRED;
    layer->bounds_ = gfx::Size(3, 14);
    layer->transform_tree_index_ = 39;
    layer->effect_tree_index_ = 17;
    layer->clip_tree_index_ = 71;
    layer->offset_to_transform_parent_ = gfx::Vector2dF(3.14f, 1.618f);
    layer->double_sided_ = !layer->double_sided_;
    layer->draws_content_ = !layer->draws_content_;
    layer->hide_layer_and_subtree_ = !layer->hide_layer_and_subtree_;
    layer->masks_to_bounds_ = !layer->masks_to_bounds_;
    layer->main_thread_scrolling_reasons_ =
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
    layer->non_fast_scrollable_region_ = Region(gfx::Rect(5, 1, 14, 3));
    layer->touch_event_handler_region_ = Region(gfx::Rect(3, 14, 1, 5));
    layer->contents_opaque_ = !layer->contents_opaque_;
    layer->opacity_ = 3.14f;
    layer->blend_mode_ = SkXfermode::kSrcIn_Mode;
    layer->is_root_for_isolated_group_ = !layer->is_root_for_isolated_group_;
    layer->position_ = gfx::PointF(3.14f, 6.28f);
    layer->is_container_for_fixed_position_layers_ =
        !layer->is_container_for_fixed_position_layers_;
    LayerPositionConstraint pos_con;
    pos_con.set_is_fixed_to_bottom_edge(true);
    layer->position_constraint_ = pos_con;
    layer->should_flatten_transform_ = !layer->should_flatten_transform_;
    layer->should_flatten_transform_from_property_tree_ =
        !layer->should_flatten_transform_from_property_tree_;
    layer->draw_blend_mode_ = SkXfermode::kSrcOut_Mode;
    layer->use_parent_backface_visibility_ =
        !layer->use_parent_backface_visibility_;
    gfx::Transform transform;
    transform.Rotate(90);
    layer->transform_ = transform;
    layer->sorting_context_id_ = 42;
    layer->num_descendants_that_draw_content_ = 5;
    layer->scroll_clip_layer_id_ = 17;
    layer->user_scrollable_horizontal_ = !layer->user_scrollable_horizontal_;
    layer->user_scrollable_vertical_ = !layer->user_scrollable_vertical_;
    layer->scroll_offset_ = gfx::ScrollOffset(3, 14);
    layer->update_rect_ = gfx::Rect(14, 15);

    VerifyBaseLayerPropertiesSerializationAndDeserialization(layer.get());
  }

  void VerifySolidColorScrollbarLayerAfterSerializationAndDeserialization(
      scoped_refptr<SolidColorScrollbarLayer> source_scrollbar) {
    proto::LayerProperties serialized_scrollbar;
    source_scrollbar->LayerSpecificPropertiesToProto(&serialized_scrollbar);

    scoped_refptr<SolidColorScrollbarLayer> deserialized_scrollbar =
        SolidColorScrollbarLayer::Create(ScrollbarOrientation::HORIZONTAL, -1,
                                         -1, false, Layer::INVALID_ID);
    deserialized_scrollbar->layer_id_ = source_scrollbar->layer_id_;

    // FromLayerSpecificPropertiesProto expects a non-null LayerTreeHost to be
    // set.
    deserialized_scrollbar->SetLayerTreeHost(layer_tree_host_.get());
    deserialized_scrollbar->FromLayerSpecificPropertiesProto(
        serialized_scrollbar);

    EXPECT_EQ(source_scrollbar->track_start_,
              deserialized_scrollbar->track_start_);
    EXPECT_EQ(source_scrollbar->thumb_thickness_,
              deserialized_scrollbar->thumb_thickness_);
    EXPECT_EQ(source_scrollbar->scroll_layer_id_,
              deserialized_scrollbar->scroll_layer_id_);
    EXPECT_EQ(source_scrollbar->is_left_side_vertical_scrollbar_,
              deserialized_scrollbar->is_left_side_vertical_scrollbar_);
    EXPECT_EQ(source_scrollbar->orientation_,
              deserialized_scrollbar->orientation_);

    deserialized_scrollbar->SetLayerTreeHost(nullptr);
  }

  void RunScrollAndClipLayersTest() {
    scoped_refptr<Layer> layer = Layer::Create();

    scoped_refptr<Layer> scroll_parent = Layer::Create();
    layer->scroll_parent_ = scroll_parent.get();
    scoped_refptr<Layer> scroll_child = Layer::Create();
    layer->scroll_children_.reset(new std::set<Layer*>);
    layer->scroll_children_->insert(scroll_child.get());

    scoped_refptr<Layer> clip_parent = Layer::Create();
    layer->clip_parent_ = clip_parent.get();
    layer->clip_children_.reset(new std::set<Layer*>);
    scoped_refptr<Layer> clip_child1 = Layer::Create();
    layer->clip_children_->insert(clip_child1.get());
    scoped_refptr<Layer> clip_child2 = Layer::Create();
    layer->clip_children_->insert(clip_child2.get());

    VerifyBaseLayerPropertiesSerializationAndDeserialization(layer.get());
  }

  void RunHierarchyDeserializationWithLayerTreeHostTest() {
    /* Testing serialization and deserialization of a tree that looks like this:
            root
               \
                a
                 \
                  b
                   \
                    c
      The root layer has a LayerTreeHost, and it should propagate to all the
      children.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    scoped_refptr<Layer> layer_src_c = Layer::Create();
    layer_src_root->AddChild(layer_src_a);
    layer_src_a->AddChild(layer_src_b);
    layer_src_b->AddChild(layer_src_c);

    proto::LayerNode proto;
    layer_src_root->ToLayerNodeProto(&proto);

    Layer::LayerIdMap empty_dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();

    layer_dest_root->FromLayerNodeProto(proto, empty_dest_layer_map,
                                        layer_tree_host_.get());

    EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
    EXPECT_EQ(nullptr, layer_dest_root->parent());
    ASSERT_EQ(1u, layer_dest_root->children().size());
    EXPECT_EQ(layer_tree_host_.get(), layer_dest_root->layer_tree_host_);

    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(layer_src_root->id(), layer_dest_a->parent()->id());
    EXPECT_EQ(1u, layer_dest_a->children().size());
    EXPECT_EQ(layer_tree_host_.get(), layer_dest_a->layer_tree_host_);

    scoped_refptr<Layer> layer_dest_b = layer_dest_a->children()[0];
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    EXPECT_EQ(layer_src_a->id(), layer_dest_b->parent()->id());
    ASSERT_EQ(1u, layer_dest_b->children().size());
    EXPECT_EQ(layer_tree_host_.get(), layer_dest_b->layer_tree_host_);

    scoped_refptr<Layer> layer_dest_c = layer_dest_b->children()[0];
    EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());
    EXPECT_EQ(layer_src_b->id(), layer_dest_c->parent()->id());
    EXPECT_EQ(0u, layer_dest_c->children().size());
    EXPECT_EQ(layer_tree_host_.get(), layer_dest_c->layer_tree_host_);

    // The layers have not been added to the LayerTreeHost layer map, so the
    // LTH pointers must be cleared manually.
    layer_dest_root->layer_tree_host_ = nullptr;
    layer_dest_a->layer_tree_host_ = nullptr;
    layer_dest_b->layer_tree_host_ = nullptr;
    layer_dest_c->layer_tree_host_ = nullptr;
  }

  void RunNonDestructiveDeserializationBaseCaseTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
            /
           a
       The source tree is then deserialized from the same structure which should
       re-use the Layers from last deserialization and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    layer_root->AddChild(layer_src_a);
    layer_root->transform_tree_index_ = 33;
    layer_src_a->transform_tree_index_ = 42;

    proto::LayerNode root_proto;
    layer_root->ToLayerNodeProto(&root_proto);

    Layer::LayerIdMap dest_layer_map;
    layer_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_root->FromLayerNodeProto(root_proto, dest_layer_map,
                                   layer_tree_host_.get());

    EXPECT_EQ(33, layer_root->transform_tree_index_);
    ASSERT_EQ(1u, layer_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_root->children()[0];
    EXPECT_EQ(layer_src_a, layer_dest_a);
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);

    // Clear the reference to the LTH for all the layers.
    layer_root->SetLayerTreeHost(nullptr);
  }

  void RunNonDestructiveDeserializationReorderChildrenTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
            /  \
           a    b
       The children are then re-ordered to:
           root
           /  \
          b    a
       The tree is then serialized and deserialized again, and the the end
       result should have the same structure and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    layer_src_root->AddChild(layer_src_a);
    layer_src_root->AddChild(layer_src_b);

    // Copy tree-structure to new root.
    proto::LayerNode root_proto_1;
    layer_src_root->ToLayerNodeProto(&root_proto_1);
    Layer::LayerIdMap dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();
    layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure initial copy is correct.
    ASSERT_EQ(2u, layer_dest_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());

    // Swap order of the children.
    scoped_refptr<Layer> tmp_a = layer_src_root->children_[0];
    layer_src_root->children_[0] = layer_src_root->children_[1];
    layer_src_root->children_[1] = tmp_a;

    // Fake the fact that the destination layers have valid indexes.
    layer_dest_root->transform_tree_index_ = 33;
    layer_dest_a->transform_tree_index_ = 42;
    layer_dest_b->transform_tree_index_ = 24;

    // Now serialize and deserialize again.
    proto::LayerNode root_proto_2;
    layer_src_root->ToLayerNodeProto(&root_proto_2);
    layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure second copy is correct.
    EXPECT_EQ(33, layer_dest_root->transform_tree_index_);
    ASSERT_EQ(2u, layer_dest_root->children().size());
    layer_dest_b = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    EXPECT_EQ(24, layer_dest_b->transform_tree_index_);
    layer_dest_a = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);

    layer_dest_root->SetLayerTreeHost(nullptr);
  }

  void RunNonDestructiveDeserializationAddChildTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
            /
           a
       A child is then added to the root:
           root
           /  \
          b    a
       The tree is then serialized and deserialized again, and the the end
       result should have the same structure and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    layer_src_root->AddChild(layer_src_a);

    // Copy tree-structure to new root.
    proto::LayerNode root_proto_1;
    layer_src_root->ToLayerNodeProto(&root_proto_1);
    Layer::LayerIdMap dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();
    layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure initial copy is correct.
    ASSERT_EQ(1u, layer_dest_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());

    // Fake the fact that the destination layer |a| now has a valid index.
    layer_dest_root->transform_tree_index_ = 33;
    layer_dest_a->transform_tree_index_ = 42;

    // Add another child.
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    layer_src_root->AddChild(layer_src_b);

    // Now serialize and deserialize again.
    proto::LayerNode root_proto_2;
    layer_src_root->ToLayerNodeProto(&root_proto_2);
    layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure second copy is correct.
    EXPECT_EQ(33, layer_dest_root->transform_tree_index_);
    ASSERT_EQ(2u, layer_dest_root->children().size());
    layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);
    scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());

    layer_dest_root->SetLayerTreeHost(nullptr);
  }

  void RunNonDestructiveDeserializationRemoveChildTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
            /  \
           a    b
       The |b| child is the removed from the root:
           root
           /
          b
       The tree is then serialized and deserialized again, and the the end
       result should have the same structure and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    layer_src_root->AddChild(layer_src_a);
    layer_src_root->AddChild(layer_src_b);

    // Copy tree-structure to new root.
    proto::LayerNode root_proto_1;
    layer_src_root->ToLayerNodeProto(&root_proto_1);
    Layer::LayerIdMap dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();
    layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure initial copy is correct.
    ASSERT_EQ(2u, layer_dest_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());

    // Remove one child.
    layer_src_b->RemoveFromParent();

    // Fake the fact that the destination layers have valid indexes.
    layer_dest_root->transform_tree_index_ = 33;
    layer_dest_a->transform_tree_index_ = 42;
    layer_dest_b->transform_tree_index_ = 24;

    // Now serialize and deserialize again.
    proto::LayerNode root_proto_2;
    layer_src_root->ToLayerNodeProto(&root_proto_2);
    layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure second copy is correct.
    EXPECT_EQ(33, layer_dest_root->transform_tree_index_);
    ASSERT_EQ(1u, layer_dest_root->children().size());
    layer_dest_a = layer_dest_root->children()[0];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);

    layer_dest_root->SetLayerTreeHost(nullptr);
  }

  void RunNonDestructiveDeserializationMoveChildEarlierTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
            /   \
           a     b
                  \
                   c
       The |c| child of |b| is then moved to become a child of |a|:
            root
           /    \
          a      b
         /
        c
       The tree is then serialized and deserialized again, and the the end
       result should have the same structure and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    scoped_refptr<Layer> layer_src_c = Layer::Create();
    layer_src_root->AddChild(layer_src_a);
    layer_src_root->AddChild(layer_src_b);
    layer_src_b->AddChild(layer_src_c);

    // Copy tree-structure to new root.
    proto::LayerNode root_proto_1;
    layer_src_root->ToLayerNodeProto(&root_proto_1);
    Layer::LayerIdMap dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();
    layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure initial copy is correct.
    ASSERT_EQ(2u, layer_dest_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    ASSERT_EQ(1u, layer_dest_b->children().size());
    scoped_refptr<Layer> layer_dest_c = layer_dest_b->children()[0];
    EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());

    // Move child |c| from |b| to |a|.
    layer_src_c->RemoveFromParent();
    layer_src_a->AddChild(layer_src_c);

    // Moving a child invalidates the |transform_tree_index_|, so forcefully
    // set it afterwards on the destination layer.
    layer_dest_root->transform_tree_index_ = 33;
    layer_dest_a->transform_tree_index_ = 42;
    layer_dest_b->transform_tree_index_ = 24;
    layer_dest_c->transform_tree_index_ = 99;

    // Now serialize and deserialize again.
    proto::LayerNode root_proto_2;
    layer_src_root->ToLayerNodeProto(&root_proto_2);
    layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure second copy is correct.
    EXPECT_EQ(33, layer_dest_root->transform_tree_index_);
    ASSERT_EQ(2u, layer_dest_root->children().size());
    layer_dest_a = layer_dest_root->children()[0];
    layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    EXPECT_EQ(24, layer_dest_b->transform_tree_index_);
    ASSERT_EQ(1u, layer_dest_a->children().size());
    layer_dest_c = layer_dest_a->children()[0];
    EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());
    EXPECT_EQ(99, layer_dest_c->transform_tree_index_);

    layer_dest_root->SetLayerTreeHost(nullptr);
  }

  void RunNonDestructiveDeserializationMoveChildLaterTest() {
    /* Testing serialization and deserialization of a tree that initially looks
       like this:
            root
           /    \
          a      b
         /
        c
       The |c| child of |a| is then moved to become a child of |b|:
            root
           /    \
          a      b
                  \
                   c
       The tree is then serialized and deserialized again, and the the end
       result should have the same structure and importantly it should
       not have called InvalidatePropertyTreesIndices() for any of the layers,
       which would happen in for example SetLayerTreeHost(...) calls.
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    scoped_refptr<Layer> layer_src_c = Layer::Create();
    layer_src_root->AddChild(layer_src_a);
    layer_src_root->AddChild(layer_src_b);
    layer_src_a->AddChild(layer_src_c);

    // Copy tree-structure to new root.
    proto::LayerNode root_proto_1;
    layer_src_root->ToLayerNodeProto(&root_proto_1);
    Layer::LayerIdMap dest_layer_map;
    scoped_refptr<Layer> layer_dest_root = Layer::Create();
    layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure initial copy is correct.
    ASSERT_EQ(2u, layer_dest_root->children().size());
    scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
    scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    ASSERT_EQ(1u, layer_dest_a->children().size());
    scoped_refptr<Layer> layer_dest_c = layer_dest_a->children()[0];
    EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());

    // Move child |c| from |b| to |a|.
    layer_src_c->RemoveFromParent();
    layer_src_b->AddChild(layer_src_c);

    // Moving a child invalidates the |transform_tree_index_|, so forcefully
    // set it afterwards on the destination layer.
    layer_dest_root->transform_tree_index_ = 33;
    layer_dest_a->transform_tree_index_ = 42;
    layer_dest_b->transform_tree_index_ = 24;
    layer_dest_c->transform_tree_index_ = 99;

    // Now serialize and deserialize again.
    proto::LayerNode root_proto_2;
    layer_src_root->ToLayerNodeProto(&root_proto_2);
    layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &dest_layer_map);
    layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map,
                                        layer_tree_host_.get());

    // Ensure second copy is correct.
    EXPECT_EQ(33, layer_dest_root->transform_tree_index_);
    ASSERT_EQ(2u, layer_dest_root->children().size());
    layer_dest_a = layer_dest_root->children()[0];
    layer_dest_b = layer_dest_root->children()[1];
    EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
    EXPECT_EQ(42, layer_dest_a->transform_tree_index_);
    EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
    EXPECT_EQ(24, layer_dest_b->transform_tree_index_);
    ASSERT_EQ(1u, layer_dest_b->children().size());
    layer_dest_c = layer_dest_b->children()[0];
    EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());
    EXPECT_EQ(99, layer_dest_c->transform_tree_index_);

    layer_dest_root->SetLayerTreeHost(nullptr);
  }

  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
};

namespace {

class MockLayerTreeHost : public LayerTreeHost {
 public:
  MockLayerTreeHost(LayerTreeHostSingleThreadClient* single_thread_client,
                    LayerTreeHost::InitParams* params)
      : LayerTreeHost(params, CompositorMode::SINGLE_THREADED) {
    InitializeSingleThreaded(single_thread_client,
                             base::ThreadTaskRunnerHandle::Get(), nullptr);
  }

  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(SetNeedsUpdateLayers, void());
  MOCK_METHOD0(SetNeedsFullTreeSync, void());
};

class LayerTest : public testing::Test {
 public:
  LayerTest()
      : host_impl_(LayerTreeSettings(),
                   &task_runner_provider_,
                   &shared_bitmap_manager_,
                   &task_graph_runner_),
        fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {
    timeline_impl_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    timeline_impl_->set_is_impl_only(true);
    host_impl_.animation_host()->AddAnimationTimeline(timeline_impl_);
  }

  const LayerTreeSettings& settings() { return settings_; }
  scoped_refptr<AnimationTimeline> timeline_impl() { return timeline_impl_; }

 protected:
  void SetUp() override {
    LayerTreeHost::InitParams params;
    params.client = &fake_client_;
    params.settings = &settings_;
    params.task_graph_runner = &task_graph_runner_;
    params.animation_host =
        AnimationHost::CreateForTesting(ThreadInstance::MAIN);

    layer_tree_host_.reset(
        new StrictMock<MockLayerTreeHost>(&fake_client_, &params));
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());
    parent_ = nullptr;
    child1_ = nullptr;
    child2_ = nullptr;
    child3_ = nullptr;
    grand_child1_ = nullptr;
    grand_child2_ = nullptr;
    grand_child3_ = nullptr;

    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
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

    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());
    layer_tree_host_->SetRootLayer(parent_);

    parent_->AddChild(child1_);
    parent_->AddChild(child2_);
    parent_->AddChild(child3_);
    child1_->AddChild(grand_child1_);
    child1_->AddChild(grand_child2_);
    child2_->AddChild(grand_child3_);

    Mock::VerifyAndClearExpectations(layer_tree_host_.get());

    VerifyTestTreeInitialState();
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;

  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<StrictMock<MockLayerTreeHost>> layer_tree_host_;
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
  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<Layer> dummy_layer1 = Layer::Create();
  scoped_refptr<Layer> dummy_layer2 = Layer::Create();

  layer_tree_host_->SetRootLayer(root);
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
  std::unique_ptr<LayerImpl> dummy_layer2_impl =
      LayerImpl::Create(host_impl_.active_tree(), dummy_layer2->id());

  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(1);
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

  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetReplicaLayer(dummy_layer2.get()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      dummy_layer2->PushPropertiesTo(dummy_layer2_impl.get()));

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
      grand_child->PushPropertiesTo(grand_child_impl.get());
      dummy_layer2->PushPropertiesTo(dummy_layer2_impl.get()));

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
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundFilters(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get()));

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetPosition(arbitrary_point_f);
  TransformNode* node = layer_tree_host_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->data.transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_host_->property_trees()->ResetAllChangeTracking());
  EXPECT_FALSE(node->data.transform_changed);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  child->SetPosition(arbitrary_point_f);
  node = layer_tree_host_->property_trees()->transform_tree.Node(
      child->transform_tree_index());
  EXPECT_TRUE(node->data.transform_changed);
  // child2 is not in the subtree of child, but its scroll parent is. So, its
  // to_screen will be effected by change in position of child2.
  layer_tree_host_->property_trees()->transform_tree.UpdateTransforms(
      child2->transform_tree_index());
  node = layer_tree_host_->property_trees()->transform_tree.Node(
      child2->transform_tree_index());
  EXPECT_TRUE(node->data.transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      child->PushPropertiesTo(child_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_host_->property_trees()->ResetAllChangeTracking());
  node = layer_tree_host_->property_trees()->transform_tree.Node(
      child->transform_tree_index());
  EXPECT_FALSE(node->data.transform_changed);

  gfx::Point3F arbitrary_point_3f = gfx::Point3F(0.125f, 0.25f, 0.f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetTransformOrigin(arbitrary_point_3f);
  node = layer_tree_host_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->data.transform_changed);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      root->PushPropertiesTo(root_impl.get());
      child->PushPropertiesTo(child_impl.get());
      child2->PushPropertiesTo(child2_impl.get());
      grand_child->PushPropertiesTo(grand_child_impl.get());
      layer_tree_host_->property_trees()->ResetAllChangeTracking());

  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  root->SetTransform(arbitrary_transform);
  node = layer_tree_host_->property_trees()->transform_tree.Node(
      root->transform_tree_index());
  EXPECT_TRUE(node->data.transform_changed);
}

TEST_F(LayerTest, AddAndRemoveChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  // Upon creation, layers should not have children or parent.
  ASSERT_EQ(0U, parent->children().size());
  EXPECT_FALSE(child->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->AddChild(child));

  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child.get(), parent->children()[0]);
  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(parent.get(), child->RootLayer());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), child->RemoveFromParent());
}

TEST_F(LayerTest, AddSameChildTwice) {
  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  layer_tree_host_->SetRootLayer(parent);

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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

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

  EXPECT_TRUE(
      layer_tree_host_->LayerNeedsPushPropertiesForTesting(child1.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, DeleteRemovedScrollChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

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

  EXPECT_TRUE(
      layer_tree_host_->LayerNeedsPushPropertiesForTesting(child2.get()));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, ReplaceChildWithSameChild) {
  CreateSimpleTestTree();

  // SetNeedsFullTreeSync / SetNeedsCommit should not be called because its the
  // same child.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(0);
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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      1, layer_tree_host_->SetRootLayer(new_parent));

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      AtLeast(1), new_parent->SetChildren(new_children));

  ASSERT_EQ(2U, new_parent->children().size());
  EXPECT_EQ(new_parent.get(), child1->parent());
  EXPECT_EQ(new_parent.get(), child2->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
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
  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());

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
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      1, layer_tree_host_->SetRootLayer(test_layer));
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
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(true));

  // sanity check of initial test condition
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  uint32_t reasons = 0, reasons_to_clear = 0, reasons_after_clearing = 0;
  reasons |= MainThreadScrollingReason::kEventHandlers;
  reasons |= MainThreadScrollingReason::kContinuingMainThreadScroll;
  reasons |= MainThreadScrollingReason::kScrollbarScrolling;

  reasons_to_clear |= MainThreadScrollingReason::kContinuingMainThreadScroll;
  reasons_to_clear |= MainThreadScrollingReason::kThreadedScrollingDisabled;

  reasons_after_clearing |= MainThreadScrollingReason::kEventHandlers;
  reasons_after_clearing |= MainThreadScrollingReason::kScrollbarScrolling;

  // Check that the reasons are added correctly.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->AddMainThreadScrollingReasons(
                                 MainThreadScrollingReason::kEventHandlers));
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
  EXPECT_SET_NEEDS_COMMIT(0, test_layer->AddMainThreadScrollingReasons(
                                 MainThreadScrollingReason::kEventHandlers));
}

TEST_F(LayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(
      1, layer_tree_host_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetIsDrawable(true));

  scoped_refptr<Layer> dummy_layer1 = Layer::Create();
  scoped_refptr<Layer> dummy_layer2 = Layer::Create();

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
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->AddMainThreadScrollingReasons(
                                 MainThreadScrollingReason::kEventHandlers));
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
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, test_layer->SetReplicaLayer(
      dummy_layer2.get()));

  // The above tests should not have caused a change to the needs_display flag.
  EXPECT_FALSE(test_layer->NeedsDisplayForTesting());

  // As layers are removed from the tree, they will cause a tree sync.
  EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times((AnyNumber()));
}

TEST_F(LayerTest, PushPropertiesAccumulatesUpdateRect) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

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

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetOpacity(0.5f));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  test_layer->PushPropertiesTo(impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
}

TEST_F(LayerTest, MaskAndReplicaHasParent) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> replica = Layer::Create();
  scoped_refptr<Layer> replica_mask = Layer::Create();
  scoped_refptr<Layer> mask_replacement = Layer::Create();
  scoped_refptr<Layer> replica_replacement = Layer::Create();
  scoped_refptr<Layer> replica_mask_replacement = Layer::Create();

  parent->AddChild(child);
  child->SetMaskLayer(mask.get());
  child->SetReplicaLayer(replica.get());
  replica->SetMaskLayer(replica_mask.get());

  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(child.get(), mask->parent());
  EXPECT_EQ(child.get(), replica->parent());
  EXPECT_EQ(replica.get(), replica_mask->parent());

  replica->SetMaskLayer(replica_mask_replacement.get());
  EXPECT_EQ(nullptr, replica_mask->parent());
  EXPECT_EQ(replica.get(), replica_mask_replacement->parent());

  child->SetMaskLayer(mask_replacement.get());
  EXPECT_EQ(nullptr, mask->parent());
  EXPECT_EQ(child.get(), mask_replacement->parent());

  child->SetReplicaLayer(replica_replacement.get());
  EXPECT_EQ(nullptr, replica->parent());
  EXPECT_EQ(child.get(), replica_replacement->parent());

  EXPECT_EQ(replica.get(), replica->mask_layer()->parent());
}

class LayerTreeHostFactory {
 public:
  LayerTreeHostFactory() : client_(FakeLayerTreeHostClient::DIRECT_3D) {}

  std::unique_ptr<LayerTreeHost> Create() {
    return Create(LayerTreeSettings());
  }

  std::unique_ptr<LayerTreeHost> Create(LayerTreeSettings settings) {
    LayerTreeHost::InitParams params;
    params.client = &client_;
    params.shared_bitmap_manager = &shared_bitmap_manager_;
    params.task_graph_runner = &task_graph_runner_;
    params.gpu_memory_buffer_manager = &gpu_memory_buffer_manager_;
    params.settings = &settings;
    params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    params.animation_host =
        AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    return LayerTreeHost::CreateSingleThreaded(&client_, &params);
  }

 private:
  FakeLayerTreeHostClient client_;
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
};

void AssertLayerTreeHostMatchesForSubtree(Layer* layer, LayerTreeHost* host) {
  EXPECT_EQ(host, layer->layer_tree_host());

  for (size_t i = 0; i < layer->children().size(); ++i)
    AssertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

  if (layer->mask_layer())
    AssertLayerTreeHostMatchesForSubtree(layer->mask_layer(), host);

  if (layer->replica_layer())
    AssertLayerTreeHostMatchesForSubtree(layer->replica_layer(), host);
}

class LayerLayerTreeHostTest : public testing::Test {};

TEST_F(LayerLayerTreeHostTest, EnteringTree) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> replica = Layer::Create();
  scoped_refptr<Layer> replica_mask = Layer::Create();

  // Set up a detached tree of layers. The host pointer should be nil for these
  // layers.
  parent->AddChild(child);
  child->SetMaskLayer(mask.get());
  child->SetReplicaLayer(replica.get());
  replica->SetMaskLayer(replica_mask.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);

  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> layer_tree_host = factory.Create();
  // Setting the root layer should set the host pointer for all layers in the
  // tree.
  layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Clearing the root layer should also clear out the host pointers for all
  // layers in the tree.
  layer_tree_host->SetRootLayer(nullptr);

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);
}

TEST_F(LayerLayerTreeHostTest, AddingLayerSubtree) {
  scoped_refptr<Layer> parent = Layer::Create();
  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> layer_tree_host = factory.Create();

  layer_tree_host->SetRootLayer(parent.get());

  EXPECT_EQ(parent->layer_tree_host(), layer_tree_host.get());

  // Adding a subtree to a layer already associated with a host should set the
  // host pointer on all layers in that subtree.
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  child->AddChild(grand_child);

  // Masks, replicas, and replica masks should pick up the new host too.
  scoped_refptr<Layer> child_mask = Layer::Create();
  child->SetMaskLayer(child_mask.get());
  scoped_refptr<Layer> child_replica = Layer::Create();
  child->SetReplicaLayer(child_replica.get());
  scoped_refptr<Layer> child_replica_mask = Layer::Create();
  child_replica->SetMaskLayer(child_replica_mask.get());

  parent->AddChild(child);
  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ChangeHost) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> replica = Layer::Create();
  scoped_refptr<Layer> replica_mask = Layer::Create();

  // Same setup as the previous test.
  parent->AddChild(child);
  child->SetMaskLayer(mask.get());
  child->SetReplicaLayer(replica.get());
  replica->SetMaskLayer(replica_mask.get());

  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> first_layer_tree_host = factory.Create();
  first_layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       first_layer_tree_host.get());

  // Now re-root the tree to a new host (simulating what we do on a context lost
  // event). This should update the host pointers for all layers in the tree.
  std::unique_ptr<LayerTreeHost> second_layer_tree_host = factory.Create();
  second_layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       second_layer_tree_host.get());

  second_layer_tree_host->SetRootLayer(nullptr);
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
  std::unique_ptr<LayerTreeHost> first_layer_tree_host = factory.Create();
  first_layer_tree_host->SetRootLayer(first_parent.get());

  AssertLayerTreeHostMatchesForSubtree(first_parent.get(),
                                       first_layer_tree_host.get());

  // Now reparent the subtree starting at second_child to a layer in a different
  // tree.
  std::unique_ptr<LayerTreeHost> second_layer_tree_host = factory.Create();
  second_layer_tree_host->SetRootLayer(second_parent.get());

  second_parent->AddChild(second_child);

  // The moved layer and its children should point to the new host.
  EXPECT_EQ(second_layer_tree_host.get(), second_child->layer_tree_host());
  EXPECT_EQ(second_layer_tree_host.get(),
            second_grand_child->layer_tree_host());

  // Test over, cleanup time.
  first_layer_tree_host->SetRootLayer(nullptr);
  second_layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ReplaceMaskAndReplicaLayer) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> mask = Layer::Create();
  scoped_refptr<Layer> replica = Layer::Create();
  scoped_refptr<Layer> mask_child = Layer::Create();
  scoped_refptr<Layer> replica_child = Layer::Create();
  scoped_refptr<Layer> mask_replacement = Layer::Create();
  scoped_refptr<Layer> replica_replacement = Layer::Create();

  parent->SetMaskLayer(mask.get());
  parent->SetReplicaLayer(replica.get());
  mask->AddChild(mask_child);
  replica->AddChild(replica_child);

  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> layer_tree_host = factory.Create();
  layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Replacing the mask should clear out the old mask's subtree's host pointers.
  parent->SetMaskLayer(mask_replacement.get());
  EXPECT_EQ(nullptr, mask->layer_tree_host());
  EXPECT_EQ(nullptr, mask_child->layer_tree_host());

  // Same for replacing a replica layer.
  parent->SetReplicaLayer(replica_replacement.get());
  EXPECT_EQ(nullptr, replica->layer_tree_host());
  EXPECT_EQ(nullptr, replica_child->layer_tree_host());

  // Test over, cleanup time.
  layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, DestroyHostWithNonNullRootLayer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  root->AddChild(child);
  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> layer_tree_host = factory.Create();
  layer_tree_host->SetRootLayer(root);
}

TEST_F(LayerTest, SafeOpaqueBackgroundColor) {
  LayerTreeHostFactory factory;
  std::unique_ptr<LayerTreeHost> layer_tree_host = factory.Create();

  scoped_refptr<Layer> layer = Layer::Create();
  layer_tree_host->SetRootLayer(layer);

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SK_ColorRED
                                               : SK_ColorTRANSPARENT);
        layer_tree_host->set_background_color(
            host_opaque ? SK_ColorRED : SK_ColorTRANSPARENT);

        layer_tree_host->property_trees()->needs_rebuild = true;
        layer_tree_host->BuildPropertyTreesForTesting();
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
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(layer));

  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  layer->OnOpacityAnimated(0.5f);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  gfx::Transform transform;
  transform.Rotate(45.0);
  layer->OnTransformAnimated(transform);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Scroll offset animation should not schedule a layer update since it is
  // handled similarly to normal compositor scroll updates.
  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(0);
  layer->OnScrollOffsetAnimated(gfx::ScrollOffset(10, 10));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(LayerTest, RecursiveHierarchySerialization) {
  /* Testing serialization and deserialization of a tree that looks like this:
          root
          /  \
         a    b
               \
                c
     Layer c also has a mask layer and a replica layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_c_mask = Layer::Create();
  scoped_refptr<Layer> layer_src_c_replica = Layer::Create();
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  layer_src_c->SetMaskLayer(layer_src_c_mask.get());
  layer_src_c->SetReplicaLayer(layer_src_c_replica.get());

  proto::LayerNode proto;
  layer_src_root->ToLayerNodeProto(&proto);

  Layer::LayerIdMap empty_dest_layer_map;
  scoped_refptr<Layer> layer_dest_root = Layer::Create();
  layer_dest_root->FromLayerNodeProto(proto, empty_dest_layer_map,
                                      layer_tree_host_.get());

  EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
  EXPECT_EQ(nullptr, layer_dest_root->parent());
  ASSERT_EQ(2u, layer_dest_root->children().size());

  scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
  EXPECT_EQ(layer_src_a->id(), layer_dest_a->id());
  EXPECT_EQ(layer_src_root->id(), layer_dest_a->parent()->id());
  EXPECT_EQ(0u, layer_dest_a->children().size());

  scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
  EXPECT_EQ(layer_src_b->id(), layer_dest_b->id());
  EXPECT_EQ(layer_src_root->id(), layer_dest_b->parent()->id());
  ASSERT_EQ(1u, layer_dest_b->children().size());

  scoped_refptr<Layer> layer_dest_c = layer_dest_b->children()[0];
  EXPECT_EQ(layer_src_c->id(), layer_dest_c->id());
  EXPECT_EQ(layer_src_b->id(), layer_dest_c->parent()->id());
  EXPECT_EQ(0u, layer_dest_c->children().size());
  EXPECT_EQ(layer_src_c_mask->id(), layer_dest_c->mask_layer()->id());
  EXPECT_EQ(layer_src_c_replica->id(), layer_dest_c->replica_layer()->id());

  layer_dest_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, RecursiveHierarchySerializationWithNodeReuse) {
  /* Testing serialization and deserialization of a tree that initially looks
     like this:
          root
          /
         a
     The source tree is then updated by adding layer |b|:
          root
          /  \
         a    b
     The deserialization should then re-use the Layers from last
     deserialization.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  layer_src_root->AddChild(layer_src_a);

  proto::LayerNode root_proto_1;
  layer_src_root->ToLayerNodeProto(&root_proto_1);

  Layer::LayerIdMap dest_layer_map_1;
  scoped_refptr<Layer> layer_dest_root = Layer::Create();
  layer_dest_root->FromLayerNodeProto(root_proto_1, dest_layer_map_1,
                                      layer_tree_host_.get());

  EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
  ASSERT_EQ(1u, layer_dest_root->children().size());
  scoped_refptr<Layer> layer_dest_a_1 = layer_dest_root->children()[0];
  EXPECT_EQ(layer_src_a->id(), layer_dest_a_1->id());

  // Setup new destination layer map.
  Layer::LayerIdMap dest_layer_map_2;
  layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
      &dest_layer_map_2);

  // Add Layer |b|.
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  layer_src_root->AddChild(layer_src_b);

  // Second serialization.
  proto::LayerNode root_proto_2;
  layer_src_root->ToLayerNodeProto(&root_proto_2);

  // Second deserialization.
  layer_dest_root->FromLayerNodeProto(root_proto_2, dest_layer_map_2,
                                      layer_tree_host_.get());

  EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
  ASSERT_EQ(2u, layer_dest_root->children().size());

  scoped_refptr<Layer> layer_dest_a_2 = layer_dest_root->children()[0];
  EXPECT_EQ(layer_src_a->id(), layer_dest_a_2->id());
  EXPECT_EQ(layer_src_root->id(), layer_dest_a_2->parent()->id());
  EXPECT_EQ(0u, layer_dest_a_2->children().size());

  scoped_refptr<Layer> layer_dest_b_2 = layer_dest_root->children()[1];
  EXPECT_EQ(layer_src_b->id(), layer_dest_b_2->id());
  EXPECT_EQ(layer_src_root->id(), layer_dest_b_2->parent()->id());
  EXPECT_EQ(0u, layer_dest_b_2->children().size());

  // Layer |a| should be the same.
  EXPECT_EQ(layer_dest_a_1.get(), layer_dest_a_2.get());

  layer_dest_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, DeletingSubtreeDeletesLayers) {
  /* Testing serialization and deserialization of a tree that initially
     looks like this:
          root
          /  \
         a    b
               \
                c
                 \
                  d
     Then the subtree rooted at node |b| is deleted in the next update.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_d = Layer::Create();
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  layer_src_c->AddChild(layer_src_d);

  // Serialization 1.
  proto::LayerNode proto1;
  layer_src_root->ToLayerNodeProto(&proto1);

  // Deserialization 1.
  Layer::LayerIdMap empty_dest_layer_map;
  scoped_refptr<Layer> layer_dest_root = Layer::Create();
  layer_dest_root->FromLayerNodeProto(proto1, empty_dest_layer_map,
                                      layer_tree_host_.get());

  EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
  ASSERT_EQ(2u, layer_dest_root->children().size());
  scoped_refptr<Layer> layer_dest_a = layer_dest_root->children()[0];
  scoped_refptr<Layer> layer_dest_b = layer_dest_root->children()[1];
  ASSERT_EQ(1u, layer_dest_b->children().size());
  scoped_refptr<Layer> layer_dest_c = layer_dest_b->children()[0];
  ASSERT_EQ(1u, layer_dest_c->children().size());
  scoped_refptr<Layer> layer_dest_d = layer_dest_c->children()[0];

  // Delete the Layer |b| subtree.
  layer_src_b->RemoveAllChildren();

  // Serialization 2.
  proto::LayerNode proto2;
  layer_src_root->ToLayerNodeProto(&proto2);

  // Deserialization 2.
  Layer::LayerIdMap dest_layer_map_2;
  layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
      &dest_layer_map_2);
  layer_dest_root->FromLayerNodeProto(proto2, dest_layer_map_2,
                                      layer_tree_host_.get());

  EXPECT_EQ(0u, layer_dest_a->children().size());
  EXPECT_EQ(0u, layer_dest_b->children().size());

  layer_dest_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, DeleteMaskAndReplicaLayer) {
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_mask = Layer::Create();
  scoped_refptr<Layer> layer_src_replica = Layer::Create();
  layer_src_root->SetMaskLayer(layer_src_mask.get());
  layer_src_root->SetReplicaLayer(layer_src_replica.get());

  // Serialization 1.
  proto::LayerNode proto1;
  layer_src_root->ToLayerNodeProto(&proto1);

  // Deserialization 1.
  Layer::LayerIdMap dest_layer_map;
  scoped_refptr<Layer> layer_dest_root = Layer::Create();
  layer_dest_root->FromLayerNodeProto(proto1, dest_layer_map,
                                      layer_tree_host_.get());

  EXPECT_EQ(layer_src_root->id(), layer_dest_root->id());
  ASSERT_TRUE(layer_dest_root->mask_layer());
  ASSERT_TRUE(layer_dest_root->replica_layer());
  EXPECT_EQ(layer_src_root->mask_layer()->id(),
            layer_dest_root->mask_layer()->id());
  // TODO(nyquist): Add test for is_mask_ when PictureLayer is supported.
  EXPECT_EQ(layer_src_root->replica_layer()->id(),
            layer_dest_root->replica_layer()->id());

  // Clear mask and replica layers.
  layer_src_root->mask_layer()->RemoveFromParent();
  layer_src_root->replica_layer()->RemoveFromParent();

  // Serialization 2.
  proto::LayerNode proto2;
  layer_src_root->ToLayerNodeProto(&proto2);

  // Deserialization 2.
  layer_dest_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
      &dest_layer_map);
  layer_dest_root->FromLayerNodeProto(proto2, dest_layer_map,
                                      layer_tree_host_.get());

  EXPECT_EQ(nullptr, layer_dest_root->mask_layer());
  EXPECT_EQ(nullptr, layer_dest_root->replica_layer());

  layer_dest_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerSerializationTest, HierarchyDeserializationWithLayerTreeHost) {
  RunHierarchyDeserializationWithLayerTreeHostTest();
}

TEST_F(LayerSerializationTest, NonDestructiveDeserializationBaseCase) {
  RunNonDestructiveDeserializationBaseCaseTest();
}

TEST_F(LayerSerializationTest, NonDestructiveDeserializationReorderChildren) {
  RunNonDestructiveDeserializationReorderChildrenTest();
}

TEST_F(LayerSerializationTest, NonDestructiveDeserializationAddChild) {
  RunNonDestructiveDeserializationAddChildTest();
}

TEST_F(LayerSerializationTest, NonDestructiveDeserializationRemoveChild) {
  RunNonDestructiveDeserializationRemoveChildTest();
}

TEST_F(LayerSerializationTest,
       NonDestructiveDeserializationMoveChildEarlierTest) {
  RunNonDestructiveDeserializationMoveChildEarlierTest();
}

TEST_F(LayerSerializationTest,
       NonDestructiveDeserializationMoveChildLaterTest) {
  RunNonDestructiveDeserializationMoveChildLaterTest();
}

TEST_F(LayerSerializationTest, NoMembersChanged) {
  RunNoMembersChangedTest();
}

TEST_F(LayerSerializationTest, ArbitraryMembersChanged) {
  RunArbitraryMembersChangedTest();
}

TEST_F(LayerSerializationTest, AllMembersChanged) {
  RunAllMembersChangedTest();
}

TEST_F(LayerSerializationTest, ScrollAndClipLayers) {
  RunScrollAndClipLayersTest();
}

TEST_F(LayerSerializationTest, SolidColorScrollbarSerialization) {
  std::vector<scoped_refptr<SolidColorScrollbarLayer>> scrollbar_layers;

  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::HORIZONTAL, 20, 5, true, 3));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::VERTICAL, 20, 5, false, 3));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::HORIZONTAL, 0, 0, true, 0));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::VERTICAL, 10, 35, true, 3));

  for (size_t i = 0; i < scrollbar_layers.size(); i++) {
    VerifySolidColorScrollbarLayerAfterSerializationAndDeserialization(
        scrollbar_layers[i]);
  }
}

TEST_F(LayerTest, ElementIdAndMutablePropertiesArePushed) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

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
