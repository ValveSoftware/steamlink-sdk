// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_host_common_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

class LayerTreeImplTest : public LayerTreeHostCommonTest {
 public:
  LayerTreeImplTest() : output_surface_(FakeOutputSurface::Create3d()) {
    LayerTreeSettings settings;
    settings.layer_transforms_should_scale_layer_contents = true;
    settings.verify_clip_tree_calculations = true;
    host_impl_.reset(new FakeLayerTreeHostImpl(settings, &task_runner_provider_,
                                               &shared_bitmap_manager_,
                                               &task_graph_runner_));
    host_impl_->SetVisible(true);
    EXPECT_TRUE(host_impl_->InitializeRenderer(output_surface_.get()));
  }

  FakeLayerTreeHostImpl& host_impl() { return *host_impl_; }

  LayerImpl* root_layer() {
    return host_impl_->active_tree()->root_layer_for_testing();
  }
  int HitTestSimpleTree(int root_id,
                        int left_child_id,
                        int right_child_id,
                        int root_sorting_context,
                        int left_child_sorting_context,
                        int right_child_sorting_context,
                        float root_depth,
                        float left_child_depth,
                        float right_child_depth);

  const LayerImplList& RenderSurfaceLayerList() const {
    return host_impl_->active_tree()->RenderSurfaceLayerList();
  }

 private:
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<FakeLayerTreeHostImpl> host_impl_;
};

TEST_F(LayerTreeImplTest, HitTestingForSingleLayer) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(101.f, 101.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, UpdateViewportAndHitTest) {
  // Ensures that the viewport rect is correctly updated by the clip tree.
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeImplTaskRunnerProvider task_runner_provider;
  LayerTreeSettings settings;
  settings.verify_clip_tree_calculations = true;
  std::unique_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  std::unique_ptr<FakeLayerTreeHostImpl> host_impl;
  host_impl.reset(new FakeLayerTreeHostImpl(settings, &task_runner_provider,
                                            &shared_bitmap_manager,
                                            &task_graph_runner));
  host_impl->SetVisible(true);
  EXPECT_TRUE(host_impl->InitializeRenderer(output_surface.get()));
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl->active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl->SetViewportSize(root->bounds());
  host_impl->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl->UpdateNumChildrenAndDrawPropertiesForActiveTree();
  EXPECT_EQ(
      gfx::RectF(gfx::SizeF(bounds)),
      host_impl->active_tree()->property_trees()->clip_tree.ViewportClip());
  EXPECT_EQ(
      gfx::Rect(bounds),
      host_impl->active_tree()->root_layer_for_testing()->visible_layer_rect());

  gfx::Size new_bounds(50, 50);
  host_impl->SetViewportSize(new_bounds);
  gfx::PointF test_point(51.f, 51.f);
  host_impl->active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(
      gfx::RectF(gfx::SizeF(new_bounds)),
      host_impl->active_tree()->property_trees()->clip_tree.ViewportClip());
  EXPECT_EQ(
      gfx::Rect(new_bounds),
      host_impl->active_tree()->root_layer_for_testing()->visible_layer_rect());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleLayerAndHud) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);
  std::unique_ptr<HeadsUpDisplayLayerImpl> hud =
      HeadsUpDisplayLayerImpl::Create(host_impl().active_tree(), 11111);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  // Create hud and add it as a child of root.
  gfx::Size hud_bounds(200, 200);
  SetLayerPropertiesForTesting(hud.get(), identity_matrix, transform_origin,
                               position, hud_bounds, true, false, false);
  hud->SetDrawsContent(true);

  host_impl().active_tree()->set_hud_layer(hud.get());
  root->test_properties()->AddChild(std::move(hud));

  host_impl().SetViewportSize(hud_bounds);
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(2u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point inside HUD, but outside root should return null
  gfx::PointF test_point(101.f, 101.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer, never the HUD
  // layer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForUninvertibleTransform) {
  std::unique_ptr<LayerImpl> root =
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
  SetLayerPropertiesForTesting(root.get(), uninvertible_transform,
                               transform_origin, position, bounds, true, false,
                               true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();
  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_FALSE(root_layer()->ScreenSpaceTransform().IsInvertible());

  // Hit testing any point should not hit the layer. If the invertible matrix is
  // accidentally ignored and treated like an identity, then the hit testing
  // will incorrectly hit the layer when it shouldn't.
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 10.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 30.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(50.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(67.f, 48.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePositionedLayer) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(49.f, 49.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the root render surface would clamp it.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleRotatedLayer) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Transform rotation45_degrees_about_center;
  rotation45_degrees_about_center.Translate(50.0, 50.0);
  rotation45_degrees_about_center.RotateAboutZAxis(45.0);
  rotation45_degrees_about_center.Translate(-50.0, -50.0);
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), rotation45_degrees_about_center,
                               transform_origin, position, bounds, true, false,
                               true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::PointF test_point(99.f, 99.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(1.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  // Hit testing the corners that would overlap the unclipped layer, but are
  // outside the clipped region.
  test_point = gfx::PointF(50.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingClipNodeDifferentTransformAndTargetIds) {
  // Tests hit testing on a layer whose clip node has different transform and
  // target id.
  gfx::Transform identity_matrix;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(500, 500), true, false,
                               true);
  gfx::Transform translation;
  translation.Translate(100, 100);
  std::unique_ptr<LayerImpl> render_surface =
      LayerImpl::Create(host_impl().active_tree(), 2);
  SetLayerPropertiesForTesting(render_surface.get(), translation,
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(100, 100), true, false, true);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2, 2);
  std::unique_ptr<LayerImpl> scale =
      LayerImpl::Create(host_impl().active_tree(), 3);
  SetLayerPropertiesForTesting(scale.get(), scale_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);

  std::unique_ptr<LayerImpl> clip =
      LayerImpl::Create(host_impl().active_tree(), 4);
  SetLayerPropertiesForTesting(clip.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(25, 25), true, false,
                               false);
  clip->SetMasksToBounds(true);

  std::unique_ptr<LayerImpl> test =
      LayerImpl::Create(host_impl().active_tree(), 5);
  SetLayerPropertiesForTesting(test.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               false);
  test->SetDrawsContent(true);

  clip->test_properties()->AddChild(std::move(test));
  scale->test_properties()->AddChild(std::move(clip));
  render_surface->test_properties()->AddChild(std::move(scale));
  root->test_properties()->AddChild(std::move(render_surface));

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  gfx::PointF test_point(160.f, 160.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(140.f, 140.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(5, result_layer->id());

  ClipTree& clip_tree = host_impl().active_tree()->property_trees()->clip_tree;
  ClipNode* clip_node = clip_tree.Node(result_layer->clip_tree_index());
  EXPECT_NE(clip_node->data.transform_id, clip_node->data.target_id);
}

TEST_F(LayerTreeImplTest, HitTestingSiblings) {
  // This tests hit testing when the test point hits only one of the siblings.
  gfx::Transform identity_matrix;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  std::unique_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl().active_tree(), 2);
  SetLayerPropertiesForTesting(child1.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(25, 25), true, false,
                               false);
  child1->SetMasksToBounds(true);
  child1->SetDrawsContent(true);
  std::unique_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl().active_tree(), 3);
  SetLayerPropertiesForTesting(child2.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(75, 75), true, false,
                               false);
  child2->SetMasksToBounds(true);
  child2->SetDrawsContent(true);
  root->test_properties()->AddChild(std::move(child1));
  root->test_properties()->AddChild(std::move(child2));

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  gfx::PointF test_point(50.f, 50.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingPointOutsideMaxTextureSize) {
  gfx::Transform identity_matrix;
  int max_texture_size =
      host_impl().active_tree()->resource_provider()->max_texture_size();
  gfx::Size bounds(max_texture_size + 100, max_texture_size + 100);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), bounds, true, false, true);

  std::unique_ptr<LayerImpl> surface =
      LayerImpl::Create(host_impl().active_tree(), 2);
  SetLayerPropertiesForTesting(surface.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), bounds, true, false, true);
  surface->SetMasksToBounds(true);
  surface->SetDrawsContent(true);

  root->test_properties()->AddChild(std::move(surface));
  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  gfx::PointF test_point(max_texture_size - 50, max_texture_size - 50);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_TRUE(result_layer);

  test_point = gfx::PointF(max_texture_size + 50, max_texture_size + 50);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePerspectiveLayer) {
  std::unique_ptr<LayerImpl> root =
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
      root.get(), perspective_projection_about_center * translation_by_z,
      transform_origin, position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(76.f, 76.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(26.f, 26.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(74.f, 74.f);
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

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    std::unique_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    clipping_layer->SetMasksToBounds(true);

    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    clipping_layer->test_properties()->AddChild(std::move(child));
    root->test_properties()->AddChild(std::move(clipping_layer));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the clipping_layer would clamp it.
  test_point = gfx::PointF(76.f, 76.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::PointF(26.f, 26.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::PointF(74.f, 74.f);
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
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 123);
  LayerImpl* root_layer = root.get();

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetMasksToBounds(true);
  {
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    std::unique_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 789);
    std::unique_ptr<LayerImpl> rotated_leaf =
        LayerImpl::Create(host_impl().active_tree(), 2468);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(80, 80);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetMasksToBounds(true);

    gfx::Transform rotation45_degrees_about_corner;
    rotation45_degrees_about_corner.RotateAboutZAxis(45.0);

    // remember, positioned with respect to its parent which is already at 10,
    // 10
    position = gfx::PointF();
    bounds =
        gfx::Size(200, 200);  // to ensure it covers at least sqrt(2) * 100.
    SetLayerPropertiesForTesting(
        grand_child.get(), rotation45_degrees_about_corner, transform_origin,
        position, bounds, true, false, false);
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
    SetLayerPropertiesForTesting(rotated_leaf.get(), rotated_leaf_transform,
                                 transform_origin, position, bounds, true,
                                 false, false);
    rotated_leaf->SetDrawsContent(true);

    grand_child->test_properties()->AddChild(std::move(rotated_leaf));
    child->test_properties()->AddChild(std::move(grand_child));
    root->test_properties()->AddChild(std::move(child));
    host_impl().active_tree()->SetRootLayerForTesting(std::move(root));

    ExecuteCalculateDrawProperties(root_layer);
  }

  host_impl().SetViewportSize(root_layer->bounds());
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();
  // (11, 89) is close to the the bottom left corner within the clip, but it is
  // not inside the layer.
  gfx::PointF test_point(11.f, 89.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Closer inwards from the bottom left will overlap the layer.
  test_point = gfx::PointF(25.f, 75.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // (4, 50) is inside the unclipped layer, but that corner of the layer should
  // be clipped away by the grandparent and should not get hit. If hit testing
  // blindly uses visible content rect without considering how parent may clip
  // the layer, then hit testing would accidentally think that the point
  // successfully hits the layer.
  test_point = gfx::PointF(4.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // (11, 50) is inside the layer and within the clipped area.
  test_point = gfx::PointF(11.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // Around the middle, just to the right and up, would have hit the layer
  // except that that area should be clipped away by the parent.
  test_point = gfx::PointF(51.f, 49.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Around the middle, just to the left and down, should successfully hit the
  // layer.
  test_point = gfx::PointF(49.f, 51.f);
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

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    std::unique_ptr<LayerImpl> intermediate_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(10.f, 10.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(intermediate_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    // Sanity check the intermediate layer should not clip.
    ASSERT_FALSE(intermediate_layer->masks_to_bounds());
    ASSERT_FALSE(intermediate_layer->test_properties()->mask_layer);

    // The child of the intermediate_layer is translated so that it does not
    // overlap intermediate_layer at all.  If child is incorrectly clipped, we
    // would not be able to hit it successfully.
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    position = gfx::PointF(60.f, 60.f);  // 70, 70 in screen space
    bounds = gfx::Size(20, 20);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    intermediate_layer->test_properties()->AddChild(std::move(child));
    root->test_properties()->AddChild(std::move(intermediate_layer));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(69.f, 69.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(91.f, 91.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::PointF(71.f, 71.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::PointF(89.f, 89.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayers) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  LayerImpl* root_layer = root.get();

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    std::unique_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    std::unique_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    child1->SetDrawsContent(true);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    child2->SetDrawsContent(true);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child1->SetDrawsContent(true);

    child1->test_properties()->AddChild(std::move(grand_child1));
    root->test_properties()->AddChild(std::move(child1));
    root->test_properties()->AddChild(std::move(child2));
    host_impl().active_tree()->SetRootLayerForTesting(std::move(root));

    ExecuteCalculateDrawProperties(root_layer);
  }

  LayerImpl* child1 = root_layer->test_properties()->children[0];
  LayerImpl* child2 = root_layer->test_properties()->children[1];
  LayerImpl* grand_child1 = child1->test_properties()->children[0];

  host_impl().SetViewportSize(root_layer->bounds());
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  RenderSurfaceImpl* root_render_surface = root_layer->render_surface();
  ASSERT_EQ(4u, root_render_surface->layer_list().size());
  ASSERT_EQ(1, root_render_surface->layer_list().at(0)->id());  // root layer
  ASSERT_EQ(2, root_render_surface->layer_list().at(1)->id());  // child1
  ASSERT_EQ(4, root_render_surface->layer_list().at(2)->id());  // grand_child1
  ASSERT_EQ(3, root_render_surface->layer_list().at(3)->id());  // child2

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

int LayerTreeImplTest::HitTestSimpleTree(int root_id,
                                         int left_child_id,
                                         int right_child_id,
                                         int root_sorting_context,
                                         int left_child_sorting_context,
                                         int right_child_sorting_context,
                                         float root_depth,
                                         float left_child_depth,
                                         float right_child_depth) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), root_id);
  std::unique_ptr<LayerImpl> left_child =
      LayerImpl::Create(host_impl().active_tree(), left_child_id);
  std::unique_ptr<LayerImpl> right_child =
      LayerImpl::Create(host_impl().active_tree(), right_child_id);

  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, root_depth);
    SetLayerPropertiesForTesting(root.get(), translate_z, transform_origin,
                                 position, bounds, false, false, true);
    root->SetDrawsContent(true);
    root->Set3dSortingContextId(root_sorting_context);
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, left_child_depth);
    SetLayerPropertiesForTesting(left_child.get(), translate_z,
                                 transform_origin, position, bounds, false,
                                 false, false);
    left_child->SetDrawsContent(true);
    left_child->Set3dSortingContextId(left_child_sorting_context);
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, right_child_depth);
    SetLayerPropertiesForTesting(right_child.get(), translate_z,
                                 transform_origin, position, bounds, false,
                                 false, false);
    right_child->SetDrawsContent(true);
    right_child->Set3dSortingContextId(right_child_sorting_context);
  }

  root->test_properties()->AddChild(std::move(left_child));
  root->test_properties()->AddChild(std::move(right_child));

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();
  CHECK_EQ(1u, RenderSurfaceLayerList().size());

  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);

  CHECK(result_layer);
  return result_layer->id();
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextTied) {
  int hit_layer_id = HitTestSimpleTree(/* ids */ 1, 2, 3,
                                       /* sorting_contexts */ 10, 10, 10,
                                       /* depths */ 0, 0, 0);
  // 3 is the last in tree order, and so should be on top.
  EXPECT_EQ(3, hit_layer_id);
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextChildWins) {
  int hit_layer_id = HitTestSimpleTree(/* ids */ 1, 2, 3,
                                       /* sorting_contexts */ 10, 10, 10,
                                       /* depths */ 0, 1, 0);
  EXPECT_EQ(2, hit_layer_id);
}

TEST_F(LayerTreeImplTest, HitTestingWithoutSortingContext) {
  int hit_layer_id = HitTestSimpleTree(/* ids */ 1, 2, 3,
                                       /* sorting_contexts */ 0, 0, 0,
                                       /* depths */ 0, 1, 0);
  EXPECT_EQ(3, hit_layer_id);
}

TEST_F(LayerTreeImplTest, HitTestingDistinctSortingContext) {
  int hit_layer_id = HitTestSimpleTree(/* ids */ 1, 2, 3,
                                       /* sorting_contexts */ 10, 11, 12,
                                       /* depths */ 0, 1, 0);
  EXPECT_EQ(3, hit_layer_id);
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextParentWins) {
  int hit_layer_id = HitTestSimpleTree(/* ids */ 1, 2, 3,
                                       /* sorting_contexts */ 10, 10, 10,
                                       /* depths */ 0, -1, -1);
  EXPECT_EQ(1, hit_layer_id);
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayersAtVaryingDepths) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  root->test_properties()->should_flatten_transform = false;
  root->Set3dSortingContextId(1);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    std::unique_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    std::unique_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    child1->SetDrawsContent(true);
    child1->test_properties()->should_flatten_transform = false;
    child1->Set3dSortingContextId(1);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10.f);
    SetLayerPropertiesForTesting(child2.get(), translate_z, transform_origin,
                                 position, bounds, true, false, false);
    child2->SetDrawsContent(true);
    child2->test_properties()->should_flatten_transform = false;
    child2->Set3dSortingContextId(1);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child1->SetDrawsContent(true);
    grand_child1->test_properties()->should_flatten_transform = false;

    child1->test_properties()->AddChild(std::move(grand_child1));
    root->test_properties()->AddChild(std::move(child1));
    root->test_properties()->AddChild(std::move(child2));
  }

  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];
  LayerImpl* grand_child1 = child1->test_properties()->children[0];

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top,
  // as it was transformed to the foreground.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to
  // be on top, as it was transformed to the foreground.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), child1, child2 and grand_child1 overlap. child2 is expected to
  // be on top, as it was transformed to the foreground.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top, as it descends from child1.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingRespectsClipParents) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  {
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 2);
    std::unique_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(1, 1);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    child->SetMasksToBounds(true);

    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child->SetDrawsContent(true);
    grand_child->SetHasRenderSurface(true);

    // This should let |grand_child| "escape" |child|'s clip.
    grand_child->test_properties()->clip_parent = root.get();
    std::unique_ptr<std::set<LayerImpl*>> clip_children(
        new std::set<LayerImpl*>);
    clip_children->insert(grand_child.get());
    root->test_properties()->clip_children.reset(clip_children.release());

    child->test_properties()->AddChild(std::move(grand_child));
    root->test_properties()->AddChild(std::move(child));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  gfx::PointF test_point(12.f, 52.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitTestingRespectsScrollParents) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  {
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 2);
    std::unique_ptr<LayerImpl> scroll_child =
        LayerImpl::Create(host_impl().active_tree(), 3);
    std::unique_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(1, 1);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    child->SetMasksToBounds(true);

    position = gfx::PointF();
    bounds = gfx::Size(200, 200);
    SetLayerPropertiesForTesting(scroll_child.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    scroll_child->SetDrawsContent(true);

    // This should cause scroll child and its descendants to be affected by
    // |child|'s clip.
    scroll_child->test_properties()->scroll_parent = child.get();
    std::unique_ptr<std::set<LayerImpl*>> scroll_children(
        new std::set<LayerImpl*>);
    scroll_children->insert(scroll_child.get());
    child->test_properties()->scroll_children.reset(scroll_children.release());

    SetLayerPropertiesForTesting(grand_child.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child->SetDrawsContent(true);
    grand_child->SetHasRenderSurface(true);

    scroll_child->test_properties()->AddChild(std::move(grand_child));
    root->test_properties()->AddChild(std::move(scroll_child));
    root->test_properties()->AddChild(std::move(child));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  gfx::PointF test_point(12.f, 52.f);
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
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  LayerImpl* root_layer = root.get();

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    std::unique_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl().active_tree(), 2);
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl().active_tree(), 3);
    std::unique_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl().active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    child1->SetDrawsContent(true);
    child1->test_properties()->force_render_surface = true;

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    child2->SetDrawsContent(true);
    child2->test_properties()->force_render_surface = true;

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child1->SetDrawsContent(true);
    grand_child1->test_properties()->force_render_surface = true;

    child1->test_properties()->AddChild(std::move(grand_child1));
    root->test_properties()->AddChild(std::move(child1));
    root->test_properties()->AddChild(std::move(child2));
    host_impl().active_tree()->SetRootLayerForTesting(std::move(root));

    ExecuteCalculateDrawProperties(root_layer);
  }

  LayerImpl* child1 = root_layer->test_properties()->children[0];
  LayerImpl* child2 = root_layer->test_properties()->children[1];
  LayerImpl* grand_child1 = child1->test_properties()->children[0];

  host_impl().SetViewportSize(root_layer->bounds());
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

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
  ASSERT_EQ(3u, root_layer->render_surface()->layer_list().size());
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
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerRegionsForSingleLayer) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit checking for any point should return a null pointer for a layer without
  // any touch event handler regions.
  gfx::PointF test_point(11.f, 11.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  host_impl()
      .active_tree()
      ->root_layer_for_testing()
      ->SetTouchEventHandlerRegion(touch_handler_region);
  // Hit checking for a point outside the layer should return a null pointer.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(11.f, 11.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(59.f, 59.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForUninvertibleTransform) {
  std::unique_ptr<LayerImpl> root =
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
  SetLayerPropertiesForTesting(root.get(), uninvertible_transform,
                               transform_origin, position, bounds, true, false,
                               true);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_FALSE(root_layer()->ScreenSpaceTransform().IsInvertible());

  // Hit checking any point should not hit the touch handler region on the
  // layer. If the invertible matrix is accidentally ignored and treated like an
  // identity, then the hit testing will incorrectly hit the layer when it
  // shouldn't.
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 10.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 30.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(50.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(67.f, 48.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSinglePositionedLayer) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::Point3F transform_origin;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit checking for a point outside the layer should return a null pointer.
  gfx::PointF test_point(49.f, 49.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer has a touch handler region containing (101, 101), it
  // should not be visible there since the root render surface would clamp it.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(61.f, 61.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(99.f, 99.f);
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
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  // Set the bounds of the root layer big enough to fit the child when scaled.
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    std::unique_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl().active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);

    test_layer->SetDrawsContent(true);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->test_properties()->AddChild(std::move(test_layer));
  }

  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  float max_page_scale_factor = 10.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);
  host_impl().SetViewportSize(scaled_bounds_for_root);

  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().active_tree()->SetViewportLayersFromIds(Layer::INVALID_ID, 1, 1,
                                                      Layer::INVALID_ID);
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, max_page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer = host_impl()
                              .active_tree()
                              ->root_layer_for_testing()
                              ->test_properties()
                              ->children[0];
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Check whether the child layer fits into the root after scaled.
  EXPECT_EQ(gfx::Rect(test_layer->bounds()), test_layer->visible_layer_rect());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not have a touch event handler, so it will not be
  // tested either).
  gfx::PointF test_point(76.f, 76.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(26.f, 26.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(34.f, 34.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(65.f, 65.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(35.f, 35.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(64.f, 64.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  // Check update of page scale factor on the active tree when page scale layer
  // is also the root layer.
  page_scale_factor *= 1.5f;
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  EXPECT_EQ(host_impl().active_tree()->root_layer_for_testing(),
            host_impl().active_tree()->PageScaleLayer());

  test_point = gfx::PointF(35.f, 35.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::PointF(64.f, 64.f);
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

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    std::unique_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    clipping_layer->SetMasksToBounds(true);

    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    child->SetTouchEventHandlerRegion(touch_handler_region);
    clipping_layer->test_properties()->AddChild(std::move(child));
    root->test_properties()->AddChild(std::move(clipping_layer));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(456, root_layer()->render_surface()->layer_list().at(0)->id());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(35.f, 35.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(25.f, 25.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::PointF(34.f, 34.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForClippedLayerWithDeviceScale) {
  // The layer's device_scale_factor and page_scale_factor should scale the
  // content rect and we should be able to hit the touch handler region by
  // scaling the points accordingly.
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  // Set the bounds of the root layer big enough to fit the child when scaled.
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  std::unique_ptr<LayerImpl> surface =
      LayerImpl::Create(host_impl().active_tree(), 2);
  SetLayerPropertiesForTesting(surface.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    std::unique_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // This layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 20.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    clipping_layer->SetMasksToBounds(true);

    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), 456);
    Region touch_handler_region(gfx::Rect(0, 0, 300, 300));
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(), identity_matrix, transform_origin,
                                 position, bounds, true, false, false);
    child->SetDrawsContent(true);
    child->SetTouchEventHandlerRegion(touch_handler_region);
    clipping_layer->test_properties()->AddChild(std::move(child));
    surface->test_properties()->AddChild(std::move(clipping_layer));
    root->test_properties()->AddChild(std::move(surface));
  }

  float device_scale_factor = 3.f;
  float page_scale_factor = 1.f;
  float max_page_scale_factor = 1.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);
  host_impl().SetViewportSize(scaled_bounds_for_root);

  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().active_tree()->SetViewportLayersFromIds(Layer::INVALID_ID, 1, 1,
                                                      Layer::INVALID_ID);
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, max_page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(2u, RenderSurfaceLayerList().size());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the child layer.
  test_point = gfx::PointF(25.f, 25.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerOverlappingRegions) {
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  {
    std::unique_ptr<LayerImpl> touch_layer =
        LayerImpl::Create(host_impl().active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position;
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(touch_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    touch_layer->SetDrawsContent(true);
    touch_layer->SetTouchEventHandlerRegion(gfx::Rect(0, 0, 50, 50));
    root->test_properties()->AddChild(std::move(touch_layer));
  }

  {
    std::unique_ptr<LayerImpl> notouch_layer =
        LayerImpl::Create(host_impl().active_tree(), 1234);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(0, 25);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(notouch_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    notouch_layer->SetDrawsContent(true);
    root->test_properties()->AddChild(std::move(notouch_layer));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(2u, root_layer()->render_surface()->layer_list().size());
  ASSERT_EQ(123, root_layer()->render_surface()->layer_list().at(0)->id());
  ASSERT_EQ(1234, root_layer()->render_surface()->layer_list().at(1)->id());

  gfx::PointF test_point(35.f, 35.f);
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

  test_point = gfx::PointF(35.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(123, result_layer->id());

  test_point = gfx::PointF(35.f, 65.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingTouchHandlerRegionsForLayerThatIsNotDrawn) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  root->SetDrawsContent(true);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position;
    gfx::Size bounds(50, 50);
    std::unique_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl().active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);

    test_layer->SetDrawsContent(false);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->test_properties()->AddChild(std::move(test_layer));
  }
  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  LayerImpl* test_layer = host_impl()
                              .active_tree()
                              ->root_layer_for_testing()
                              ->test_properties()
                              ->children[0];
  // As test_layer doesn't draw content, the layer list of root's render surface
  // should contain only the root layer.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  // Hit testing for a point outside the test layer should return null pointer.
  // We also implicitly check that the updated screen space transform of a layer
  // that is not in drawn render surface layer list (test_layer) is used during
  // hit testing (becuase the point is inside test_layer with respect to the old
  // screen space transform).
  gfx::PointF test_point(24.f, 24.f);
  test_layer->SetPosition(gfx::PointF(25.f, 25.f));
  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Translate(25.f, 25.f);

  host_impl().active_tree()->property_trees()->needs_rebuild = true;
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
  EXPECT_FALSE(test_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(
          test_layer,
          host_impl().active_tree()->property_trees()->transform_tree));

  // We change the position of the test layer such that the test point is now
  // inside the test_layer.
  test_layer = host_impl()
                   .active_tree()
                   ->root_layer_for_testing()
                   ->test_properties()
                   ->children[0];
  test_layer->SetPosition(gfx::PointF(10.f, 10.f));
  test_layer->NoteLayerPropertyChanged();
  expected_screen_space_transform.MakeIdentity();
  expected_screen_space_transform.Translate(10.f, 10.f);

  host_impl().active_tree()->property_trees()->needs_rebuild = true;
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  ASSERT_EQ(test_layer, result_layer);
  EXPECT_FALSE(result_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(
          test_layer,
          host_impl().active_tree()->property_trees()->transform_tree));
}

TEST_F(LayerTreeImplTest, SelectionBoundsForSingleLayer) {
  int root_layer_id = 12345;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), root_layer_id);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());
  ASSERT_EQ(1u, root_layer()->render_surface()->layer_list().size());

  LayerSelection input;

  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_top = gfx::Point(10, 10);
  input.start.edge_bottom = gfx::Point(10, 20);
  input.start.layer_id = root_layer_id;

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_top = gfx::Point(50, 10);
  input.end.edge_bottom = gfx::Point(50, 30);
  input.end.layer_id = root_layer_id;

  Selection<gfx::SelectionBound> output;

  // Empty input bounds should produce empty output bounds.
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(gfx::SelectionBound(), output.start);
  EXPECT_EQ(gfx::SelectionBound(), output.end);

  // Selection bounds should produce distinct left and right bounds.
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  EXPECT_EQ(gfx::PointF(input.start.edge_bottom), output.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(input.start.edge_top), output.start.edge_top());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  EXPECT_EQ(gfx::PointF(input.end.edge_bottom), output.end.edge_bottom());
  EXPECT_EQ(gfx::PointF(input.end.edge_top), output.end.edge_top());
  EXPECT_TRUE(output.end.visible());
  EXPECT_EQ(input.is_editable, output.is_editable);
  EXPECT_EQ(input.is_empty_text_form_control,
            output.is_empty_text_form_control);

  // Insertion bounds should produce identical left and right bounds.
  LayerSelection insertion_input;
  insertion_input.start.type = gfx::SelectionBound::CENTER;
  insertion_input.start.edge_top = gfx::Point(15, 10);
  insertion_input.start.edge_bottom = gfx::Point(15, 30);
  insertion_input.start.layer_id = root_layer_id;
  insertion_input.is_editable = true;
  insertion_input.is_empty_text_form_control = true;
  insertion_input.end = insertion_input.start;
  host_impl().active_tree()->RegisterSelection(insertion_input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(insertion_input.start.type, output.start.type());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_bottom),
            output.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_top),
            output.start.edge_top());
  EXPECT_EQ(insertion_input.is_editable, output.is_editable);
  EXPECT_EQ(insertion_input.is_empty_text_form_control,
            output.is_empty_text_form_control);
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(output.start, output.end);
}

TEST_F(LayerTreeImplTest, SelectionBoundsForPartialOccludedLayers) {
  int root_layer_id = 12345;
  int clip_layer_id = 1234;
  int clipped_layer_id = 123;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), root_layer_id);
  root->SetDrawsContent(true);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);

  gfx::Vector2dF clipping_offset(10, 10);
  {
    std::unique_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl().active_tree(), clip_layer_id);
    // The clipping layer should occlude the right selection bound.
    gfx::PointF position = gfx::PointF() + clipping_offset;
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    clipping_layer->SetMasksToBounds(true);

    std::unique_ptr<LayerImpl> clipped_layer =
        LayerImpl::Create(host_impl().active_tree(), clipped_layer_id);
    position = gfx::PointF();
    bounds = gfx::Size(100, 100);
    SetLayerPropertiesForTesting(clipped_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    clipped_layer->SetDrawsContent(true);
    clipping_layer->test_properties()->AddChild(std::move(clipped_layer));
    root->test_properties()->AddChild(std::move(clipping_layer));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  LayerSelection input;
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_top = gfx::Point(25, 10);
  input.start.edge_bottom = gfx::Point(25, 30);
  input.start.layer_id = clipped_layer_id;

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_top = gfx::Point(75, 10);
  input.end.edge_bottom = gfx::Point(75, 30);
  input.end.layer_id = clipped_layer_id;
  host_impl().active_tree()->RegisterSelection(input);

  // The left bound should be occluded by the clip layer.
  Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  auto expected_output_start_top = gfx::PointF(input.start.edge_top);
  auto expected_output_edge_botom = gfx::PointF(input.start.edge_bottom);
  expected_output_start_top.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_edge_botom.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_start_top, output.start.edge_top());
  EXPECT_EQ(expected_output_edge_botom, output.start.edge_bottom());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  auto expected_output_end_top = gfx::PointF(input.end.edge_top);
  auto expected_output_end_bottom = gfx::PointF(input.end.edge_bottom);
  expected_output_end_bottom.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_end_top.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_end_top, output.end.edge_top());
  EXPECT_EQ(expected_output_end_bottom, output.end.edge_bottom());
  EXPECT_FALSE(output.end.visible());

  // Handles outside the viewport bounds should be marked invisible.
  input.start.edge_top = gfx::Point(-25, 0);
  input.start.edge_bottom = gfx::Point(-25, 20);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_FALSE(output.start.visible());

  input.start.edge_top = gfx::Point(0, -25);
  input.start.edge_bottom = gfx::Point(0, -5);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_FALSE(output.start.visible());

  // If the handle bottom is partially visible, the handle is marked visible.
  input.start.edge_top = gfx::Point(0, -20);
  input.start.edge_bottom = gfx::Point(0, 1);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_TRUE(output.start.visible());
}

TEST_F(LayerTreeImplTest, SelectionBoundsForScaledLayers) {
  int root_layer_id = 1;
  int sub_layer_id = 2;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), root_layer_id);
  root->SetDrawsContent(true);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);

  gfx::Vector2dF sub_layer_offset(10, 0);
  {
    std::unique_ptr<LayerImpl> sub_layer =
        LayerImpl::Create(host_impl().active_tree(), sub_layer_id);
    gfx::PointF position = gfx::PointF() + sub_layer_offset;
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(sub_layer.get(), identity_matrix,
                                 transform_origin, position, bounds, true,
                                 false, false);
    sub_layer->SetDrawsContent(true);
    root->test_properties()->AddChild(std::move(sub_layer));
  }

  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);
  host_impl().SetViewportSize(scaled_bounds_for_root);

  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().active_tree()->SetViewportLayersFromIds(Layer::INVALID_ID, 1, 1,
                                                      Layer::INVALID_ID);
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, RenderSurfaceLayerList().size());

  LayerSelection input;
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_top = gfx::Point(10, 10);
  input.start.edge_bottom = gfx::Point(10, 30);
  input.start.layer_id = root_layer_id;

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_top = gfx::Point(0, 0);
  input.end.edge_bottom = gfx::Point(0, 20);
  input.end.layer_id = sub_layer_id;
  host_impl().active_tree()->RegisterSelection(input);

  // The viewport bounds should be properly scaled by the page scale, but should
  // remain in DIP coordinates.
  Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  auto expected_output_start_top = gfx::PointF(input.start.edge_top);
  auto expected_output_edge_bottom = gfx::PointF(input.start.edge_bottom);
  expected_output_start_top.Scale(page_scale_factor);
  expected_output_edge_bottom.Scale(page_scale_factor);
  EXPECT_EQ(expected_output_start_top, output.start.edge_top());
  EXPECT_EQ(expected_output_edge_bottom, output.start.edge_bottom());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());

  auto expected_output_end_top = gfx::PointF(input.end.edge_top);
  auto expected_output_end_bottom = gfx::PointF(input.end.edge_bottom);
  expected_output_end_top.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_end_bottom.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_end_top.Scale(page_scale_factor);
  expected_output_end_bottom.Scale(page_scale_factor);
  EXPECT_EQ(expected_output_end_top, output.end.edge_top());
  EXPECT_EQ(expected_output_end_bottom, output.end.edge_bottom());
  EXPECT_TRUE(output.end.visible());
}

TEST_F(LayerTreeImplTest, SelectionBoundsWithLargeTransforms) {
  int root_id = 1;
  int child_id = 2;
  int grand_child_id = 3;

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), root_id);
  gfx::Size bounds(100, 100);
  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;

  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);

  gfx::Transform large_transform;
  large_transform.Scale(SkDoubleToMScalar(1e37), SkDoubleToMScalar(1e37));
  large_transform.RotateAboutYAxis(30);

  {
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl().active_tree(), child_id);
    SetLayerPropertiesForTesting(child.get(), large_transform, transform_origin,
                                 position, bounds, true, false, false);

    std::unique_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl().active_tree(), grand_child_id);
    SetLayerPropertiesForTesting(grand_child.get(), large_transform,
                                 transform_origin, position, bounds, true,
                                 false, false);
    grand_child->SetDrawsContent(true);

    child->test_properties()->AddChild(std::move(grand_child));
    root->test_properties()->AddChild(std::move(child));
  }

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  LayerSelection input;

  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_top = gfx::Point(10, 10);
  input.start.edge_bottom = gfx::Point(10, 20);
  input.start.layer_id = grand_child_id;

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_top = gfx::Point(50, 10);
  input.end.edge_bottom = gfx::Point(50, 30);
  input.end.layer_id = grand_child_id;

  host_impl().active_tree()->RegisterSelection(input);

  Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);

  // edge_bottom and edge_top aren't allowed to have NaNs, so the selection
  // should be empty.
  EXPECT_EQ(gfx::SelectionBound(), output.start);
  EXPECT_EQ(gfx::SelectionBound(), output.end);
}

TEST_F(LayerTreeImplTest, NumLayersTestOne) {
  EXPECT_EQ(0u, host_impl().active_tree()->NumLayers());
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  EXPECT_EQ(1u, host_impl().active_tree()->NumLayers());
}

TEST_F(LayerTreeImplTest, NumLayersSmallTree) {
  EXPECT_EQ(0u, host_impl().active_tree()->NumLayers());
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl().active_tree(), 2));
  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl().active_tree(), 3));
  root->test_properties()->children[1]->test_properties()->AddChild(
      LayerImpl::Create(host_impl().active_tree(), 4));
  EXPECT_EQ(4u, host_impl().active_tree()->NumLayers());
}

TEST_F(LayerTreeImplTest, DeviceScaleFactorNeedsDrawPropertiesUpdate) {
  host_impl().active_tree()->SetDeviceScaleFactor(1.f);
  host_impl().active_tree()->UpdateDrawProperties(false);
  EXPECT_FALSE(host_impl().active_tree()->needs_update_draw_properties());
  host_impl().active_tree()->SetDeviceScaleFactor(2.f);
  EXPECT_TRUE(host_impl().active_tree()->needs_update_draw_properties());
}

TEST_F(LayerTreeImplTest, HitTestingCorrectLayerWheelListener) {
  host_impl().active_tree()->set_event_listener_properties(
      EventListenerClass::kMouseWheel, EventListenerProperties::kBlocking);
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl().active_tree(), 1);
  std::unique_ptr<LayerImpl> left_child =
      LayerImpl::Create(host_impl().active_tree(), 2);
  std::unique_ptr<LayerImpl> right_child =
      LayerImpl::Create(host_impl().active_tree(), 3);

  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    SetLayerPropertiesForTesting(root.get(), translate_z, transform_origin,
                                 position, bounds, false, false, true);
    root->SetDrawsContent(true);
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    SetLayerPropertiesForTesting(left_child.get(), translate_z,
                                 transform_origin, position, bounds, false,
                                 false, false);
    left_child->SetDrawsContent(true);
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    SetLayerPropertiesForTesting(right_child.get(), translate_z,
                                 transform_origin, position, bounds, false,
                                 false, false);
  }

  root->test_properties()->AddChild(std::move(left_child));
  root->test_properties()->AddChild(std::move(right_child));

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();
  CHECK_EQ(1u, RenderSurfaceLayerList().size());

  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);

  CHECK(result_layer);
  EXPECT_EQ(2, result_layer->id());
}

}  // namespace
}  // namespace cc
