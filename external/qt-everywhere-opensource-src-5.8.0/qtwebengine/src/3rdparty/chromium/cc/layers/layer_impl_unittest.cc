// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "cc/animation/mutable_properties.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                    \
  root->layer_tree_impl()->ResetAllChangeTracking();                        \
  code_to_test;                                                             \
  EXPECT_TRUE(                                                              \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(root));   \
  EXPECT_FALSE(                                                             \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(child));  \
  EXPECT_FALSE(root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting( \
      grand_child));                                                        \
  EXPECT_TRUE(root->LayerPropertyChanged());                                \
  EXPECT_TRUE(child->LayerPropertyChanged());                               \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(code_to_test)             \
  root->layer_tree_impl()->ResetAllChangeTracking();                        \
  code_to_test;                                                             \
  EXPECT_FALSE(                                                             \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(root));   \
  EXPECT_FALSE(                                                             \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(child));  \
  EXPECT_FALSE(root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting( \
      grand_child));                                                        \
  EXPECT_FALSE(root->LayerPropertyChanged());                               \
  EXPECT_FALSE(child->LayerPropertyChanged());                              \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE( \
    code_to_test)                                                            \
  root->layer_tree_impl()->ResetAllChangeTracking();                         \
  code_to_test;                                                              \
  EXPECT_TRUE(                                                               \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(root));    \
  EXPECT_FALSE(                                                              \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(child));   \
  EXPECT_FALSE(root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(  \
      grand_child));                                                         \
  EXPECT_FALSE(root->LayerPropertyChanged());                                \
  EXPECT_FALSE(child->LayerPropertyChanged());                               \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)                 \
  root->layer_tree_impl()->ResetAllChangeTracking();                        \
  root->layer_tree_impl()->property_trees()->full_tree_damaged = false;     \
  code_to_test;                                                             \
  EXPECT_TRUE(                                                              \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(root));   \
  EXPECT_FALSE(                                                             \
      root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting(child));  \
  EXPECT_FALSE(root->layer_tree_impl()->LayerNeedsPushPropertiesForTesting( \
      grand_child));                                                        \
  EXPECT_TRUE(root->LayerPropertyChanged());                                \
  EXPECT_FALSE(child->LayerPropertyChanged());                              \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                \
  root->layer_tree_impl()->ResetAllChangeTracking();                     \
  host_impl.active_tree()->property_trees()->needs_rebuild = true;       \
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();   \
  host_impl.ForcePrepareToDraw();                                        \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties()); \
  code_to_test;                                                          \
  EXPECT_TRUE(host_impl.active_tree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)             \
  root->layer_tree_impl()->ResetAllChangeTracking();                     \
  host_impl.active_tree()->property_trees()->needs_rebuild = true;       \
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();   \
  host_impl.ForcePrepareToDraw();                                        \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties()); \
  code_to_test;                                                          \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());

static gfx::Vector2dF ScrollDelta(LayerImpl* layer_impl) {
  gfx::ScrollOffset delta =
      layer_impl->layer_tree_impl()
          ->property_trees()
          ->scroll_tree.GetScrollOffsetDeltaForTesting(layer_impl->id());
  return gfx::Vector2dF(delta.x(), delta.y());
}

TEST(LayerImplTest, VerifyLayerChangesAreTrackedProperly) {
  //
  // This test checks that layerPropertyChanged() has the correct behavior.
  //

  // The constructor on this will fake that we are on the correct thread.
  // Create a simple LayerImpl tree:
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));
  std::unique_ptr<LayerImpl> root_clip_ptr =
      LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root_clip = root_clip_ptr.get();
  std::unique_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* root = root_ptr.get();
  root_clip_ptr->test_properties()->AddChild(std::move(root_ptr));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root_clip_ptr));

  root->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->ResetAllChangeTracking();

  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl.active_tree(), 7));
  LayerImpl* child = root->test_properties()->children[0];
  child->test_properties()->AddChild(
      LayerImpl::Create(host_impl.active_tree(), 8));
  LayerImpl* grand_child = child->test_properties()->children[0];
  root->SetScrollClipLayer(root_clip->id());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();

  // Adding children is an internal operation and should not mark layers as
  // changed.
  EXPECT_FALSE(root->LayerPropertyChanged());
  EXPECT_FALSE(child->LayerPropertyChanged());
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

  // These properties are internal, and should not be considered "change" when
  // they are used.
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetUpdateRect(arbitrary_rect));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(arbitrary_size));
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();

  // Changing these properties affects the entire subtree of layers.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->OnFilterAnimated(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->OnFilterAnimated(FilterOperations()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->OnOpacityAnimated(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->OnTransformAnimated(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->ScrollBy(arbitrary_vector2d);
                                     root->SetNeedsPushProperties());
  // SetBoundsDelta changes subtree only when masks_to_bounds is true and it
  // doesn't set needs_push_properties as it is always called on active tree.
  root->SetMasksToBounds(true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBoundsDelta(arbitrary_vector2d);
                                     root->SetNeedsPushProperties());

  // Changing these properties only affects the layer itself.
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundColor(arbitrary_color));

  // Special case: check that SetBounds changes behavior depending on
  // masksToBounds.
  gfx::Size bounds_size(135, 246);
  root->SetMasksToBounds(false);
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(bounds_size));
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();

  root->SetMasksToBounds(true);
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();

  // Changing these properties does not cause the layer to be marked as changed
  // but does cause the layer to need to push properties.
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetElementId(ElementId(2, 0)));
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetMutableProperties(MutableProperty::kOpacity);
      root->SetNeedsPushProperties());

  // After setting all these properties already, setting to the exact same
  // values again should not cause any change.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetPosition(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->Set3dSortingContextId(1));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetTransform(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetBounds(bounds_size));
}

TEST(LayerImplTest, VerifyNeedsUpdateDrawProperties) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));
  host_impl.active_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* root = host_impl.active_tree()->root_layer_for_testing();
  root->SetHasRenderSurface(true);
  std::unique_ptr<LayerImpl> layer_ptr =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* layer = layer_ptr.get();
  root->test_properties()->AddChild(std::move(layer_ptr));
  layer->SetScrollClipLayer(root->id());
  std::unique_ptr<LayerImpl> layer2_ptr =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* layer2 = layer2_ptr.get();
  root->test_properties()->AddChild(std::move(layer2_ptr));
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  DCHECK(host_impl.CanDraw());

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Size large_size = gfx::Size(1000, 1000);
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

  // Set layer to draw content so that their draw property by property trees is
  // verified.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer2->SetDrawsContent(true));
  // Render surface functions should not trigger update draw properties, because
  // creating render surface is part of update draw properties.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetHasRenderSurface(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetHasRenderSurface(false));
  // Create a render surface, because we must have a render surface if we have
  // filters.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetHasRenderSurface(true));

  // Related filter functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->OnFilterAnimated(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->OnFilterAnimated(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->OnFilterAnimated(FilterOperations()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->OnFilterAnimated(arbitrary_filters));

  // Related scrolling functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(gfx::Vector2d()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->layer_tree_impl()->DidUpdateScrollOffset(
          layer->id(), layer->transform_tree_index()));
  layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.SetScrollOffsetDeltaForTesting(layer->id(),
                                                   gfx::Vector2dF());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetCurrentScrollOffset(
      gfx::ScrollOffset(arbitrary_vector2d.x(), arbitrary_vector2d.y())));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetCurrentScrollOffset(
      gfx::ScrollOffset(arbitrary_vector2d.x(), arbitrary_vector2d.y())));

  // Unrelated functions, always set to new values, always set needs update.
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetMasksToBounds(true);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer2->SetPosition(arbitrary_point_f);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->Set3dSortingContextId(1);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->OnOpacityAnimated(arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBlendMode(arbitrary_blend_mode);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->OnTransformAnimated(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size);
                                      layer->NoteLayerPropertyChanged());

  // Unrelated functions, set to the same values, no needs update.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetMasksToBounds(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer2->SetPosition(arbitrary_point_f));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->Set3dSortingContextId(1));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBlendMode(arbitrary_blend_mode));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetElementId(ElementId(2, 0)));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetMutableProperties(MutableProperty::kTransform));
}

TEST(LayerImplTest, SafeOpaqueBackgroundColor) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));
  host_impl.active_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* layer = host_impl.active_tree()->root_layer_for_testing();

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SK_ColorRED
                                               : SK_ColorTRANSPARENT);
        host_impl.active_tree()->set_background_color(
            host_opaque ? SK_ColorRED : SK_ColorTRANSPARENT);
        host_impl.active_tree()->property_trees()->needs_rebuild = true;
        host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();

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

class LayerImplScrollTest : public testing::Test {
 public:
  LayerImplScrollTest()
      : host_impl_(settings(),
                   &task_runner_provider_,
                   &shared_bitmap_manager_,
                   &task_graph_runner_),
        root_id_(7) {
    host_impl_.active_tree()->SetRootLayerForTesting(
        LayerImpl::Create(host_impl_.active_tree(), root_id_));
    host_impl_.active_tree()
        ->root_layer_for_testing()
        ->test_properties()
        ->AddChild(LayerImpl::Create(host_impl_.active_tree(), root_id_ + 1));
    layer()->SetScrollClipLayer(root_id_);
    // Set the max scroll offset by noting that the root layer has bounds (1,1),
    // thus whatever bounds are set for the layer will be the max scroll
    // offset plus 1 in each direction.
    host_impl_.active_tree()->root_layer_for_testing()->SetBounds(
        gfx::Size(1, 1));
    gfx::Vector2d max_scroll_offset(51, 81);
    layer()->SetBounds(gfx::Size(max_scroll_offset.x(), max_scroll_offset.y()));
    host_impl_.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  }

  LayerImpl* layer() {
    return host_impl_.active_tree()
        ->root_layer_for_testing()
        ->test_properties()
        ->children[0];
  }

  ScrollTree* scroll_tree(LayerImpl* layer_impl) {
    return &layer_impl->layer_tree_impl()->property_trees()->scroll_tree;
  }

  LayerTreeHostImpl& host_impl() { return host_impl_; }

  LayerTreeImpl* tree() { return host_impl_.active_tree(); }

  LayerTreeSettings settings() {
    LayerTreeSettings settings;
    settings.verify_clip_tree_calculations = true;
    return settings;
  }

 private:
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  int root_id_;
};

TEST_F(LayerImplScrollTest, ScrollByWithZeroOffset) {
  // Test that LayerImpl::ScrollBy only affects ScrollDelta and total scroll
  // offset is bounded by the range [0, max scroll offset].

  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(),
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), ScrollDelta(layer()));

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->CurrentScrollOffset());

  EXPECT_VECTOR_EQ(ScrollDelta(layer()), layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(),
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->CurrentScrollOffset());

  EXPECT_VECTOR_EQ(ScrollDelta(layer()), layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(),
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));
}

TEST_F(LayerImplScrollTest, ScrollByWithNonZeroOffset) {
  gfx::ScrollOffset scroll_offset(10, 5);
  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->id(),
                                                         scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      scroll_offset,
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), ScrollDelta(layer()));

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->CurrentScrollOffset());

  EXPECT_VECTOR_EQ(
      gfx::ScrollOffsetWithDelta(scroll_offset, ScrollDelta(layer())),
      layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      scroll_offset,
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->CurrentScrollOffset());

  EXPECT_VECTOR_EQ(
      gfx::ScrollOffsetWithDelta(scroll_offset, ScrollDelta(layer())),
      layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(
      scroll_offset,
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));
}

TEST_F(LayerImplScrollTest, ApplySentScrollsNoListener) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);
  gfx::Vector2d sent_scroll_delta(12, -3);

  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->id(),
                                                         scroll_offset);
  layer()->ScrollBy(sent_scroll_delta);
  scroll_tree(layer())->CollectScrollDeltasForTesting();
  layer()->SetCurrentScrollOffset(scroll_offset +
                                  gfx::ScrollOffset(scroll_delta));

  EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(scroll_offset, scroll_delta),
                   layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta, ScrollDelta(layer()));
  EXPECT_VECTOR_EQ(
      scroll_offset,
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));

  scroll_tree(layer())->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(scroll_offset, scroll_delta),
                   layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta - sent_scroll_delta, ScrollDelta(layer()));
  EXPECT_VECTOR_EQ(
      gfx::ScrollOffsetWithDelta(scroll_offset, sent_scroll_delta),
      scroll_tree(layer())->GetScrollOffsetBaseForTesting(layer()->id()));
}

TEST_F(LayerImplScrollTest, ScrollUserUnscrollableLayer) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  layer()->set_user_scrollable_vertical(false);
  layer()->layer_tree_impl()->property_trees()->needs_rebuild = true;
  layer()->layer_tree_impl()->BuildLayerListAndPropertyTreesForTesting();
  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->id(),
                                                         scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 8.5f), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30.5f, 5), layer()->CurrentScrollOffset());
}

TEST_F(LayerImplScrollTest, PushPropertiesToMirrorsCurrentScrollOffset) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(12, 18);

  host_impl().CreatePendingTree();

  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->id(),
                                                         scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(22, 23), layer()->CurrentScrollOffset());

  scroll_tree(layer())->CollectScrollDeltasForTesting();

  std::unique_ptr<LayerImpl> pending_layer =
      LayerImpl::Create(host_impl().sync_tree(), layer()->id());
  scroll_tree(pending_layer.get())
      ->UpdateScrollOffsetBaseForTesting(pending_layer->id(),
                                         layer()->CurrentScrollOffset());

  pending_layer->PushPropertiesTo(layer());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(22, 23), layer()->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(layer()->CurrentScrollOffset(),
                   pending_layer->CurrentScrollOffset());
}

}  // namespace
}  // namespace cc
