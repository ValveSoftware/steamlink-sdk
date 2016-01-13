// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_host_common_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

class LayerTreeImplTest : public LayerTreeHostCommonTest {
 public:
  LayerTreeImplTest() {
    LayerTreeSettings settings;
    settings.layer_transforms_should_scale_layer_contents = true;
    host_impl_.reset(
        new FakeLayerTreeHostImpl(settings, &proxy_, &shared_bitmap_manager_));
    EXPECT_TRUE(host_impl_->InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>()));
  }

  FakeLayerTreeHostImpl& host_impl() { return *host_impl_; }

  LayerImpl* root_layer() { return host_impl_->active_tree()->root_layer(); }

  const LayerImplList& RenderSurfaceLayerList() const {
    return host_impl_->active_tree()->RenderSurfaceLayerList();
  }

 private:
  TestSharedBitmapManager shared_bitmap_manager_;
  FakeImplProxy proxy_;
  scoped_ptr<FakeLayerTreeHostImpl> host_impl_;
};

TEST_F(LayerTreeImplTest, HitTestingForSingleLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(1, 1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleLayerAndHud) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);
  scoped_ptr<HeadsUpDisplayLayerImpl> hud =
      HeadsUpDisplayLayerImpl::Create(host_impl().active_tree(), 11111);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  // Create hud and add it as a child of root.
  gfx::Size hud_bounds(200, 200);
  SetLayerPropertiesForTesting(hud.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               hud_bounds,
                               true,
                               false);
  hud->SetDrawsContent(true);

  host_impl().active_tree()->set_hud_layer(hud.get());
  root->AddChild(hud.PassAs<LayerImpl>());

  host_impl().SetViewportSize(hud_bounds);
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(2u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point inside HUD, but outside root should return null
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer, never the HUD
  // layer.
  test_point = gfx::Point(1, 1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForUninvertibleTransform) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_transform,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();
  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_FALSE(root_layer()->screen_space_transform().IsInvertible());

  // Hit testing any point should not hit the layer. If the invertible matrix is
  // accidentally ignored and treated like an identity, then the hit testing
  // will incorrectly hit the layer when it shouldn't.
  gfx::Point test_point(1, 1);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 10);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 30);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(50, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(67, 48);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePositionedLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(49, 49);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the root render surface would clamp it.
  test_point = gfx::Point(101, 101);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(51, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleRotatedLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Transform rotation45_degrees_about_center;
  rotation45_degrees_about_center.Translate(50.0, 50.0);
  rotation45_degrees_about_center.RotateAboutZAxis(45.0);
  rotation45_degrees_about_center.Translate(-50.0, -50.0);
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               rotation45_degrees_about_center,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::Point test_point(99, 99);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(1, 1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(1, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  // Hit testing the corners that would overlap the unclipped layer, but are
  // outside the clipped region.
  test_point = gfx::Point(50, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);

  test_point = gfx::Point(-1, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePerspectiveLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;

  // perspective_projection_about_center * translation_by_z is designed so that
  // the 100 x 100 layer becomes 50 x 50, and remains centered at (50, 50).
  gfx::Transform perspective_projection_about_center;
  perspective_projection_about_center.Translate(50.0, 50.0);
  perspective_projection_about_center.ApplyPerspectiveDepth(1.0);
  perspective_projection_about_center.Translate(-50.0, -50.0);
  gfx::Transform translation_by_z;
  translation_by_z.Translate3d(0.0, 0.0, -1.0);

  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(
      root.get(),
      perspective_projection_about_center * translation_by_z,
      transform_origin,
      position,
      bounds,
      true,
      false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(76, 76);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(26, 26);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleLayerWithScaledContents) {
  // A layer's visible content rect is actually in the layer's content space.
  // The screen space transform converts from the layer's origin space to screen
  // space. This test makes sure that hit testing works correctly accounts for
  // the contents scale.  A contents scale that is not 1 effectively forces a
  // non-identity transform between layer's content space and layer's origin
  // space. The hit testing code must take this into account.
  //
  // To test this, the layer is positioned at (25, 25), and is size (50, 50). If
  // contents scale is ignored, then hit testing will mis-interpret the visible
  // content rect as being larger than the actual bounds of the layer.
  //
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl().active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);

    // override content bounds and contents scale
    test_layer->SetContentBounds(gfx::Size(100, 100));
    test_layer->SetContentsScale(2, 2);

    test_layer->SetDrawsContent(true);
    root->AddChild(test_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer =
      host_impl().active_tree()->root_layer()->children()[0];
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), test_layer->visible_content_rect());
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer (the
  // root layer does not draw content, so it will not be hit tested either).
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(24, 24);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(76, 76);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the test layer.
  test_point = gfx::Point(26, 26);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForSimpleClippedLayer) {
  // Test that hit-testing will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    scoped_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    clipping_layer->SetMasksToBounds(true);

    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetDrawsContent(true);
    clipping_layer->AddChild(child.Pass());
    root->AddChild(clipping_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the clipping_layer would clamp it.
  test_point = gfx::Point(76, 76);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::Point(26, 26);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForMultiClippedRotatedLayer) {
  // This test checks whether hit testing correctly avoids hit testing with
  // multiple ancestors that clip in non axis-aligned ways. To pass this test,
  // the hit testing algorithm needs to recognize that multiple parent layers
  // may clip the layer, and should not actually hit those clipped areas.
  //
  // The child and grand_child layers are both initialized to clip the
  // rotated_leaf. The child layer is rotated about the top-left corner, so that
  // the root + child clips combined create a triangle. The rotated_leaf will
  // only be visible where it overlaps this triangle.
  //
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 123);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetMasksToBounds(true);
  {
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    scoped_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 789);
    scoped_ptr<LayerImpl> rotated_leaf =
        LayerImpl::Create(host_impl().active_tree(), 2468);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(80, 80);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetMasksToBounds(true);

    gfx::Transform rotation45_degrees_about_corner;
    rotation45_degrees_about_corner.RotateAboutZAxis(45.0);

    // remember, positioned with respect to its parent which is already at 10,
    // 10
    position = gfx::PointF();
    bounds =
        gfx::Size(200, 200);  // to ensure it covers at least sqrt(2) * 100.
    SetLayerPropertiesForTesting(grand_child.get(),
                                 rotation45_degrees_about_corner,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child->SetMasksToBounds(true);

    // Rotates about the center of the layer
    gfx::Transform rotated_leaf_transform;
    rotated_leaf_transform.Translate(
        -10.0, -10.0);  // cancel out the grand_parent's position
    rotated_leaf_transform.RotateAboutZAxis(
        -45.0);  // cancel out the corner 45-degree rotation of the parent.
    rotated_leaf_transform.Translate(50.0, 50.0);
    rotated_leaf_transform.RotateAboutZAxis(45.0);
    rotated_leaf_transform.Translate(-50.0, -50.0);
    position = gfx::PointF();
    bounds = gfx::Size(100, 100);
    SetLayerPropertiesForTesting(rotated_leaf.get(),
                                 rotated_leaf_transform,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    rotated_leaf->SetDrawsContent(true);

    grand_child->AddChild(rotated_leaf.Pass());
    child->AddChild(grand_child.Pass());
    root->AddChild(child.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  // The grand_child is expected to create a render surface because it
  // MasksToBounds and is not axis aligned.
  ASSERT_EQ(2u, RenderSurfaceLayerList().size());
  ASSERT_EQ(
      1u,
      RenderSurfaceLayerList().at(0)->render_surface()->layer_list().size());
  ASSERT_EQ(789,
            RenderSurfaceLayerList()
                .at(0)
                ->render_surface()
                ->layer_list()
                .at(0)
                ->id());  // grand_child's surface.
  ASSERT_EQ(
      1u,
      RenderSurfaceLayerList().at(1)->render_surface()->layer_list().size());
  ASSERT_EQ(
      2468,
      RenderSurfaceLayerList()[1]->render_surface()->layer_list().at(0)->id());

  // (11, 89) is close to the the bottom left corner within the clip, but it is
  // not inside the layer.
  gfx::Point test_point(11, 89);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Closer inwards from the bottom left will overlap the layer.
  test_point = gfx::Point(25, 75);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // (4, 50) is inside the unclipped layer, but that corner of the layer should
  // be clipped away by the grandparent and should not get hit. If hit testing
  // blindly uses visible content rect without considering how parent may clip
  // the layer, then hit testing would accidentally think that the point
  // successfully hits the layer.
  test_point = gfx::Point(4, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // (11, 50) is inside the layer and within the clipped area.
  test_point = gfx::Point(11, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // Around the middle, just to the right and up, would have hit the layer
  // except that that area should be clipped away by the parent.
  test_point = gfx::Point(51, 49);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Around the middle, just to the left and down, should successfully hit the
  // layer.
  test_point = gfx::Point(49, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForNonClippingIntermediateLayer) {
  // This test checks that hit testing code does not accidentally clip to layer
  // bounds for a layer that actually does not clip.
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    scoped_ptr<LayerImpl> intermediate_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(10.f, 10.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(intermediate_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    // Sanity check the intermediate layer should not clip.
    ASSERT_FALSE(intermediate_layer->masks_to_bounds());
    ASSERT_FALSE(intermediate_layer->mask_layer());

    // The child of the intermediate_layer is translated so that it does not
    // overlap intermediate_layer at all.  If child is incorrectly clipped, we
    // would not be able to hit it successfully.
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    position = gfx::PointF(60.f, 60.f);  // 70, 70 in screen space
    bounds = gfx::Size(20, 20);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetDrawsContent(true);
    intermediate_layer->AddChild(child.Pass());
    root->AddChild(intermediate_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(69, 69);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(91, 91);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::Point(71, 71);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(89, 89);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayers) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    scoped_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child1->SetDrawsContent(true);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child2->SetDrawsContent(true);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child1->SetDrawsContent(true);

    child1->AddChild(grand_child1.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());
  }

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = child1->children()[0];

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  RenderSurfaceImpl* root_render_surface = root_layer()->render_surface();
  ASSERT_EQ(4u, root_render_surface->layer_list().size());
  ASSERT_EQ(1, root_render_surface->layer_list().at(0)->id());  // root layer
  ASSERT_EQ(2, root_render_surface->layer_list().at(1)->id());  // child1
  ASSERT_EQ(4, root_render_surface->layer_list().at(2)->id());  // grand_child1
  ASSERT_EQ(3, root_render_surface->layer_list().at(3)->id());  // child2

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::Point test_point = gfx::Point(1, 1);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::Point(15, 15);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::Point(51, 20);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::Point(80, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::Point(51, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::Point(20, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayersAtVaryingDepths) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  root->SetShouldFlattenTransform(false);
  root->Set3dSortingContextId(1);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    scoped_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child1->SetDrawsContent(true);
    child1->SetShouldFlattenTransform(false);
    child1->Set3dSortingContextId(1);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, -10.f);
    SetLayerPropertiesForTesting(child2.get(),
                                 translate_z,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child2->SetDrawsContent(true);
    child2->SetShouldFlattenTransform(false);
    child2->Set3dSortingContextId(1);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child1->SetDrawsContent(true);
    grand_child1->SetShouldFlattenTransform(false);

    child1->AddChild(grand_child1.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());
  }

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = child1->children()[0];

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  RenderSurfaceImpl* root_render_surface =
      host_impl().active_tree()->root_layer()->render_surface();
  ASSERT_EQ(4u, root_render_surface->layer_list().size());
  ASSERT_EQ(3, root_render_surface->layer_list().at(0)->id());
  ASSERT_EQ(1, root_render_surface->layer_list().at(1)->id());
  ASSERT_EQ(2, root_render_surface->layer_list().at(2)->id());
  ASSERT_EQ(4, root_render_surface->layer_list().at(3)->id());

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::Point test_point = gfx::Point(1, 1);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::Point(15, 15);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  // (because 3 is transformed to the back).
  test_point = gfx::Point(51, 20);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // 3 Would have been on top if it hadn't been transformed to the background.
  // Make sure that it isn't hit.
  test_point = gfx::Point(80, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());

  // 3 Would have been on top if it hadn't been transformed to the background.
  // Make sure that it isn't hit.
  test_point = gfx::Point(51, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::Point(20, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingRespectsClipParents) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  {
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 2);
    scoped_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(1, 1);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetDrawsContent(true);
    child->SetMasksToBounds(true);

    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child->SetDrawsContent(true);
    grand_child->SetForceRenderSurface(true);

    // This should let |grand_child| "escape" |child|'s clip.
    grand_child->SetClipParent(root.get());

    child->AddChild(grand_child.Pass());
    root->AddChild(child.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  gfx::Point test_point = gfx::Point(12, 52);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingRespectsScrollParents) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  {
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 2);
    scoped_ptr<LayerImpl> scroll_child =
        LayerImpl::Create(host_impl().active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(1, 1);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetDrawsContent(true);
    child->SetMasksToBounds(true);

    position = gfx::PointF();
    bounds = gfx::Size(200, 200);
    SetLayerPropertiesForTesting(scroll_child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    scroll_child->SetDrawsContent(true);

    // This should cause scroll child and its descendants to be affected by
    // |child|'s clip.
    scroll_child->SetScrollParent(child.get());

    SetLayerPropertiesForTesting(grand_child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child->SetDrawsContent(true);
    grand_child->SetForceRenderSurface(true);

    scroll_child->AddChild(grand_child.Pass());
    root->AddChild(scroll_child.Pass());
    root->AddChild(child.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  gfx::Point test_point = gfx::Point(12, 52);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  // The |test_point| should have been clipped away by |child|, the scroll
  // parent, so the only thing that should be hit is |root|.
  ASSERT_TRUE(result_layer);
  ASSERT_EQ(1, result_layer->id());
}
TEST_F(LayerTreeImplTest, HitTestingForMultipleLayerLists) {
  //
  // The geometry is set up similarly to the previous case, but
  // all layers are forced to be render surfaces now.
  //
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    scoped_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child1->SetDrawsContent(true);
    child1->SetForceRenderSurface(true);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child2->SetDrawsContent(true);
    child2->SetForceRenderSurface(true);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    grand_child1->SetDrawsContent(true);
    grand_child1->SetForceRenderSurface(true);

    child1->AddChild(grand_child1.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());
  }

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = child1->children()[0];

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_TRUE(child1->render_surface());
  ASSERT_TRUE(child2->render_surface());
  ASSERT_TRUE(grand_child1->render_surface());
  ASSERT_EQ(4u, RenderSurfaceLayerList().size());
  // The root surface has the root layer, and child1's and child2's render
  // surfaces.
  ASSERT_EQ(3u, root_layer()->render_surface()->layer_list().size());
  // The child1 surface has the child1 layer and grand_child1's render surface.
  ASSERT_EQ(2u, child1->render_surface()->layer_list().size());
  ASSERT_EQ(1u, child2->render_surface()->layer_list().size());
  ASSERT_EQ(1u, grand_child1->render_surface()->layer_list().size());
  ASSERT_EQ(1, RenderSurfaceLayerList().at(0)->id());  // root layer
  ASSERT_EQ(2, RenderSurfaceLayerList()[1]->id());     // child1
  ASSERT_EQ(4, RenderSurfaceLayerList().at(2)->id());  // grand_child1
  ASSERT_EQ(3, RenderSurfaceLayerList()[3]->id());     // child2

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::Point test_point = gfx::Point(1, 1);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::Point(15, 15);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::Point(51, 20);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::Point(80, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::Point(51, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::Point(20, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerRegionsForSingleLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit checking for any point should return a null pointer for a layer without
  // any touch event handler regions.
  gfx::Point test_point(11, 11);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  host_impl().active_tree()->root_layer()->SetTouchEventHandlerRegion(
      touch_handler_region);
  // Hit checking for a point outside the layer should return a null pointer.
  test_point = gfx::Point(101, 101);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(1, 1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(11, 11);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(59, 59);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForUninvertibleTransform) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_transform,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_FALSE(root_layer()->screen_space_transform().IsInvertible());

  // Hit checking any point should not hit the touch handler region on the
  // layer. If the invertible matrix is accidentally ignored and treated like an
  // identity, then the hit testing will incorrectly hit the layer when it
  // shouldn't.
  gfx::Point test_point(1, 1);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 10);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 30);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(50, 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(67, 48);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSinglePositionedLayer) {
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::Point3F transform_origin;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               position,
                               bounds,
                               true,
                               false);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit checking for a point outside the layer should return a null pointer.
  gfx::Point test_point(49, 49);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer has a touch handler region containing (101, 101), it
  // should not be visible there since the root render surface would clamp it.
  test_point = gfx::Point(101, 101);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(51, 51);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(61, 61);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSingleLayerWithScaledContents) {
  // A layer's visible content rect is actually in the layer's content space.
  // The screen space transform converts from the layer's origin space to screen
  // space. This test makes sure that hit testing works correctly accounts for
  // the contents scale.  A contents scale that is not 1 effectively forces a
  // non-identity transform between layer's content space and layer's origin
  // space. The hit testing code must take this into account.
  //
  // To test this, the layer is positioned at (25, 25), and is size (50, 50). If
  // contents scale is ignored, then hit checking will mis-interpret the visible
  // content rect as being larger than the actual bounds of the layer.
  //
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl().active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);

    // override content bounds and contents scale
    test_layer->SetContentBounds(gfx::Size(100, 100));
    test_layer->SetContentsScale(2, 2);

    test_layer->SetDrawsContent(true);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->AddChild(test_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer =
      host_impl().active_tree()->root_layer()->children()[0];
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), test_layer->visible_content_rect());
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not draw content, so it will not be tested either).
  gfx::Point test_point(76, 76);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(26, 26);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(34, 34);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(65, 65);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(35, 35);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(64, 64);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSingleLayerWithDeviceScale) {
  // The layer's device_scale_factor and page_scale_factor should scale the
  // content rect and we should be able to hit the touch handler region by
  // scaling the points accordingly.
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  // Set the bounds of the root layer big enough to fit the child when scaled.
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl().active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);

    test_layer->SetDrawsContent(true);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->AddChild(test_layer.Pass());
  }

  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  gfx::Size scaled_bounds_for_root = gfx::ToCeiledSize(
      gfx::ScaleSize(root->bounds(), device_scale_factor * page_scale_factor));
  host_impl().SetViewportSize(scaled_bounds_for_root);

  host_impl().SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->SetPageScaleFactorAndLimits(
      page_scale_factor, page_scale_factor, page_scale_factor);
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->SetViewportLayersFromIds(1, 1, Layer::INVALID_ID);
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer =
      host_impl().active_tree()->root_layer()->children()[0];
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Check whether the child layer fits into the root after scaled.
  EXPECT_RECT_EQ(gfx::Rect(test_layer->content_bounds()),
                 test_layer->visible_content_rect());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not draw content, so it will not be tested either).
  gfx::PointF test_point(76.f, 76.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(26, 26);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(34, 34);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(65, 65);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(35, 35);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(64, 64);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerRegionsForSimpleClippedLayer) {
  // Test that hit-checking will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    scoped_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    clipping_layer->SetMasksToBounds(true);

    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    child->SetDrawsContent(true);
    child->SetTouchEventHandlerRegion(touch_handler_region);
    clipping_layer->AddChild(child.Pass());
    root->AddChild(clipping_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(35, 35);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(25, 25);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(34, 34);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerOverlappingRegions) {
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               transform_origin,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  {
    scoped_ptr<LayerImpl> touch_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position;
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(touch_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    touch_layer->SetDrawsContent(true);
    touch_layer->SetTouchEventHandlerRegion(gfx::Rect(0, 0, 50, 50));
    root->AddChild(touch_layer.Pass());
  }

  {
    scoped_ptr<LayerImpl> notouch_layer =
        LayerImpl::Create(host_impl().active_tree(), 1234);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(0, 25);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(notouch_layer.get(),
                                 identity_matrix,
                                 transform_origin,
                                 position,
                                 bounds,
                                 true,
                                 false);
    notouch_layer->SetDrawsContent(true);
    root->AddChild(notouch_layer.Pass());
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(root.Pass());
  host_impl().active_tree()->UpdateDrawProperties();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(2u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(123, root_layer()->render_surface()->layer_list().at(0)->id());
  ASSERT_EQ(1234, root_layer()->render_surface()->layer_list().at(1)->id());

  gfx::Point test_point(35, 35);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);

  // We should have passed through the no-touch layer and found the layer
  // behind it.
  EXPECT_TRUE(result_layer);

  host_impl().active_tree()->LayerById(1234)->SetContentsOpaque(true);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);

  // Even with an opaque layer in the middle, we should still find the layer
  // with
  // the touch handler behind it (since we can't assume that opaque layers are
  // opaque to hit testing).
  EXPECT_TRUE(result_layer);

  test_point = gfx::Point(35, 15);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(123, result_layer->id());

  test_point = gfx::Point(35, 65);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

}  // namespace
}  // namespace cc
