// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_utils.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/transform_operations.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/test/gfx_util.h"

namespace cc {
namespace {

float diagonal(float width, float height) {
  return std::sqrt(width * width + height * height);
}

class LayerUtilsGetAnimationBoundsTest : public testing::Test {
 public:
  LayerUtilsGetAnimationBoundsTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        root_(CreateTwoForkTree(&host_impl_)),
        parent1_(root_->test_properties()->children[0]),
        parent2_(root_->test_properties()->children[1]),
        child1_(parent1_->test_properties()->children[0]),
        child2_(parent2_->test_properties()->children[0]),
        grand_child_(child2_->test_properties()->children[0]),
        great_grand_child_(grand_child_->test_properties()->children[0]) {
    timeline_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    host_impl_.animation_host()->AddAnimationTimeline(timeline_);
    host_impl_.active_tree()->SetElementIdsForTesting();
  }

  LayerImpl* root() { return root_; }
  LayerImpl* parent1() { return parent1_; }
  LayerImpl* child1() { return child1_; }
  LayerImpl* parent2() { return parent2_; }
  LayerImpl* child2() { return child2_; }
  LayerImpl* grand_child() { return grand_child_; }
  LayerImpl* great_grand_child() { return great_grand_child_; }

  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }
  FakeLayerTreeHostImpl& host_impl() { return host_impl_; }

 private:
  static LayerImpl* CreateTwoForkTree(LayerTreeHostImpl* host_impl) {
    std::unique_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl->active_tree(), 1);
    LayerImpl* root_ptr = root.get();
    root->test_properties()->AddChild(
        LayerImpl::Create(host_impl->active_tree(), 2));
    root->test_properties()->children[0]->test_properties()->AddChild(
        LayerImpl::Create(host_impl->active_tree(), 3));
    root->test_properties()->AddChild(
        LayerImpl::Create(host_impl->active_tree(), 4));
    root->test_properties()->children[1]->test_properties()->AddChild(
        LayerImpl::Create(host_impl->active_tree(), 5));
    root->test_properties()
        ->children[1]
        ->test_properties()
        ->children[0]
        ->test_properties()
        ->AddChild(LayerImpl::Create(host_impl->active_tree(), 6));
    root->test_properties()
        ->children[1]
        ->test_properties()
        ->children[0]
        ->test_properties()
        ->children[0]
        ->test_properties()
        ->AddChild(LayerImpl::Create(host_impl->active_tree(), 7));
    host_impl->active_tree()->SetRootLayerForTesting(std::move(root));
    return root_ptr;
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  LayerImpl* root_;
  LayerImpl* parent1_;
  LayerImpl* parent2_;
  LayerImpl* child1_;
  LayerImpl* child2_;
  LayerImpl* grand_child_;
  LayerImpl* great_grand_child_;
  scoped_refptr<AnimationTimeline> timeline_;
};

TEST_F(LayerUtilsGetAnimationBoundsTest, ScaleRoot) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendScale(1.f, 1.f, 1.f);
  TransformOperations end;
  end.AppendScale(2.f, 2.f, 1.f);
  AddAnimatedTransformToElementWithPlayer(root()->element_id(), timeline(),
                                          duration, start, end);

  root()->SetPosition(gfx::PointF());
  parent1()->SetPosition(gfx::PointF());
  parent1()->SetBounds(gfx::Size(350, 200));

  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(gfx::Size(100, 200));

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(150.f, 50.f, 0.f, 350.f, 450.f, 0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, TranslateParentLayer) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendTranslate(0.f, 0.f, 0.f);
  TransformOperations end;
  end.AppendTranslate(50.f, 50.f, 0.f);
  AddAnimatedTransformToElementWithPlayer(parent1()->element_id(), timeline(),
                                          duration, start, end);

  parent1()->SetBounds(gfx::Size(350, 200));

  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(gfx::Size(100, 200));

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(150.f, 50.f, 0.f, 150.f, 250.f, 0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, TranslateChildLayer) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendTranslate(0.f, 0.f, 0.f);
  TransformOperations end;
  end.AppendTranslate(50.f, 50.f, 0.f);
  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, end);
  parent1()->SetBounds(gfx::Size(350, 200));

  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(gfx::Size(100, 200));

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(150.f, 50.f, 0.f, 150.f, 250.f, 0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, TranslateBothLayers) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendTranslate(0.f, 0.f, 0.f);
  TransformOperations child_end;
  child_end.AppendTranslate(50.f, 0.f, 0.f);
  AddAnimatedTransformToElementWithPlayer(parent1()->element_id(), timeline(),
                                          duration, start, child_end);

  TransformOperations grand_child_end;
  grand_child_end.AppendTranslate(0.f, 50.f, 0.f);
  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, grand_child_end);

  parent1()->SetBounds(gfx::Size(350, 200));

  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(gfx::Size(100, 200));

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(150.f, 50.f, 0.f, 150.f, 250.f, 0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, RotateXNoPerspective) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(1.f, 0.f, 0.f, 0.f);
  TransformOperations end;
  end.AppendRotate(1.f, 0.f, 0.f, 90.f);

  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, end);

  parent1()->SetBounds(gfx::Size(350, 200));

  gfx::Size bounds(100, 100);
  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(bounds);
  child1()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(150.f, 50.f, -50.f, 100.f, 100.f, 100.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, RotateXWithPerspective) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(1.f, 0.f, 0.f, 0.f);
  TransformOperations end;
  end.AppendRotate(1.f, 0.f, 0.f, 90.f);

  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, end);

  // Make the anchor point not the default 0.5 value and line up with the
  // child center to make the math easier.
  parent1()->test_properties()->transform_origin =
      gfx::Point3F(0.375f * 400.f, 0.375f * 400.f, 0.f);
  parent1()->SetBounds(gfx::Size(400, 400));

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100.f);
  parent1()->test_properties()->transform = perspective;

  gfx::Size bounds(100, 100);
  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(100.f, 100.f));
  child1()->SetBounds(bounds);
  child1()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(50.f, 50.f, -33.333336f, 200.f, 200.f, 133.333344f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, RotateXWithPerspectiveOnSameLayer) {
  // This test is used to check whether GetAnimationBounds correctly ignores the
  // local transform when there is an animation applied to the layer / node.
  // The intended behavior is that the animation should overwrite the transform.

  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(1.f, 0.f, 0.f, 0.f);
  TransformOperations end;
  end.AppendRotate(1.f, 0.f, 0.f, 90.f);

  AddAnimatedTransformToElementWithPlayer(parent1()->element_id(), timeline(),
                                          duration, start, end);

  // Make the anchor point not the default 0.5 value and line up
  // with the child center to make the math easier.
  parent1()->test_properties()->transform_origin =
      gfx::Point3F(0.375f * 400.f, 0.375f * 400.f, 0.f);
  parent1()->SetBounds(gfx::Size(400, 400));

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100.f);
  parent1()->test_properties()->transform = perspective;

  gfx::Size bounds(100, 100);
  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(100.f, 100.f));
  child1()->SetBounds(bounds);
  child1()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(100.f, 100.f, -50.f, 100.f, 100.f, 100.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, RotateZ) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(0.f, 0.f, 1.f, 0.f);
  TransformOperations end;
  end.AppendRotate(0.f, 0.f, 1.f, 90.f);
  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, end);

  parent1()->SetBounds(gfx::Size(350, 200));

  gfx::Size bounds(100, 100);
  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(bounds);
  child1()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_TRUE(success);
  float diag = diagonal(bounds.width(), bounds.height());
  gfx::BoxF expected(150.f + 0.5f * (bounds.width() - diag),
                     50.f + 0.5f * (bounds.height() - diag),
                     0.f,
                     diag,
                     diag,
                     0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest, MismatchedTransforms) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendTranslate(5, 6, 7);
  TransformOperations end;
  end.AppendRotate(0.f, 0.f, 1.f, 90.f);
  AddAnimatedTransformToElementWithPlayer(child1()->element_id(), timeline(),
                                          duration, start, end);

  parent1()->SetBounds(gfx::Size(350, 200));

  gfx::Size bounds(100, 100);
  child1()->SetDrawsContent(true);
  child1()->SetPosition(gfx::PointF(150.f, 50.f));
  child1()->SetBounds(bounds);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*child1(), &box);
  EXPECT_FALSE(success);
}

TEST_F(LayerUtilsGetAnimationBoundsTest,
       TransformUnderAncestorsWithPositionOr2DTransform) {
  // Tree topology:
  // +root (Bounds)
  // +--parent2 (Position)
  // +----child2 (2d transform)
  // +------grand_child (Transform animation)
  // +--------great_grand_child (DrawsContent)
  // This test is used to check if GetAnimationBounds correctly skips layers and
  // take layers which do not own a transform_node into consideration.
  // Under this topology, only root and grand_child own transform_nodes.

  double duration = 1.0;

  TransformOperations start;
  start.AppendTranslate(0.f, 0.f, 0.f);
  TransformOperations great_grand_child_end;
  great_grand_child_end.AppendTranslate(50.f, 0.f, 0.f);
  AddAnimatedTransformToElementWithPlayer(grand_child()->element_id(),
                                          timeline(), duration, start,
                                          great_grand_child_end);

  gfx::Transform translate_2d_transform;
  translate_2d_transform.Translate(80.f, 60.f);
  root()->SetBounds(gfx::Size(350, 200));
  parent2()->SetPosition(gfx::PointF(40.f, 45.f));
  child2()->test_properties()->transform = translate_2d_transform;
  great_grand_child()->SetDrawsContent(true);
  great_grand_child()->SetPosition(gfx::PointF(150.f, 50.f));
  great_grand_child()->SetBounds(gfx::Size(100, 200));
  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*great_grand_child(), &box);
  EXPECT_TRUE(success);
  gfx::BoxF expected(270.f, 155.f, 0.f, 150.f, 200.f, 0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest,
       RotateZUnderAncestorsWithPositionOr2DTransform) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(0.f, 0.f, 1.f, 0.f);
  TransformOperations great_grand_child_end;
  great_grand_child_end.AppendRotate(0.f, 0.f, 1.f, 90.f);
  AddAnimatedTransformToElementWithPlayer(grand_child()->element_id(),
                                          timeline(), duration, start,
                                          great_grand_child_end);

  gfx::Transform translate_2d_transform;
  translate_2d_transform.Translate(80.f, 60.f);
  root()->SetBounds(gfx::Size(350, 200));
  parent2()->SetPosition(gfx::PointF(40.f, 45.f));
  child2()->test_properties()->transform = translate_2d_transform;

  gfx::Size bounds(100, 100);
  grand_child()->SetPosition(gfx::PointF(150.f, 50.f));
  grand_child()->SetBounds(bounds);
  grand_child()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  great_grand_child()->SetPosition(gfx::PointF(25.f, 25.f));
  great_grand_child()->SetBounds(gfx::Size(50.f, 50.f));
  great_grand_child()->SetDrawsContent(true);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*great_grand_child(), &box);
  EXPECT_TRUE(success);

  float diag = diagonal(50.f, 50.f);
  gfx::BoxF expected(320.f - 0.5f * diag, 205.f - 0.5f * diag, 0.f, diag, diag,
                     0.f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest,
       RotateXWithPerspectiveUnderAncestorsWithPositionOr2DTransform) {
  // Tree topology:
  // +root (Bounds)
  // +--parent2 (Position)
  // +----child2 (2d transform)
  // +------grand_child (Perspective)
  // +--------great_grand_child (RotateX, DrawsContent)
  // Due to the RotateX animation, great_grand_child also owns a transform_node

  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(1.f, 0.f, 0.f, 0.f);
  TransformOperations great_grand_child_end;
  great_grand_child_end.AppendRotate(1.f, 0.f, 0.f, 90.f);
  AddAnimatedTransformToElementWithPlayer(great_grand_child()->element_id(),
                                          timeline(), duration, start,
                                          great_grand_child_end);

  gfx::Transform translate_2d_transform;
  translate_2d_transform.Translate(80.f, 60.f);

  root()->SetBounds(gfx::Size(350, 200));
  parent2()->SetPosition(gfx::PointF(40.f, 45.f));
  child2()->test_properties()->transform = translate_2d_transform;

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100.f);

  gfx::Size bounds(100.f, 100.f);
  grand_child()->SetPosition(gfx::PointF(150.f, 50.f));
  grand_child()->SetBounds(bounds);
  grand_child()->test_properties()->transform = perspective;
  grand_child()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.5f, bounds.height() * 0.5f, 0);

  great_grand_child()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.25f, bounds.height() * 0.25f, 0);
  great_grand_child()->SetPosition(gfx::PointF(25.f, 25.f));
  great_grand_child()->SetBounds(gfx::Size(50.f, 50.f));
  great_grand_child()->SetDrawsContent(true);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*great_grand_child(), &box);
  EXPECT_TRUE(success);

  // Translate and position offset to the root:
  //   40 + 150 + 80 + 25 = 295
  //   45 + 60 +50 + 25 = 180
  //   0
  // Animation Bounds Before Perspective:
  //   0, 0, -25, 50, 50, 50
  // Apply Perspective:
  //   When RotateX theta:
  //                          y
  //                          ^
  //              d1          |
  //             |  /         |
  //             | /          |
  //             |/           |
  //            /|            |
  //           / |            |
  //          /  |            |
  //         theta            |
  //  z<----------------------X
  //          d2
  //
  //     the diag is 25 * 2
  //
  //     box w: 2 * 25 * 100 / (100 - 25 * sin(theta)), max = 200 / 3 = 66.67
  //         h: same as w, 66.67
  //         d1: 25 * 100 * sin(theta) / (100 + 25 * sin(theta)), max = 100 / 5
  //             = 20
  //         d2: 25 * 100 * sin(theta) / (100 - 25 * sin(theta)), max = 100 / 3
  //             = 33.33
  //         d = d1 + d2 = 53.33
  //
  // Position Fix:
  //   295 - (66.67 - 50) / 2 = 286.67
  //   180 - (66.67 - 50) / 2 = 171.67
  //   0 - 20 = -20

  gfx::BoxF expected(286.666687f, 171.666672f, -20.f, 66.666656f, 66.666672f,
                     53.333336f);
  EXPECT_BOXF_EQ(expected, box);
}

TEST_F(LayerUtilsGetAnimationBoundsTest,
       RotateXNoPerspectiveUnderAncestorsWithPositionOr2DTransform) {
  double duration = 1.0;

  TransformOperations start;
  start.AppendRotate(1.f, 0.f, 0.f, 0.f);
  TransformOperations rotate_x_end;
  rotate_x_end.AppendRotate(1.f, 0.f, 0.f, 90.f);
  AddAnimatedTransformToElementWithPlayer(great_grand_child()->element_id(),
                                          timeline(), duration, start,
                                          rotate_x_end);

  gfx::Transform translate_2d_transform;
  translate_2d_transform.Translate(80.f, 60.f);

  root()->SetBounds(gfx::Size(350, 200));
  parent2()->SetPosition(gfx::PointF(40.f, 45.f));
  child2()->test_properties()->transform = translate_2d_transform;

  gfx::Size bounds(100.f, 100.f);
  grand_child()->SetPosition(gfx::PointF(150.f, 50.f));
  grand_child()->SetBounds(bounds);

  great_grand_child()->test_properties()->transform_origin =
      gfx::Point3F(bounds.width() * 0.25f, bounds.height() * 0.25f, 0);
  great_grand_child()->SetPosition(gfx::PointF(25.f, 25.f));
  great_grand_child()->SetBounds(
      gfx::Size(bounds.width() * 0.5f, bounds.height() * 0.5f));
  great_grand_child()->SetDrawsContent(true);

  host_impl().active_tree()->BuildLayerListAndPropertyTreesForTesting();

  gfx::BoxF box;
  bool success = LayerUtils::GetAnimationBounds(*great_grand_child(), &box);
  EXPECT_TRUE(success);

  // Same as RotateXWithPerspectiveUnderAncestorsWithPositionOr2DTransform test,
  // except for the perspective calculations.

  gfx::BoxF expected(295.f, 180.f, -25.f, 50.f, 50.f, 50.f);
  EXPECT_BOXF_EQ(expected, box);
}

}  // namespace

}  // namespace cc
