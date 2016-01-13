// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_sorter.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

// Note: In the following overlap tests, the "camera" is looking down the
// negative Z axis, meaning that layers with smaller z values (more negative)
// are further from the camera and therefore must be drawn before layers with
// higher z values.

TEST(LayerSorterTest, BasicOverlap) {
  LayerSorter::ABCompareResult overlap_result;
  const float z_threshold = 0.1f;
  float weight = 0.f;

  // Trivial test, with one layer directly obscuring the other.
  gfx::Transform neg4_translate;
  neg4_translate.Translate3d(0.0, 0.0, -4.0);
  LayerShape front(2.f, 2.f, neg4_translate);

  gfx::Transform neg5_translate;
  neg5_translate.Translate3d(0.0, 0.0, -5.0);
  LayerShape back(2.f, 2.f, neg5_translate);

  overlap_result =
      LayerSorter::CheckOverlap(&front, &back, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::BBeforeA, overlap_result);
  EXPECT_EQ(1.f, weight);

  overlap_result =
      LayerSorter::CheckOverlap(&back, &front, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::ABeforeB, overlap_result);
  EXPECT_EQ(1.f, weight);

  // One layer translated off to the right. No overlap should be detected.
  gfx::Transform right_translate;
  right_translate.Translate3d(10.0, 0.0, -5.0);
  LayerShape back_right(2.f, 2.f, right_translate);
  overlap_result =
      LayerSorter::CheckOverlap(&front, &back_right, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::None, overlap_result);

  // When comparing a layer with itself, z difference is always 0.
  overlap_result =
      LayerSorter::CheckOverlap(&front, &front, z_threshold, &weight);
  EXPECT_EQ(0.f, weight);
}

TEST(LayerSorterTest, RightAngleOverlap) {
  LayerSorter::ABCompareResult overlap_result;
  const float z_threshold = 0.1f;
  float weight = 0.f;

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(1000.0);

  // Two layers forming a right angle with a perspective viewing transform.
  gfx::Transform left_face_matrix;
  left_face_matrix.Translate3d(-1.0, 0.0, -5.0);
  left_face_matrix.RotateAboutYAxis(-90.0);
  left_face_matrix.Translate(-1.0, -1.0);
  LayerShape left_face(2.f, 2.f, perspective_matrix * left_face_matrix);
  gfx::Transform front_face_matrix;
  front_face_matrix.Translate3d(0.0, 0.0, -4.0);
  front_face_matrix.Translate(-1.0, -1.0);
  LayerShape front_face(2.f, 2.f, perspective_matrix * front_face_matrix);

  overlap_result =
      LayerSorter::CheckOverlap(&front_face, &left_face, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::BBeforeA, overlap_result);
}

TEST(LayerSorterTest, IntersectingLayerOverlap) {
  LayerSorter::ABCompareResult overlap_result;
  const float z_threshold = 0.1f;
  float weight = 0.f;

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(1000.0);

  // Intersecting layers. An explicit order will be returned based on relative z
  // values at the overlapping features but the weight returned should be zero.
  gfx::Transform front_face_matrix;
  front_face_matrix.Translate3d(0.0, 0.0, -4.0);
  front_face_matrix.Translate(-1.0, -1.0);
  LayerShape front_face(2.f, 2.f, perspective_matrix * front_face_matrix);

  gfx::Transform through_matrix;
  through_matrix.Translate3d(0.0, 0.0, -4.0);
  through_matrix.RotateAboutYAxis(45.0);
  through_matrix.Translate(-1.0, -1.0);
  LayerShape rotated_face(2.f, 2.f, perspective_matrix * through_matrix);
  overlap_result = LayerSorter::CheckOverlap(&front_face,
                                             &rotated_face,
                                             z_threshold,
                                             &weight);
  EXPECT_NE(LayerSorter::None, overlap_result);
  EXPECT_EQ(0.f, weight);
}

TEST(LayerSorterTest, LayersAtAngleOverlap) {
  LayerSorter::ABCompareResult overlap_result;
  const float z_threshold = 0.1f;
  float weight = 0.f;

  // Trickier test with layers at an angle.
  //
  //   -x . . . . 0 . . . . +x
  // -z             /
  //  :            /----B----
  //  0           C
  //  : ----A----/
  // +z         /
  //
  // C is in front of A and behind B (not what you'd expect by comparing
  // centers). A and B don't overlap, so they're incomparable.

  gfx::Transform transform_a;
  transform_a.Translate3d(-6.0, 0.0, 1.0);
  transform_a.Translate(-4.0, -10.0);
  LayerShape layer_a(8.f, 20.f, transform_a);

  gfx::Transform transform_b;
  transform_b.Translate3d(6.0, 0.0, -1.0);
  transform_b.Translate(-4.0, -10.0);
  LayerShape layer_b(8.f, 20.f, transform_b);

  gfx::Transform transform_c;
  transform_c.RotateAboutYAxis(40.0);
  transform_c.Translate(-4.0, -10.0);
  LayerShape layer_c(8.f, 20.f, transform_c);

  overlap_result =
      LayerSorter::CheckOverlap(&layer_a, &layer_c, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::ABeforeB, overlap_result);
  overlap_result =
      LayerSorter::CheckOverlap(&layer_c, &layer_b, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::ABeforeB, overlap_result);
  overlap_result =
      LayerSorter::CheckOverlap(&layer_a, &layer_b, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::None, overlap_result);
}

TEST(LayerSorterTest, LayersUnderPathologicalPerspectiveTransform) {
  LayerSorter::ABCompareResult overlap_result;
  const float z_threshold = 0.1f;
  float weight = 0.f;

  // On perspective projection, if w becomes negative, the re-projected point
  // will be invalid and un-usable. Correct code needs to clip away portions of
  // the geometry where w < 0. If the code uses the invalid value, it will think
  // that a layer has different bounds than it really does, which can cause
  // things to sort incorrectly.

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(1);

  gfx::Transform transform_a;
  transform_a.Translate3d(-15.0, 0.0, -2.0);
  transform_a.Translate(-5.0, -5.0);
  LayerShape layer_a(10.f, 10.f, perspective_matrix * transform_a);

  // With this sequence of transforms, when layer B is correctly clipped, it
  // will be visible on the left half of the projection plane, in front of
  // layer_a. When it is not clipped, its bounds will actually incorrectly
  // appear much smaller and the correct sorting dependency will not be found.
  gfx::Transform transform_b;
  transform_b.Translate3d(0.f, 0.f, 0.7f);
  transform_b.RotateAboutYAxis(45.0);
  transform_b.Translate(-5.0, -5.0);
  LayerShape layer_b(10.f, 10.f, perspective_matrix * transform_b);

  // Sanity check that the test case actually covers the intended scenario,
  // where part of layer B go behind the w = 0 plane.
  gfx::QuadF test_quad = gfx::QuadF(gfx::RectF(-0.5f, -0.5f, 1.f, 1.f));
  bool clipped = false;
  MathUtil::MapQuad(perspective_matrix * transform_b, test_quad, &clipped);
  ASSERT_TRUE(clipped);

  overlap_result =
      LayerSorter::CheckOverlap(&layer_a, &layer_b, z_threshold, &weight);
  EXPECT_EQ(LayerSorter::ABeforeB, overlap_result);
}

TEST(LayerSorterTest, VerifyExistingOrderingPreservedWhenNoZDiff) {
  // If there is no reason to re-sort the layers (i.e. no 3d z difference), then
  // the existing ordering provided on input should be retained. This test
  // covers the fix in https://bugs.webkit.org/show_bug.cgi?id=75046. Before
  // this fix, ordering was accidentally reversed, causing bugs in z-index
  // ordering on websites when preserves3D triggered the LayerSorter.

  // Input list of layers: [1, 2, 3, 4, 5].
  // Expected output: [3, 4, 1, 2, 5].
  //    - 1, 2, and 5 do not have a 3d z difference, and therefore their
  //      relative ordering should be retained.
  //    - 3 and 4 do not have a 3d z difference, and therefore their relative
  //      ordering should be retained.
  //    - 3 and 4 should be re-sorted so they are in front of 1, 2, and 5.

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);

  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> layer2 = LayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<LayerImpl> layer3 = LayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<LayerImpl> layer4 = LayerImpl::Create(host_impl.active_tree(), 4);
  scoped_ptr<LayerImpl> layer5 = LayerImpl::Create(host_impl.active_tree(), 5);

  gfx::Transform BehindMatrix;
  BehindMatrix.Translate3d(0.0, 0.0, 2.0);
  gfx::Transform FrontMatrix;
  FrontMatrix.Translate3d(0.0, 0.0, 1.0);

  layer1->SetBounds(gfx::Size(10, 10));
  layer1->SetContentBounds(gfx::Size(10, 10));
  layer1->draw_properties().target_space_transform = BehindMatrix;
  layer1->SetDrawsContent(true);

  layer2->SetBounds(gfx::Size(20, 20));
  layer2->SetContentBounds(gfx::Size(20, 20));
  layer2->draw_properties().target_space_transform = BehindMatrix;
  layer2->SetDrawsContent(true);

  layer3->SetBounds(gfx::Size(30, 30));
  layer3->SetContentBounds(gfx::Size(30, 30));
  layer3->draw_properties().target_space_transform = FrontMatrix;
  layer3->SetDrawsContent(true);

  layer4->SetBounds(gfx::Size(40, 40));
  layer4->SetContentBounds(gfx::Size(40, 40));
  layer4->draw_properties().target_space_transform = FrontMatrix;
  layer4->SetDrawsContent(true);

  layer5->SetBounds(gfx::Size(50, 50));
  layer5->SetContentBounds(gfx::Size(50, 50));
  layer5->draw_properties().target_space_transform = BehindMatrix;
  layer5->SetDrawsContent(true);

  LayerImplList layer_list;
  layer_list.push_back(layer1.get());
  layer_list.push_back(layer2.get());
  layer_list.push_back(layer3.get());
  layer_list.push_back(layer4.get());
  layer_list.push_back(layer5.get());

  ASSERT_EQ(5u, layer_list.size());
  EXPECT_EQ(1, layer_list[0]->id());
  EXPECT_EQ(2, layer_list[1]->id());
  EXPECT_EQ(3, layer_list[2]->id());
  EXPECT_EQ(4, layer_list[3]->id());
  EXPECT_EQ(5, layer_list[4]->id());

  LayerSorter layer_sorter;
  layer_sorter.Sort(layer_list.begin(), layer_list.end());

  ASSERT_EQ(5u, layer_list.size());
  EXPECT_EQ(3, layer_list[0]->id());
  EXPECT_EQ(4, layer_list[1]->id());
  EXPECT_EQ(1, layer_list[2]->id());
  EXPECT_EQ(2, layer_list[3]->id());
  EXPECT_EQ(5, layer_list[4]->id());
}

TEST(LayerSorterTest, VerifyConcidentLayerPrecisionLossResultsInDocumentOrder) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);

  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> layer2 = LayerImpl::Create(host_impl.active_tree(), 2);

  // Layer 1 should occur before layer 2 in paint.  However, due to numeric
  // issues in the sorter, it will put the layers in the wrong order
  // in some situations.  Here we test a patch that results in  document
  // order rather than calculated order when numeric percision is suspect
  // in calculated order.

  gfx::Transform BehindMatrix;
  BehindMatrix.Translate3d(0.f, 0.f, 0.999999f);
  BehindMatrix.RotateAboutXAxis(38.5);
  BehindMatrix.RotateAboutYAxis(77.0);
  gfx::Transform FrontMatrix;
  FrontMatrix.Translate3d(0, 0, 1.0);
  FrontMatrix.RotateAboutXAxis(38.5);
  FrontMatrix.RotateAboutYAxis(77.0);

  layer1->SetBounds(gfx::Size(10, 10));
  layer1->SetContentBounds(gfx::Size(10, 10));
  layer1->draw_properties().target_space_transform = BehindMatrix;
  layer1->SetDrawsContent(true);

  layer2->SetBounds(gfx::Size(10, 10));
  layer2->SetContentBounds(gfx::Size(10, 10));
  layer2->draw_properties().target_space_transform = FrontMatrix;
  layer2->SetDrawsContent(true);

  LayerImplList layer_list;
  layer_list.push_back(layer1.get());
  layer_list.push_back(layer2.get());

  ASSERT_EQ(2u, layer_list.size());
  EXPECT_EQ(1, layer_list[0]->id());
  EXPECT_EQ(2, layer_list[1]->id());

  LayerSorter layer_sorter;
  layer_sorter.Sort(layer_list.begin(), layer_list.end());

  ASSERT_EQ(2u, layer_list.size());
  EXPECT_EQ(1, layer_list[0]->id());
  EXPECT_EQ(2, layer_list[1]->id());
}

}  // namespace
}  // namespace cc

