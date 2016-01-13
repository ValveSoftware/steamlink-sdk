// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                       \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->needs_push_properties());                                  \
  EXPECT_FALSE(child->needs_push_properties());                                \
  EXPECT_FALSE(grand_child->needs_push_properties());                          \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_TRUE(child->LayerPropertyChanged());                                  \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(code_to_test)                \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_FALSE(root->needs_push_properties());                                 \
  EXPECT_FALSE(child->needs_push_properties());                                \
  EXPECT_FALSE(grand_child->needs_push_properties());                          \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(   \
      code_to_test)                                                            \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->needs_push_properties());                                  \
  EXPECT_FALSE(child->needs_push_properties());                                \
  EXPECT_FALSE(grand_child->needs_push_properties());                          \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)                    \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->needs_push_properties());                                  \
  EXPECT_FALSE(child->needs_push_properties());                                \
  EXPECT_FALSE(grand_child->needs_push_properties());                          \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                      \
  root->ResetAllChangeTrackingForSubtree();                                    \
  host_impl.ForcePrepareToDraw();                                              \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());       \
  code_to_test;                                                                \
  EXPECT_TRUE(host_impl.active_tree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                   \
  root->ResetAllChangeTrackingForSubtree();                                    \
  host_impl.ForcePrepareToDraw();                                              \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());       \
  code_to_test;                                                                \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());

TEST(LayerImplTest, VerifyLayerChangesAreTrackedProperly) {
  //
  // This test checks that layerPropertyChanged() has the correct behavior.
  //

  // The constructor on this will fake that we are on the correct thread.
  // Create a simple LayerImpl tree:
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  EXPECT_TRUE(host_impl.InitializeRenderer(
      FakeOutputSurface::Create3d().PassAs<OutputSurface>()));
  scoped_ptr<LayerImpl> root_clip =
      LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* root = root_ptr.get();
  root_clip->AddChild(root_ptr.Pass());
  scoped_ptr<LayerImpl> scroll_parent =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* scroll_child = LayerImpl::Create(host_impl.active_tree(), 4).get();
  std::set<LayerImpl*>* scroll_children = new std::set<LayerImpl*>();
  scroll_children->insert(scroll_child);
  scroll_children->insert(root);

  scoped_ptr<LayerImpl> clip_parent =
      LayerImpl::Create(host_impl.active_tree(), 5);
  LayerImpl* clip_child = LayerImpl::Create(host_impl.active_tree(), 6).get();
  std::set<LayerImpl*>* clip_children = new std::set<LayerImpl*>();
  clip_children->insert(clip_child);
  clip_children->insert(root);

  root->AddChild(LayerImpl::Create(host_impl.active_tree(), 7));
  LayerImpl* child = root->children()[0];
  child->AddChild(LayerImpl::Create(host_impl.active_tree(), 8));
  LayerImpl* grand_child = child->children()[0];

  root->SetScrollClipLayer(root_clip->id());

  // Adding children is an internal operation and should not mark layers as
  // changed.
  EXPECT_FALSE(root->LayerPropertyChanged());
  EXPECT_FALSE(child->LayerPropertyChanged());
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  gfx::Point3F arbitrary_point_3f = gfx::Point3F(0.125f, 0.25f, 0.f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  gfx::RectF arbitrary_rect_f =
      gfx::RectF(arbitrary_point_f, gfx::SizeF(1.234f, 5.678f));
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

  // These properties are internal, and should not be considered "change" when
  // they are used.
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetUpdateRect(arbitrary_rect_f));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(arbitrary_size));

  // Changing these properties affects the entire subtree of layers.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetTransformOrigin(arbitrary_point_3f));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(FilterOperations()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 9)));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetReplicaLayer(LayerImpl::Create(host_impl.active_tree(), 10)));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetPosition(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetShouldFlattenTransform(false));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->Set3dSortingContextId(1));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->ScrollBy(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollDelta(gfx::Vector2d()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollOffset(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetHideLayerAndSubtree(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetTransform(arbitrary_transform));

  // Changing these properties only affects the layer itself.
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundColor(arbitrary_color));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundFilters(arbitrary_filters));

  // Special case: check that SetBounds changes behavior depending on
  // masksToBounds.
  root->SetMasksToBounds(false);
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(gfx::Size(135, 246)));
  root->SetMasksToBounds(true);
  // Should be a different size than previous call, to ensure it marks tree
  // changed.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBounds(arbitrary_size));

  // Changing this property does not cause the layer to be marked as changed
  // but does cause the layer to need to push properties.
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetIsRootForIsolatedGroup(true));

  // Changing these properties should cause the layer to need to push properties
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollParent(scroll_parent.get()));
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollChildren(scroll_children));
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetClipParent(clip_parent.get()));
  EXECUTE_AND_VERIFY_NEEDS_PUSH_PROPERTIES_AND_SUBTREE_DID_NOT_CHANGE(
      root->SetClipChildren(clip_children));

  // After setting all these properties already, setting to the exact same
  // values again should not cause any change.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetTransformOrigin(arbitrary_point_3f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetPosition(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetShouldFlattenTransform(false));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->Set3dSortingContextId(1));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetTransform(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollDelta(gfx::Vector2d()));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollOffset(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetIsRootForIsolatedGroup(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollParent(scroll_parent.get()));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollChildren(scroll_children));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetClipParent(clip_parent.get()));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetClipChildren(clip_children));
}

TEST(LayerImplTest, VerifyNeedsUpdateDrawProperties) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  EXPECT_TRUE(host_impl.InitializeRenderer(
      FakeOutputSurface::Create3d().PassAs<OutputSurface>()));
  host_impl.active_tree()->SetRootLayer(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* root = host_impl.active_tree()->root_layer();
  scoped_ptr<LayerImpl> layer_ptr =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* layer = layer_ptr.get();
  root->AddChild(layer_ptr.Pass());
  layer->SetScrollClipLayer(root->id());
  DCHECK(host_impl.CanDraw());

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Size large_size = gfx::Size(1000, 1000);
  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  gfx::RectF arbitrary_rect_f =
      gfx::RectF(arbitrary_point_f, gfx::SizeF(1.234f, 5.678f));
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

  // Related filter functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(FilterOperations()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(arbitrary_filters));

  // Related scrolling functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(gfx::Vector2d()));
  layer->SetScrollDelta(gfx::Vector2d(0, 0));
  host_impl.ForcePrepareToDraw();
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetScrollDelta(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetScrollDelta(arbitrary_vector2d));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetScrollOffset(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetScrollOffset(arbitrary_vector2d));

  // Unrelated functions, always set to new values, always set needs update.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 4)));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetMasksToBounds(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetReplicaLayer(LayerImpl::Create(host_impl.active_tree(), 5)));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetPosition(arbitrary_point_f));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetShouldFlattenTransform(false));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->Set3dSortingContextId(1));

  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetDoubleSided(false));  // constructor initializes it to "true".
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentBounds(arbitrary_size));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetOpacity(arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBlendMode(arbitrary_blend_mode));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size));

  // Unrelated functions, set to the same values, no needs update.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetIsRootForIsolatedGroup(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetMasksToBounds(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetPosition(arbitrary_point_f));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->Set3dSortingContextId(1));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetDoubleSided(false));  // constructor initializes it to "true".
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetContentBounds(arbitrary_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetOpacity(arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBlendMode(arbitrary_blend_mode));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetIsRootForIsolatedGroup(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size));
}

TEST(LayerImplTest, SafeOpaqueBackgroundColor) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  EXPECT_TRUE(host_impl.InitializeRenderer(
      FakeOutputSurface::Create3d().PassAs<OutputSurface>()));
  scoped_ptr<LayerImpl> layer = LayerImpl::Create(host_impl.active_tree(), 1);

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SK_ColorRED
                                               : SK_ColorTRANSPARENT);
        host_impl.active_tree()->set_background_color(
            host_opaque ? SK_ColorRED : SK_ColorTRANSPARENT);

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

TEST(LayerImplTest, TransformInvertibility) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);

  scoped_ptr<LayerImpl> layer = LayerImpl::Create(host_impl.active_tree(), 1);
  EXPECT_TRUE(layer->transform().IsInvertible());
  EXPECT_TRUE(layer->transform_is_invertible());

  gfx::Transform transform;
  transform.Scale3d(
      SkDoubleToMScalar(1.0), SkDoubleToMScalar(1.0), SkDoubleToMScalar(0.0));
  layer->SetTransform(transform);
  EXPECT_FALSE(layer->transform().IsInvertible());
  EXPECT_FALSE(layer->transform_is_invertible());

  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(SkDoubleToMScalar(100.0));
  transform.RotateAboutZAxis(75.0);
  transform.RotateAboutXAxis(32.2);
  transform.RotateAboutZAxis(-75.0);
  transform.Translate3d(SkDoubleToMScalar(50.5),
                        SkDoubleToMScalar(42.42),
                        SkDoubleToMScalar(-100.25));

  layer->SetTransform(transform);
  EXPECT_TRUE(layer->transform().IsInvertible());
  EXPECT_TRUE(layer->transform_is_invertible());
}

class LayerImplScrollTest : public testing::Test {
 public:
  LayerImplScrollTest()
      : host_impl_(&proxy_, &shared_bitmap_manager_), root_id_(7) {
    host_impl_.active_tree()->SetRootLayer(
        LayerImpl::Create(host_impl_.active_tree(), root_id_));
    host_impl_.active_tree()->root_layer()->AddChild(
        LayerImpl::Create(host_impl_.active_tree(), root_id_ + 1));
    layer()->SetScrollClipLayer(root_id_);
    // Set the max scroll offset by noting that the root layer has bounds (1,1),
    // thus whatever bounds are set for the layer will be the max scroll
    // offset plus 1 in each direction.
    host_impl_.active_tree()->root_layer()->SetBounds(gfx::Size(1, 1));
    gfx::Vector2d max_scroll_offset(51, 81);
    layer()->SetBounds(gfx::Size(max_scroll_offset.x(), max_scroll_offset.y()));
  }

  LayerImpl* layer() {
    return host_impl_.active_tree()->root_layer()->children()[0];
  }

 private:
  FakeImplProxy proxy_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakeLayerTreeHostImpl host_impl_;
  int root_id_;
};

TEST_F(LayerImplScrollTest, ScrollByWithZeroOffset) {
  // Test that LayerImpl::ScrollBy only affects ScrollDelta and total scroll
  // offset is bounded by the range [0, max scroll offset].

  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());
}

TEST_F(LayerImplScrollTest, ScrollByWithNonZeroOffset) {
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta() + scroll_offset,
                   layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta() + scroll_offset,
                   layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

class ScrollDelegateIgnore : public LayerImpl::ScrollOffsetDelegate {
 public:
  virtual void SetTotalScrollOffset(const gfx::Vector2dF& new_value) OVERRIDE {}
  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return fixed_offset_;
  }
  virtual bool IsExternalFlingActive() const OVERRIDE { return false; }

  void set_fixed_offset(const gfx::Vector2dF& fixed_offset) {
    fixed_offset_ = fixed_offset;
  }

 private:
  gfx::Vector2dF fixed_offset_;
};

TEST_F(LayerImplScrollTest, ScrollByWithIgnoringDelegate) {
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  ScrollDelegateIgnore delegate;
  gfx::Vector2dF fixed_offset(32, 12);
  delegate.set_fixed_offset(fixed_offset);
  layer()->SetScrollOffsetDelegate(&delegate);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->SetScrollOffsetDelegate(NULL);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  gfx::Vector2dF scroll_delta(1, 1);
  layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(fixed_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

class ScrollDelegateAccept : public LayerImpl::ScrollOffsetDelegate {
 public:
  virtual void SetTotalScrollOffset(const gfx::Vector2dF& new_value) OVERRIDE {
    current_offset_ = new_value;
  }
  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return current_offset_;
  }
  virtual bool IsExternalFlingActive() const OVERRIDE { return false; }

 private:
  gfx::Vector2dF current_offset_;
};

TEST_F(LayerImplScrollTest, ScrollByWithAcceptingDelegate) {
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  ScrollDelegateAccept delegate;
  layer()->SetScrollOffsetDelegate(&delegate);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->SetScrollOffsetDelegate(NULL);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  gfx::Vector2dF scroll_delta(1, 1);
  layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(1, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsNoDelegate) {
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);
  gfx::Vector2d sent_scroll_delta(12, -3);

  layer()->SetScrollOffset(scroll_offset);
  layer()->ScrollBy(scroll_delta);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta, layer()->ScrollDelta());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta - sent_scroll_delta, layer()->ScrollDelta());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsWithIgnoringDelegate) {
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2d sent_scroll_delta(12, -3);
  gfx::Vector2dF fixed_offset(32, 12);

  layer()->SetScrollOffset(scroll_offset);
  ScrollDelegateIgnore delegate;
  delegate.set_fixed_offset(fixed_offset);
  layer()->SetScrollOffsetDelegate(&delegate);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsWithAcceptingDelegate) {
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2d sent_scroll_delta(12, -3);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  layer()->SetScrollOffset(scroll_offset);
  ScrollDelegateAccept delegate;
  layer()->SetScrollOffsetDelegate(&delegate);
  layer()->ScrollBy(scroll_delta);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

// The user-scrollability breaks for zoomed-in pages. So disable this.
// http://crbug.com/322223
TEST_F(LayerImplScrollTest, DISABLED_ScrollUserUnscrollableLayer) {
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  layer()->set_user_scrollable_vertical(false);
  layer()->SetScrollOffset(scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 8.5f), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30.5f, 5), layer()->TotalScrollOffset());
}

}  // namespace
}  // namespace cc
