// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_compositor_frame_sink.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class VerifyTreeCalcsLayerTreeSettings : public LayerTreeSettings {
 public:
  VerifyTreeCalcsLayerTreeSettings() {
    verify_clip_tree_calculations = true;
  }
};

class LayerTreeHostCommonTestBase : public LayerTestCommon::LayerImplTest {
 public:
  LayerTreeHostCommonTestBase()
      : LayerTestCommon::LayerImplTest(VerifyTreeCalcsLayerTreeSettings()) {}
  explicit LayerTreeHostCommonTestBase(const LayerTreeSettings& settings)
      : LayerTestCommon::LayerImplTest(settings) {}

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(layer_impl->id(),
                                                         delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(layer_impl->id());
  }

  static float GetMaximumAnimationScale(LayerImpl* layer_impl) {
    return layer_impl->layer_tree_impl()
        ->property_trees()
        ->GetAnimationScales(layer_impl->transform_tree_index(),
                             layer_impl->layer_tree_impl())
        .maximum_animation_scale;
  }

  static float GetStartingAnimationScale(LayerImpl* layer_impl) {
    return layer_impl->layer_tree_impl()
        ->property_trees()
        ->GetAnimationScales(layer_impl->transform_tree_index(),
                             layer_impl->layer_tree_impl())
        .starting_animation_scale;
  }

  void ExecuteCalculateDrawProperties(Layer* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      Layer* page_scale_layer,
                                      Layer* inner_viewport_scroll_layer,
                                      Layer* outer_viewport_scroll_layer) {
    PropertyTreeBuilder::PreCalculateMetaInformation(root_layer);

    EXPECT_TRUE(page_scale_layer || (page_scale_factor == 1.f));
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);

    // We are probably not testing what is intended if the root_layer bounds are
    // empty.
    DCHECK(!root_layer->bounds().IsEmpty());
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root_layer, device_viewport_size);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_layer = page_scale_layer;
    inputs.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
    inputs.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  }

  void ExecuteCalculateDrawProperties(
      LayerImpl* root_layer,
      float device_scale_factor,
      float page_scale_factor,
      LayerImpl* page_scale_layer,
      LayerImpl* inner_viewport_scroll_layer,
      LayerImpl* outer_viewport_scroll_layer,
      bool skip_verify_visible_rect_calculations = false) {
    if (device_scale_factor !=
        root_layer->layer_tree_impl()->device_scale_factor())
      root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;

    root_layer->layer_tree_impl()->SetDeviceScaleFactor(device_scale_factor);

    EXPECT_TRUE(page_scale_layer || (page_scale_factor == 1.f));

    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);

    render_surface_layer_list_impl_.reset(new LayerImplList);

    // We are probably not testing what is intended if the root_layer bounds are
    // empty.
    DCHECK(!root_layer->bounds().IsEmpty());
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root_layer, device_viewport_size,
        render_surface_layer_list_impl_.get());
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_layer = page_scale_layer;
    inputs.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
    inputs.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
    inputs.can_adjust_raster_scales = true;
    if (skip_verify_visible_rect_calculations)
      inputs.verify_visible_rect_calculations = false;

    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer) {
    LayerType* page_scale_application_layer = nullptr;
    LayerType* inner_viewport_scroll_layer = nullptr;
    LayerType* outer_viewport_scroll_layer = nullptr;
    ExecuteCalculateDrawProperties(
        root_layer, 1.f, 1.f, page_scale_application_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer,
                                      float device_scale_factor) {
    LayerType* page_scale_application_layer = nullptr;
    LayerType* inner_viewport_scroll_layer = nullptr;
    LayerType* outer_viewport_scroll_layer = nullptr;
    ExecuteCalculateDrawProperties(
        root_layer, device_scale_factor, 1.f, page_scale_application_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);
  }

  const LayerList* GetUpdateLayerList() { return &update_layer_list_; }

  void ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(Layer* root_layer) {
    DCHECK(root_layer->GetLayerTree());
    PropertyTreeBuilder::PreCalculateMetaInformation(root_layer);

    bool can_render_to_separate_surface = true;

    const Layer* page_scale_layer =
        root_layer->GetLayerTree()->page_scale_layer();
    Layer* inner_viewport_scroll_layer =
        root_layer->GetLayerTree()->inner_viewport_scroll_layer();
    Layer* outer_viewport_scroll_layer =
        root_layer->GetLayerTree()->outer_viewport_scroll_layer();
    const Layer* overscroll_elasticity_layer =
        root_layer->GetLayerTree()->overscroll_elasticity_layer();
    gfx::Vector2dF elastic_overscroll =
        root_layer->GetLayerTree()->elastic_overscroll();
    float page_scale_factor = 1.f;
    float device_scale_factor = 1.f;
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);
    PropertyTrees* property_trees =
        root_layer->GetLayerTree()->property_trees();
    update_layer_list_.clear();
    PropertyTreeBuilder::BuildPropertyTrees(
        root_layer, page_scale_layer, inner_viewport_scroll_layer,
        outer_viewport_scroll_layer, overscroll_elasticity_layer,
        elastic_overscroll, page_scale_factor, device_scale_factor,
        gfx::Rect(device_viewport_size), gfx::Transform(), property_trees);
    draw_property_utils::UpdatePropertyTrees(property_trees,
                                             can_render_to_separate_surface);
    draw_property_utils::FindLayersThatNeedUpdates(
        root_layer->GetLayerTree(), property_trees, &update_layer_list_);
  }

  void ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(
      LayerImpl* root_layer,
      bool skip_verify_visible_rect_calculations = false) {
    DCHECK(root_layer->layer_tree_impl());
    PropertyTreeBuilder::PreCalculateMetaInformationForTesting(root_layer);

    bool can_render_to_separate_surface = true;

    LayerImpl* page_scale_layer = nullptr;
    LayerImpl* inner_viewport_scroll_layer =
        root_layer->layer_tree_impl()->InnerViewportScrollLayer();
    LayerImpl* outer_viewport_scroll_layer =
        root_layer->layer_tree_impl()->OuterViewportScrollLayer();
    LayerImpl* overscroll_elasticity_layer =
        root_layer->layer_tree_impl()->OverscrollElasticityLayer();
    gfx::Vector2dF elastic_overscroll =
        root_layer->layer_tree_impl()->elastic_overscroll()->Current(
            root_layer->layer_tree_impl()->IsActiveTree());
    float page_scale_factor = 1.f;
    float device_scale_factor = 1.f;
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);
    update_layer_list_impl_.reset(new LayerImplList);
    root_layer->layer_tree_impl()->BuildLayerListForTesting();
    PropertyTrees* property_trees =
        root_layer->layer_tree_impl()->property_trees();
    draw_property_utils::BuildPropertyTreesAndComputeVisibleRects(
        root_layer, page_scale_layer, inner_viewport_scroll_layer,
        outer_viewport_scroll_layer, overscroll_elasticity_layer,
        elastic_overscroll, page_scale_factor, device_scale_factor,
        gfx::Rect(device_viewport_size), gfx::Transform(),
        can_render_to_separate_surface, property_trees,
        update_layer_list_impl_.get());
    draw_property_utils::VerifyClipTreeCalculations(*update_layer_list_impl_,
                                                    property_trees);
    if (!skip_verify_visible_rect_calculations)
      draw_property_utils::VerifyVisibleRectsCalculations(
          *update_layer_list_impl_, property_trees);
  }

  void ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(
      LayerImpl* root_layer) {
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width(), root_layer->bounds().height());
    render_surface_layer_list_impl_.reset(new LayerImplList);

    DCHECK(!root_layer->bounds().IsEmpty());
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root_layer, device_viewport_size,
        render_surface_layer_list_impl_.get());
    inputs.can_adjust_raster_scales = true;
    inputs.can_render_to_separate_surface = false;

    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  }

  bool UpdateLayerListImplContains(int id) const {
    for (auto* layer : *update_layer_list_impl_) {
      if (layer->id() == id)
        return true;
    }
    return false;
  }

  bool UpdateLayerListContains(int id) const {
    for (const auto& layer : update_layer_list_) {
      if (layer->id() == id)
        return true;
    }
    return false;
  }

  const LayerImplList* render_surface_layer_list_impl() const {
    return render_surface_layer_list_impl_.get();
  }
  const LayerImplList* update_layer_list_impl() const {
    return update_layer_list_impl_.get();
  }
  const LayerList& update_layer_list() const { return update_layer_list_; }

  bool VerifyLayerInList(scoped_refptr<Layer> layer,
                         const LayerList* layer_list) {
    return std::find(layer_list->begin(), layer_list->end(), layer) !=
           layer_list->end();
  }

 private:
  std::unique_ptr<std::vector<LayerImpl*>> render_surface_layer_list_impl_;
  LayerList update_layer_list_;
  std::unique_ptr<LayerImplList> update_layer_list_impl_;
};

class LayerTreeHostCommonTest : public LayerTreeHostCommonTestBase,
                                public testing::Test {};

class LayerWithForcedDrawsContent : public Layer {
 public:
  LayerWithForcedDrawsContent() {}

  bool DrawsContent() const override { return true; }

 private:
  ~LayerWithForcedDrawsContent() override {}
};

class LayerTreeSettingsScaleContent : public VerifyTreeCalcsLayerTreeSettings {
 public:
  LayerTreeSettingsScaleContent() {
    layer_transforms_should_scale_layer_contents = true;
  }
};

class LayerTreeHostCommonScalingTest : public LayerTreeHostCommonTestBase,
                                       public testing::Test {
 public:
  LayerTreeHostCommonScalingTest()
      : LayerTreeHostCommonTestBase(LayerTreeSettingsScaleContent()) {}
};

class LayerTreeHostCommonDrawRectsTest : public LayerTreeHostCommonTest {
 public:
  LayerTreeHostCommonDrawRectsTest() : LayerTreeHostCommonTest() {}

  LayerImpl* TestVisibleRectAndDrawableContentRect(
      const gfx::Rect& target_rect,
      const gfx::Transform& layer_transform,
      const gfx::Rect& layer_rect) {
    LayerImpl* root = root_layer_for_testing();
    LayerImpl* target = AddChild<LayerImpl>(root);
    LayerImpl* drawing_layer = AddChild<LayerImpl>(target);

    root->SetDrawsContent(true);
    target->SetDrawsContent(true);
    target->SetMasksToBounds(true);
    drawing_layer->SetDrawsContent(true);

    root->SetBounds(gfx::Size(500, 500));
    root->test_properties()->force_render_surface = true;
    target->SetPosition(gfx::PointF(target_rect.origin()));
    target->SetBounds(target_rect.size());
    target->test_properties()->force_render_surface = true;
    drawing_layer->test_properties()->transform = layer_transform;
    drawing_layer->SetPosition(gfx::PointF(layer_rect.origin()));
    drawing_layer->SetBounds(layer_rect.size());
    drawing_layer->test_properties()->should_flatten_transform = false;

    host_impl()->active_tree()->property_trees()->needs_rebuild = true;
    ExecuteCalculateDrawProperties(root);

    return drawing_layer;
  }
};

TEST_F(LayerTreeHostCommonTest, TransformsForNoOpLayer) {
  // Sanity check: For layers positioned at zero, with zero size,
  // and with identity transforms, then the draw transform,
  // screen space transform, and the hierarchy passed on to children
  // layers should also be identity transforms.

  LayerImpl* parent = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  parent->SetBounds(gfx::Size(100, 100));

  ExecuteCalculateDrawProperties(parent);

  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  child->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  grand_child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  grand_child->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, EffectTreeTransformIdTest) {
  // Tests that effect tree node gets a valid transform id when a layer
  // has opacity but doesn't create a render surface.
  LayerImpl* parent = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(parent);
  child->SetDrawsContent(true);

  parent->SetBounds(gfx::Size(100, 100));
  child->SetPosition(gfx::PointF(10, 10));
  child->SetBounds(gfx::Size(100, 100));
  child->test_properties()->opacity = 0.f;
  ExecuteCalculateDrawProperties(parent);
  EffectTree& effect_tree =
      parent->layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(child->effect_tree_index());
  const int transform_tree_size = parent->layer_tree_impl()
                                      ->property_trees()
                                      ->transform_tree.next_available_id();
  EXPECT_LT(node->transform_id, transform_tree_size);
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleLayer) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* layer = AddChild<LayerImpl>(root);

  TransformTree& transform_tree =
      host_impl()->active_tree()->property_trees()->transform_tree;
  EffectTree& effect_tree =
      host_impl()->active_tree()->property_trees()->effect_tree;

  root->SetBounds(gfx::Size(1, 2));

  // Case 1: Setting the bounds of the layer should not affect either the draw
  // transform or the screenspace transform.
  layer->SetBounds(gfx::Size(10, 12));
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 2: The anchor point by itself (without a layer transform) should have
  // no effect on the transforms.
  layer->test_properties()->transform_origin = gfx::Point3F(2.5f, 3.0f, 0.f);
  layer->SetBounds(gfx::Size(10, 12));
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 3: A change in actual position affects both the draw transform and
  // screen space transform.
  gfx::Transform position_transform;
  position_transform.Translate(0.f, 1.2f);
  layer->SetPosition(gfx::PointF(0.f, 1.2f));
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      position_transform,
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      position_transform,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 4: In the correct sequence of transforms, the layer transform should
  // pre-multiply the translation-to-center. This is easily tested by using a
  // scale transform, because scale and translation are not commutative.
  gfx::Transform layer_transform;
  layer_transform.Scale3d(2.0, 2.0, 1.0);
  layer->test_properties()->transform = layer_transform;
  layer->test_properties()->transform_origin = gfx::Point3F();
  layer->SetPosition(gfx::PointF());
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      layer_transform,
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      layer_transform,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 5: The layer transform should occur with respect to the anchor point.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(5.0, 0.0);
  gfx::Transform expected_result =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);
  layer->test_properties()->transform_origin = gfx::Point3F(5.f, 0.f, 0.f);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_result,
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_result,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 6: Verify that position pre-multiplies the layer transform.  The
  // current implementation of CalculateDrawProperties does this implicitly, but
  // it is still worth testing to detect accidental regressions.
  expected_result = position_transform * translation_to_anchor *
                    layer_transform * Inverse(translation_to_anchor);
  layer->SetPosition(gfx::PointF(0.f, 1.2f));
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_result,
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_result,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));
}

TEST_F(LayerTreeHostCommonTest, TransformsAboutScrollOffset) {
  const gfx::ScrollOffset kScrollOffset(50, 100);
  const gfx::Vector2dF kScrollDelta(2.34f, 5.67f);
  const gfx::Vector2d kMaxScrollOffset(200, 200);
  const gfx::PointF kScrollLayerPosition(-kScrollOffset.x(),
                                         -kScrollOffset.y());
  float page_scale = 0.888f;
  const float kDeviceScale = 1.666f;

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> sublayer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* sublayer = sublayer_scoped_ptr.get();
  sublayer->SetDrawsContent(true);
  sublayer->SetBounds(gfx::Size(500, 500));

  std::unique_ptr<LayerImpl> scroll_layer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 2));
  LayerImpl* scroll_layer = scroll_layer_scoped_ptr.get();
  scroll_layer->SetBounds(gfx::Size(10, 20));
  std::unique_ptr<LayerImpl> clip_layer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 4));
  LayerImpl* clip_layer = clip_layer_scoped_ptr.get();

  scroll_layer->SetScrollClipLayer(clip_layer->id());
  clip_layer->SetBounds(
      gfx::Size(scroll_layer->bounds().width() + kMaxScrollOffset.x(),
                scroll_layer->bounds().height() + kMaxScrollOffset.y()));
  scroll_layer->SetScrollClipLayer(clip_layer->id());
  SetScrollOffsetDelta(scroll_layer, kScrollDelta);
  gfx::Transform impl_transform;
  scroll_layer->test_properties()->AddChild(std::move(sublayer_scoped_ptr));
  LayerImpl* scroll_layer_raw_ptr = scroll_layer_scoped_ptr.get();
  clip_layer->test_properties()->AddChild(std::move(scroll_layer_scoped_ptr));
  scroll_layer_raw_ptr->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer_raw_ptr->id(),
                                                     kScrollOffset);

  std::unique_ptr<LayerImpl> root(
      LayerImpl::Create(host_impl.active_tree(), 3));
  root->SetBounds(gfx::Size(3, 4));
  root->test_properties()->AddChild(std::move(clip_layer_scoped_ptr));
  root->SetHasRenderSurface(true);
  LayerImpl* root_layer = root.get();
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));

  ExecuteCalculateDrawProperties(root_layer, kDeviceScale, page_scale,
                                 scroll_layer->test_properties()->parent,
                                 nullptr, nullptr);
  gfx::Transform expected_transform;
  gfx::PointF sub_layer_screen_position = kScrollLayerPosition - kScrollDelta;
  expected_transform.Translate(MathUtil::Round(sub_layer_screen_position.x() *
                                               page_scale * kDeviceScale),
                               MathUtil::Round(sub_layer_screen_position.y() *
                                               page_scale * kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->ScreenSpaceTransform());

  gfx::Transform arbitrary_translate;
  const float kTranslateX = 10.6f;
  const float kTranslateY = 20.6f;
  arbitrary_translate.Translate(kTranslateX, kTranslateY);
  scroll_layer->test_properties()->transform = arbitrary_translate;
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_layer, kDeviceScale, page_scale,
                                 scroll_layer->test_properties()->parent,
                                 nullptr, nullptr);
  expected_transform.MakeIdentity();
  expected_transform.Translate(
      MathUtil::Round(kTranslateX * page_scale * kDeviceScale +
                      sub_layer_screen_position.x() * page_scale *
                          kDeviceScale),
      MathUtil::Round(kTranslateY * page_scale * kDeviceScale +
                      sub_layer_screen_position.y() * page_scale *
                          kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->DrawTransform());

  // Test that page scale is updated even when we don't rebuild property trees.
  page_scale = 1.888f;
  root_layer->layer_tree_impl()->SetViewportLayersFromIds(
      Layer::INVALID_ID, scroll_layer->test_properties()->parent->id(),
      Layer::INVALID_ID, Layer::INVALID_ID);
  root_layer->layer_tree_impl()->SetPageScaleOnActiveTree(page_scale);
  EXPECT_FALSE(root_layer->layer_tree_impl()->property_trees()->needs_rebuild);
  ExecuteCalculateDrawProperties(root_layer, kDeviceScale, page_scale,
                                 scroll_layer->test_properties()->parent,
                                 nullptr, nullptr);

  expected_transform.MakeIdentity();
  expected_transform.Translate(
      MathUtil::Round(kTranslateX * page_scale * kDeviceScale +
                      sub_layer_screen_position.x() * page_scale *
                          kDeviceScale),
      MathUtil::Round(kTranslateY * page_scale * kDeviceScale +
                      sub_layer_screen_position.y() * page_scale *
                          kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->DrawTransform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSimpleHierarchy) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  // One-time setup of root layer
  root->SetBounds(gfx::Size(1, 2));

  TransformTree& transform_tree =
      host_impl()->active_tree()->property_trees()->transform_tree;
  EffectTree& effect_tree =
      host_impl()->active_tree()->property_trees()->effect_tree;

  // Case 1: parent's anchor point should not affect child or grand_child.
  parent->test_properties()->transform_origin = gfx::Point3F(2.5f, 3.0f, 0.f);
  parent->SetBounds(gfx::Size(10, 12));
  child->SetBounds(gfx::Size(16, 18));
  grand_child->SetBounds(gfx::Size(76, 78));
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), draw_property_utils::DrawTransform(
                            grand_child, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));

  // Case 2: parent's position affects child and grand_child.
  gfx::Transform parent_position_transform;
  parent_position_transform.Translate(0.f, 1.2f);
  parent->SetPosition(gfx::PointF(0.f, 1.2f));
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_position_transform,
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_position_transform,
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_position_transform, draw_property_utils::DrawTransform(
                                     grand_child, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_position_transform,
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));

  // Case 3: parent's local transform affects child and grandchild
  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  parent->test_properties()->transform = parent_layer_transform;
  parent->SetPosition(gfx::PointF());
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_composite_transform,
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_composite_transform,
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_composite_transform,
      draw_property_utils::DrawTransform(grand_child, transform_tree,
                                         effect_tree));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      parent_composite_transform,
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(1.f, 0.9f, 1.f);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(25.0, 30.0);

  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  gfx::Vector2dF parent_composite_scale =
      MathUtil::ComputeTransform2dScaleComponents(parent_composite_transform,
                                                  1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform surface_sublayer_composite_transform =
      parent_composite_transform * Inverse(surface_sublayer_transform);

  root->SetBounds(gfx::Size(1, 2));
  parent->test_properties()->transform = parent_layer_transform;
  parent->test_properties()->transform_origin = gfx::Point3F(2.5f, 30.f, 0.f);
  parent->SetBounds(gfx::Size(100, 120));
  child->SetBounds(gfx::Size(16, 18));
  child->test_properties()->force_render_surface = true;
  grand_child->SetBounds(gfx::Size(8, 10));
  grand_child->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  // Render surface should have been created now.
  ASSERT_TRUE(child->render_surface());
  ASSERT_EQ(child->render_surface(), child->render_target());

  // The child layer's draw transform should refer to its new render surface.
  // The screen-space transform, however, should still refer to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(surface_sublayer_transform,
                                  child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->ScreenSpaceTransform());

  // Because the grand_child is the only drawable content, the child's render
  // surface will tighten its bounds to the grand_child.  The scale at which the
  // surface's subtree is drawn must be removed from the composite transform.
  EXPECT_TRANSFORMATION_MATRIX_EQ(surface_sublayer_composite_transform,
                                  child->render_target()->draw_transform());

  // The screen space is the same as the target since the child surface draws
  // into the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_sublayer_composite_transform,
      child->render_target()->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsWhenCannotRenderToSeparateSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform parent_transform;
  parent_transform.Translate(10.0, 10.0);

  gfx::Transform child_transform;
  child_transform.Rotate(45.0);

  root->SetBounds(gfx::Size(100, 100));
  parent->test_properties()->transform = parent_transform;
  parent->SetBounds(gfx::Size(10, 10));
  child->test_properties()->transform = child_transform;
  child->SetBounds(gfx::Size(10, 10));
  child->test_properties()->force_render_surface = true;
  grand_child->SetPosition(gfx::PointF(2.f, 2.f));
  grand_child->SetBounds(gfx::Size(20, 20));
  grand_child->SetDrawsContent(true);

  gfx::Transform expected_grand_child_screen_space_transform;
  expected_grand_child_screen_space_transform.Translate(10.0, 10.0);
  expected_grand_child_screen_space_transform.Rotate(45.0);
  expected_grand_child_screen_space_transform.Translate(2.0, 2.0);

  // First compute draw properties with separate surfaces enabled.
  ExecuteCalculateDrawProperties(root);

  // The grand child's draw transform should be its offset wrt the child.
  gfx::Transform expected_grand_child_draw_transform;
  expected_grand_child_draw_transform.Translate(2.0, 2.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_draw_transform,
                                  grand_child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->ScreenSpaceTransform());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);

  // With separate surfaces disabled, the grand child's draw transform should be
  // the same as its screen space transform.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForRenderSurfaceHierarchy) {
  // This test creates a more complex tree and verifies it all at once. This
  // covers the following cases:
  //   - layers that are described w.r.t. a render surface: should have draw
  //   transforms described w.r.t. that surface
  //   - A render surface described w.r.t. an ancestor render surface: should
  //   have a draw transform described w.r.t. that ancestor surface
  //   - Sanity check on recursion: verify transforms of layers described w.r.t.
  //   a render surface that is described w.r.t. an ancestor render surface.
  //   - verifying that each layer has a reference to the correct render surface
  //   and render target values.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  parent->SetDrawsContent(true);
  LayerImpl* render_surface1 = AddChild<LayerImpl>(parent);
  render_surface1->SetDrawsContent(true);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(render_surface1);
  render_surface2->SetDrawsContent(true);
  LayerImpl* child_of_root = AddChild<LayerImpl>(parent);
  child_of_root->SetDrawsContent(true);
  LayerImpl* child_of_rs1 = AddChild<LayerImpl>(render_surface1);
  child_of_rs1->SetDrawsContent(true);
  LayerImpl* child_of_rs2 = AddChild<LayerImpl>(render_surface2);
  child_of_rs2->SetDrawsContent(true);
  LayerImpl* grand_child_of_root = AddChild<LayerImpl>(child_of_root);
  grand_child_of_root->SetDrawsContent(true);
  LayerImpl* grand_child_of_rs1 = AddChild<LayerImpl>(child_of_rs1);
  grand_child_of_rs1->SetDrawsContent(true);
  LayerImpl* grand_child_of_rs2 = AddChild<LayerImpl>(child_of_rs2);
  grand_child_of_rs2->SetDrawsContent(true);

  // In combination with descendant draws content, opacity != 1 forces the layer
  // to have a new render surface.
  render_surface1->test_properties()->opacity = 0.5f;
  render_surface2->test_properties()->opacity = 0.33f;

  // All layers in the tree are initialized with an anchor at .25 and a size of
  // (10,10).  Matrix "A" is the composite layer transform used in all layers.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(2.5, 0.0);
  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);

  gfx::Transform A =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);

  gfx::Vector2dF surface1_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(A, 1.f);
  gfx::Transform surface1_sublayer_transform;
  surface1_sublayer_transform.Scale(surface1_parent_transform_scale.x(),
                                    surface1_parent_transform_scale.y());

  // SS1 = transform given to the subtree of render_surface1
  gfx::Transform SS1 = surface1_sublayer_transform;
  // S1 = transform to move from render_surface1 pixels to the layer space of
  // the owning layer
  gfx::Transform S1 = Inverse(surface1_sublayer_transform);

  gfx::Vector2dF surface2_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(SS1 * A, 1.f);
  gfx::Transform surface2_sublayer_transform;
  surface2_sublayer_transform.Scale(surface2_parent_transform_scale.x(),
                                    surface2_parent_transform_scale.y());

  // SS2 = transform given to the subtree of render_surface2
  gfx::Transform SS2 = surface2_sublayer_transform;
  // S2 = transform to move from render_surface2 pixels to the layer space of
  // the owning layer
  gfx::Transform S2 = Inverse(surface2_sublayer_transform);

  root->SetBounds(gfx::Size(1, 2));
  parent->test_properties()->transform_origin = gfx::Point3F(2.5f, 0.f, 0.f);
  parent->test_properties()->transform = layer_transform;
  parent->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  render_surface1->test_properties()->transform = layer_transform;
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  render_surface2->test_properties()->transform = layer_transform;
  render_surface2->SetBounds(gfx::Size(10, 10));
  render_surface2->test_properties()->force_render_surface = true;
  child_of_root->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_root->test_properties()->transform = layer_transform;
  child_of_root->SetBounds(gfx::Size(10, 10));
  child_of_rs1->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_rs1->test_properties()->transform = layer_transform;
  child_of_rs1->SetBounds(gfx::Size(10, 10));
  child_of_rs2->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_rs2->test_properties()->transform = layer_transform;
  child_of_rs2->SetBounds(gfx::Size(10, 10));
  grand_child_of_root->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_root->test_properties()->transform = layer_transform;
  grand_child_of_root->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs1->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_rs1->test_properties()->transform = layer_transform;
  grand_child_of_rs1->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs2->test_properties()->transform_origin =
      gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_rs2->test_properties()->transform = layer_transform;
  grand_child_of_rs2->SetBounds(gfx::Size(10, 10));

  ExecuteCalculateDrawProperties(root);

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(root->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(root->render_surface(), parent->render_target());
  EXPECT_EQ(root->render_surface(), child_of_root->render_target());
  EXPECT_EQ(root->render_surface(), grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1->render_surface(),
            render_surface1->render_target());
  EXPECT_EQ(render_surface1->render_surface(), child_of_rs1->render_target());
  EXPECT_EQ(render_surface1->render_surface(),
            grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2->render_surface(),
            render_surface2->render_target());
  EXPECT_EQ(render_surface2->render_surface(), child_of_rs2->render_target());
  EXPECT_EQ(render_surface2->render_surface(),
            grand_child_of_rs2->render_target());

  // Verify layer draw transforms note that draw transforms are described with
  // respect to the nearest ancestor render surface but screen space transforms
  // are described with respect to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A, child_of_root->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  grand_child_of_root->DrawTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1, render_surface1->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * A, child_of_rs1->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * A * A,
                                  grand_child_of_rs1->DrawTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2, render_surface2->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * A, child_of_rs2->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * A * A,
                                  grand_child_of_rs2->DrawTransform());

  // Verify layer screen-space transforms
  //
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A, child_of_root->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  grand_child_of_root->ScreenSpaceTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A,
                                  render_surface1->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  child_of_rs1->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A,
                                  grand_child_of_rs1->ScreenSpaceTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  render_surface2->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A,
                                  child_of_rs2->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A * A,
                                  grand_child_of_rs2->ScreenSpaceTransform());

  // Verify render surface transforms.
  //
  // Draw transform of render surface 1 is described with respect to root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * S1, render_surface1->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * S1, render_surface1->render_surface()->screen_space_transform());
  // Draw transform of render surface 2 is described with respect to render
  // surface 1.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      SS1 * A * S2, render_surface2->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * A * S2,
      render_surface2->render_surface()->screen_space_transform());

  // Sanity check. If these fail there is probably a bug in the test itself.  It
  // is expected that we correctly set up transforms so that the y-component of
  // the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->ScreenSpaceTransform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0, child_of_rs1->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->ScreenSpaceTransform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0, child_of_rs2->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->ScreenSpaceTransform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, TransformsForFlatteningLayer) {
  // For layers that flatten their subtree, there should be an orthographic
  // projection (for x and y values) in the middle of the transform sequence.
  // Note that the way the code is currently implemented, it is not expected to
  // use a canonical orthographic projection.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  child->SetDrawsContent(true);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  grand_child->SetDrawsContent(true);
  LayerImpl* great_grand_child = AddChild<LayerImpl>(grand_child);
  great_grand_child->SetDrawsContent(true);

  gfx::Transform rotation_about_y_axis;
  rotation_about_y_axis.RotateAboutYAxis(30.0);

  root->SetBounds(gfx::Size(100, 100));
  child->test_properties()->transform = rotation_about_y_axis;
  child->SetBounds(gfx::Size(10, 10));
  child->test_properties()->force_render_surface = true;
  grand_child->test_properties()->transform = rotation_about_y_axis;
  grand_child->SetBounds(gfx::Size(10, 10));
  great_grand_child->SetBounds(gfx::Size(10, 10));

  // No layers in this test should preserve 3d.
  ASSERT_TRUE(root->test_properties()->should_flatten_transform);
  ASSERT_TRUE(child->test_properties()->should_flatten_transform);
  ASSERT_TRUE(grand_child->test_properties()->should_flatten_transform);
  ASSERT_TRUE(great_grand_child->test_properties()->should_flatten_transform);

  gfx::Transform expected_child_draw_transform = rotation_about_y_axis;
  gfx::Transform expected_child_screen_space_transform = rotation_about_y_axis;
  gfx::Transform expected_grand_child_draw_transform =
      rotation_about_y_axis;  // draws onto child's render surface
  gfx::Transform flattened_rotation_about_y = rotation_about_y_axis;
  flattened_rotation_about_y.FlattenTo2d();
  gfx::Transform expected_grand_child_screen_space_transform =
      flattened_rotation_about_y * rotation_about_y_axis;
  gfx::Transform expected_great_grand_child_draw_transform =
      flattened_rotation_about_y;
  gfx::Transform expected_great_grand_child_screen_space_transform =
      flattened_rotation_about_y * flattened_rotation_about_y;

  ExecuteCalculateDrawProperties(root);

  // The child's draw transform should have been taken by its surface.
  ASSERT_TRUE(child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_draw_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_child_screen_space_transform,
      child->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_screen_space_transform,
                                  child->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_draw_transform,
                                  grand_child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_draw_transform,
                                  great_grand_child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_screen_space_transform,
      great_grand_child->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, LayerFullyContainedWithinClipInTargetSpace) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform child_transform;
  child_transform.Translate(50.0, 50.0);
  child_transform.RotateAboutZAxis(30.0);

  gfx::Transform grand_child_transform;
  grand_child_transform.RotateAboutYAxis(90.0);

  root->SetBounds(gfx::Size(200, 200));
  child->test_properties()->transform = child_transform;
  child->SetBounds(gfx::Size(10, 10));
  grand_child->test_properties()->transform = grand_child_transform;
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->test_properties()->should_flatten_transform = false;
  grand_child->SetDrawsContent(true);
  float device_scale_factor = 1.f;
  float page_scale_factor = 1.f;
  LayerImpl* page_scale_layer = nullptr;
  LayerImpl* inner_viewport_scroll_layer = nullptr;
  LayerImpl* outer_viewport_scroll_layer = nullptr;
  // Visible rects computed by combining clips in target space and root space
  // don't match because of rotation transforms. So, we skip
  // verify_visible_rect_calculations.
  bool skip_verify_visible_rect_calculations = true;
  ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                 page_scale_layer, inner_viewport_scroll_layer,
                                 outer_viewport_scroll_layer,
                                 skip_verify_visible_rect_calculations);

  // Mapping grand_child's bounds to target space produces a non-empty rect
  // that is fully contained within the target's bounds, so grand_child should
  // be considered fully visible.
  EXPECT_EQ(gfx::Rect(grand_child->bounds()),
            grand_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, TransformsForDegenerateIntermediateLayer) {
  // A layer that is empty in one axis, but not the other, was accidentally
  // skipping a necessary translation.  Without that translation, the coordinate
  // space of the layer's draw transform is incorrect.
  //
  // Normally this isn't a problem, because the layer wouldn't be drawn anyway,
  // but if that layer becomes a render surface, then its draw transform is
  // implicitly inherited by the rest of the subtree, which then is positioned
  // incorrectly as a result.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  grand_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(100, 100));
  // The child height is zero, but has non-zero width that should be accounted
  // for while computing draw transforms.
  child->SetBounds(gfx::Size(10, 0));
  child->test_properties()->force_render_surface = true;
  grand_child->SetBounds(gfx::Size(10, 10));
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(child->has_render_surface());
  // This is the real test, the rest are sanity checks.
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  grand_child->DrawTransform());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceWithSublayerScale) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(render_surface);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform translate;
  translate.Translate3d(5, 5, 5);

  root->SetBounds(gfx::Size(100, 100));
  render_surface->test_properties()->transform = translate;
  render_surface->SetBounds(gfx::Size(100, 100));
  render_surface->test_properties()->force_render_surface = true;
  child->test_properties()->transform = translate;
  child->SetBounds(gfx::Size(100, 100));
  grand_child->test_properties()->transform = translate;
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);

  // render_surface will have a sublayer scale because of device scale factor.
  float device_scale_factor = 2.0f;
  LayerImplList render_surface_layer_list_impl;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, root->bounds(), translate, &render_surface_layer_list_impl);
  inputs.device_scale_factor = device_scale_factor;
  inputs.property_trees->needs_rebuild = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // Between grand_child and render_surface, we translate by (10, 10) and scale
  // by a factor of 2.
  gfx::Vector2dF expected_translation(20.0f, 20.0f);
  EXPECT_EQ(grand_child->DrawTransform().To2dTranslation(),
            expected_translation);
}

TEST_F(LayerTreeHostCommonTest, TransformAboveRootLayer) {
  // Transformations applied at the root of the tree should be forwarded
  // to child layers instead of applied to the root RenderSurface.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  root->SetDrawsContent(true);
  root->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  child->SetScrollClipLayer(root->id());
  child->SetBounds(gfx::Size(100, 100));
  child->SetMasksToBounds(true);

  gfx::Transform translate;
  translate.Translate(50, 50);
  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), translate, &render_surface_layer_list_impl);
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        translate, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        translate, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(translate, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(50, 50, 100, 100), child->clip_rect());
  }

  gfx::Transform scale;
  scale.Scale(2, 2);
  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), scale, &render_surface_layer_list_impl);
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        scale, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        scale, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(scale, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200), child->clip_rect());
  }

  gfx::Transform rotate;
  rotate.Rotate(2);
  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), rotate, &render_surface_layer_list_impl);
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        rotate, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        rotate, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotate, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(-4, 0, 104, 104), child->clip_rect());
  }

  gfx::Transform composite;
  composite.ConcatTransform(translate);
  composite.ConcatTransform(scale);
  composite.ConcatTransform(rotate);
  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), composite, &render_surface_layer_list_impl);
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        composite, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        composite, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(composite, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(89, 103, 208, 208), child->clip_rect());
  }

  // Verify it composes correctly with device scale.
  float device_scale_factor = 1.5f;

  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), translate, &render_surface_layer_list_impl);
    inputs.device_scale_factor = device_scale_factor;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    gfx::Transform device_scaled_translate = translate;
    device_scaled_translate.Scale(device_scale_factor, device_scale_factor);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        device_scaled_translate,
        root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        device_scaled_translate,
        child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(device_scaled_translate,
                                    child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(50, 50, 150, 150), child->clip_rect());
  }

  // Verify it composes correctly with page scale.
  float page_scale_factor = 2.f;

  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), translate, &render_surface_layer_list_impl);
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_layer = root;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    gfx::Transform page_scaled_translate = translate;
    page_scaled_translate.Scale(page_scale_factor, page_scale_factor);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        page_scaled_translate, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        page_scaled_translate, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(page_scaled_translate,
                                    child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(50, 50, 200, 200), child->clip_rect());
  }

  // Verify that it composes correctly with transforms directly on root layer.
  root->test_properties()->transform = composite;

  {
    LayerImplList render_surface_layer_list_impl;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), composite, &render_surface_layer_list_impl);
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    gfx::Transform compositeSquared = composite;
    compositeSquared.ConcatTransform(composite);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeSquared, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeSquared, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                    root->render_surface()->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(compositeSquared,
                                    child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(254, 316, 428, 428), child->clip_rect());
  }
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceListForRenderSurfaceWithClippedLayer) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);

  root->SetBounds(gfx::Size(10, 10));
  root->SetMasksToBounds(true);
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  child->SetDrawsContent(true);
  child->SetPosition(gfx::PointF(30.f, 30.f));
  child->SetBounds(gfx::Size(10, 10));
  ExecuteCalculateDrawProperties(root);

  // The child layer's content is entirely outside the root's clip rect, so
  // the intermediate render surface should not be listed here, even if it was
  // forced to be created. Render surfaces without children or visible content
  // are unexpected at draw time (e.g. we might try to create a content texture
  // of size 0).
  ASSERT_TRUE(root->render_surface());
  EXPECT_EQ(1U, render_surface_layer_list_impl()->size());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceListForTransparentChild) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);

  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  render_surface1->test_properties()->opacity = 0.f;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // Since the layer is transparent, render_surface1->render_surface() should
  // not have gotten added anywhere.  Also, the drawable content rect should not
  // have been extended by the children.
  ASSERT_TRUE(root->render_surface());
  EXPECT_EQ(0U, root->render_surface()->layer_list().size());
  EXPECT_EQ(1U, render_surface_layer_list.size());
  EXPECT_EQ(root->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(gfx::Rect(), root->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceListForTransparentChildWithBackgroundFilter) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);

  root->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  render_surface1->test_properties()->opacity = 0.f;
  render_surface1->SetDrawsContent(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(1.5f));
  render_surface1->test_properties()->background_filters = filters;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
    EXPECT_EQ(2U, render_surface_layer_list.size());
  }
  // The layer is fully transparent, but has a background filter, so it
  // shouldn't be skipped and should be drawn.
  ASSERT_TRUE(root->render_surface());
  EXPECT_EQ(1U, root->render_surface()->layer_list().size());
  EXPECT_EQ(gfx::RectF(0, 0, 10, 10),
            root->render_surface()->DrawableContentRect());
  EffectTree& effect_tree =
      root->layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(render_surface1->effect_tree_index());
  EXPECT_TRUE(node->is_drawn);

  // When root is transparent, the layer should not be drawn.
  effect_tree.OnOpacityAnimated(0.f, root->effect_tree_index(),
                                root->layer_tree_impl());
  effect_tree.OnOpacityAnimated(1.f, render_surface1->effect_tree_index(),
                                root->layer_tree_impl());
  render_surface1->set_visible_layer_rect(gfx::Rect());
  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  }

  node = effect_tree.Node(render_surface1->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);
  EXPECT_EQ(gfx::Rect(), render_surface1->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceListForFilter) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2.0f, 2.0f);

  root->SetBounds(gfx::Size(100, 100));
  parent->test_properties()->transform = scale_matrix;
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(10.0f));
  parent->test_properties()->filters = filters;
  parent->test_properties()->force_render_surface = true;
  child1->SetBounds(gfx::Size(25, 25));
  child1->SetDrawsContent(true);
  child1->test_properties()->force_render_surface = true;
  child2->SetPosition(gfx::PointF(25, 25));
  child2->SetBounds(gfx::Size(25, 25));
  child2->SetDrawsContent(true);
  child2->test_properties()->force_render_surface = true;

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  ASSERT_TRUE(parent->render_surface());
  EXPECT_EQ(2U, parent->render_surface()->layer_list().size());
  EXPECT_EQ(4U, render_surface_layer_list.size());

  // The rectangle enclosing child1 and child2 (0,0 50x50), expanded for the
  // blur (-30,-30 110x110), and then scaled by the scale matrix
  // (-60,-60 220x220).
  EXPECT_EQ(gfx::RectF(-60, -60, 220, 220),
            parent->render_surface()->DrawableContentRect());
}

TEST_F(LayerTreeHostCommonTest, DrawableContentRectForReferenceFilter) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(25, 25));
  child->SetDrawsContent(true);
  child->test_properties()->force_render_surface = true;
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      SkOffsetImageFilter::Make(50, 50, nullptr)));
  child->test_properties()->filters = filters;
  ExecuteCalculateDrawProperties(root);

  // The render surface's size should be unaffected by the offset image filter;
  // it need only have a drawable content rect large enough to contain the
  // contents (at the new offset).
  ASSERT_TRUE(child->render_surface());
  EXPECT_EQ(gfx::RectF(50, 50, 25, 25),
            child->render_surface()->DrawableContentRect());
}

TEST_F(LayerTreeHostCommonTest, DrawableContentRectForReferenceFilterHighDpi) {
  const float device_scale_factor = 2.0f;

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(25, 25));
  child->SetDrawsContent(true);
  child->test_properties()->force_render_surface = true;

  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      SkOffsetImageFilter::Make(50, 50, nullptr)));
  child->test_properties()->filters = filters;

  ExecuteCalculateDrawProperties(root, device_scale_factor);

  // The render surface's size should be unaffected by the offset image filter;
  // it need only have a drawable content rect large enough to contain the
  // contents (at the new offset). All coordinates should be scaled by 2,
  // corresponding to the device scale factor.
  ASSERT_TRUE(child->render_surface());
  EXPECT_EQ(gfx::RectF(100, 100, 50, 50),
            child->render_surface()->DrawableContentRect());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceForBlendMode) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  child->test_properties()->blend_mode = SkXfermode::kMultiply_Mode;
  child->test_properties()->opacity = 0.5f;
  child->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);

  // Since the child layer has a blend mode other than normal, it should get
  // its own render surface. Also, layer's draw_properties should contain the
  // default blend mode, since the render surface becomes responsible for
  // applying the blend mode.
  ASSERT_TRUE(child->render_surface());
  EXPECT_EQ(1.0f, child->draw_opacity());
  EXPECT_EQ(0.5f, child->render_surface()->draw_opacity());
  EXPECT_EQ(SkXfermode::kSrcOver_Mode, child->draw_blend_mode());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceDrawOpacity) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* not_surface = AddChild<LayerImpl>(surface1);
  LayerImpl* surface2 = AddChild<LayerImpl>(not_surface);

  root->SetBounds(gfx::Size(10, 10));
  surface1->SetBounds(gfx::Size(10, 10));
  surface1->SetDrawsContent(true);
  surface1->test_properties()->opacity = 0.5f;
  surface1->test_properties()->force_render_surface = true;
  not_surface->SetBounds(gfx::Size(10, 10));
  not_surface->test_properties()->opacity = 0.5f;
  surface2->SetBounds(gfx::Size(10, 10));
  surface2->SetDrawsContent(true);
  surface2->test_properties()->opacity = 0.5f;
  surface2->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(surface1->render_surface());
  ASSERT_FALSE(not_surface->render_surface());
  ASSERT_TRUE(surface2->render_surface());
  EXPECT_EQ(0.5f, surface1->render_surface()->draw_opacity());
  // surface2's draw opacity should include the opacity of not-surface and
  // itself, but not the opacity of surface1.
  EXPECT_EQ(0.25f, surface2->render_surface()->draw_opacity());
}

TEST_F(LayerTreeHostCommonTest, DrawOpacityWhenCannotRenderToSeparateSurface) {
  // Tests that when separate surfaces are disabled, a layer's draw opacity is
  // the product of all ancestor layer opacties and the layer's own opacity.
  // (Rendering will still be incorrect in situations where we really do need
  // surfaces to apply opacity, such as when we have overlapping layers with an
  // ancestor whose opacity is <1.)
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child1);
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(child2);

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child1->SetBounds(gfx::Size(100, 100));
  child1->SetDrawsContent(true);
  child1->test_properties()->opacity = 0.5f;
  child1->test_properties()->force_render_surface = true;
  child2->SetBounds(gfx::Size(100, 100));
  child2->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  grand_child->test_properties()->opacity = 0.5f;
  grand_child->test_properties()->force_render_surface = true;
  leaf_node1->SetBounds(gfx::Size(100, 100));
  leaf_node1->SetDrawsContent(true);
  leaf_node1->test_properties()->opacity = 0.5f;
  leaf_node2->SetBounds(gfx::Size(100, 100));
  leaf_node2->SetDrawsContent(true);
  leaf_node2->test_properties()->opacity = 0.5f;

  // With surfaces enabled, each layer's draw opacity is the product of layer
  // opacities on the path from the layer to its render target, not including
  // the opacity of the layer that owns the target surface (since that opacity
  // is applied by the surface).
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(1.f, root->draw_opacity());
  EXPECT_EQ(1.f, parent->draw_opacity());
  EXPECT_EQ(1.f, child1->draw_opacity());
  EXPECT_EQ(1.f, child2->draw_opacity());
  EXPECT_EQ(1.f, grand_child->draw_opacity());
  EXPECT_EQ(0.5f, leaf_node1->draw_opacity());
  EXPECT_EQ(0.5f, leaf_node2->draw_opacity());

  // With surfaces disabled, each layer's draw opacity is the product of layer
  // opacities on the path from the layer to the root.
  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_EQ(1.f, root->draw_opacity());
  EXPECT_EQ(1.f, parent->draw_opacity());
  EXPECT_EQ(0.5f, child1->draw_opacity());
  EXPECT_EQ(1.f, child2->draw_opacity());
  EXPECT_EQ(0.25f, grand_child->draw_opacity());
  EXPECT_EQ(0.125f, leaf_node1->draw_opacity());
  EXPECT_EQ(0.5f, leaf_node2->draw_opacity());
}

TEST_F(LayerTreeHostCommonTest, ForceRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);

  root->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  {
    ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

    // The root layer always creates a render surface
    EXPECT_TRUE(root->has_render_surface());
    EXPECT_TRUE(render_surface1->has_render_surface());
  }

  {
    render_surface1->test_properties()->force_render_surface = false;
    render_surface1->layer_tree_impl()->property_trees()->needs_rebuild = true;
    ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);
    EXPECT_TRUE(root->has_render_surface());
    EXPECT_FALSE(render_surface1->has_render_surface());
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfacesFlattenScreenSpaceTransform) {
  // Render surfaces act as a flattening point for their subtree, so should
  // always flatten the target-to-screen space transform seen by descendants.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform rotation_about_y_axis;
  rotation_about_y_axis.RotateAboutYAxis(30.0);

  root->SetBounds(gfx::Size(100, 100));
  parent->test_properties()->transform = rotation_about_y_axis;
  parent->SetBounds(gfx::Size(10, 10));
  parent->test_properties()->force_render_surface = true;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);
  grand_child->test_properties()->should_flatten_transform = false;
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(parent->render_surface());
  EXPECT_FALSE(child->render_surface());
  EXPECT_FALSE(grand_child->render_surface());

  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  grand_child->DrawTransform());

  // The screen-space transform inherited by |child| and |grand_child| should
  // have been flattened at their render target. In particular, the fact that
  // |grand_child| happens to preserve 3d shouldn't affect this flattening.
  gfx::Transform flattened_rotation_about_y = rotation_about_y_axis;
  flattened_rotation_about_y.FlattenTo2d();
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_y,
                                  child->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_y,
                                  grand_child->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsRenderSurfaces) {
  // The entire subtree of layers that are outside the clip rect should be
  // culled away, and should not affect the render_surface_layer_list.
  //
  // The test tree is set up as follows:
  //  - all layers except the leaf_nodes are forced to be a new render surface
  //  that have something to draw.
  //  - parent is a large container layer.
  //  - child has MasksToBounds=true to cause clipping.
  //  - grand_child is positioned outside of the child's bounds
  //  - great_grand_child is also kept outside child's bounds.
  //
  // In this configuration, grand_child and great_grand_child are completely
  // outside the clip rect, and they should never get scheduled on the list of
  // render surfaces.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  LayerImpl* great_grand_child = AddChild<LayerImpl>(grand_child);

  // leaf_node1 ensures that root and child are kept on the
  // render_surface_layer_list, even though grand_child and great_grand_child
  // should be clipped.
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(child);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(great_grand_child);

  root->SetBounds(gfx::Size(500, 500));
  child->SetBounds(gfx::Size(20, 20));
  child->SetMasksToBounds(true);
  child->test_properties()->force_render_surface = true;
  grand_child->SetPosition(gfx::PointF(45.f, 45.f));
  grand_child->SetBounds(gfx::Size(10, 10));
  great_grand_child->SetBounds(gfx::Size(10, 10));
  leaf_node1->SetBounds(gfx::Size(500, 500));
  leaf_node1->SetDrawsContent(true);
  leaf_node1->SetBounds(gfx::Size(20, 20));
  leaf_node2->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  ASSERT_EQ(2U, render_surface_layer_list_impl()->size());
  EXPECT_EQ(root->id(), render_surface_layer_list_impl()->at(0)->id());
  EXPECT_EQ(child->id(), render_surface_layer_list_impl()->at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsSurfaceWithoutVisibleContent) {
  // When a render surface has a clip rect, it is used to clip the content rect
  // of the surface.

  // The test tree is set up as follows:
  //  - root is a container layer that masksToBounds=true to cause clipping.
  //  - child is a render surface, which has a clip rect set to the bounds of
  //  the root.
  //  - grand_child is a render surface, and the only visible content in child.
  //  It is positioned outside of the clip rect from root.

  // In this configuration, grand_child should be outside the clipped
  // content rect of the child, making grand_child not appear in the
  // render_surface_layer_list.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  LayerImpl* leaf_node = AddChild<LayerImpl>(grand_child);

  root->SetMasksToBounds(true);
  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(20, 20));
  child->test_properties()->force_render_surface = true;
  grand_child->SetPosition(gfx::PointF(200.f, 200.f));
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->test_properties()->force_render_surface = true;
  leaf_node->SetBounds(gfx::Size(10, 10));
  leaf_node->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  // We should cull child and grand_child from the
  // render_surface_layer_list.
  ASSERT_EQ(1U, render_surface_layer_list_impl()->size());
  EXPECT_EQ(root->id(), render_surface_layer_list_impl()->at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, IsClippedIsSetCorrectlyLayerImpl) {
  // Tests that LayerImpl's IsClipped() property is set to true when:
  //  - the layer clips its subtree, e.g. masks to bounds,
  //  - the layer is clipped by an ancestor that contributes to the same
  //    render target,
  //  - a surface is clipped by an ancestor that contributes to the same
  //    render target.
  //
  // In particular, for a layer that owns a render surface:
  //  - the render surface inherits any clip from ancestors, and does NOT
  //    pass that clipped status to the layer itself.
  //  - but if the layer itself masks to bounds, it is considered clipped
  //    and propagates the clip to the subtree.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child1);
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(child2);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child1->SetBounds(gfx::Size(100, 100));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(100, 100));
  child2->SetDrawsContent(true);
  child2->test_properties()->force_render_surface = true;
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  leaf_node1->SetBounds(gfx::Size(100, 100));
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetBounds(gfx::Size(100, 100));
  leaf_node2->SetDrawsContent(true);

  // Case 1: nothing is clipped except the root render surface.
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(child2->render_surface());

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(child2->render_surface()->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  // Case 2: parent MasksToBounds, so the parent, child1, and child2's
  // surface are clipped. But layers that contribute to child2's surface are
  // not clipped explicitly because child2's surface already accounts for
  // that clip.
  parent->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(child2->render_surface());

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_TRUE(child2->render_surface()->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  parent->SetMasksToBounds(false);

  // Case 3: child2 MasksToBounds. The layer and subtree are clipped, and
  // child2's render surface is not clipped.
  child2->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(child2->render_surface());

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(child2->render_surface()->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, UpdateClipRectCorrectly) {
  // Tests that when as long as layer is clipped, it's clip rect is set to
  // correct value.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(parent);

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  child->SetMasksToBounds(true);

  ExecuteCalculateDrawProperties(root);

  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_TRUE(child->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), child->clip_rect());

  parent->SetMasksToBounds(true);
  child->SetPosition(gfx::PointF(100.f, 100.f));
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child->is_clipped());
  EXPECT_EQ(gfx::Rect(), child->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, IsClippedWhenCannotRenderToSeparateSurface) {
  // Tests that when separate surfaces are disabled, is_clipped is true exactly
  // when a layer or its ancestor has a clip; in particular, if a layer
  // is_clipped, so is its entire subtree (since there are no render surfaces
  // that can reset is_clipped).
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child1);
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(child2);

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child1->SetBounds(gfx::Size(100, 100));
  child1->SetDrawsContent(true);
  child1->test_properties()->force_render_surface = true;
  child2->SetBounds(gfx::Size(100, 100));
  child2->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  grand_child->test_properties()->force_render_surface = true;
  leaf_node1->SetBounds(gfx::Size(100, 100));
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetBounds(gfx::Size(100, 100));
  leaf_node2->SetDrawsContent(true);

  // Case 1: Nothing is clipped. In this case, is_clipped is always false, with
  // or without surfaces.
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  // Case 2: The root is clipped. With surfaces, this only persists until the
  // next render surface. Without surfaces, the entire tree is clipped.
  root->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_TRUE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());

  root->SetMasksToBounds(false);

  // Case 3: The parent is clipped. Again, with surfaces, this only persists
  // until the next render surface. Without surfaces, parent's entire subtree is
  // clipped.
  parent->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());

  parent->SetMasksToBounds(false);

  // Case 4: child1 is clipped. With surfaces, only child1 is_clipped, since it
  // has no non-surface children. Without surfaces, child1's entire subtree is
  // clipped.
  child1->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  child1->SetMasksToBounds(false);

  // Case 5: Only the leaf nodes are clipped. The behavior with and without
  // surfaces is the same.
  leaf_node1->SetMasksToBounds(true);
  leaf_node2->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, DrawableContentRectForLayers) {
  // Verify that layers get the appropriate DrawableContentRect when their
  // parent MasksToBounds is true.
  //
  //   grand_child1 - completely inside the region; DrawableContentRect should
  //   be the layer rect expressed in target space.
  //   grand_child2 - partially clipped but NOT MasksToBounds; the clip rect
  //   will be the intersection of layer bounds and the mask region.
  //   grand_child3 - partially clipped and MasksToBounds; the
  //   DrawableContentRect will still be the intersection of layer bounds and
  //   the mask region.
  //   grand_child4 - outside parent's clip rect; the DrawableContentRect should
  //   be empty.

  LayerImpl* parent = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child1 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child2 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child3 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child4 = AddChild<LayerImpl>(child);

  parent->SetBounds(gfx::Size(500, 500));
  child->SetMasksToBounds(true);
  child->SetBounds(gfx::Size(20, 20));
  child->test_properties()->force_render_surface = true;
  grand_child1->SetPosition(gfx::PointF(5.f, 5.f));
  grand_child1->SetBounds(gfx::Size(10, 10));
  grand_child1->SetDrawsContent(true);
  grand_child2->SetPosition(gfx::PointF(15.f, 15.f));
  grand_child2->SetBounds(gfx::Size(10, 10));
  grand_child2->SetDrawsContent(true);
  grand_child3->SetPosition(gfx::PointF(15.f, 15.f));
  grand_child3->SetMasksToBounds(true);
  grand_child3->SetBounds(gfx::Size(10, 10));
  grand_child3->SetDrawsContent(true);
  grand_child4->SetPosition(gfx::PointF(45.f, 45.f));
  grand_child4->SetBounds(gfx::Size(10, 10));
  grand_child4->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(parent);

  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5), grand_child3->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5), grand_child3->drawable_content_rect());
  EXPECT_TRUE(grand_child4->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest, ClipRectIsPropagatedCorrectlyToSurfaces) {
  // Verify that render surfaces (and their layers) get the appropriate
  // clip rects when their parent MasksToBounds is true.
  //
  // Layers that own render surfaces (at least for now) do not inherit any
  // clipping; instead the surface will enforce the clip for the entire subtree.
  // They may still have a clip rect of their own layer bounds, however, if
  // MasksToBounds was true.
  LayerImpl* parent = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child1 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child2 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child3 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child4 = AddChild<LayerImpl>(child);
  // The leaf nodes ensure that these grand_children become render surfaces for
  // this test.
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child1);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(grand_child2);
  LayerImpl* leaf_node3 = AddChild<LayerImpl>(grand_child3);
  LayerImpl* leaf_node4 = AddChild<LayerImpl>(grand_child4);

  parent->SetBounds(gfx::Size(500, 500));
  child->SetBounds(gfx::Size(20, 20));
  child->SetMasksToBounds(true);
  child->test_properties()->force_render_surface = true;
  grand_child1->SetPosition(gfx::PointF(5.f, 5.f));
  grand_child1->SetBounds(gfx::Size(10, 10));
  grand_child1->test_properties()->force_render_surface = true;
  grand_child2->SetPosition(gfx::PointF(15.f, 15.f));
  grand_child2->SetBounds(gfx::Size(10, 10));
  grand_child2->test_properties()->force_render_surface = true;
  grand_child3->SetPosition(gfx::PointF(15.f, 15.f));
  grand_child3->SetBounds(gfx::Size(10, 10));
  grand_child3->SetMasksToBounds(true);
  grand_child3->test_properties()->force_render_surface = true;
  grand_child4->SetPosition(gfx::PointF(45.f, 45.f));
  grand_child4->SetBounds(gfx::Size(10, 10));
  grand_child4->SetMasksToBounds(true);
  grand_child4->test_properties()->force_render_surface = true;
  leaf_node1->SetBounds(gfx::Size(10, 10));
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetBounds(gfx::Size(10, 10));
  leaf_node2->SetDrawsContent(true);
  leaf_node3->SetBounds(gfx::Size(10, 10));
  leaf_node3->SetDrawsContent(true);
  leaf_node4->SetBounds(gfx::Size(10, 10));
  leaf_node4->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(parent);

  ASSERT_TRUE(grand_child1->render_surface());
  ASSERT_TRUE(grand_child2->render_surface());
  ASSERT_TRUE(grand_child3->render_surface());

  // Surfaces are clipped by their parent, but un-affected by the owning layer's
  // MasksToBounds.
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child1->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child2->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child3->render_surface()->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, ClipRectWhenCannotRenderToSeparateSurface) {
  // Tests that when separate surfaces are disabled, a layer's clip_rect is the
  // intersection of all ancestor clips in screen space; in particular, if a
  // layer masks to bounds, it contributes to the clip_rect of all layers in its
  // subtree (since there are no render surfaces that can reset the clip_rect).
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child1);
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(child2);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetPosition(gfx::PointF(2.f, 2.f));
  parent->SetBounds(gfx::Size(400, 400));
  child1->SetPosition(gfx::PointF(4.f, 4.f));
  child1->SetBounds(gfx::Size(800, 800));
  child2->SetPosition(gfx::PointF(3.f, 3.f));
  child2->SetBounds(gfx::Size(800, 800));
  grand_child->SetPosition(gfx::PointF(8.f, 8.f));
  grand_child->SetBounds(gfx::Size(1500, 1500));
  leaf_node1->SetPosition(gfx::PointF(16.f, 16.f));
  leaf_node1->SetBounds(gfx::Size(2000, 2000));
  leaf_node2->SetPosition(gfx::PointF(9.f, 9.f));
  leaf_node2->SetBounds(gfx::Size(2000, 2000));

  root->SetDrawsContent(true);
  parent->SetDrawsContent(true);
  child1->SetDrawsContent(true);
  child2->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetDrawsContent(true);

  root->test_properties()->force_render_surface = true;
  child1->test_properties()->force_render_surface = true;
  grand_child->test_properties()->force_render_surface = true;

  // Case 1: Nothing is clipped. In this case, each layer's clip rect is its
  // bounds in target space. The only thing that changes when surfaces are
  // disabled is that target space is always screen space.
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(root->has_render_surface());
  EXPECT_FALSE(parent->has_render_surface());
  EXPECT_TRUE(child1->has_render_surface());
  EXPECT_FALSE(child2->has_render_surface());
  EXPECT_TRUE(grand_child->has_render_surface());
  EXPECT_FALSE(leaf_node1->has_render_surface());
  EXPECT_FALSE(leaf_node2->has_render_surface());
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_FALSE(child1->render_surface()->is_clipped());
  EXPECT_FALSE(grand_child->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->render_surface()->clip_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->render_surface()->clip_rect());

  // Case 2: The root is clipped. In this case, layers that draw into the root
  // render surface are clipped by the root's bounds.
  root->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  root->test_properties()->force_render_surface = true;
  child1->test_properties()->force_render_surface = true;
  grand_child->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(root->has_render_surface());
  EXPECT_FALSE(parent->has_render_surface());
  EXPECT_TRUE(child1->has_render_surface());
  EXPECT_FALSE(child2->has_render_surface());
  EXPECT_TRUE(grand_child->has_render_surface());
  EXPECT_FALSE(leaf_node1->has_render_surface());
  EXPECT_FALSE(leaf_node2->has_render_surface());
  EXPECT_TRUE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_TRUE(child1->render_surface()->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(grand_child->render_surface()->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), parent->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), child1->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), child2->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), leaf_node2->clip_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_TRUE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), parent->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), child1->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), child2->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), grand_child->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), leaf_node1->clip_rect());
  EXPECT_EQ(gfx::Rect(100, 100), leaf_node2->clip_rect());

  root->SetMasksToBounds(false);

  // Case 3: The parent and child1 are clipped. When surfaces are enabled, the
  // parent clip rect only contributes to the subtree rooted at child2, since
  // the subtree rooted at child1 renders into a separate surface. Similarly,
  // child1's clip rect doesn't contribute to its descendants, since its only
  // child is a render surface. However, without surfaces, these clip rects
  // contribute to all descendants.
  parent->SetMasksToBounds(true);
  child1->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  root->test_properties()->force_render_surface = true;
  child1->test_properties()->force_render_surface = true;
  grand_child->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(root->has_render_surface());
  EXPECT_FALSE(parent->has_render_surface());
  EXPECT_TRUE(child1->has_render_surface());
  EXPECT_FALSE(child2->has_render_surface());
  EXPECT_TRUE(grand_child->has_render_surface());
  EXPECT_FALSE(leaf_node1->has_render_surface());
  EXPECT_FALSE(leaf_node2->has_render_surface());
  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_TRUE(grand_child->render_surface()->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->clip_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child1->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), child2->clip_rect());
  EXPECT_EQ(gfx::Rect(800, 800), grand_child->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), leaf_node2->clip_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(root->render_surface()->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), root->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->clip_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 396, 396), child1->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), child2->clip_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 396, 396), grand_child->clip_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 396, 396), leaf_node1->clip_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), leaf_node2->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, HitTestingWhenSurfacesDisabled) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  LayerImpl* leaf_node = AddChild<LayerImpl>(grand_child);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetPosition(gfx::PointF(2.f, 2.f));
  parent->SetBounds(gfx::Size(400, 400));
  parent->SetMasksToBounds(true);
  child->SetPosition(gfx::PointF(4.f, 4.f));
  child->SetBounds(gfx::Size(800, 800));
  child->SetMasksToBounds(true);
  child->test_properties()->force_render_surface = true;
  grand_child->SetPosition(gfx::PointF(8.f, 8.f));
  grand_child->SetBounds(gfx::Size(1500, 1500));
  grand_child->test_properties()->force_render_surface = true;
  leaf_node->SetPosition(gfx::PointF(16.f, 16.f));
  leaf_node->SetBounds(gfx::Size(2000, 2000));

  root->SetDrawsContent(true);
  parent->SetDrawsContent(true);
  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  leaf_node->SetDrawsContent(true);

  host_impl()->set_resourceless_software_draw_for_testing();
  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  gfx::PointF test_point(90.f, 90.f);
  LayerImpl* result_layer =
      root->layer_tree_impl()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(leaf_node, result_layer);
}

TEST_F(LayerTreeHostCommonTest, SurfacesDisabledAndReEnabled) {
  // Tests that draw properties are computed correctly when we disable and then
  // re-enable separate surfaces.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  LayerImpl* leaf_node = AddChild<LayerImpl>(grand_child);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetPosition(gfx::PointF(2.f, 2.f));
  parent->SetBounds(gfx::Size(400, 400));
  parent->SetMasksToBounds(true);
  child->SetPosition(gfx::PointF(4.f, 4.f));
  child->SetBounds(gfx::Size(800, 800));
  child->SetMasksToBounds(true);
  child->test_properties()->force_render_surface = true;
  grand_child->SetPosition(gfx::PointF(8.f, 8.f));
  grand_child->SetBounds(gfx::Size(1500, 1500));
  grand_child->test_properties()->force_render_surface = true;
  leaf_node->SetPosition(gfx::PointF(16.f, 16.f));
  leaf_node->SetBounds(gfx::Size(2000, 2000));

  root->SetDrawsContent(true);
  parent->SetDrawsContent(true);
  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  leaf_node->SetDrawsContent(true);

  gfx::Transform expected_leaf_draw_transform_with_surfaces;
  expected_leaf_draw_transform_with_surfaces.Translate(16.0, 16.0);

  gfx::Transform expected_leaf_draw_transform_without_surfaces;
  expected_leaf_draw_transform_without_surfaces.Translate(30.0, 30.0);

  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(leaf_node->is_clipped());
  EXPECT_TRUE(leaf_node->render_target()->is_clipped());
  EXPECT_EQ(gfx::Rect(16, 16, 2000, 2000), leaf_node->drawable_content_rect());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_leaf_draw_transform_with_surfaces,
                                  leaf_node->DrawTransform());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_TRUE(leaf_node->is_clipped());
  EXPECT_EQ(gfx::Rect(6, 6, 396, 396), leaf_node->clip_rect());
  EXPECT_EQ(gfx::Rect(30, 30, 372, 372), leaf_node->drawable_content_rect());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_leaf_draw_transform_without_surfaces,
                                  leaf_node->DrawTransform());

  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(leaf_node->is_clipped());
  EXPECT_TRUE(leaf_node->render_target()->is_clipped());
  EXPECT_EQ(gfx::Rect(16, 16, 2000, 2000), leaf_node->drawable_content_rect());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_leaf_draw_transform_with_surfaces,
                                  leaf_node->DrawTransform());
}

TEST_F(LayerTreeHostCommonTest, AnimationsForRenderSurfaceHierarchy) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child_of_rs1 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* grand_child_of_rs1 = AddChild<LayerImpl>(child_of_rs1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* child_of_rs2 = AddChild<LayerImpl>(render_surface2);
  LayerImpl* grand_child_of_rs2 = AddChild<LayerImpl>(child_of_rs2);
  LayerImpl* child_of_root = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child_of_root = AddChild<LayerImpl>(child_of_root);

  root->SetDrawsContent(true);
  render_surface1->SetDrawsContent(true);
  child_of_rs1->SetDrawsContent(true);
  grand_child_of_rs1->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);
  child_of_rs2->SetDrawsContent(true);
  grand_child_of_rs2->SetDrawsContent(true);
  child_of_root->SetDrawsContent(true);
  grand_child_of_root->SetDrawsContent(true);

  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);

  root->test_properties()->transform = layer_transform;
  root->SetPosition(gfx::PointF(2.5f, 0.f));
  root->SetBounds(gfx::Size(10, 10));
  root->test_properties()->transform_origin = gfx::Point3F(0.25f, 0.f, 0.f);
  render_surface1->test_properties()->transform = layer_transform;
  render_surface1->SetPosition(gfx::PointF(2.5f, 0.f));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->test_properties()->transform = layer_transform;
  render_surface2->SetPosition(gfx::PointF(2.5f, 0.f));
  render_surface2->SetBounds(gfx::Size(10, 10));
  render_surface2->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  render_surface2->test_properties()->force_render_surface = true;
  child_of_root->test_properties()->transform = layer_transform;
  child_of_root->SetPosition(gfx::PointF(2.5f, 0.f));
  child_of_root->SetBounds(gfx::Size(10, 10));
  child_of_root->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  child_of_rs1->test_properties()->transform = layer_transform;
  child_of_rs1->SetPosition(gfx::PointF(2.5f, 0.f));
  child_of_rs1->SetBounds(gfx::Size(10, 10));
  child_of_rs1->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  child_of_rs2->test_properties()->transform = layer_transform;
  child_of_rs2->SetPosition(gfx::PointF(2.5f, 0.f));
  child_of_rs2->SetBounds(gfx::Size(10, 10));
  child_of_rs2->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_root->test_properties()->transform = layer_transform;
  grand_child_of_root->SetPosition(gfx::PointF(2.5f, 0.f));
  grand_child_of_root->SetBounds(gfx::Size(10, 10));
  grand_child_of_root->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_rs1->test_properties()->transform = layer_transform;
  grand_child_of_rs1->SetPosition(gfx::PointF(2.5f, 0.f));
  grand_child_of_rs1->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs1->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_rs2->test_properties()->transform = layer_transform;
  grand_child_of_rs2->SetPosition(gfx::PointF(2.5f, 0.f));
  grand_child_of_rs2->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs2->test_properties()->transform_origin =
      gfx::Point3F(0.25f, 0.f, 0.f);

  root->layer_tree_impl()->BuildLayerListAndPropertyTreesForTesting();
  SetElementIdsForTesting();

  // Put an animated opacity on the render surface.
  AddOpacityTransitionToElementWithPlayer(
      render_surface1->element_id(), timeline_impl(), 10.0, 1.f, 0.f, false);

  // Also put an animated opacity on a layer without descendants.
  AddOpacityTransitionToElementWithPlayer(grand_child_of_root->element_id(),
                                          timeline_impl(), 10.0, 1.f, 0.f,
                                          false);

  // Put a transform animation on the render surface.
  AddAnimatedTransformToElementWithPlayer(render_surface2->element_id(),
                                          timeline_impl(), 10.0, 30, 0);

  // Also put transform animations on grand_child_of_root, and
  // grand_child_of_rs2
  AddAnimatedTransformToElementWithPlayer(grand_child_of_root->element_id(),
                                          timeline_impl(), 10.0, 30, 0);
  AddAnimatedTransformToElementWithPlayer(grand_child_of_rs2->element_id(),
                                          timeline_impl(), 10.0, 30, 0);

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(root->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(root->render_surface(), root->render_target());
  EXPECT_EQ(root->render_surface(), child_of_root->render_target());
  EXPECT_EQ(root->render_surface(), grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1->render_surface(),
            render_surface1->render_target());
  EXPECT_EQ(render_surface1->render_surface(), child_of_rs1->render_target());
  EXPECT_EQ(render_surface1->render_surface(),
            grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2->render_surface(),
            render_surface2->render_target());
  EXPECT_EQ(render_surface2->render_surface(), child_of_rs2->render_target());
  EXPECT_EQ(render_surface2->render_surface(),
            grand_child_of_rs2->render_target());

  // Verify screen_space_transform_is_animating values
  EXPECT_FALSE(root->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_root->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_root->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_rs1->screen_space_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->screen_space_transform_is_animating());
  EXPECT_TRUE(child_of_rs2->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->screen_space_transform_is_animating());

  // Sanity check. If these fail there is probably a bug in the test itself.
  // It is expected that we correctly set up transforms so that the y-component
  // of the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, root->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->ScreenSpaceTransform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0, child_of_rs1->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->ScreenSpaceTransform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0, child_of_rs2->ScreenSpaceTransform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->ScreenSpaceTransform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, LargeTransforms) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform large_transform;
  large_transform.Scale(SkDoubleToMScalar(1e37), SkDoubleToMScalar(1e37));

  root->SetBounds(gfx::Size(10, 10));
  child->test_properties()->transform = large_transform;
  child->SetBounds(gfx::Size(10, 10));
  grand_child->test_properties()->transform = large_transform;
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       ScreenSpaceTransformIsAnimatingWithDelayedAnimation) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);
  LayerImpl* great_grand_child = AddChild<LayerImpl>(grand_child);

  root->SetDrawsContent(true);
  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  great_grand_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(10, 10));
  great_grand_child->SetBounds(gfx::Size(10, 10));

  SetElementIdsForTesting();

  // Add a transform animation with a start delay to |grand_child|.
  std::unique_ptr<Animation> animation = Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 0, 1,
      TargetProperty::TRANSFORM);
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));
  AddAnimationToElementWithPlayer(grand_child->element_id(), timeline_impl(),
                                  std::move(animation));
  ExecuteCalculateDrawProperties(root);

  EXPECT_FALSE(root->screen_space_transform_is_animating());
  EXPECT_FALSE(child->screen_space_transform_is_animating());

  EXPECT_FALSE(grand_child->TransformIsAnimating());
  EXPECT_TRUE(grand_child->HasPotentiallyRunningTransformAnimation());
  EXPECT_TRUE(grand_child->screen_space_transform_is_animating());
  EXPECT_TRUE(great_grand_child->screen_space_transform_is_animating());
}

TEST_F(LayerTreeHostCommonDrawRectsTest, DrawRectsForIdentityTransform) {
  // Test visible layer rect and drawable content rect are calculated correctly
  // correctly for identity transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  gfx::Rect layer_content_rect = gfx::Rect(10, 10, 30, 30);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(30, 30);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(10, 10, 30, 30);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 2: Layer is outside the surface rect.
  layer_content_rect = gfx::Rect(120, 120, 30, 30);
  expected_visible_layer_rect = gfx::Rect();
  expected_drawable_content_rect = gfx::Rect();
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_content_rect = gfx::Rect(80, 80, 30, 30);
  expected_visible_layer_rect = gfx::Rect(20, 20);
  expected_drawable_content_rect = gfx::Rect(80, 80, 20, 20);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest, DrawRectsFor2DRotations) {
  // Test visible layer rect and drawable content rect are calculated correctly
  // for rotations about z-axis (i.e. 2D rotations).

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.Rotate(45.0);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(30, 30);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(28, 50, 44, 43);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-50.0, 0.0);
  layer_to_surface_transform.Rotate(45.0);
  expected_visible_layer_rect = gfx::Rect();
  expected_drawable_content_rect = gfx::Rect();
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 3: The layer is rotated about its top-left corner. In surface space,
  // the layer is oriented diagonally, with the left half outside of the render
  // surface. In this case, the g should still be the entire layer
  // (remember the g is computed in layer space); both the top-left
  // and bottom-right corners of the layer are still visible.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Rotate(45.0);
  expected_visible_layer_rect = gfx::Rect(30, 30);
  expected_drawable_content_rect = gfx::Rect(22, 43);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 4: The layer is rotated about its top-left corner, and translated
  // upwards. In surface space, the layer is oriented diagonally, with only the
  // top corner of the surface overlapping the layer. In layer space, the render
  // surface overlaps the right side of the layer. The g should be
  // the layer's right half.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(0.0, -sqrt(2.0) * 15.0);
  layer_to_surface_transform.Rotate(45.0);
  // Right half of layer bounds.
  expected_visible_layer_rect = gfx::Rect(15, 0, 15, 30);
  expected_drawable_content_rect = gfx::Rect(22, 22);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest, DrawRectsFor3dOrthographicTransform) {
  // Test visible layer rect and drawable content rect are calculated correctly
  // for 3d transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, should be fully contained in the render surface.
  // 100 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(100, 100);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(71, 100);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 2: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, but shifted to the side so only the right-half the layer would be
  // visible on the surface.
  // 50 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  SkMScalar half_width_of_rotated_layer =
      SkDoubleToMScalar((100.0 / sqrt(2.0)) * 0.5);
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-half_width_of_rotated_layer, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);  // Rotates about the left
                                                      // edge of the layer.
  // Tight half of the layer.
  expected_visible_layer_rect = gfx::Rect(50, 0, 50, 100);
  expected_drawable_content_rect = gfx::Rect(36, 100);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest, DrawRectsFor3dPerspectiveTransform) {
  // Test visible layer rect and drawable content rect are calculated correctly
  // when the layer has a perspective projection onto the target surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-50, -50, 200, 200);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Even though the layer is twice as large as the surface, due to
  // perspective foreshortening, the layer will fit fully in the surface when
  // its translated more than the perspective amount.
  layer_to_surface_transform.MakeIdentity();

  // The following sequence of transforms applies the perspective about the
  // center of the surface.
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.ApplyPerspectiveDepth(9.0);
  layer_to_surface_transform.Translate(-50.0, -50.0);

  // This translate places the layer in front of the surface's projection plane.
  layer_to_surface_transform.Translate3d(0.0, 0.0, -27.0);

  // Layer position is (-50, -50), visible rect in layer space is layer bounds
  // offset by layer position.
  gfx::Rect expected_visible_layer_rect = gfx::Rect(50, 50, 150, 150);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(38, 38);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());

  // Case 2: same projection as before, except that the layer is also translated
  // to the side, so that only the right half of the layer should be visible.
  //
  // Explanation of expected result: The perspective ratio is (z distance
  // between layer and camera origin) / (z distance between projection plane and
  // camera origin) == ((-27 - 9) / 9) Then, by similar triangles, if we want to
  // move a layer by translating -25 units in projected surface units (so that
  // only half of it is visible), then we would need to translate by (-36 / 9) *
  // -25 == -100 in the layer's units.
  layer_to_surface_transform.Translate3d(-100.0, 0.0, 0.0);
  // Visible layer rect is moved by 100, and drawable content rect is in target
  // space and is moved by 25.
  expected_visible_layer_rect = gfx::Rect(150, 50, 50, 150);
  expected_drawable_content_rect = gfx::Rect(13, 38);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest,
       DrawRectsFor3dOrthographicIsNotClippedBehindSurface) {
  // There is currently no explicit concept of an orthographic projection plane
  // in our code (nor in the CSS spec to my knowledge). Therefore, layers that
  // are technically behind the surface in an orthographic world should not be
  // clipped when they are flattened to the surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // This sequence of transforms effectively rotates the layer about the y-axis
  // at the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.Translate(-50.0, 0.0);

  // Layer is rotated about Y Axis, and its width is 100/sqrt(2) in surface
  // space.
  gfx::Rect expected_visible_layer_rect = gfx::Rect(100, 100);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(14, 0, 72, 100);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest,
       DrawRectsFor3dPerspectiveWhenClippedByW) {
  // Test visible layer rect and drawable content rect are calculated correctly
  // when projecting a surface onto a layer, but the layer is partially behind
  // the camera (not just behind the projection plane). In this case, the
  // cartesian coordinates may seem to be valid, but actually they are not. The
  // visible rect needs to be properly clipped by the w = 0 plane in homogeneous
  // coordinates before converting to cartesian coordinates. The drawable
  // content rect would be entire surface rect because layer is rotated at the
  // camera position.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 200, 200);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 20, 2);
  gfx::Transform layer_to_surface_transform;

  // The layer is positioned so that the right half of the layer should be in
  // front of the camera, while the other half is behind the surface's
  // projection plane. The following sequence of transforms applies the
  // perspective and rotation about the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(10.0, 0.0, 1.0);
  layer_to_surface_transform.RotateAboutYAxis(-45.0);
  layer_to_surface_transform.Translate(-10, -1);

  // Sanity check that this transform does indeed cause w < 0 when applying the
  // transform, otherwise this code is not testing the intended scenario.
  bool clipped;
  MathUtil::MapQuad(layer_to_surface_transform,
                    gfx::QuadF(gfx::RectF(layer_content_rect)),
                    &clipped);
  ASSERT_TRUE(clipped);

  gfx::Rect expected_visible_layer_rect = gfx::Rect(0, 1, 10, 1);
  gfx::Rect expected_drawable_content_rect = target_surface_rect;
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonDrawRectsTest, DrawRectsForPerspectiveUnprojection) {
  // To determine visible rect in layer space, there needs to be an
  // un-projection from surface space to layer space. When the original
  // transform was a perspective projection that was clipped, it returns a rect
  // that encloses the clipped bounds.  Un-projecting this new rect may require
  // clipping again.

  // This sequence of transforms causes one corner of the layer to protrude
  // across the w = 0 plane, and should be clipped.
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 150, 150);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 20, 20);
  gfx::Transform layer_to_surface_transform;
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(10, 10);
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(0.0, 0.0, -5.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.RotateAboutXAxis(80.0);
  layer_to_surface_transform.Translate(-10, -10);

  // Sanity check that un-projection does indeed cause w < 0, otherwise this
  // code is not testing the intended scenario.
  bool clipped;
  gfx::RectF clipped_rect = MathUtil::MapClippedRect(
      layer_to_surface_transform, gfx::RectF(layer_content_rect));
  MathUtil::ProjectQuad(
      Inverse(layer_to_surface_transform), gfx::QuadF(clipped_rect), &clipped);
  ASSERT_TRUE(clipped);

  // Only the corner of the layer is not visible on the surface because of being
  // clipped. But, the net result of rounding visible region to an axis-aligned
  // rect is that the entire layer should still be considered visible.
  gfx::Rect expected_visible_layer_rect = layer_content_rect;
  gfx::Rect expected_drawable_content_rect = target_surface_rect;
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectsForPositionedRootLayerClippedByViewport) {
  LayerImpl* root = root_layer_for_testing();

  root->SetPosition(gfx::PointF(60, 70));
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());
  // In target space, not clipped.
  EXPECT_EQ(gfx::Rect(60, 70, 100, 100), root->drawable_content_rect());
  // In layer space, clipped.
  EXPECT_EQ(gfx::Rect(40, 30), root->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsForSimpleLayers) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child1_layer = AddChildToRoot<LayerImpl>();
  LayerImpl* child2_layer = AddChildToRoot<LayerImpl>();
  LayerImpl* child3_layer = AddChildToRoot<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child1_layer->SetBounds(gfx::Size(50, 50));
  child1_layer->SetDrawsContent(true);
  child2_layer->SetPosition(gfx::PointF(75.f, 75.f));
  child2_layer->SetBounds(gfx::Size(50, 50));
  child2_layer->SetDrawsContent(true);
  child3_layer->SetPosition(gfx::PointF(125.f, 125.f));
  child3_layer->SetBounds(gfx::Size(50, 50));
  child3_layer->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible_layer_rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // layer visible_layer_rects are clipped by their target surface.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2_layer->visible_layer_rect());
  EXPECT_TRUE(child3_layer->visible_layer_rect().IsEmpty());

  // layer drawable_content_rects are not clipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1_layer->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2_layer->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersClippedByLayer) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child1 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child2 = AddChild<LayerImpl>(child);
  LayerImpl* grand_child3 = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetMasksToBounds(true);
  grand_child1->SetPosition(gfx::PointF(5.f, 5.f));
  grand_child1->SetBounds(gfx::Size(50, 50));
  grand_child1->SetDrawsContent(true);
  grand_child2->SetPosition(gfx::PointF(75.f, 75.f));
  grand_child2->SetBounds(gfx::Size(50, 50));
  grand_child2->SetDrawsContent(true);
  grand_child3->SetPosition(gfx::PointF(125.f, 125.f));
  grand_child3->SetBounds(gfx::Size(50, 50));
  grand_child3->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child->visible_layer_rect());

  // All grandchild visible content rects should be clipped by child.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), grand_child2->visible_layer_rect());
  EXPECT_TRUE(grand_child3->visible_layer_rect().IsEmpty());

  // All grandchild DrawableContentRects should also be clipped by child.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25), grand_child2->drawable_content_rect());
  EXPECT_TRUE(grand_child3->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectWithClippingAndScaling) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform child_scale_matrix;
  child_scale_matrix.Scale(0.25f, 0.25f);
  gfx::Transform grand_child_scale_matrix;
  grand_child_scale_matrix.Scale(0.246f, 0.246f);

  root->SetBounds(gfx::Size(100, 100));
  child->test_properties()->transform = child_scale_matrix;
  child->SetBounds(gfx::Size(10, 10));
  child->SetMasksToBounds(true);
  grand_child->test_properties()->transform = grand_child_scale_matrix;
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  // The visible rect is expanded to integer coordinates.
  EXPECT_EQ(gfx::Rect(41, 41), grand_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInUnclippedRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface);
  LayerImpl* child2 = AddChild<LayerImpl>(render_surface);
  LayerImpl* child3 = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(3, 4));
  render_surface->test_properties()->force_render_surface = true;
  child1->SetPosition(gfx::PointF(5.f, 5.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetPosition(gfx::PointF(75.f, 75.f));
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetPosition(gfx::PointF(125.f, 125.f));
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(render_surface->render_surface());

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // An unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 170.f, 170.f),
            render_surface->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  // Only the viewport clip should apply
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleRectsWhenCannotRenderToSeparateSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChild<LayerImpl>(root);
  LayerImpl* child1 = AddChild<LayerImpl>(parent);
  LayerImpl* child2 = AddChild<LayerImpl>(parent);
  LayerImpl* grand_child1 = AddChild<LayerImpl>(child1);
  LayerImpl* grand_child2 = AddChild<LayerImpl>(child2);
  LayerImpl* leaf_node1 = AddChild<LayerImpl>(grand_child1);
  LayerImpl* leaf_node2 = AddChild<LayerImpl>(grand_child2);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetPosition(gfx::PointF(2.f, 2.f));
  parent->SetBounds(gfx::Size(400, 400));
  child1->SetPosition(gfx::PointF(4.f, 4.f));
  child1->SetBounds(gfx::Size(800, 800));
  child1->test_properties()->force_render_surface = true;
  child2->SetPosition(gfx::PointF(3.f, 3.f));
  child2->SetBounds(gfx::Size(800, 800));
  child2->test_properties()->force_render_surface = true;
  grand_child1->SetPosition(gfx::PointF(8.f, 8.f));
  grand_child1->SetBounds(gfx::Size(1500, 1500));
  grand_child2->SetPosition(gfx::PointF(7.f, 7.f));
  grand_child2->SetBounds(gfx::Size(1500, 1500));
  leaf_node1->SetPosition(gfx::PointF(16.f, 16.f));
  leaf_node1->SetBounds(gfx::Size(2000, 2000));
  leaf_node2->SetPosition(gfx::PointF(9.f, 9.f));
  leaf_node2->SetBounds(gfx::Size(2000, 2000));

  root->SetDrawsContent(true);
  parent->SetDrawsContent(true);
  child1->SetDrawsContent(true);
  child2->SetDrawsContent(true);
  grand_child1->SetDrawsContent(true);
  grand_child2->SetDrawsContent(true);
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetDrawsContent(true);

  // Case 1: No layers clip. Visible rects are clipped by the viewport.
  // Each layer's drawable content rect is its bounds in target space; the only
  // thing that changes with surfaces disabled is that target space is always
  // screen space.
  root->SetHasRenderSurface(true);
  child1->SetHasRenderSurface(true);
  child2->SetHasRenderSurface(true);
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(8, 8, 1500, 1500), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(7, 7, 1500, 1500), grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(24, 24, 2000, 2000), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(16, 16, 2000, 2000), leaf_node2->drawable_content_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 800, 800), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(5, 5, 800, 800), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(14, 14, 1500, 1500),
            grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(12, 12, 1500, 1500),
            grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(30, 30, 2000, 2000), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(21, 21, 2000, 2000), leaf_node2->drawable_content_rect());

  // Case 2: The parent clips. In this case, neither surface is unclipped, so
  // all visible layer rects are clipped by the intersection of all ancestor
  // clips, whether or not surfaces are disabled. However, drawable content
  // rects are clipped only until the next render surface is reached, so
  // descendants of parent have their drawable content rects clipped only when
  // surfaces are disabled.
  parent->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(8, 8, 1500, 1500), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(7, 7, 1500, 1500), grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(24, 24, 2000, 2000), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(16, 16, 2000, 2000), leaf_node2->drawable_content_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 396, 396), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(5, 5, 397, 397), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(14, 14, 388, 388), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(12, 12, 390, 390), grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(30, 30, 372, 372), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(21, 21, 381, 381), leaf_node2->drawable_content_rect());

  parent->SetMasksToBounds(false);

  // Case 3: child1 and grand_child2 clip. In this case, descendants of these
  // layers have their visible rects clipped by them; Similarly, descendants of
  // these layers have their drawable content rects clipped by them.
  child1->SetMasksToBounds(true);
  grand_child2->SetMasksToBounds(true);
  host_impl()->active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(800, 800), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(8, 8, 792, 792), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(7, 7, 1500, 1500), grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(24, 24, 776, 776), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(16, 16, 1491, 1491), leaf_node2->drawable_content_rect());

  ExecuteCalculateDrawPropertiesWithoutSeparateSurfaces(root);
  EXPECT_EQ(gfx::Rect(100, 100), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(98, 98), parent->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(94, 94), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(95, 95), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(86, 86), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(88, 88), grand_child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(70, 70), leaf_node1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(79, 79), leaf_node2->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(100, 100), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 400, 400), parent->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(6, 6, 800, 800), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(5, 5, 800, 800), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(14, 14, 792, 792), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(12, 12, 1500, 1500),
            grand_child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(30, 30, 776, 776), leaf_node1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(21, 21, 1491, 1491), leaf_node2->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleContentRectsForClippedSurfaceWithEmptyClip) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child1 = AddChild<LayerImpl>(root);
  LayerImpl* child2 = AddChild<LayerImpl>(root);
  LayerImpl* child3 = AddChild<LayerImpl>(root);

  root->SetBounds(gfx::Size(100, 100));
  child1->SetPosition(gfx::PointF(5.f, 5.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetPosition(gfx::PointF(75.f, 75.f));
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetPosition(gfx::PointF(125.f, 125.f));
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  LayerImplList render_surface_layer_list_impl;
  // Now set the root render surface an empty clip.
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, gfx::Size(), &render_surface_layer_list_impl);

  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  ASSERT_TRUE(root->render_surface());
  EXPECT_FALSE(root->is_clipped());

  gfx::Rect empty;
  EXPECT_EQ(empty, root->render_surface()->clip_rect());
  EXPECT_TRUE(root->render_surface()->is_clipped());

  // Visible content rect calculation will check if the target surface is
  // clipped or not. An empty clip rect does not indicate the render surface
  // is unclipped.
  EXPECT_EQ(empty, child1->visible_layer_rect());
  EXPECT_EQ(empty, child2->visible_layer_rect());
  EXPECT_EQ(empty, child3->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersWithUninvertibleTransform) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetPosition(gfx::PointF(5.f, 5.f));
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);

  // Case 1: a truly degenerate matrix
  gfx::Transform uninvertible_matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  child->test_properties()->transform = uninvertible_matrix;
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());

  // Case 2: a matrix with flattened z, uninvertible and not visible according
  // to the CSS spec.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  child->test_properties()->transform = uninvertible_matrix;
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());

  // Case 3: a matrix with flattened z, also uninvertible and not visible.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.Translate(500.0, 0.0);
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  child->test_properties()->transform = uninvertible_matrix;
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleContentRectForLayerWithUninvertibleDrawTransform) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(SkDoubleToMScalar(1e-12));

  gfx::Transform rotation;
  rotation.RotateAboutYAxis(45.0);

  root->SetBounds(gfx::Size(100, 100));
  child->test_properties()->transform = perspective;
  child->SetPosition(gfx::PointF(10.f, 10.f));
  child->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  child->Set3dSortingContextId(1);
  child->test_properties()->should_flatten_transform = false;
  grand_child->test_properties()->transform = rotation;
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  grand_child->Set3dSortingContextId(1);
  grand_child->test_properties()->should_flatten_transform = false;
  ExecuteCalculateDrawProperties(root);

  // Though all layers have invertible transforms, matrix multiplication using
  // floating-point math makes the draw transform uninvertible.
  EXPECT_FALSE(root->layer_tree_impl()
                   ->property_trees()
                   ->transform_tree.Node(grand_child->transform_tree_index())
                   ->ancestors_are_invertible);

  // CalcDrawProps skips a subtree when a layer's screen space transform is
  // uninvertible
  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, OcclusionBySiblingOfTarget) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.active_tree(), 2);
  std::unique_ptr<TextureLayerImpl> surface =
      TextureLayerImpl::Create(host_impl.active_tree(), 3);
  std::unique_ptr<TextureLayerImpl> surface_child =
      TextureLayerImpl::Create(host_impl.active_tree(), 4);
  std::unique_ptr<TextureLayerImpl> surface_sibling =
      TextureLayerImpl::Create(host_impl.active_tree(), 5);
  std::unique_ptr<TextureLayerImpl> surface_child_mask =
      TextureLayerImpl::Create(host_impl.active_tree(), 6);

  surface->SetDrawsContent(true);
  surface_child->SetDrawsContent(true);
  surface_sibling->SetDrawsContent(true);
  surface_child_mask->SetDrawsContent(true);
  surface->SetContentsOpaque(true);
  surface_child->SetContentsOpaque(true);
  surface_sibling->SetContentsOpaque(true);
  surface_child_mask->SetContentsOpaque(true);

  gfx::Transform translate;
  translate.Translate(20.f, 20.f);

  root->SetBounds(gfx::Size(1000, 1000));
  child->SetBounds(gfx::Size(300, 300));
  surface->test_properties()->transform = translate;
  surface->SetBounds(gfx::Size(300, 300));
  surface->test_properties()->force_render_surface = true;
  surface_child->SetBounds(gfx::Size(300, 300));
  surface_child->test_properties()->force_render_surface = true;
  surface_sibling->SetBounds(gfx::Size(200, 200));

  LayerImpl* surface_ptr = surface.get();
  LayerImpl* surface_child_ptr = surface_child.get();
  LayerImpl* surface_child_mask_ptr = surface_child_mask.get();

  host_impl.SetViewportSize(root->bounds());

  surface_child->test_properties()->SetMaskLayer(std::move(surface_child_mask));
  surface->test_properties()->AddChild(std::move(surface_child));
  child->test_properties()->AddChild(std::move(surface));
  child->test_properties()->AddChild(std::move(surface_sibling));
  root->test_properties()->AddChild(std::move(child));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  bool update_lcd_text = false;
  host_impl.active_tree()->UpdateDrawProperties(update_lcd_text);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_ptr->render_surface()->draw_transform(), translate);
  // surface_sibling draws into the root render surface and occludes
  // surface_child's contents.
  Occlusion actual_occlusion =
      surface_child_ptr->render_surface()->occlusion_in_content_space();
  Occlusion expected_occlusion(translate, SimpleEnclosedRegion(gfx::Rect()),
                               SimpleEnclosedRegion(gfx::Rect(200, 200)));
  EXPECT_TRUE(expected_occlusion.IsEqual(actual_occlusion));

  // Mask layer should have the same occlusion.
  actual_occlusion =
      surface_child_mask_ptr->draw_properties().occlusion_in_content_space;
  EXPECT_TRUE(expected_occlusion.IsEqual(actual_occlusion));
}

TEST_F(LayerTreeHostCommonTest, TextureLayerSnapping) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<TextureLayerImpl> child =
      TextureLayerImpl::Create(host_impl.active_tree(), 2);

  LayerImpl* child_ptr = child.get();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  gfx::Transform fractional_translate;
  fractional_translate.Translate(10.5f, 20.3f);
  child->test_properties()->transform = fractional_translate;

  host_impl.SetViewportSize(root->bounds());

  root->test_properties()->AddChild(std::move(child));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  bool update_lcd_text = false;
  host_impl.active_tree()->UpdateDrawProperties(update_lcd_text);

  EXPECT_NE(child_ptr->ScreenSpaceTransform(), fractional_translate);
  fractional_translate.RoundTranslationComponents();
  EXPECT_TRANSFORMATION_MATRIX_EQ(child_ptr->ScreenSpaceTransform(),
                                  fractional_translate);
  gfx::RectF layer_bounds_in_screen_space =
      MathUtil::MapClippedRect(child_ptr->ScreenSpaceTransform(),
                               gfx::RectF(gfx::SizeF(child_ptr->bounds())));
  EXPECT_EQ(layer_bounds_in_screen_space, gfx::RectF(11.f, 20.f, 100.f, 100.f));
}

TEST_F(LayerTreeHostCommonTest,
       OcclusionForLayerWithUninvertibleDrawTransform) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.active_tree(), 2);
  std::unique_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.active_tree(), 3);
  std::unique_ptr<LayerImpl> occluding_child =
      LayerImpl::Create(host_impl.active_tree(), 4);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(SkDoubleToMScalar(1e-12));

  gfx::Transform rotation;
  rotation.RotateAboutYAxis(45.0);

  root->SetBounds(gfx::Size(1000, 1000));
  child->test_properties()->transform = perspective;
  child->SetPosition(gfx::PointF(10.f, 10.f));
  child->SetBounds(gfx::Size(300, 300));
  child->test_properties()->should_flatten_transform = false;
  child->Set3dSortingContextId(1);
  grand_child->test_properties()->transform = rotation;
  grand_child->SetBounds(gfx::Size(200, 200));
  grand_child->test_properties()->should_flatten_transform = false;
  grand_child->Set3dSortingContextId(1);
  occluding_child->SetBounds(gfx::Size(200, 200));
  occluding_child->test_properties()->should_flatten_transform = false;

  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  occluding_child->SetDrawsContent(true);
  occluding_child->SetContentsOpaque(true);

  host_impl.SetViewportSize(root->bounds());

  child->test_properties()->AddChild(std::move(grand_child));
  root->test_properties()->AddChild(std::move(child));
  root->test_properties()->AddChild(std::move(occluding_child));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  bool update_lcd_text = false;
  host_impl.active_tree()->UpdateDrawProperties(update_lcd_text);

  LayerImpl* grand_child_ptr = host_impl.active_tree()
                                   ->root_layer_for_testing()
                                   ->test_properties()
                                   ->children[0]
                                   ->test_properties()
                                   ->children[0];

  // Though all layers have invertible transforms, matrix multiplication using
  // floating-point math makes the draw transform uninvertible.
  EXPECT_FALSE(
      host_impl.active_tree()
          ->property_trees()
          ->transform_tree.Node(grand_child_ptr->transform_tree_index())
          ->ancestors_are_invertible);

  // Since |grand_child| has an uninvertible screen space transform, it is
  // skipped so
  // that we are not computing its occlusion_in_content_space.
  gfx::Rect layer_bounds = gfx::Rect();
  EXPECT_EQ(
      layer_bounds,
      grand_child_ptr->draw_properties()
          .occlusion_in_content_space.GetUnoccludedContentRect(layer_bounds));
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInClippedRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface);
  LayerImpl* child2 = AddChild<LayerImpl>(render_surface);
  LayerImpl* child3 = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(100, 100));
  root->SetMasksToBounds(true);
  render_surface->SetBounds(gfx::Size(3, 4));
  render_surface->test_properties()->force_render_surface = true;
  child1->SetPosition(gfx::PointF(5.f, 5.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetPosition(gfx::PointF(75.f, 75.f));
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetPosition(gfx::PointF(125.f, 125.f));
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(render_surface->render_surface());

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            render_surface->render_surface()->DrawableContentRect());

  // All layers that draw content into the surface have their visible content
  // rect clipped by the surface clip rect.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_TRUE(child3->visible_layer_rect().IsEmpty());

  // But the DrawableContentRects are unclipped.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForSurfaceHierarchy) {
  // Check that clipping does not propagate down surfaces.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface2 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface2);
  LayerImpl* child2 = AddChild<LayerImpl>(render_surface2);
  LayerImpl* child3 = AddChild<LayerImpl>(render_surface2);

  root->SetBounds(gfx::Size(100, 100));
  root->SetMasksToBounds(true);
  render_surface1->SetBounds(gfx::Size(3, 4));
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->SetBounds(gfx::Size(7, 13));
  render_surface2->test_properties()->force_render_surface = true;
  child1->SetPosition(gfx::PointF(5.f, 5.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetPosition(gfx::PointF(75.f, 75.f));
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetPosition(gfx::PointF(125.f, 125.f));
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface2->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            render_surface1->render_surface()->DrawableContentRect());

  // render_surface1 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1's content rect. So,
  // render_surface2 grows to enclose all drawable content of its subtree.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 170.f, 170.f),
            render_surface2->render_surface()->DrawableContentRect());

  // All layers that draw content into render_surface2 think they are unclipped
  // by the surface. So, only the viewport clip applies.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());

  // DrawableContentRects are also unclipped.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectsForClippedDescendantsOfUnclippedSurfaces) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* child2 = AddChild<LayerImpl>(child1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(child2);

  root->SetBounds(gfx::Size(100, 100));
  render_surface1->SetBounds(gfx::Size(100, 100));
  render_surface1->test_properties()->force_render_surface = true;
  child1->SetBounds(gfx::Size(500, 500));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(700, 700));
  child2->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(1000, 1000));
  render_surface2->test_properties()->force_render_surface = true;
  render_surface2->SetDrawsContent(true);

  child1->SetMasksToBounds(true);
  child2->SetMasksToBounds(true);
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(100, 100), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(100, 100), render_surface2->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectsWhenClipChildIsBetweenTwoRenderSurfaces) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(100, 100));

  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetMasksToBounds(true);
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  render_surface1->SetBounds(gfx::Size(20, 20));
  render_surface1->SetMasksToBounds(true);
  render_surface1->SetDrawsContent(true);
  render_surface1->test_properties()->force_render_surface = true;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  clip_child->test_properties()->clip_parent = clip_parent;

  render_surface2->SetBounds(gfx::Size(60, 60));
  render_surface2->SetDrawsContent(true);
  render_surface2->test_properties()->force_render_surface = true;

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(20, 20), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), clip_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), render_surface2->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, ClipRectOfSurfaceWhoseParentIsAClipChild) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(100, 100));

  clip_parent->SetPosition(gfx::PointF(2.f, 2.f));
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  render_surface1->SetBounds(gfx::Size(20, 20));
  render_surface1->SetDrawsContent(true);
  render_surface1->test_properties()->force_render_surface = true;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  clip_child->test_properties()->clip_parent = clip_parent;

  render_surface2->SetBounds(gfx::Size(60, 60));
  render_surface2->SetDrawsContent(true);
  render_surface2->test_properties()->force_render_surface = true;

  clip_parent->SetMasksToBounds(true);
  render_surface1->SetMasksToBounds(true);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(50, 50), render_surface2->render_surface()->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceContentRectWhenLayerNotDrawn) {
  // Test that only drawn layers contribute to render surface content rect.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* surface = AddChildToRoot<LayerImpl>();
  LayerImpl* test_layer = AddChild<LayerImpl>(surface);

  root->SetBounds(gfx::Size(200, 200));
  surface->SetBounds(gfx::Size(100, 100));
  surface->SetDrawsContent(true);
  surface->test_properties()->force_render_surface = true;
  test_layer->SetBounds(gfx::Size(150, 150));

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(100, 100), surface->render_surface()->content_rect());

  test_layer->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(150, 150), surface->render_surface()->content_rect());
}

TEST_F(LayerTreeHostCommonTest, VisibleRectsMultipleSurfaces) {
  // Tests visible rects computation when we have unclipped_surface->
  // surface_with_unclipped_descendants->clipped_surface, checks that the bounds
  // of surface_with_unclipped_descendants doesn't propagate to the
  // clipped_surface below it.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* unclipped_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_parent = AddChild<LayerImpl>(unclipped_surface);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface);
  LayerImpl* clipped_surface = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(100, 100));

  unclipped_surface->SetBounds(gfx::Size(30, 30));
  unclipped_surface->SetDrawsContent(true);
  unclipped_surface->test_properties()->force_render_surface = true;

  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  unclipped_desc_surface->SetBounds(gfx::Size(20, 20));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->test_properties()->clip_parent = clip_parent;

  clipped_surface->SetBounds(gfx::Size(60, 60));
  clipped_surface->SetDrawsContent(true);
  clipped_surface->test_properties()->force_render_surface = true;

  clip_parent->SetMasksToBounds(true);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(30, 30), unclipped_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), unclipped_desc_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), clipped_surface->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, RootClipPropagationToClippedSurface) {
  // Tests visible rects computation when we have unclipped_surface->
  // surface_with_unclipped_descendants->clipped_surface, checks that the bounds
  // of root propagate to the clipped_surface.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* unclipped_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_parent = AddChild<LayerImpl>(unclipped_surface);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface);
  LayerImpl* clipped_surface = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(10, 10));

  unclipped_surface->SetBounds(gfx::Size(50, 50));
  unclipped_surface->SetDrawsContent(true);
  unclipped_surface->test_properties()->force_render_surface = true;

  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;

  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->test_properties()->clip_parent = clip_parent;

  clipped_surface->SetBounds(gfx::Size(50, 50));
  clipped_surface->SetDrawsContent(true);
  clipped_surface->test_properties()->force_render_surface = true;

  clip_parent->SetMasksToBounds(true);
  unclipped_desc_surface->SetMasksToBounds(true);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(10, 10), unclipped_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), unclipped_desc_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), clipped_surface->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnUnclippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface);

  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(3, 4));
  render_surface->test_properties()->force_render_surface = true;
  child1->test_properties()->transform = child_rotation;
  child1->SetPosition(gfx::PointF(25.f, 25.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child1->test_properties()->transform_origin = gfx::Point3F(25.f, 25.f, 0.f);
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(render_surface->render_surface());

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            root->render_surface()->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // The unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect expected_surface_drawable_content =
      gfx::Rect(50 - diagonal_radius,
                50 - diagonal_radius,
                diagonal_radius * 2,
                diagonal_radius * 2);
  EXPECT_EQ(gfx::RectF(expected_surface_drawable_content),
            render_surface->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(expected_surface_drawable_content, child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnClippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface);

  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);

  root->SetBounds(gfx::Size(50, 50));
  root->SetMasksToBounds(true);
  render_surface->SetBounds(gfx::Size(3, 4));
  render_surface->test_properties()->force_render_surface = true;
  child1->SetPosition(gfx::PointF(25.f, 25.f));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child1->test_properties()->transform = child_rotation;
  child1->test_properties()->transform_origin = gfx::Point3F(25.f, 25.f, 0.f);
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(render_surface->render_surface());

  // The clipped surface clamps the DrawableContentRect that encloses the
  // rotated layer.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect unclipped_surface_content = gfx::Rect(50 - diagonal_radius,
                                                  50 - diagonal_radius,
                                                  diagonal_radius * 2,
                                                  diagonal_radius * 2);
  gfx::RectF expected_surface_drawable_content(
      gfx::IntersectRects(unclipped_surface_content, gfx::Rect(50, 50)));
  EXPECT_EQ(expected_surface_drawable_content,
            render_surface->render_surface()->DrawableContentRect());

  // On the clipped surface, only a quarter  of the child1 is visible, but when
  // rotating it back to  child1's content space, the actual enclosing rect ends
  // up covering the full left half of child1.
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50), child1->visible_layer_rect());

  // The child's DrawableContentRect is unclipped.
  EXPECT_EQ(unclipped_surface_content, child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsInHighDPI) {
  LayerImpl* root = root_layer_for_testing();
  FakePictureLayerImpl* render_surface1 =
      AddChildToRoot<FakePictureLayerImpl>();
  FakePictureLayerImpl* render_surface2 =
      AddChild<FakePictureLayerImpl>(render_surface1);
  FakePictureLayerImpl* child1 =
      AddChild<FakePictureLayerImpl>(render_surface2);
  FakePictureLayerImpl* child2 =
      AddChild<FakePictureLayerImpl>(render_surface2);
  FakePictureLayerImpl* child3 =
      AddChild<FakePictureLayerImpl>(render_surface2);

  root->SetBounds(gfx::Size(100, 100));
  root->SetMasksToBounds(true);
  render_surface1->SetBounds(gfx::Size(3, 4));
  render_surface1->SetPosition(gfx::PointF(5.f, 5.f));
  render_surface1->SetDrawsContent(true);
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->SetBounds(gfx::Size(7, 13));
  render_surface2->SetPosition(gfx::PointF(5.f, 5.f));
  render_surface2->SetDrawsContent(true);
  render_surface2->test_properties()->force_render_surface = true;
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetPosition(gfx::PointF(5.f, 5.f));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetPosition(gfx::PointF(75.f, 75.f));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetPosition(gfx::PointF(125.f, 125.f));
  child3->SetDrawsContent(true);
  float device_scale_factor = 2.f;
  ExecuteCalculateDrawProperties(root, device_scale_factor);

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  // drawable_content_rects for all layers and surfaces are scaled by
  // device_scale_factor.
  EXPECT_EQ(gfx::RectF(200.f, 200.f),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::RectF(10.f, 10.f, 190.f, 190.f),
            render_surface1->render_surface()->DrawableContentRect());

  // render_surface2 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1.
  EXPECT_EQ(gfx::RectF(10.f, 10.f, 350.f, 350.f),
            render_surface2->render_surface()->DrawableContentRect());

  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(150, 150, 100, 100), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(250, 250, 100, 100), child3->drawable_content_rect());

  // The root layer does not actually draw content of its own.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // All layer visible content rects are not expressed in content space of each
  // layer, so they are not scaled by the device_scale_factor.
  EXPECT_EQ(gfx::Rect(0, 0, 3, 4), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 13), render_surface2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithoutPreserves3d) {
  // Verify the behavior of back-face culling when there are no preserve-3d
  // layers. Note that 3d transforms still apply in this case, but they are
  // "flattened" to each parent layer according to current W3C spec.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* front_facing_child = AddChildToRoot<LayerImpl>();
  LayerImpl* back_facing_child = AddChildToRoot<LayerImpl>();
  LayerImpl* front_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* back_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* front_facing_child_of_front_facing_surface =
      AddChild<LayerImpl>(front_facing_surface);
  LayerImpl* back_facing_child_of_front_facing_surface =
      AddChild<LayerImpl>(front_facing_surface);
  LayerImpl* front_facing_child_of_back_facing_surface =
      AddChild<LayerImpl>(back_facing_surface);
  LayerImpl* back_facing_child_of_back_facing_surface =
      AddChild<LayerImpl>(back_facing_surface);

  // Nothing is double-sided
  front_facing_child->test_properties()->double_sided = false;
  back_facing_child->test_properties()->double_sided = false;
  front_facing_surface->test_properties()->double_sided = false;
  back_facing_surface->test_properties()->double_sided = false;
  front_facing_child_of_front_facing_surface->test_properties()->double_sided =
      false;
  back_facing_child_of_front_facing_surface->test_properties()->double_sided =
      false;
  front_facing_child_of_back_facing_surface->test_properties()->double_sided =
      false;
  back_facing_child_of_back_facing_surface->test_properties()->double_sided =
      false;

  // Everything draws content.
  front_facing_child->SetDrawsContent(true);
  back_facing_child->SetDrawsContent(true);
  front_facing_surface->SetDrawsContent(true);
  back_facing_surface->SetDrawsContent(true);
  front_facing_child_of_front_facing_surface->SetDrawsContent(true);
  back_facing_child_of_front_facing_surface->SetDrawsContent(true);
  front_facing_child_of_back_facing_surface->SetDrawsContent(true);
  back_facing_child_of_back_facing_surface->SetDrawsContent(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  root->SetBounds(gfx::Size(100, 100));
  front_facing_child->SetBounds(gfx::Size(100, 100));
  back_facing_child->SetBounds(gfx::Size(100, 100));
  front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));

  front_facing_surface->test_properties()->force_render_surface = true;
  back_facing_surface->test_properties()->force_render_surface = true;

  back_facing_child->test_properties()->transform = backface_matrix;
  back_facing_surface->test_properties()->transform = backface_matrix;
  back_facing_child_of_front_facing_surface->test_properties()->transform =
      backface_matrix;
  back_facing_child_of_back_facing_surface->test_properties()->transform =
      backface_matrix;

  // Note: No layers preserve 3d. According to current W3C CSS gfx::Transforms
  // spec, these layers should blindly use their own local transforms to
  // determine back-face culling.
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  // Verify which render surfaces were created.
  EXPECT_FALSE(front_facing_child->has_render_surface());
  EXPECT_FALSE(back_facing_child->has_render_surface());
  EXPECT_TRUE(front_facing_surface->has_render_surface());
  EXPECT_TRUE(back_facing_surface->has_render_surface());
  EXPECT_FALSE(
      front_facing_child_of_front_facing_surface->has_render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->has_render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->has_render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->has_render_surface());

  EXPECT_EQ(3u, update_layer_list_impl()->size());
  EXPECT_TRUE(UpdateLayerListImplContains(front_facing_child->id()));
  EXPECT_TRUE(UpdateLayerListImplContains(front_facing_surface->id()));
  EXPECT_TRUE(UpdateLayerListImplContains(
      front_facing_child_of_front_facing_surface->id()));
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithPreserves3d) {
  // Verify the behavior of back-face culling when preserves-3d transform style
  // is used.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* front_facing_child = AddChildToRoot<LayerImpl>();
  LayerImpl* back_facing_child = AddChildToRoot<LayerImpl>();
  LayerImpl* front_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* back_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* front_facing_child_of_front_facing_surface =
      AddChild<LayerImpl>(front_facing_surface);
  LayerImpl* back_facing_child_of_front_facing_surface =
      AddChild<LayerImpl>(front_facing_surface);
  LayerImpl* front_facing_child_of_back_facing_surface =
      AddChild<LayerImpl>(back_facing_surface);
  LayerImpl* back_facing_child_of_back_facing_surface =
      AddChild<LayerImpl>(back_facing_surface);
  // Opacity will not force creation of render surfaces in this case because of
  // the preserve-3d transform style. Instead, an example of when a surface
  // would be created with preserve-3d is when there is a mask layer.
  LayerImpl* dummy_mask_layer1 = AddMaskLayer<LayerImpl>(front_facing_surface);
  LayerImpl* dummy_mask_layer2 = AddMaskLayer<LayerImpl>(back_facing_surface);

  // Nothing is double-sided
  front_facing_child->test_properties()->double_sided = false;
  back_facing_child->test_properties()->double_sided = false;
  front_facing_surface->test_properties()->double_sided = false;
  back_facing_surface->test_properties()->double_sided = false;
  front_facing_child_of_front_facing_surface->test_properties()->double_sided =
      false;
  back_facing_child_of_front_facing_surface->test_properties()->double_sided =
      false;
  front_facing_child_of_back_facing_surface->test_properties()->double_sided =
      false;
  back_facing_child_of_back_facing_surface->test_properties()->double_sided =
      false;

  // Everything draws content.
  front_facing_child->SetDrawsContent(true);
  back_facing_child->SetDrawsContent(true);
  front_facing_surface->SetDrawsContent(true);
  back_facing_surface->SetDrawsContent(true);
  front_facing_child_of_front_facing_surface->SetDrawsContent(true);
  back_facing_child_of_front_facing_surface->SetDrawsContent(true);
  front_facing_child_of_back_facing_surface->SetDrawsContent(true);
  back_facing_child_of_back_facing_surface->SetDrawsContent(true);
  dummy_mask_layer1->SetDrawsContent(true);
  dummy_mask_layer2->SetDrawsContent(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  root->SetBounds(gfx::Size(100, 100));
  front_facing_child->SetBounds(gfx::Size(100, 100));
  back_facing_child->SetBounds(gfx::Size(100, 100));
  front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));

  back_facing_child->test_properties()->transform = backface_matrix;
  back_facing_surface->test_properties()->transform = backface_matrix;
  back_facing_child_of_front_facing_surface->test_properties()->transform =
      backface_matrix;
  back_facing_child_of_back_facing_surface->test_properties()->transform =
      backface_matrix;

  // Each surface creates its own new 3d rendering context (as defined by W3C
  // spec).  According to current W3C CSS gfx::Transforms spec, layers in a 3d
  // rendering context should use the transform with respect to that context.
  // This 3d rendering context occurs when (a) parent's transform style is flat
  // and (b) the layer's transform style is preserve-3d.
  front_facing_surface->test_properties()->should_flatten_transform = false;
  front_facing_surface->Set3dSortingContextId(1);
  back_facing_surface->test_properties()->should_flatten_transform = false;
  back_facing_surface->Set3dSortingContextId(1);
  front_facing_child_of_front_facing_surface->Set3dSortingContextId(1);
  back_facing_child_of_front_facing_surface->Set3dSortingContextId(1);
  front_facing_child_of_back_facing_surface->Set3dSortingContextId(1);
  back_facing_child_of_back_facing_surface->Set3dSortingContextId(1);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  // Verify which render surfaces were created and used.
  EXPECT_FALSE(front_facing_child->has_render_surface());
  EXPECT_FALSE(back_facing_child->has_render_surface());
  EXPECT_TRUE(front_facing_surface->has_render_surface());
  // We expect that a has_render_surface was created but not used.
  EXPECT_TRUE(back_facing_surface->has_render_surface());
  EXPECT_FALSE(
      front_facing_child_of_front_facing_surface->has_render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->has_render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->has_render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->has_render_surface());

  EXPECT_EQ(3u, update_layer_list_impl()->size());

  EXPECT_TRUE(UpdateLayerListImplContains(front_facing_child->id()));
  EXPECT_TRUE(UpdateLayerListImplContains(front_facing_surface->id()));
  EXPECT_TRUE(UpdateLayerListImplContains(
      front_facing_child_of_front_facing_surface->id()));
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithAnimatingTransforms) {
  // Verify that layers are appropriately culled when their back face is showing
  // and they are not double sided, while animations are going on.
  //
  // Even layers that are animating get culled if their back face is showing and
  // they are not double sided.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChildToRoot<LayerImpl>();
  LayerImpl* animating_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child_of_animating_surface =
      AddChild<LayerImpl>(animating_surface);
  LayerImpl* animating_child = AddChildToRoot<LayerImpl>();
  LayerImpl* child2 = AddChildToRoot<LayerImpl>();

  // Nothing is double-sided
  child->test_properties()->double_sided = false;
  child2->test_properties()->double_sided = false;
  animating_surface->test_properties()->double_sided = false;
  child_of_animating_surface->test_properties()->double_sided = false;
  animating_child->test_properties()->double_sided = false;

  // Everything draws content.
  child->SetDrawsContent(true);
  child2->SetDrawsContent(true);
  animating_surface->SetDrawsContent(true);
  child_of_animating_surface->SetDrawsContent(true);
  animating_child->SetDrawsContent(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  SetElementIdsForTesting();

  // Animate the transform on the render surface.
  AddAnimatedTransformToElementWithPlayer(animating_surface->element_id(),
                                          timeline_impl(), 10.0, 30, 0);
  // This is just an animating layer, not a surface.
  AddAnimatedTransformToElementWithPlayer(animating_child->element_id(),
                                          timeline_impl(), 10.0, 30, 0);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->test_properties()->transform = backface_matrix;
  animating_surface->SetBounds(gfx::Size(100, 100));
  animating_surface->test_properties()->transform = backface_matrix;
  animating_surface->test_properties()->force_render_surface = true;
  child_of_animating_surface->SetBounds(gfx::Size(100, 100));
  child_of_animating_surface->test_properties()->transform = backface_matrix;
  animating_child->SetBounds(gfx::Size(100, 100));
  animating_child->test_properties()->transform = backface_matrix;
  child2->SetBounds(gfx::Size(100, 100));

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  EXPECT_FALSE(child->has_render_surface());
  EXPECT_TRUE(animating_surface->has_render_surface());
  EXPECT_FALSE(child_of_animating_surface->has_render_surface());
  EXPECT_FALSE(animating_child->has_render_surface());
  EXPECT_FALSE(child2->has_render_surface());

  EXPECT_EQ(1u, update_layer_list_impl()->size());

  // The back facing layers are culled from the layer list, and have an empty
  // visible rect.
  EXPECT_TRUE(UpdateLayerListImplContains(child2->id()));
  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(animating_surface->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child_of_animating_surface->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(animating_child->visible_layer_rect().IsEmpty());

  EXPECT_EQ(gfx::Rect(100, 100), child2->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
     BackFaceCullingWithPreserves3dForFlatteningSurface) {
  // Verify the behavior of back-face culling for a render surface that is
  // created when it flattens its subtree, and its parent has preserves-3d.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* front_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* back_facing_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* child1 = AddChild<LayerImpl>(front_facing_surface);
  LayerImpl* child2 = AddChild<LayerImpl>(back_facing_surface);

  // RenderSurfaces are not double-sided
  front_facing_surface->test_properties()->double_sided = false;
  back_facing_surface->test_properties()->double_sided = false;

  // Everything draws content.
  front_facing_surface->SetDrawsContent(true);
  back_facing_surface->SetDrawsContent(true);
  child1->SetDrawsContent(true);
  child2->SetDrawsContent(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  root->SetBounds(gfx::Size(100, 100));
  front_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_surface->test_properties()->force_render_surface = true;
  back_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_surface->test_properties()->transform = backface_matrix;
  back_facing_surface->test_properties()->force_render_surface = true;
  child1->SetBounds(gfx::Size(100, 100));
  child2->SetBounds(gfx::Size(100, 100));

  front_facing_surface->Set3dSortingContextId(1);
  back_facing_surface->Set3dSortingContextId(1);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  // Verify which render surfaces were created and used.
  EXPECT_TRUE(front_facing_surface->has_render_surface());

  // We expect the render surface to have been created, but remain unused.
  EXPECT_TRUE(back_facing_surface->has_render_surface());
  EXPECT_FALSE(child1->has_render_surface());
  EXPECT_FALSE(child2->has_render_surface());

  EXPECT_EQ(2u, update_layer_list_impl()->size());
  EXPECT_TRUE(UpdateLayerListImplContains(front_facing_surface->id()));
  EXPECT_TRUE(UpdateLayerListImplContains(child1->id()));
}

TEST_F(LayerTreeHostCommonScalingTest, LayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers not in a surface.
  LayerImpl* root = root_layer_for_testing();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);

  LayerImpl* child = AddChildToRoot<LayerImpl>();
  child->SetPosition(gfx::PointF(2.f, 2.f));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  LayerImpl* child2 = AddChildToRoot<LayerImpl>();
  child2->SetPosition(gfx::PointF(2.f, 2.f));
  child2->SetBounds(gfx::Size(5, 5));
  child2->SetDrawsContent(true);

  float device_scale_factor = 2.5f;
  gfx::Size viewport_size(100, 100);
  ExecuteCalculateDrawProperties(root, device_scale_factor);

  EXPECT_FLOAT_EQ(device_scale_factor, root->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(device_scale_factor, child->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(device_scale_factor, child2->GetIdealContentsScale());

  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());

  // Verify root transforms
  gfx::Transform expected_root_transform;
  expected_root_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_root_transform,
                                  root->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_root_transform,
                                  root->DrawTransform());

  // Verify results of transformed root rects
  gfx::RectF root_bounds(gfx::SizeF(root->bounds()));

  gfx::RectF root_draw_rect =
      MathUtil::MapClippedRect(root->DrawTransform(), root_bounds);
  gfx::RectF root_screen_space_rect =
      MathUtil::MapClippedRect(root->ScreenSpaceTransform(), root_bounds);

  gfx::RectF expected_root_draw_rect(gfx::SizeF(root->bounds()));
  expected_root_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_root_draw_rect, root_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_root_draw_rect, root_screen_space_rect);

  // Verify child and child2 transforms. They should match.
  gfx::Transform expected_child_transform;
  expected_child_transform.Scale(device_scale_factor, device_scale_factor);
  expected_child_transform.Translate(child->position().x(),
                                     child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child2->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child2->ScreenSpaceTransform());

  // Verify results of transformed child and child2 rects. They should
  // match.
  gfx::RectF child_bounds(gfx::SizeF(child->bounds()));

  gfx::RectF child_draw_rect =
      MathUtil::MapClippedRect(child->DrawTransform(), child_bounds);
  gfx::RectF child_screen_space_rect =
      MathUtil::MapClippedRect(child->ScreenSpaceTransform(), child_bounds);

  gfx::RectF child2_draw_rect =
      MathUtil::MapClippedRect(child2->DrawTransform(), child_bounds);
  gfx::RectF child2_screen_space_rect =
      MathUtil::MapClippedRect(child2->ScreenSpaceTransform(), child_bounds);

  gfx::RectF expected_child_draw_rect(child->position(),
                                      gfx::SizeF(child->bounds()));
  expected_child_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_screen_space_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child2_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child2_screen_space_rect);
}

TEST_F(LayerTreeHostCommonScalingTest, SurfaceLayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers in a surface.
  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.0);

  gfx::Transform scale_small_matrix;
  scale_small_matrix.Scale(SK_MScalar1 / 10.f, SK_MScalar1 / 12.f);

  LayerImpl* root = root_layer_for_testing();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* page_scale = AddChildToRoot<LayerImpl>();
  page_scale->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddChild<LayerImpl>(page_scale);
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);

  LayerImpl* perspective_surface = AddChild<LayerImpl>(parent);
  perspective_surface->SetPosition(gfx::PointF(2.f, 2.f));
  perspective_surface->SetBounds(gfx::Size(10, 10));
  perspective_surface->test_properties()->transform =
      perspective_matrix * scale_small_matrix;
  perspective_surface->SetDrawsContent(true);
  perspective_surface->test_properties()->force_render_surface = true;

  LayerImpl* scale_surface = AddChild<LayerImpl>(parent);
  scale_surface->SetPosition(gfx::PointF(2.f, 2.f));
  scale_surface->SetBounds(gfx::Size(10, 10));
  scale_surface->test_properties()->transform = scale_small_matrix;
  scale_surface->SetDrawsContent(true);
  scale_surface->test_properties()->force_render_surface = true;

  float device_scale_factor = 2.5f;
  float page_scale_factor = 3.f;
  root->layer_tree_impl()->SetViewportLayersFromIds(
      Layer::INVALID_ID, page_scale->id(), Layer::INVALID_ID,
      Layer::INVALID_ID);
  root->layer_tree_impl()->BuildLayerListAndPropertyTreesForTesting();
  root->layer_tree_impl()->SetPageScaleOnActiveTree(page_scale_factor);
  ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                 root, nullptr, nullptr);

  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor,
                  parent->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor,
                  perspective_surface->GetIdealContentsScale());
  // Ideal scale is the max 2d scale component of the combined transform up to
  // the nearest render target. Here this includes the layer transform as well
  // as the device and page scale factors.
  gfx::Transform transform = scale_small_matrix;
  transform.Scale(device_scale_factor * page_scale_factor,
                  device_scale_factor * page_scale_factor);
  gfx::Vector2dF scales =
      MathUtil::ComputeTransform2dScaleComponents(transform, 0.f);
  float max_2d_scale = std::max(scales.x(), scales.y());
  EXPECT_FLOAT_EQ(max_2d_scale, scale_surface->GetIdealContentsScale());

  // The ideal scale will draw 1:1 with its render target space along
  // the larger-scale axis.
  gfx::Vector2dF target_space_transform_scales =
      MathUtil::ComputeTransform2dScaleComponents(
          scale_surface->draw_properties().target_space_transform, 0.f);
  EXPECT_FLOAT_EQ(max_2d_scale,
                  std::max(target_space_transform_scales.x(),
                           target_space_transform_scales.y()));

  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());

  gfx::Transform expected_parent_draw_transform;
  expected_parent_draw_transform.Scale(device_scale_factor * page_scale_factor,
                                       device_scale_factor * page_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_draw_transform,
                                  parent->DrawTransform());

  // The scale for the perspective surface is not known, so it is rendered 1:1
  // with the screen, and then scaled during drawing.
  gfx::Transform expected_perspective_surface_draw_transform;
  expected_perspective_surface_draw_transform.Translate(
      device_scale_factor * page_scale_factor *
      perspective_surface->position().x(),
      device_scale_factor * page_scale_factor *
      perspective_surface->position().y());
  expected_perspective_surface_draw_transform.PreconcatTransform(
      perspective_matrix);
  expected_perspective_surface_draw_transform.PreconcatTransform(
      scale_small_matrix);
  gfx::Transform expected_perspective_surface_layer_draw_transform;
  expected_perspective_surface_layer_draw_transform.Scale(
      device_scale_factor * page_scale_factor,
      device_scale_factor * page_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_draw_transform,
      perspective_surface->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_layer_draw_transform,
      perspective_surface->DrawTransform());
}

TEST_F(LayerTreeHostCommonScalingTest, SmallIdealScale) {
  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 0.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer_for_testing();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  parent->SetBounds(gfx::Size(100, 100));
  parent->test_properties()->transform = parent_scale_matrix;
  parent->SetDrawsContent(true);

  LayerImpl* child_scale = AddChild<LayerImpl>(parent);
  child_scale->SetPosition(gfx::PointF(2.f, 2.f));
  child_scale->SetBounds(gfx::Size(10, 10));
  child_scale->test_properties()->transform = child_scale_matrix;
  child_scale->SetDrawsContent(true);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 0.01f;

  {
    ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                   root, nullptr, nullptr);

    // The ideal scale is able to go below 1.
    float expected_ideal_scale =
        device_scale_factor * page_scale_factor * initial_parent_scale;
    EXPECT_LT(expected_ideal_scale, 1.f);
    EXPECT_FLOAT_EQ(expected_ideal_scale, parent->GetIdealContentsScale());

    expected_ideal_scale = device_scale_factor * page_scale_factor *
                           initial_parent_scale * initial_child_scale;
    EXPECT_LT(expected_ideal_scale, 1.f);
    EXPECT_FLOAT_EQ(expected_ideal_scale, child_scale->GetIdealContentsScale());
  }
}

TEST_F(LayerTreeHostCommonScalingTest, IdealScaleForAnimatingLayer) {
  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer_for_testing();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  parent->SetBounds(gfx::Size(100, 100));
  parent->test_properties()->transform = parent_scale_matrix;
  parent->SetDrawsContent(true);

  LayerImpl* child_scale = AddChild<LayerImpl>(parent);
  child_scale->SetBounds(gfx::Size(10, 10));
  child_scale->SetPosition(gfx::PointF(2.f, 2.f));
  child_scale->test_properties()->transform = child_scale_matrix;
  child_scale->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(root);

  EXPECT_FLOAT_EQ(initial_parent_scale, parent->GetIdealContentsScale());
  // Animating layers compute ideal scale in the same way as when
  // they are static.
  EXPECT_FLOAT_EQ(initial_child_scale * initial_parent_scale,
                  child_scale->GetIdealContentsScale());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceTransformsInHighDPI) {
  LayerImpl* parent = root_layer_for_testing();
  parent->SetBounds(gfx::Size(30, 30));
  parent->SetDrawsContent(true);
  parent->Set3dSortingContextId(1);
  parent->test_properties()->should_flatten_transform = false;

  LayerImpl* child = AddChildToRoot<LayerImpl>();
  child->SetBounds(gfx::Size(10, 10));
  child->SetPosition(gfx::PointF(2.f, 2.f));
  child->SetDrawsContent(true);
  child->test_properties()->force_render_surface = true;

  // This layer should end up in the same surface as child, with the same draw
  // and screen space transforms.
  LayerImpl* duplicate_child_non_owner = AddChild<LayerImpl>(child);
  duplicate_child_non_owner->SetBounds(gfx::Size(10, 10));
  duplicate_child_non_owner->SetDrawsContent(true);

  float device_scale_factor = 1.5f;
  ExecuteCalculateDrawProperties(parent, device_scale_factor);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because of force_render_surface).
  EXPECT_EQ(2u, render_surface_layer_list_impl()->size());

  gfx::Transform expected_parent_transform;
  expected_parent_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->ScreenSpaceTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->DrawTransform());

  gfx::Transform expected_draw_transform;
  expected_draw_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_draw_transform,
                                  child->DrawTransform());

  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Scale(device_scale_factor,
                                        device_scale_factor);
  expected_screen_space_transform.Translate(child->position().x(),
                                            child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_screen_space_transform,
                                  child->ScreenSpaceTransform());

  gfx::Transform expected_duplicate_child_draw_transform =
      child->DrawTransform();
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_duplicate_child_draw_transform,
                                  duplicate_child_non_owner->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      child->ScreenSpaceTransform(),
      duplicate_child_non_owner->ScreenSpaceTransform());
  EXPECT_EQ(child->drawable_content_rect(),
            duplicate_child_non_owner->drawable_content_rect());
  EXPECT_EQ(child->bounds(), duplicate_child_non_owner->bounds());

  gfx::Transform expected_render_surface_draw_transform;
  expected_render_surface_draw_transform.Translate(
      device_scale_factor * child->position().x(),
      device_scale_factor * child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_render_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(device_scale_factor * 2.f,
                                            device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_screen_space_transform;
  expected_surface_screen_space_transform.Translate(device_scale_factor * 2.f,
                                                    device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_screen_space_transform,
      child->render_surface()->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest,
     RenderSurfaceTransformsInHighDPIAccurateScaleZeroPosition) {
  LayerImpl* parent = root_layer_for_testing();
  parent->SetBounds(gfx::Size(33, 31));
  parent->SetDrawsContent(true);

  LayerImpl* child = AddChildToRoot<LayerImpl>();
  child->SetBounds(gfx::Size(13, 11));
  child->SetDrawsContent(true);
  child->test_properties()->force_render_surface = true;

  float device_scale_factor = 1.7f;
  ExecuteCalculateDrawProperties(parent, device_scale_factor);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because of force_render_surface).
  EXPECT_EQ(2u, render_surface_layer_list_impl()->size());

  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(gfx::Transform(),
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), child->render_surface()->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, LayerSearch) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<Layer> mask_layer = Layer::Create();

  child->AddChild(grand_child.get());
  child->SetMaskLayer(mask_layer.get());
  root->AddChild(child.get());

  host()->SetRootLayer(root);

  int nonexistent_id = -1;
  LayerTree* layer_tree = host()->GetLayerTree();
  EXPECT_EQ(root.get(), layer_tree->LayerById(root->id()));
  EXPECT_EQ(child.get(), layer_tree->LayerById(child->id()));
  EXPECT_EQ(grand_child.get(), layer_tree->LayerById(grand_child->id()));
  EXPECT_EQ(mask_layer.get(), layer_tree->LayerById(mask_layer->id()));
  EXPECT_FALSE(layer_tree->LayerById(nonexistent_id));
}

TEST_F(LayerTreeHostCommonTest, TransparentChildRenderSurfaceCreation) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->test_properties()->opacity = 0.5f;
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(child->has_render_surface());
}

TEST_F(LayerTreeHostCommonTest, OpacityAnimatingOnPendingTree) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(host()->GetSettings(), &task_runner_provider,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);

  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);
  child->test_properties()->opacity = 0.0f;

  const int child_id = child->id();
  root->test_properties()->AddChild(std::move(child));
  root->SetHasRenderSurface(true);
  LayerImpl* root_layer = root.get();
  host_impl.pending_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.pending_tree()->BuildLayerListAndPropertyTreesForTesting();

  // Add opacity animation.
  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host_impl.animation_host()->AddAnimationTimeline(timeline);
  host_impl.pending_tree()->SetElementIdsForTesting();

  ElementId child_element_id =
      host_impl.pending_tree()->LayerById(child_id)->element_id();

  AddOpacityTransitionToElementWithPlayer(child_element_id, timeline, 10.0,
                                          0.0f, 1.0f, false);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // We should have one render surface and two layers. The child
  // layer should be included even though it is transparent.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root_layer->render_surface()->layer_list().size());

  // If the root itself is hidden, the child should not be drawn even if it has
  // an animating opacity.
  root_layer->test_properties()->opacity = 0.0f;
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  LayerImplList render_surface_layer_list2;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs2(
      root_layer, root_layer->bounds(), &render_surface_layer_list2);
  inputs2.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs2);

  LayerImpl* child_ptr = root_layer->layer_tree_impl()->LayerById(2);
  EffectTree& tree =
      root_layer->layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = tree.Node(child_ptr->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);

  // A layer should be drawn and it should contribute to drawn surface when
  // it has animating opacity even if it has opacity 0.
  root_layer->test_properties()->opacity = 1.0f;
  child_ptr->test_properties()->opacity = 0.0f;
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  LayerImplList render_surface_layer_list3;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs3(
      root_layer, root_layer->bounds(), &render_surface_layer_list3);
  inputs3.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs3);

  child_ptr = root_layer->layer_tree_impl()->LayerById(2);
  tree = root_layer->layer_tree_impl()->property_trees()->effect_tree;
  node = tree.Node(child_ptr->effect_tree_index());
  EXPECT_TRUE(node->is_drawn);
  EXPECT_TRUE(tree.ContributesToDrawnSurface(child_ptr->effect_tree_index()));

  // But if the opacity of the layer remains 0 after activation, it should not
  // be drawn.
  host_impl.ActivateSyncTree();
  LayerImpl* active_root = host_impl.active_tree()->LayerById(root_layer->id());
  LayerImpl* active_child = host_impl.active_tree()->LayerById(child_ptr->id());

  EffectTree& active_effect_tree =
      host_impl.active_tree()->property_trees()->effect_tree;
  EXPECT_TRUE(active_effect_tree.needs_update());

  ExecuteCalculateDrawProperties(active_root);

  node = active_effect_tree.Node(active_child->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);
  EXPECT_FALSE(active_effect_tree.ContributesToDrawnSurface(
      active_child->effect_tree_index()));
}

using LCDTextTestParam = std::tr1::tuple<bool, bool, bool>;
class LCDTextTest : public LayerTreeHostCommonTestBase,
                    public testing::TestWithParam<LCDTextTestParam> {
 public:
  LCDTextTest()
      : LayerTreeHostCommonTestBase(LCDTextTestLayerTreeSettings()),
        host_impl_(LCDTextTestLayerTreeSettings(),
                   &task_runner_provider_,
                   &task_graph_runner_) {}

  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }

 protected:
  LayerTreeSettings LCDTextTestLayerTreeSettings() {
    LayerTreeSettings settings = VerifyTreeCalcsLayerTreeSettings();

    can_use_lcd_text_ = std::tr1::get<0>(GetParam());
    layers_always_allowed_lcd_text_ = std::tr1::get<1>(GetParam());
    settings.can_use_lcd_text = can_use_lcd_text_;
    settings.layers_always_allowed_lcd_text = layers_always_allowed_lcd_text_;
    return settings;
  }

  void SetUp() override {
    timeline_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    host_impl_.animation_host()->AddAnimationTimeline(timeline_);

    std::unique_ptr<LayerImpl> root_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<LayerImpl> child_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    std::unique_ptr<LayerImpl> grand_child_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 3);

    // Stash raw pointers to look at later.
    root_ = root_ptr.get();
    child_ = child_ptr.get();
    grand_child_ = grand_child_ptr.get();

    child_->test_properties()->AddChild(std::move(grand_child_ptr));
    root_->test_properties()->AddChild(std::move(child_ptr));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(root_ptr));

    host_impl_.active_tree()->SetElementIdsForTesting();

    root_->SetContentsOpaque(true);
    child_->SetContentsOpaque(true);
    grand_child_->SetContentsOpaque(true);

    root_->SetDrawsContent(true);
    child_->SetDrawsContent(true);
    grand_child_->SetDrawsContent(true);

    root_->SetBounds(gfx::Size(1, 1));
    child_->SetBounds(gfx::Size(1, 1));
    grand_child_->SetBounds(gfx::Size(1, 1));

    child_->test_properties()->force_render_surface =
        std::tr1::get<2>(GetParam());
  }

  bool can_use_lcd_text_;
  bool layers_always_allowed_lcd_text_;

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_refptr<AnimationTimeline> timeline_;

  LayerImpl* root_ = nullptr;
  LayerImpl* child_ = nullptr;
  LayerImpl* grand_child_ = nullptr;
};

TEST_P(LCDTextTest, CanUseLCDText) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Case 1: Identity transform.
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());

  // Case 2: Integral translation.
  gfx::Transform integral_translation;
  integral_translation.Translate(1.0, 2.0);
  child_->test_properties()->transform = integral_translation;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());

  // Case 3: Non-integral translation.
  gfx::Transform non_integral_translation;
  non_integral_translation.Translate(1.5, 2.5);
  child_->test_properties()->transform = non_integral_translation;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());

  // Case 4: Rotation.
  gfx::Transform rotation;
  rotation.Rotate(10.0);
  child_->test_properties()->transform = rotation;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());

  // Case 5: Scale.
  gfx::Transform scale;
  scale.Scale(2.0, 2.0);
  child_->test_properties()->transform = scale;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());

  // Case 6: Skew.
  gfx::Transform skew;
  skew.Skew(10.0, 0.0);
  child_->test_properties()->transform = skew;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());

  // Case 7: Translucent.
  child_->test_properties()->transform = gfx::Transform();
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  child_->test_properties()->opacity = 0.5f;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());

  // Case 8: Sanity check: restore transform and opacity.
  child_->test_properties()->transform = gfx::Transform();
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  child_->test_properties()->opacity = 1.f;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());

  // Case 9: Non-opaque content.
  child_->SetContentsOpaque(false);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());

  // Case 10: Sanity check: restore content opaqueness.
  child_->SetContentsOpaque(true);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());
}

TEST_P(LCDTextTest, CanUseLCDTextWithAnimation) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Sanity check: Make sure can_use_lcd_text_ is set on each node.
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());

  // Add opacity animation.
  child_->test_properties()->opacity = 0.9f;
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;

  SetElementIdsForTesting();

  AddOpacityTransitionToElementWithPlayer(child_->element_id(), timeline(),
                                          10.0, 0.9f, 0.1f, false);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  // Text LCD should be adjusted while animation is active.
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->CanUseLCDText());
}

TEST_P(LCDTextTest, CanUseLCDTextWithAnimationContentsOpaque) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Sanity check: Make sure can_use_lcd_text_ is set on each node.
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());
  SetElementIdsForTesting();

  // Mark contents non-opaque within the first animation frame.
  child_->SetContentsOpaque(false);
  AddOpacityTransitionToElementWithPlayer(child_->element_id(), timeline(),
                                          10.0, 0.9f, 0.1f, false);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, nullptr, nullptr, nullptr);
  // LCD text should be disabled for non-opaque layers even during animations.
  EXPECT_EQ(expect_lcd_text, root_->CanUseLCDText());
  EXPECT_EQ(expect_not_lcd_text, child_->CanUseLCDText());
  EXPECT_EQ(expect_lcd_text, grand_child_->CanUseLCDText());
}

INSTANTIATE_TEST_CASE_P(LayerTreeHostCommonTest,
                        LCDTextTest,
                        testing::Combine(testing::Bool(),
                                         testing::Bool(),
                                         testing::Bool()));

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_SingleLayerImpl) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  host_impl.CreatePendingTree();

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  LayerImpl* root_layer = root.get();

  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  child->SetBounds(gfx::Size(40, 40));
  child->SetDrawsContent(true);

  std::unique_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  grand_child->SetBounds(gfx::Size(30, 30));
  grand_child->SetDrawsContent(true);
  grand_child->test_properties()->hide_layer_and_subtree = true;

  child->test_properties()->AddChild(std::move(grand_child));
  root->test_properties()->AddChild(std::move(child));
  root->SetHasRenderSurface(true);
  host_impl.pending_tree()->SetRootLayerForTesting(std::move(root));

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root_layer->render_surface()->layer_list().size());
  EXPECT_EQ(1, root_layer->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(2, root_layer->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_TwoLayersImpl) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  host_impl.CreatePendingTree();

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  LayerImpl* root_layer = root.get();

  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  child->SetBounds(gfx::Size(40, 40));
  child->SetDrawsContent(true);
  child->test_properties()->hide_layer_and_subtree = true;

  std::unique_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  grand_child->SetBounds(gfx::Size(30, 30));
  grand_child->SetDrawsContent(true);

  child->test_properties()->AddChild(std::move(grand_child));
  root->test_properties()->AddChild(std::move(child));
  host_impl.pending_tree()->SetRootLayerForTesting(std::move(root));

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // We should have one render surface and one layers. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root_layer->render_surface()->layer_list().size());
  EXPECT_EQ(1, root_layer->render_surface()->layer_list().at(0)->id());
}

void EmptyCopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {}

TEST_F(LayerTreeHostCommonTest, SubtreeHiddenWithCopyRequest) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  host_impl.CreatePendingTree();

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  LayerImpl* root_layer = root.get();

  std::unique_ptr<LayerImpl> copy_grand_parent =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  copy_grand_parent->SetBounds(gfx::Size(40, 40));
  copy_grand_parent->SetDrawsContent(true);
  LayerImpl* copy_grand_parent_layer = copy_grand_parent.get();

  std::unique_ptr<LayerImpl> copy_parent =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  copy_parent->SetBounds(gfx::Size(30, 30));
  copy_parent->SetDrawsContent(true);
  copy_parent->test_properties()->force_render_surface = true;
  LayerImpl* copy_parent_layer = copy_parent.get();

  std::unique_ptr<LayerImpl> copy_request =
      LayerImpl::Create(host_impl.pending_tree(), 4);
  copy_request->SetBounds(gfx::Size(20, 20));
  copy_request->SetDrawsContent(true);
  copy_request->test_properties()->force_render_surface = true;
  LayerImpl* copy_layer = copy_request.get();

  std::unique_ptr<LayerImpl> copy_child =
      LayerImpl::Create(host_impl.pending_tree(), 5);
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetDrawsContent(true);
  LayerImpl* copy_child_layer = copy_child.get();

  std::unique_ptr<LayerImpl> copy_grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 6);
  copy_grand_child->SetBounds(gfx::Size(20, 20));
  copy_grand_child->SetDrawsContent(true);
  LayerImpl* copy_grand_child_layer = copy_grand_child.get();

  std::unique_ptr<LayerImpl> copy_grand_parent_sibling_before =
      LayerImpl::Create(host_impl.pending_tree(), 7);
  copy_grand_parent_sibling_before->SetBounds(gfx::Size(40, 40));
  copy_grand_parent_sibling_before->SetDrawsContent(true);
  LayerImpl* copy_grand_parent_sibling_before_layer =
      copy_grand_parent_sibling_before.get();

  std::unique_ptr<LayerImpl> copy_grand_parent_sibling_after =
      LayerImpl::Create(host_impl.pending_tree(), 8);
  copy_grand_parent_sibling_after->SetBounds(gfx::Size(40, 40));
  copy_grand_parent_sibling_after->SetDrawsContent(true);
  LayerImpl* copy_grand_parent_sibling_after_layer =
      copy_grand_parent_sibling_after.get();

  copy_child->test_properties()->AddChild(std::move(copy_grand_child));
  copy_request->test_properties()->AddChild(std::move(copy_child));
  copy_parent->test_properties()->AddChild(std::move(copy_request));
  copy_grand_parent->test_properties()->AddChild(std::move(copy_parent));
  root->test_properties()->AddChild(
      std::move(copy_grand_parent_sibling_before));
  root->test_properties()->AddChild(std::move(copy_grand_parent));
  root->test_properties()->AddChild(std::move(copy_grand_parent_sibling_after));
  host_impl.pending_tree()->SetRootLayerForTesting(std::move(root));

  // Hide the copy_grand_parent and its subtree. But make a copy request in that
  // hidden subtree on copy_layer. Also hide the copy grand child and its
  // subtree.
  copy_grand_parent_layer->test_properties()->hide_layer_and_subtree = true;
  copy_grand_parent_sibling_before_layer->test_properties()
      ->hide_layer_and_subtree = true;
  copy_grand_parent_sibling_after_layer->test_properties()
      ->hide_layer_and_subtree = true;
  copy_grand_child_layer->test_properties()->hide_layer_and_subtree = true;

  copy_layer->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  EXPECT_GT(root_layer->num_copy_requests_in_target_subtree(), 0);
  EXPECT_GT(copy_grand_parent_layer->num_copy_requests_in_target_subtree(), 0);
  EXPECT_GT(copy_parent_layer->num_copy_requests_in_target_subtree(), 0);
  EXPECT_GT(copy_layer->num_copy_requests_in_target_subtree(), 0);

  // We should have four render surfaces, one for the root, one for the grand
  // parent since it has opacity and two drawing descendants, one for the parent
  // since it owns a surface, and one for the copy_layer.
  ASSERT_EQ(4u, render_surface_layer_list.size());
  EXPECT_EQ(root_layer->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(copy_grand_parent_layer->id(),
            render_surface_layer_list.at(1)->id());
  EXPECT_EQ(copy_parent_layer->id(), render_surface_layer_list.at(2)->id());
  EXPECT_EQ(copy_layer->id(), render_surface_layer_list.at(3)->id());

  // The root render surface should have 2 contributing layers.
  ASSERT_EQ(2u, root_layer->render_surface()->layer_list().size());
  EXPECT_EQ(root_layer->id(),
            root_layer->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_grand_parent_layer->id(),
            root_layer->render_surface()->layer_list().at(1)->id());

  // Nothing actually draws into the copy parent, so only the copy_layer will
  // appear in its list, since it needs to be drawn for the copy request.
  ASSERT_EQ(1u, copy_parent_layer->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_parent_layer->render_surface()->layer_list().at(0)->id());

  // The copy_layer's render surface should have two contributing layers.
  ASSERT_EQ(2u, copy_layer->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_layer->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_child_layer->id(),
            copy_layer->render_surface()->layer_list().at(1)->id());

  // copy_grand_parent, copy_parent shouldn't be drawn because they are hidden,
  // but the copy_layer and copy_child should be drawn for the copy request.
  // copy grand child should not be drawn as its hidden even in the copy
  // request.
  EffectTree& tree =
      root_layer->layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = tree.Node(copy_grand_parent_layer->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);
  node = tree.Node(copy_parent_layer->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);
  node = tree.Node(copy_layer->effect_tree_index());
  EXPECT_TRUE(node->is_drawn);
  node = tree.Node(copy_child_layer->effect_tree_index());
  EXPECT_TRUE(node->is_drawn);
  node = tree.Node(copy_grand_child_layer->effect_tree_index());
  EXPECT_FALSE(node->is_drawn);

  // Though copy_layer is drawn, it shouldn't contribute to drawn surface as its
  // actually hidden.
  EXPECT_FALSE(copy_layer->render_surface()->contributes_to_drawn_surface());
}

TEST_F(LayerTreeHostCommonTest, ClippedOutCopyRequest) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  host_impl.CreatePendingTree();

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);

  std::unique_ptr<LayerImpl> copy_parent =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  copy_parent->SetDrawsContent(true);
  copy_parent->SetMasksToBounds(true);

  std::unique_ptr<LayerImpl> copy_layer =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  copy_layer->SetBounds(gfx::Size(30, 30));
  copy_layer->SetDrawsContent(true);
  copy_layer->test_properties()->force_render_surface = true;

  std::unique_ptr<LayerImpl> copy_child =
      LayerImpl::Create(host_impl.pending_tree(), 4);
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetDrawsContent(true);

  copy_layer->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));

  copy_layer->test_properties()->AddChild(std::move(copy_child));
  copy_parent->test_properties()->AddChild(std::move(copy_layer));
  root->test_properties()->AddChild(std::move(copy_parent));

  LayerImplList render_surface_layer_list;
  LayerImpl* root_layer = root.get();
  root_layer->layer_tree_impl()->SetRootLayerForTesting(std::move(root));
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // We should have two render surface, as the others are clipped out.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(root_layer->id(), render_surface_layer_list.at(0)->id());

  // The root render surface should only have 2 contributing layer, since the
  // other layers are empty/clipped away.
  ASSERT_EQ(2u, root_layer->render_surface()->layer_list().size());
  EXPECT_EQ(root_layer->id(),
            root_layer->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, VisibleRectInNonRootCopyRequest) {
  LayerImpl* root = root_layer_for_testing();
  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);

  LayerImpl* copy_layer = AddChild<LayerImpl>(root);
  copy_layer->SetBounds(gfx::Size(100, 100));
  copy_layer->SetDrawsContent(true);
  copy_layer->test_properties()->force_render_surface = true;

  LayerImpl* copy_child = AddChild<LayerImpl>(copy_layer);
  copy_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetDrawsContent(true);

  LayerImpl* copy_clip = AddChild<LayerImpl>(copy_layer);
  copy_clip->SetBounds(gfx::Size(55, 55));
  copy_clip->SetMasksToBounds(true);

  LayerImpl* copy_clipped_child = AddChild<LayerImpl>(copy_clip);
  copy_clipped_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_clipped_child->SetBounds(gfx::Size(20, 20));
  copy_clipped_child->SetDrawsContent(true);

  LayerImpl* copy_surface = AddChild<LayerImpl>(copy_clip);
  copy_surface->SetPosition(gfx::PointF(45.f, 45.f));
  copy_surface->SetBounds(gfx::Size(20, 20));
  copy_surface->SetDrawsContent(true);
  copy_surface->test_properties()->force_render_surface = true;

  copy_layer->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));

  DCHECK(!copy_layer->test_properties()->copy_requests.empty());
  ExecuteCalculateDrawProperties(root);
  DCHECK(copy_layer->test_properties()->copy_requests.empty());

  EXPECT_EQ(gfx::Rect(100, 100), copy_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), copy_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(15, 15), copy_clipped_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), copy_surface->visible_layer_rect());

  // Case 2: When the non root copy request layer is clipped.
  copy_layer->SetBounds(gfx::Size(50, 50));
  copy_layer->SetMasksToBounds(true);
  copy_layer->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;

  DCHECK(!copy_layer->test_properties()->copy_requests.empty());
  ExecuteCalculateDrawProperties(root);
  DCHECK(copy_layer->test_properties()->copy_requests.empty());

  EXPECT_EQ(gfx::Rect(50, 50), copy_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), copy_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), copy_clipped_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), copy_surface->visible_layer_rect());

  // Case 3: When there is device scale factor.
  float device_scale_factor = 2.f;
  copy_layer->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));

  DCHECK(!copy_layer->test_properties()->copy_requests.empty());
  ExecuteCalculateDrawProperties(root, device_scale_factor);
  DCHECK(copy_layer->test_properties()->copy_requests.empty());

  EXPECT_EQ(gfx::Rect(50, 50), copy_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), copy_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), copy_clipped_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), copy_surface->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, TransformedClipParent) {
  // Ensure that a transform between the layer and its render surface is not a
  // problem. Constructs the following layer tree.
  //
  //   root (a render surface)
  //     + render_surface
  //       + clip_parent (scaled)
  //         + intervening_clipping_layer
  //           + clip_child
  //
  // The render surface should be resized correctly and the clip child should
  // inherit the right clip rect.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_parent = AddChild<LayerImpl>(render_surface);
  clip_parent->SetDrawsContent(true);
  LayerImpl* intervening = AddChild<LayerImpl>(clip_parent);
  intervening->SetDrawsContent(true);
  LayerImpl* clip_child = AddChild<LayerImpl>(intervening);
  clip_child->SetDrawsContent(true);
  clip_child->test_properties()->clip_parent = clip_parent;
  std::unique_ptr<std::set<LayerImpl*>> clip_children(new std::set<LayerImpl*>);
  clip_children->insert(clip_child);
  clip_parent->test_properties()->clip_children.reset(clip_children.release());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  gfx::Transform scale_transform;
  scale_transform.Scale(2, 2);

  root->SetBounds(gfx::Size(50, 50));
  render_surface->SetBounds(gfx::Size(10, 10));
  render_surface->test_properties()->force_render_surface = true;
  clip_parent->test_properties()->transform = scale_transform;
  clip_parent->SetPosition(gfx::PointF(1.f, 1.f));
  clip_parent->SetBounds(gfx::Size(10, 10));
  intervening->SetPosition(gfx::PointF(1.f, 1.f));
  intervening->SetBounds(gfx::Size(5, 5));
  clip_child->SetPosition(gfx::PointF(1.f, 1.f));
  clip_child->SetBounds(gfx::Size(10, 10));
  ExecuteCalculateDrawProperties(root);

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(render_surface->render_surface());

  // Ensure that we've inherited our clip parent's clip and weren't affected
  // by the intervening clip layer.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20), clip_parent->clip_rect());
  ASSERT_EQ(clip_parent->clip_rect(), clip_child->clip_rect());
  ASSERT_EQ(gfx::Rect(3, 3, 10, 10), intervening->clip_rect());

  // Ensure that the render surface reports a content rect that has been grown
  // to accomodate for the clip child.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20),
            render_surface->render_surface()->content_rect());

  // The above check implies the two below, but they nicely demonstrate that
  // we've grown, despite the intervening layer's clip.
  ASSERT_TRUE(clip_parent->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
  ASSERT_FALSE(intervening->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
}

TEST_F(LayerTreeHostCommonTest, ClipParentWithInterveningRenderSurface) {
  // Ensure that intervening render surfaces are not a problem in the basic
  // case. In the following tree, both render surfaces should be resized to
  // accomodate for the clip child, despite an intervening clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* intervening = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(intervening);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface2);
  render_surface1->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  root->SetBounds(gfx::Size(50, 50));
  clip_parent->SetPosition(gfx::PointF(1.f, 1.f));
  clip_parent->SetBounds(gfx::Size(40, 40));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  intervening->SetPosition(gfx::PointF(1.f, 1.f));
  intervening->SetBounds(gfx::Size(5, 5));
  render_surface2->SetBounds(gfx::Size(10, 10));
  render_surface2->test_properties()->force_render_surface = true;
  clip_child->SetPosition(gfx::PointF(-10.f, -10.f));
  clip_child->SetBounds(gfx::Size(60, 60));
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            render_surface1->render_surface()->clip_rect());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            render_surface2->render_surface()->clip_rect());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), render_surface1->clip_rect());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), render_surface2->clip_rect());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40),
            render_surface1->render_surface()->content_rect());
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40),
            render_surface2->render_surface()->content_rect());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(9, 9, 40, 40), clip_child->visible_layer_rect());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClipParentScrolledInterveningLayer) {
  // Ensure that intervening render surfaces are not a problem, even if there
  // is a scroll involved. Note, we do _not_ have to consider any other sort
  // of transform.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds AND scrolls)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* intervening = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(intervening);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface2);
  render_surface1->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);
  intervening->SetScrollClipLayer(clip_parent->id());
  intervening->SetCurrentScrollOffset(gfx::ScrollOffset(3, 3));

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  root->SetBounds(gfx::Size(50, 50));
  clip_parent->test_properties()->transform = translation_transform;
  clip_parent->SetPosition(gfx::PointF(1.f, 1.f));
  clip_parent->SetBounds(gfx::Size(40, 40));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->test_properties()->force_render_surface = true;
  intervening->SetPosition(gfx::PointF(1.f, 1.f));
  intervening->SetBounds(gfx::Size(5, 5));
  render_surface2->SetBounds(gfx::Size(10, 10));
  render_surface2->test_properties()->force_render_surface = true;
  clip_child->SetPosition(gfx::PointF(-10.f, -10.f));
  clip_child->SetBounds(gfx::Size(60, 60));
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            render_surface1->render_surface()->clip_rect());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            render_surface2->render_surface()->clip_rect());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), render_surface1->clip_rect());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(2, 2, 3, 3), render_surface2->clip_rect());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40),
            render_surface1->render_surface()->content_rect());
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40),
            render_surface2->render_surface()->content_rect());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(12, 12, 40, 40), clip_child->visible_layer_rect());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, DescendantsOfClipChildren) {
  // Ensures that descendants of the clip child inherit the correct clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + intervening (masks to bounds)
  //        + clip_child
  //          + child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChild<LayerImpl>(root);
  LayerImpl* intervening = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(intervening);
  LayerImpl* child = AddChild<LayerImpl>(clip_child);
  clip_child->SetDrawsContent(true);
  child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  root->SetBounds(gfx::Size(50, 50));
  clip_parent->SetBounds(gfx::Size(40, 40));
  clip_parent->SetMasksToBounds(true);
  intervening->SetBounds(gfx::Size(5, 5));
  intervening->SetMasksToBounds(true);
  clip_child->SetBounds(gfx::Size(60, 60));
  child->SetBounds(gfx::Size(60, 60));

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  // Neither the clip child nor its descendant should have inherited the clip
  // from |intervening|.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), clip_child->clip_rect());
  EXPECT_TRUE(clip_child->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), child->visible_layer_rect());
  EXPECT_TRUE(child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest,
       SurfacesShouldBeUnaffectedByNonDescendantClipChildren) {
  // Ensures that non-descendant clip children in the tree do not affect
  // render surfaces.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1
  //        + clip_child
  //      + render_surface2
  //        + non_clip_child
  //
  // In this example render_surface2 should be unaffected by clip_child.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(clip_parent);
  LayerImpl* non_clip_child = AddChild<LayerImpl>(render_surface2);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  clip_parent->SetMasksToBounds(true);
  render_surface1->SetMasksToBounds(true);

  render_surface1->SetDrawsContent(true);
  clip_child->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);
  non_clip_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(15, 15));
  clip_parent->SetBounds(gfx::Size(10, 10));
  render_surface1->SetPosition(gfx::PointF(5, 5));
  render_surface1->SetBounds(gfx::Size(5, 5));
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->SetBounds(gfx::Size(5, 5));
  render_surface2->test_properties()->force_render_surface = true;
  clip_child->SetPosition(gfx::PointF(-1, 1));
  clip_child->SetBounds(gfx::Size(10, 10));
  non_clip_child->SetBounds(gfx::Size(5, 5));

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), render_surface1->clip_rect());
  EXPECT_TRUE(render_surface1->is_clipped());

  // The render surface should not clip (it has unclipped descendants), instead
  // it should rely on layer clipping.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            render_surface1->render_surface()->clip_rect());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());

  // That said, it should have grown to accomodate the unclipped descendant and
  // its own size.
  EXPECT_EQ(gfx::Rect(-1, 0, 6, 5),
            render_surface1->render_surface()->content_rect());

  // This render surface should clip. It has no unclipped descendants.
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
            render_surface2->render_surface()->clip_rect());
  EXPECT_TRUE(render_surface2->render_surface()->is_clipped());
  EXPECT_FALSE(render_surface2->is_clipped());

  // It also shouldn't have grown to accomodate the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5),
            render_surface2->render_surface()->content_rect());

  // Sanity check our num_unclipped_descendants values.
  EXPECT_EQ(1u, render_surface1->test_properties()->num_unclipped_descendants);
  EXPECT_EQ(0u, render_surface2->test_properties()->num_unclipped_descendants);
}

TEST_F(LayerTreeHostCommonTest,
       CreateRenderSurfaceWhenFlattenInsideRenderingContext) {
  // Verifies that Render Surfaces are created at the edge of rendering context.

  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child2 = AddChild<LayerImpl>(child1);
  LayerImpl* child3 = AddChild<LayerImpl>(child2);
  root->SetDrawsContent(true);

  gfx::Size bounds(100, 100);

  root->SetBounds(bounds);
  child1->SetBounds(bounds);
  child1->SetDrawsContent(true);
  child1->Set3dSortingContextId(1);
  child1->test_properties()->should_flatten_transform = false;
  child2->SetBounds(bounds);
  child2->SetDrawsContent(true);
  child2->Set3dSortingContextId(1);
  child3->SetBounds(bounds);
  child3->SetDrawsContent(true);
  child3->Set3dSortingContextId(1);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  // Verify which render surfaces were created.
  EXPECT_TRUE(root->has_render_surface());
  EXPECT_FALSE(child1->has_render_surface());
  EXPECT_TRUE(child2->has_render_surface());
  EXPECT_FALSE(child3->has_render_surface());
}

TEST_F(LayerTreeHostCommonTest, CanRenderToSeparateSurface) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);
  std::unique_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl.active_tree(), 123456);
  std::unique_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl.active_tree(), 1234567);
  std::unique_ptr<LayerImpl> child3 =
      LayerImpl::Create(host_impl.active_tree(), 12345678);

  gfx::Size bounds(100, 100);

  root->SetBounds(bounds);
  root->SetDrawsContent(true);

  // This layer structure normally forces render surface due to preserves3d
  // behavior.
  child1->SetBounds(bounds);
  child1->SetDrawsContent(true);
  child1->Set3dSortingContextId(1);
  child1->test_properties()->should_flatten_transform = false;
  child2->SetBounds(bounds);
  child2->SetDrawsContent(true);
  child2->Set3dSortingContextId(1);
  child3->SetBounds(bounds);
  child3->SetDrawsContent(true);
  child3->Set3dSortingContextId(1);

  child2->test_properties()->AddChild(std::move(child3));
  child1->test_properties()->AddChild(std::move(child2));
  root->test_properties()->AddChild(std::move(child1));
  LayerImpl* root_layer = root.get();
  root_layer->layer_tree_impl()->SetRootLayerForTesting(std::move(root));

  {
    LayerImplList render_surface_layer_list;
    FakeLayerTreeHostImpl::RecursiveUpdateNumChildren(root_layer);
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root_layer, root_layer->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    EXPECT_EQ(2u, render_surface_layer_list.size());

    int count_represents_target_render_surface = 0;
    int count_represents_contributing_render_surface = 0;
    int count_represents_itself = 0;
    LayerIterator end = LayerIterator::End(&render_surface_layer_list);
    for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list);
         it != end; ++it) {
      if (it.represents_target_render_surface())
        count_represents_target_render_surface++;
      if (it.represents_contributing_render_surface())
        count_represents_contributing_render_surface++;
      if (it.represents_itself())
        count_represents_itself++;
    }

    // Two render surfaces.
    EXPECT_EQ(2, count_represents_target_render_surface);
    // Second render surface contributes to root render surface.
    EXPECT_EQ(1, count_represents_contributing_render_surface);
    // All 4 layers represent itself.
    EXPECT_EQ(4, count_represents_itself);
  }

  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root_layer, root_layer->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = false;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    EXPECT_EQ(1u, render_surface_layer_list.size());

    int count_represents_target_render_surface = 0;
    int count_represents_contributing_render_surface = 0;
    int count_represents_itself = 0;
    LayerIterator end = LayerIterator::End(&render_surface_layer_list);
    for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list);
         it != end; ++it) {
      if (it.represents_target_render_surface())
        count_represents_target_render_surface++;
      if (it.represents_contributing_render_surface())
        count_represents_contributing_render_surface++;
      if (it.represents_itself())
        count_represents_itself++;
    }

    // Only root layer has a render surface.
    EXPECT_EQ(1, count_represents_target_render_surface);
    // No layer contributes a render surface to root render surface.
    EXPECT_EQ(0, count_represents_contributing_render_surface);
    // All 4 layers represent itself.
    EXPECT_EQ(4, count_represents_itself);
  }
}

TEST_F(LayerTreeHostCommonTest, DoNotIncludeBackfaceInvisibleSurfaces) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* back_facing = AddChild<LayerImpl>(root);

  LayerImpl* render_surface1 = AddChild<LayerImpl>(back_facing);
  LayerImpl* child1 = AddChild<LayerImpl>(render_surface1);

  LayerImpl* flattener = AddChild<LayerImpl>(back_facing);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(flattener);
  LayerImpl* child2 = AddChild<LayerImpl>(render_surface2);

  child1->SetDrawsContent(true);
  child2->SetDrawsContent(true);

  root->SetBounds(gfx::Size(50, 50));
  back_facing->SetBounds(gfx::Size(50, 50));
  back_facing->test_properties()->should_flatten_transform = false;

  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->test_properties()->should_flatten_transform = false;
  render_surface1->test_properties()->force_render_surface = true;
  render_surface1->test_properties()->double_sided = false;
  child1->SetBounds(gfx::Size(20, 20));

  flattener->SetBounds(gfx::Size(30, 30));
  render_surface2->SetBounds(gfx::Size(30, 30));
  render_surface2->test_properties()->should_flatten_transform = false;
  render_surface2->test_properties()->force_render_surface = true;
  render_surface2->test_properties()->double_sided = false;
  child2->SetBounds(gfx::Size(20, 20));

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(2u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());
  EXPECT_EQ(1u, render_surface_layer_list_impl()
                    ->at(1)
                    ->render_surface()
                    ->layer_list()
                    .size());

  gfx::Transform rotation_transform;
  rotation_transform.RotateAboutXAxis(180.0);

  back_facing->test_properties()->transform = rotation_transform;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);

  // render_surface1 is in the same 3d rendering context as back_facing and is
  // not double sided, so it should not be in RSLL. render_surface2 is also not
  // double-sided, but will still be in RSLL as it's in a different 3d rendering
  // context.
  EXPECT_EQ(2u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(1u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());
}

TEST_F(LayerTreeHostCommonTest, DoNotIncludeBackfaceInvisibleLayers) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  root->test_properties()->should_flatten_transform = false;
  child->SetBounds(gfx::Size(30, 30));
  child->test_properties()->double_sided = false;
  child->test_properties()->should_flatten_transform = false;
  grand_child->SetBounds(gfx::Size(20, 20));
  grand_child->SetDrawsContent(true);
  grand_child->SetUseParentBackfaceVisibility(true);
  grand_child->test_properties()->should_flatten_transform = false;
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(grand_child, render_surface_layer_list_impl()
                             ->at(0)
                             ->render_surface()
                             ->layer_list()[0]);

  // As all layers have identity transform, we shouldn't check for backface
  // visibility.
  EXPECT_FALSE(root->should_check_backface_visibility());
  EXPECT_FALSE(child->should_check_backface_visibility());
  EXPECT_FALSE(grand_child->should_check_backface_visibility());
  // As there are no 3d rendering contexts, all layers should use their local
  // transform for backface visibility.
  EXPECT_TRUE(root->use_local_transform_for_backface_visibility());
  EXPECT_TRUE(child->use_local_transform_for_backface_visibility());
  EXPECT_TRUE(grand_child->use_local_transform_for_backface_visibility());

  gfx::Transform rotation_transform;
  rotation_transform.RotateAboutXAxis(180.0);

  child->test_properties()->transform = rotation_transform;
  child->Set3dSortingContextId(1);
  grand_child->Set3dSortingContextId(1);
  child->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(0u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());

  // We should check for backface visibilty of child as it has a rotation
  // transform. We should also check for grand_child as it uses the backface
  // visibility of its parent.
  EXPECT_FALSE(root->should_check_backface_visibility());
  EXPECT_TRUE(child->should_check_backface_visibility());
  EXPECT_TRUE(grand_child->should_check_backface_visibility());
  // child uses its local transform for backface visibility as it is the root of
  // a 3d rendering context. grand_child is in a 3d rendering context and is not
  // the root, but it derives its backface visibility from its parent which uses
  // its local transform.
  EXPECT_TRUE(root->use_local_transform_for_backface_visibility());
  EXPECT_TRUE(child->use_local_transform_for_backface_visibility());
  EXPECT_TRUE(grand_child->use_local_transform_for_backface_visibility());

  grand_child->SetUseParentBackfaceVisibility(false);
  grand_child->test_properties()->double_sided = false;
  grand_child->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(0u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());

  // We should check the backface visibility of child as it has a rotation
  // transform and for grand_child as it is in a 3d rendering context and not
  // the root of it.
  EXPECT_FALSE(root->should_check_backface_visibility());
  EXPECT_TRUE(child->should_check_backface_visibility());
  EXPECT_TRUE(grand_child->should_check_backface_visibility());
  // grand_child is in an existing 3d rendering context, so it should not use
  // local transform for backface visibility.
  EXPECT_TRUE(root->use_local_transform_for_backface_visibility());
  EXPECT_TRUE(child->use_local_transform_for_backface_visibility());
  EXPECT_FALSE(grand_child->use_local_transform_for_backface_visibility());
}

TEST_F(LayerTreeHostCommonTest, TransformAnimationUpdatesBackfaceVisibility) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* back_facing = AddChild<LayerImpl>(root);
  LayerImpl* render_surface1 = AddChild<LayerImpl>(back_facing);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(back_facing);

  gfx::Transform rotate_about_y;
  rotate_about_y.RotateAboutYAxis(180.0);

  root->SetBounds(gfx::Size(50, 50));
  root->Set3dSortingContextId(1);
  root->test_properties()->should_flatten_transform = false;
  back_facing->test_properties()->transform = rotate_about_y;
  back_facing->SetBounds(gfx::Size(50, 50));
  back_facing->Set3dSortingContextId(1);
  back_facing->test_properties()->should_flatten_transform = false;
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->Set3dSortingContextId(1);
  render_surface1->test_properties()->should_flatten_transform = false;
  render_surface1->test_properties()->double_sided = false;
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->SetBounds(gfx::Size(30, 30));
  render_surface2->Set3dSortingContextId(1);
  render_surface2->test_properties()->should_flatten_transform = false;
  render_surface2->test_properties()->double_sided = false;
  render_surface2->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);

  const EffectTree& tree =
      root->layer_tree_impl()->property_trees()->effect_tree;
  EXPECT_TRUE(tree.Node(render_surface1->effect_tree_index())
                  ->hidden_by_backface_visibility);
  EXPECT_TRUE(tree.Node(render_surface2->effect_tree_index())
                  ->hidden_by_backface_visibility);

  root->layer_tree_impl()->property_trees()->transform_tree.OnTransformAnimated(
      gfx::Transform(), back_facing->transform_tree_index(),
      root->layer_tree_impl());
  root->layer_tree_impl()->property_trees()->transform_tree.OnTransformAnimated(
      rotate_about_y, render_surface2->transform_tree_index(),
      root->layer_tree_impl());
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(tree.Node(render_surface1->effect_tree_index())
                   ->hidden_by_backface_visibility);
  EXPECT_TRUE(tree.Node(render_surface2->effect_tree_index())
                  ->hidden_by_backface_visibility);

  root->layer_tree_impl()->property_trees()->transform_tree.OnTransformAnimated(
      rotate_about_y, render_surface1->transform_tree_index(),
      root->layer_tree_impl());
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(tree.Node(render_surface1->effect_tree_index())
                  ->hidden_by_backface_visibility);
  EXPECT_TRUE(tree.Node(render_surface2->effect_tree_index())
                  ->hidden_by_backface_visibility);
}

TEST_F(LayerTreeHostCommonTest, ClippedByScrollParent) {
  // Checks that the simple case (being clipped by a scroll parent that would
  // have been processed before you anyhow) results in the right clips.
  //
  // + root
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* scroll_parent_border = AddChildToRoot<LayerImpl>();
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* scroll_child = AddChild<LayerImpl>(root);

  scroll_parent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);
  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  root->SetBounds(gfx::Size(50, 50));
  scroll_parent_border->SetBounds(gfx::Size(40, 40));
  scroll_parent_clip->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetBounds(gfx::Size(50, 50));
  scroll_child->SetBounds(gfx::Size(50, 50));
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30), scroll_child->clip_rect());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ScrollChildAndScrollParentDifferentTargets) {
  // Tests the computation of draw transform for the scroll child when its
  // target is different from its scroll parent's target.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* scroll_child_target = AddChildToRoot<LayerImpl>();
  LayerImpl* scroll_child = AddChild<LayerImpl>(scroll_child_target);
  LayerImpl* scroll_parent_target = AddChild<LayerImpl>(scroll_child_target);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_target);

  scroll_parent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  root->SetBounds(gfx::Size(50, 50));
  scroll_child_target->SetBounds(gfx::Size(50, 50));
  scroll_child_target->test_properties()->force_render_surface = true;
  scroll_child->SetBounds(gfx::Size(50, 50));
  scroll_parent_target->SetPosition(gfx::PointF(10, 10));
  scroll_parent_target->SetBounds(gfx::Size(50, 50));
  scroll_parent_target->SetMasksToBounds(true);
  scroll_parent_target->test_properties()->force_render_surface = true;
  scroll_parent->SetBounds(gfx::Size(50, 50));

  float device_scale_factor = 1.5f;
  LayerImplList render_surface_layer_list_impl;
  gfx::Size device_viewport_size =
      gfx::Size(root->bounds().width() * device_scale_factor,
                root->bounds().height() * device_scale_factor);
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, device_viewport_size, gfx::Transform(),
      &render_surface_layer_list_impl);
  inputs.device_scale_factor = device_scale_factor;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  EXPECT_EQ(scroll_child->effect_tree_index(),
            scroll_child_target->effect_tree_index());
  EXPECT_EQ(scroll_child->visible_layer_rect(), gfx::Rect(10, 10, 40, 40));
  EXPECT_EQ(scroll_child->clip_rect(), gfx::Rect(15, 15, 75, 75));
  gfx::Transform scale;
  scale.Scale(1.5f, 1.5f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(scroll_child->DrawTransform(), scale);
}

TEST_F(LayerTreeHostCommonTest, SingularTransformSubtreesDoNotDraw) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  LayerImpl* child = AddChild<LayerImpl>(parent);

  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  root->Set3dSortingContextId(1);
  parent->SetBounds(gfx::Size(30, 30));
  parent->SetDrawsContent(true);
  parent->Set3dSortingContextId(1);
  parent->test_properties()->force_render_surface = true;
  child->SetBounds(gfx::Size(20, 20));
  child->SetDrawsContent(true);
  child->Set3dSortingContextId(1);
  child->test_properties()->force_render_surface = true;
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());

  gfx::Transform singular_transform;
  singular_transform.Scale3d(
      SkDoubleToMScalar(1.0), SkDoubleToMScalar(1.0), SkDoubleToMScalar(0.0));

  child->test_properties()->transform = singular_transform;

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(2u, render_surface_layer_list_impl()->size());

  // Ensure that the entire subtree under a layer with singular transform does
  // not get rendered.
  parent->test_properties()->transform = singular_transform;
  child->test_properties()->transform = gfx::Transform();

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollParent) {
  // Checks that clipping by a scroll parent that follows you in paint order
  // still results in correct clipping.
  //
  // + root
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //   + scroll_child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* scroll_child = AddChild<LayerImpl>(root);

  scroll_parent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(50, 50));
  scroll_parent_border->SetBounds(gfx::Size(40, 40));
  scroll_parent_clip->SetBounds(gfx::Size(30, 30));
  scroll_parent_clip->SetMasksToBounds(true);
  scroll_parent->SetBounds(gfx::Size(50, 50));
  scroll_child->SetBounds(gfx::Size(50, 50));

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30), scroll_child->clip_rect());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollGrandparent) {
  // Checks that clipping by a scroll parent and scroll grandparent that follow
  // you in paint order still results in correct clipping.
  //
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* scroll_child = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* scroll_grandparent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_grandparent_clip =
      AddChild<LayerImpl>(scroll_grandparent_border);
  LayerImpl* scroll_grandparent = AddChild<LayerImpl>(scroll_grandparent_clip);

  scroll_parent->SetDrawsContent(true);
  scroll_grandparent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  scroll_parent_border->test_properties()->scroll_parent = scroll_grandparent;
  scroll_grandparent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_grandparent->test_properties()->scroll_children->insert(
      scroll_parent_border);

  root->SetBounds(gfx::Size(50, 50));
  scroll_grandparent_border->SetBounds(gfx::Size(40, 40));
  scroll_grandparent_clip->SetBounds(gfx::Size(20, 20));
  scroll_grandparent->SetBounds(gfx::Size(50, 50));
  scroll_parent_border->SetBounds(gfx::Size(40, 40));
  scroll_parent_clip->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetBounds(gfx::Size(50, 50));
  scroll_child->SetBounds(gfx::Size(50, 50));

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20), scroll_child->clip_rect());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we visited the above layers out of order to get the
  // correct clip, the layer lists should be unaffected.
  EXPECT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(scroll_child, root->render_surface()->layer_list().at(0));
  EXPECT_EQ(scroll_parent, root->render_surface()->layer_list().at(1));
  EXPECT_EQ(scroll_grandparent, root->render_surface()->layer_list().at(2));
}

TEST_F(LayerTreeHostCommonTest, OutOfOrderClippingRequiresRSLLSorting) {
  // Ensures that even if we visit layers out of order, we still produce a
  // correctly ordered render surface layer list.
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //         + render_surface2
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //         + render_surface1
  //
  LayerImpl* root = root_layer_for_testing();
  root->SetDrawsContent(true);

  LayerImpl* scroll_child = AddChild<LayerImpl>(root);
  scroll_child->SetDrawsContent(true);

  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(scroll_parent);
  LayerImpl* scroll_grandparent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_grandparent_clip =
      AddChild<LayerImpl>(scroll_grandparent_border);
  LayerImpl* scroll_grandparent = AddChild<LayerImpl>(scroll_grandparent_clip);
  LayerImpl* render_surface1 = AddChild<LayerImpl>(scroll_grandparent);

  scroll_parent->SetDrawsContent(true);
  render_surface1->SetDrawsContent(true);
  scroll_grandparent->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  scroll_parent_border->test_properties()->scroll_parent = scroll_grandparent;
  scroll_grandparent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_grandparent->test_properties()->scroll_children->insert(
      scroll_parent_border);

  root->SetBounds(gfx::Size(50, 50));
  scroll_grandparent_border->SetBounds(gfx::Size(40, 40));
  scroll_grandparent_clip->SetBounds(gfx::Size(20, 20));
  scroll_grandparent->SetBounds(gfx::Size(50, 50));
  render_surface1->SetBounds(gfx::Size(50, 50));
  render_surface1->test_properties()->force_render_surface = true;
  scroll_parent_border->SetBounds(gfx::Size(40, 40));
  scroll_parent_clip->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetBounds(gfx::Size(50, 50));
  render_surface2->SetBounds(gfx::Size(50, 50));
  render_surface2->test_properties()->force_render_surface = true;
  scroll_child->SetBounds(gfx::Size(50, 50));

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20), scroll_child->clip_rect());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we had to process the layers out of order to get the
  // right clip, our render_surface_layer_list's order should be unaffected.
  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(root, render_surface_layer_list_impl()->at(0));
  EXPECT_EQ(render_surface2, render_surface_layer_list_impl()->at(1));
  EXPECT_EQ(render_surface1, render_surface_layer_list_impl()->at(2));
  EXPECT_TRUE(render_surface_layer_list_impl()->at(0)->render_surface());
  EXPECT_TRUE(render_surface_layer_list_impl()->at(1)->render_surface());
  EXPECT_TRUE(render_surface_layer_list_impl()->at(2)->render_surface());
}

TEST_F(LayerTreeHostCommonTest, FixedPositionWithInterveningRenderSurface) {
  // Ensures that when we have a render surface between a fixed position layer
  // and its container, we compute the fixed position layer's draw transform
  // with respect to that intervening render surface, not with respect to its
  // container's render target.
  //
  // + root
  //   + render_surface
  //     + fixed
  //       + child
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* fixed = AddChild<LayerImpl>(render_surface);
  LayerImpl* child = AddChild<LayerImpl>(fixed);

  render_surface->test_properties()->force_render_surface = true;
  root->test_properties()->is_container_for_fixed_position_layers = true;

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->test_properties()->position_constraint = constraint;

  root->SetBounds(gfx::Size(50, 50));
  render_surface->SetPosition(gfx::PointF(7.f, 9.f));
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetDrawsContent(true);
  fixed->SetPosition(gfx::PointF(10.f, 15.f));
  fixed->SetBounds(gfx::Size(50, 50));
  fixed->SetDrawsContent(true);
  child->SetPosition(gfx::PointF(1.f, 2.f));
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  TransformTree& transform_tree =
      host_impl()->active_tree()->property_trees()->transform_tree;
  EffectTree& effect_tree =
      host_impl()->active_tree()->property_trees()->effect_tree;

  gfx::Transform expected_fixed_draw_transform;
  expected_fixed_draw_transform.Translate(10.f, 15.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_fixed_draw_transform,
      draw_property_utils::DrawTransform(fixed, transform_tree, effect_tree));

  gfx::Transform expected_fixed_screen_space_transform;
  expected_fixed_screen_space_transform.Translate(17.f, 24.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_fixed_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(fixed, transform_tree));

  gfx::Transform expected_child_draw_transform;
  expected_child_draw_transform.Translate(11.f, 17.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_child_draw_transform,
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));

  gfx::Transform expected_child_screen_space_transform;
  expected_child_screen_space_transform.Translate(18.f, 26.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_child_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
}

TEST_F(LayerTreeHostCommonTest, ScrollCompensationWithRounding) {
  // This test verifies that a scrolling layer that gets snapped to
  // integer coordinates doesn't move a fixed position child.
  //
  // + root
  //   + container
  //     + scroller
  //       + fixed
  //
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  host_impl.CreatePendingTree();
  std::unique_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root = root_ptr.get();
  std::unique_ptr<LayerImpl> container =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* container_layer = container.get();
  std::unique_ptr<LayerImpl> scroller =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* scroll_layer = scroller.get();
  std::unique_ptr<LayerImpl> fixed =
      LayerImpl::Create(host_impl.active_tree(), 4);
  LayerImpl* fixed_layer = fixed.get();

  container->test_properties()->is_container_for_fixed_position_layers = true;

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->test_properties()->position_constraint = constraint;

  scroller->SetScrollClipLayer(container->id());

  gfx::Transform container_transform;
  container_transform.Translate3d(10.0, 20.0, 0.0);
  gfx::Vector2dF container_offset = container_transform.To2dTranslation();

  root->SetBounds(gfx::Size(50, 50));
  container->test_properties()->transform = container_transform;
  container->SetBounds(gfx::Size(40, 40));
  container->SetDrawsContent(true);
  scroller->SetBounds(gfx::Size(30, 30));
  scroller->SetDrawsContent(true);
  fixed->SetBounds(gfx::Size(50, 50));
  fixed->SetDrawsContent(true);

  scroller->test_properties()->AddChild(std::move(fixed));
  container->test_properties()->AddChild(std::move(scroller));
  root->test_properties()->AddChild(std::move(container));
  root->layer_tree_impl()->SetRootLayerForTesting(std::move(root_ptr));
  root->layer_tree_impl()->BuildPropertyTreesForTesting();

  // Rounded to integers already.
  {
    gfx::Vector2dF scroll_delta(3.0, 5.0);
    SetScrollOffsetDelta(scroll_layer, scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list);
    root->layer_tree_impl()
        ->property_trees()
        ->transform_tree.set_source_to_parent_updates_allowed(false);
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - scroll_delta);
  }

  // Scroll delta requiring rounding.
  {
    gfx::Vector2dF scroll_delta(4.1f, 8.1f);
    SetScrollOffsetDelta(scroll_layer, scroll_delta);

    gfx::Vector2dF rounded_scroll_delta(4.f, 8.f);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - rounded_scroll_delta);
  }

  // Scale is applied earlier in the tree.
  {
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
    gfx::Transform scaled_container_transform = container_transform;
    scaled_container_transform.Scale3d(2.0, 2.0, 1.0);
    container_layer->test_properties()->transform = scaled_container_transform;

    root->layer_tree_impl()->property_trees()->needs_rebuild = true;

    gfx::Vector2dF scroll_delta(4.5f, 8.5f);
    SetScrollOffsetDelta(scroll_layer, scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);

    container_layer->test_properties()->transform = container_transform;
  }
}

TEST_F(LayerTreeHostCommonTest,
       ScrollSnappingWithAnimatedScreenSpaceTransform) {
  // This test verifies that a scrolling layer whose screen space transform is
  // animating doesn't get snapped to integer coordinates.
  //
  // + root
  //   + animated layer
  //     + surface
  //       + container
  //         + scroller
  //
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* animated_layer = AddChildToRoot<FakePictureLayerImpl>();
  LayerImpl* surface = AddChild<LayerImpl>(animated_layer);
  LayerImpl* container = AddChild<LayerImpl>(surface);
  LayerImpl* scroller = AddChild<LayerImpl>(container);

  gfx::Transform start_scale;
  start_scale.Scale(1.5f, 1.5f);

  root->SetBounds(gfx::Size(50, 50));
  animated_layer->test_properties()->transform = start_scale;
  animated_layer->SetBounds(gfx::Size(50, 50));
  surface->SetBounds(gfx::Size(50, 50));
  surface->test_properties()->force_render_surface = true;
  container->SetBounds(gfx::Size(50, 50));
  scroller->SetBounds(gfx::Size(100, 100));
  scroller->SetScrollClipLayer(container->id());
  scroller->SetDrawsContent(true);

  gfx::Transform end_scale;
  end_scale.Scale(2.f, 2.f);
  TransformOperations start_operations;
  start_operations.AppendMatrix(start_scale);
  TransformOperations end_operations;
  end_operations.AppendMatrix(end_scale);
  SetElementIdsForTesting();

  AddAnimatedTransformToElementWithPlayer(animated_layer->element_id(),
                                          timeline_impl(), 1.0,
                                          start_operations, end_operations);
  gfx::Vector2dF scroll_delta(5.f, 9.f);
  SetScrollOffsetDelta(scroller, scroll_delta);

  ExecuteCalculateDrawProperties(root);

  gfx::Vector2dF expected_draw_transform_translation(-7.5f, -13.5f);
  EXPECT_VECTOR2DF_EQ(expected_draw_transform_translation,
                      scroller->DrawTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, ScrollSnappingWithScrollChild) {
  // This test verifies that a scrolling child of a scrolling layer doesn't get
  // snapped to integer coordinates.
  //
  // + root
  //   + container
  //    + scroller
  //   + scroll_child
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> scroll_child = Layer::Create();
  root->AddChild(container);
  root->AddChild(scroll_child);
  container->AddChild(scroller);
  host()->SetRootLayer(root);

  scroller->SetScrollClipLayerId(container->id());
  scroll_child->SetScrollParent(scroller.get());

  gfx::Transform rotate;
  rotate.RotateAboutYAxis(30);
  root->SetBounds(gfx::Size(50, 50));
  container->SetBounds(gfx::Size(50, 50));
  scroller->SetBounds(gfx::Size(100, 100));
  scroller->SetPosition(gfx::PointF(10.3f, 10.3f));
  scroll_child->SetBounds(gfx::Size(10, 10));
  scroll_child->SetTransform(rotate);

  ExecuteCalculateDrawProperties(root.get());

  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* scroll_child_impl = layer_tree_impl->LayerById(scroll_child->id());
  gfx::Vector2dF scroll_delta(5.f, 9.f);
  SetScrollOffsetDelta(scroller_impl, scroll_delta);

  ExecuteCalculateDrawProperties(root_impl);

  gfx::Vector2dF expected_scroller_screen_space_transform_translation(5.f, 1.f);
  EXPECT_VECTOR2DF_EQ(expected_scroller_screen_space_transform_translation,
                      scroller_impl->ScreenSpaceTransform().To2dTranslation());

  gfx::Transform expected_scroll_child_screen_space_transform;
  expected_scroll_child_screen_space_transform.Translate(-5.3f, -9.3f);
  expected_scroll_child_screen_space_transform.RotateAboutYAxis(30);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_scroll_child_screen_space_transform,
                                  scroll_child_impl->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionTop) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(10, 20);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(10, 20));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 20.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(5.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5.f, 15.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 15.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container (note: this element does not
  // have its own layer as it does not need to be composited).
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 50.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, -10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionTopScrollParent) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  root->AddChild(sticky_pos);
  sticky_pos->SetScrollParent(scroller.get());
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(10, 20);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(200, 200));
  container->SetBounds(gfx::Size(100, 100));
  container->SetPosition(gfx::PointF(50, 50));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(60, 70));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(60.f, 70.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(5.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(55.f, 65.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 15.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(45.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(45.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container (note: this element does not
  // have its own layer as it does not need to be composited).
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 50.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(45.f, 40.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionSubpixelScroll) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 200);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 200, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 100, 500);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(100, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 200));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 0.8f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionBottom) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 150);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 150, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 100, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 150));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  // Initially the sticky element is moved up to the top of the container.
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 100.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 95.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Once we get past the top of the container it moves to be aligned 10px
  // up from the the bottom of the scroller.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 30.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Once we scroll past its initial location, it sticks there.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 150.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionBottomInnerViewportDelta) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(root->id());
  host()->GetLayerTree()->RegisterViewportLayers(nullptr, root, scroller,
                                                 nullptr);

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 70);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 70, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 60, 100, 100);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(100, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 70));

  ExecuteCalculateDrawProperties(root.get(), 1.f, 1.f, root.get(),
                                 scroller.get(), nullptr);
  host()->CommitAndCreateLayerImplTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();
  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  ASSERT_EQ(scroller->id(), layer_tree_impl->InnerViewportScrollLayer()->id());

  LayerImpl* inner_scroll = layer_tree_impl->InnerViewportScrollLayer();
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  // Initially the sticky element is moved to the bottom of the container.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // We start to hide the toolbar, but not far enough that the sticky element
  // should be moved up yet.
  root_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -10.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 nullptr);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // On hiding more of the toolbar the sticky element starts to stick.
  root_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -20.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 nullptr);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // On hiding more the sticky element stops moving as it has reached its
  // limit.
  root_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -30.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 nullptr);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionBottomOuterViewportDelta) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> outer_clip = Layer::Create();
  scoped_refptr<Layer> outer_viewport = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(scroller);
  scroller->AddChild(outer_clip);
  outer_clip->AddChild(outer_viewport);
  outer_viewport->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(root->id());
  outer_viewport->SetScrollClipLayerId(outer_clip->id());
  host()->GetLayerTree()->RegisterViewportLayers(nullptr, root, scroller,
                                                 outer_viewport);

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 70);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 70, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 60, 100, 100);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(100, 1000));
  outer_clip->SetBounds(gfx::Size(100, 100));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 70));

  ExecuteCalculateDrawProperties(root.get(), 1.f, 1.f, root.get(),
                                 scroller.get(), outer_viewport.get());
  host()->CommitAndCreateLayerImplTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();
  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  ASSERT_EQ(outer_viewport->id(),
            layer_tree_impl->OuterViewportScrollLayer()->id());

  LayerImpl* inner_scroll = layer_tree_impl->InnerViewportScrollLayer();
  LayerImpl* outer_scroll = layer_tree_impl->OuterViewportScrollLayer();
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());
  LayerImpl* outer_clip_impl = layer_tree_impl->LayerById(outer_clip->id());

  // Initially the sticky element is moved to the bottom of the container.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // We start to hide the toolbar, but not far enough that the sticky element
  // should be moved up yet.
  outer_clip_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -10.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 outer_scroll);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // On hiding more of the toolbar the sticky element starts to stick.
  outer_clip_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -20.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 outer_scroll);

  // On hiding more the sticky element stops moving as it has reached its
  // limit.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  outer_clip_impl->SetBoundsDelta(gfx::Vector2dF(0.f, -30.f));
  ExecuteCalculateDrawProperties(root_impl, 1.f, 1.f, root_impl, inner_scroll,
                                 outer_scroll);

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, StickyPositionLeftRight) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_left = true;
  sticky_position.is_anchored_right = true;
  sticky_position.left_offset = 10.f;
  sticky_position.right_offset = 10.f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(145, 0);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(145, 0, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(100, 0, 100, 100);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(145, 0));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  // Initially the sticky element is moved the leftmost side of the container.
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(100.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(5.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(95.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Once we get past the left side of the container it moves to be aligned 10px
  // up from the the right of the scroller.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(25.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(80.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(30.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(80.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // When we get to the sticky element's original position we stop sticking
  // to the right.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(95.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(50.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(105.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(40.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // The element starts sticking to the left once we scroll far enough.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(150.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(155.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Finally it stops sticking when it hits the right side of the container.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(190.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(195.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

// This test ensures that the compositor sticky position code correctly accounts
// for the element having been given a position from the main thread sticky
// position code.
TEST_F(LayerTreeHostCommonTest, StickyPositionMainThreadUpdates) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(10, 20);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(10, 20));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 20.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(5.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5.f, 15.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(15.f, 15.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Now the main thread commits the new position of the sticky element.
  scroller->SetScrollOffset(gfx::ScrollOffset(15, 15));
  sticky_pos->SetPosition(gfx::PointF(10, 25));
  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  layer_tree_impl = host()->host_impl()->active_tree();
  root_impl = layer_tree_impl->LayerById(root->id());
  scroller_impl = layer_tree_impl->LayerById(scroller->id());
  sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  // The element should still be where it was before. We reset the delta to
  // (0, 0) because we have synced a scroll offset of (15, 15) from the main
  // thread.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // And if we scroll a little further it remains there.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 10.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

// This tests the main thread updates with a composited sticky container. In
// this case the position received from main is relative to the container but
// the constraint rects are relative to the ancestor scroller.
TEST_F(LayerTreeHostCommonTest, StickyPositionCompositedContainer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_container = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_container);
  sticky_container->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  // The sticky position layer is only offset by (0, 10) from its parent
  // layer, this position is used to determine the offset applied by the main
  // thread.
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 10);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(20, 30, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(20, 20, 30, 30);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_container->SetPosition(gfx::PointF(20, 20));
  sticky_container->SetBounds(gfx::Size(30, 30));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 10));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 30.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 25.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Now the main thread commits the new position of the sticky element.
  scroller->SetScrollOffset(gfx::ScrollOffset(0, 25));
  sticky_pos->SetPosition(gfx::PointF(0, 15));
  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  layer_tree_impl = host()->host_impl()->active_tree();
  root_impl = layer_tree_impl->LayerById(root->id());
  scroller_impl = layer_tree_impl->LayerById(scroller->id());
  sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  // The element should still be where it was before. We reset the delta to
  // (0, 0) because we have synced a scroll offset of (0, 25) from the main
  // thread.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 0.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // And if we scroll a little further it remains there.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 5.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // And hits the bottom of the container.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 10.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 5.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

// A transform on a sticky element should not affect its sticky position.
TEST_F(LayerTreeHostCommonTest, StickyPositionScaledStickyBox) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());
  gfx::Transform t;
  t.Scale(2, 2);
  sticky_pos->SetTransform(t);

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 0.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 20);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 20));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 20.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 15.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 5.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the box is positioned at the scroller
  // edge.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 30.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 50.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, -10.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

// Tests that a transform does not affect the sticking points. The sticky
// element will however move relative to the viewport due to its transform.
TEST_F(LayerTreeHostCommonTest, StickyPositionScaledContainer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> sticky_container = Layer::Create();
  scoped_refptr<Layer> sticky_pos = Layer::Create();
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(sticky_container);
  sticky_container->AddChild(sticky_pos);
  host()->SetRootLayer(root);
  scroller->SetScrollClipLayerId(container->id());
  gfx::Transform t;
  t.Scale(2, 2);
  sticky_container->SetTransform(t);

  LayerStickyPositionConstraint sticky_position;
  sticky_position.is_sticky = true;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 0.0f;
  sticky_position.parent_relative_sticky_box_offset = gfx::Point(0, 20);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::Rect(0, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::Rect(0, 0, 50, 50);
  sticky_pos->SetStickyPositionConstraint(sticky_position);

  root->SetBounds(gfx::Size(100, 100));
  container->SetBounds(gfx::Size(100, 100));
  scroller->SetBounds(gfx::Size(1000, 1000));
  sticky_container->SetBounds(gfx::Size(50, 50));
  sticky_pos->SetBounds(gfx::Size(10, 10));
  sticky_pos->SetPosition(gfx::PointF(0, 20));

  ExecuteCalculateDrawProperties(root.get());
  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* sticky_pos_impl = layer_tree_impl->LayerById(sticky_pos->id());

  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 40.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 15.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the box is positioned at the scroller
  // edge but is also scaled by its container so it begins to move down.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 25.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 30.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 30.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container.
  SetScrollOffsetDelta(scroller_impl, gfx::Vector2dF(0.f, 50.f));
  ExecuteCalculateDrawProperties(root_impl);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 30.f),
      sticky_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostCommonTest, NonFlatContainerForFixedPosLayer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> fixed_pos = Layer::Create();

  scroller->SetIsContainerForFixedPositionLayers(true);
  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(fixed_pos);
  host()->SetRootLayer(root);

  LayerPositionConstraint fixed_position;
  fixed_position.set_is_fixed_position(true);
  scroller->SetScrollClipLayerId(container->id());
  fixed_pos->SetPositionConstraint(fixed_position);

  root->SetBounds(gfx::Size(50, 50));
  container->SetBounds(gfx::Size(50, 50));
  scroller->SetBounds(gfx::Size(50, 50));
  fixed_pos->SetBounds(gfx::Size(50, 50));

  gfx::Transform rotate;
  rotate.RotateAboutXAxis(20);
  container->SetTransform(rotate);

  ExecuteCalculateDrawProperties(root.get());
  TransformTree& tree = root->GetLayerTree()->property_trees()->transform_tree;
  gfx::Transform transform;
  tree.ComputeTranslation(fixed_pos->transform_tree_index(),
                          container->transform_tree_index(), &transform);
  EXPECT_TRUE(transform.IsIdentity());
}

TEST_F(LayerTreeHostCommonTest, ScrollSnappingWithFixedPosChild) {
  // This test verifies that a fixed pos child of a scrolling layer doesn't get
  // snapped to integer coordinates.
  //
  // + root
  //   + container
  //    + scroller
  //     + fixed_pos
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> container = Layer::Create();
  scoped_refptr<Layer> scroller = Layer::Create();
  scoped_refptr<Layer> fixed_pos = Layer::Create();

  scroller->SetIsContainerForFixedPositionLayers(true);

  root->AddChild(container);
  container->AddChild(scroller);
  scroller->AddChild(fixed_pos);
  host()->SetRootLayer(root);

  LayerPositionConstraint fixed_position;
  fixed_position.set_is_fixed_position(true);
  scroller->SetScrollClipLayerId(container->id());
  fixed_pos->SetPositionConstraint(fixed_position);

  root->SetBounds(gfx::Size(50, 50));
  container->SetBounds(gfx::Size(50, 50));
  scroller->SetBounds(gfx::Size(100, 100));
  scroller->SetPosition(gfx::PointF(10.3f, 10.3f));
  fixed_pos->SetBounds(gfx::Size(10, 10));

  ExecuteCalculateDrawProperties(root.get());

  host()->host_impl()->CreatePendingTree();
  host()->CommitAndCreatePendingTree();
  host()->host_impl()->ActivateSyncTree();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  LayerImpl* root_impl = layer_tree_impl->LayerById(root->id());
  LayerImpl* scroller_impl = layer_tree_impl->LayerById(scroller->id());
  LayerImpl* fixed_pos_impl = layer_tree_impl->LayerById(fixed_pos->id());
  gfx::Vector2dF scroll_delta(5.f, 9.f);
  SetScrollOffsetDelta(scroller_impl, scroll_delta);

  ExecuteCalculateDrawProperties(root_impl);

  gfx::Vector2dF expected_scroller_screen_space_transform_translation(5.f, 1.f);
  EXPECT_VECTOR2DF_EQ(expected_scroller_screen_space_transform_translation,
                      scroller_impl->ScreenSpaceTransform().To2dTranslation());

  gfx::Vector2dF expected_fixed_pos_screen_space_transform_translation(10.3f,
                                                                       10.3f);
  EXPECT_VECTOR2DF_EQ(expected_fixed_pos_screen_space_transform_translation,
                      fixed_pos_impl->ScreenSpaceTransform().To2dTranslation());
}

class AnimationScaleFactorTrackingLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return base::WrapUnique(
        new AnimationScaleFactorTrackingLayerImpl(tree_impl, id));
  }

  ~AnimationScaleFactorTrackingLayerImpl() override {}

 private:
  explicit AnimationScaleFactorTrackingLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
      : LayerImpl(tree_impl, id) {
    SetDrawsContent(true);
  }
};

TEST_F(LayerTreeHostCommonTest, MaximumAnimationScaleFactor) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings = host()->GetSettings();
  settings.layer_transforms_should_scale_layer_contents = true;
  FakeLayerTreeHostImpl host_impl(settings, &task_runner_provider,
                                  &task_graph_runner);
  std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> grand_parent =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> parent =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 2);
  std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> child =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 3);
  std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> grand_child =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 4);

  AnimationScaleFactorTrackingLayerImpl* parent_raw = parent.get();
  AnimationScaleFactorTrackingLayerImpl* child_raw = child.get();
  AnimationScaleFactorTrackingLayerImpl* grand_child_raw = grand_child.get();
  AnimationScaleFactorTrackingLayerImpl* grand_parent_raw = grand_parent.get();

  grand_parent->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(1, 2));
  child->SetBounds(gfx::Size(1, 2));
  grand_child->SetBounds(gfx::Size(1, 2));

  child->test_properties()->AddChild(std::move(grand_child));
  parent->test_properties()->AddChild(std::move(child));
  grand_parent->test_properties()->AddChild(std::move(parent));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(grand_parent));

  ExecuteCalculateDrawProperties(grand_parent_raw);

  // No layers have animations.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  TransformOperations translation;
  translation.AppendTranslate(1.f, 2.f, 3.f);

  scoped_refptr<AnimationTimeline> timeline;
  timeline = AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host_impl.animation_host()->AddAnimationTimeline(timeline);

  host_impl.active_tree()->SetElementIdsForTesting();

  scoped_refptr<AnimationPlayer> grand_parent_player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline->AttachPlayer(grand_parent_player);
  grand_parent_player->AttachElement(grand_parent_raw->element_id());

  scoped_refptr<AnimationPlayer> parent_player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline->AttachPlayer(parent_player);
  parent_player->AttachElement(parent_raw->element_id());

  scoped_refptr<AnimationPlayer> child_player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline->AttachPlayer(child_player);
  child_player->AttachElement(child_raw->element_id());

  scoped_refptr<AnimationPlayer> grand_child_player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline->AttachPlayer(grand_child_player);
  grand_child_player->AttachElement(grand_child_raw->element_id());

  AddAnimatedTransformToPlayer(parent_player.get(), 1.0, TransformOperations(),
                               translation);

  // No layers have scale-affecting animations.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  TransformOperations scale;
  scale.AppendScale(5.f, 4.f, 3.f);

  AddAnimatedTransformToPlayer(child_player.get(), 1.0, TransformOperations(),
                               scale);
  child_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // Only |child| has a scale-affecting animation.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(5.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(5.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(1.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(1.f, GetStartingAnimationScale(grand_child_raw));

  AddAnimatedTransformToPlayer(grand_parent_player.get(), 1.0,
                               TransformOperations(), scale);
  grand_parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |grand_parent| and |child| have scale-affecting animations.
  EXPECT_EQ(5.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(5.f, GetMaximumAnimationScale(parent_raw));
  // We don't support combining animated scales from two nodes; 0.f means
  // that the maximum scale could not be computed.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(1.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(1.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  AddAnimatedTransformToPlayer(parent_player.get(), 1.0, TransformOperations(),
                               scale);
  parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |grand_parent|, |parent|, and |child| have scale-affecting animations.
  EXPECT_EQ(5.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(1.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  grand_parent_player->AbortAnimations(TargetProperty::TRANSFORM, false);
  parent_player->AbortAnimations(TargetProperty::TRANSFORM, false);
  child_player->AbortAnimations(TargetProperty::TRANSFORM, false);

  TransformOperations perspective;
  perspective.AppendPerspective(10.f);

  AddAnimatedTransformToPlayer(child_player.get(), 1.0, TransformOperations(),
                               perspective);
  child_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |child| has a scale-affecting animation but computing the maximum of this
  // animation is not supported.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  child_player->AbortAnimations(TargetProperty::TRANSFORM, false);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(1.f, 2.f);
  grand_parent_raw->test_properties()->transform = scale_matrix;
  parent_raw->test_properties()->transform = scale_matrix;
  grand_parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  AddAnimatedTransformToPlayer(parent_player.get(), 1.0, TransformOperations(),
                               scale);
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |grand_parent| and |parent| each have scale 2.f. |parent| has a  scale
  // animation with maximum scale 5.f.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(10.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(10.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(10.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(2.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(2.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(2.f, GetStartingAnimationScale(grand_child_raw));

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.f);
  child_raw->test_properties()->transform = perspective_matrix;
  grand_parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |child| has a transform that's neither a translation nor a scale.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(10.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(2.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  parent_raw->test_properties()->transform = perspective_matrix;
  grand_parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |parent| and |child| have transforms that are neither translations nor
  // scales.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));

  parent_raw->test_properties()->transform = gfx::Transform();
  child_raw->test_properties()->transform = gfx::Transform();
  grand_parent_raw->test_properties()->transform = perspective_matrix;
  grand_parent_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(grand_parent_raw);

  // |grand_parent| has a transform that's neither a translation nor a scale.
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetMaximumAnimationScale(grand_child_raw));

  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(parent_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(child_raw));
  EXPECT_EQ(0.f, GetStartingAnimationScale(grand_child_raw));
}

static void GatherDrawnLayers(const LayerImplList* rsll,
                              std::set<LayerImpl*>* drawn_layers) {
  for (LayerIterator it = LayerIterator::Begin(rsll),
                     end = LayerIterator::End(rsll);
       it != end; ++it) {
    LayerImpl* layer = *it;
    if (it.represents_itself())
      drawn_layers->insert(layer);

    if (!it.represents_contributing_render_surface())
      continue;

    if (layer->render_surface()->MaskLayer())
      drawn_layers->insert(layer->render_surface()->MaskLayer());
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceLayerListMembership) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> grand_parent =
      LayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<LayerImpl> parent =
      LayerImpl::Create(host_impl.active_tree(), 3);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.active_tree(), 5);
  std::unique_ptr<LayerImpl> grand_child1 =
      LayerImpl::Create(host_impl.active_tree(), 7);
  std::unique_ptr<LayerImpl> grand_child2 =
      LayerImpl::Create(host_impl.active_tree(), 9);

  LayerImpl* grand_parent_raw = grand_parent.get();
  LayerImpl* parent_raw = parent.get();
  LayerImpl* child_raw = child.get();
  LayerImpl* grand_child1_raw = grand_child1.get();
  LayerImpl* grand_child2_raw = grand_child2.get();

  grand_parent->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(1, 2));
  child->SetBounds(gfx::Size(1, 2));
  grand_child1->SetBounds(gfx::Size(1, 2));
  grand_child2->SetBounds(gfx::Size(1, 2));

  child->test_properties()->AddChild(std::move(grand_child1));
  child->test_properties()->AddChild(std::move(grand_child2));
  parent->test_properties()->AddChild(std::move(child));
  grand_parent->test_properties()->AddChild(std::move(parent));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(grand_parent));

  // Start with nothing being drawn.
  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  std::set<LayerImpl*> expected;
  std::set<LayerImpl*> actual;
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // If we force render surface, but none of the layers are in the layer list,
  // then this layer should not appear in RSLL.
  grand_child1_raw->SetHasRenderSurface(true);
  grand_child1_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // However, if we say that this layer also draws content, it will appear in
  // RSLL.
  grand_child1_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(grand_child1_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Now child is forced to have a render surface, and one if its children draws
  // content.
  grand_child1_raw->SetDrawsContent(false);
  grand_child1_raw->SetHasRenderSurface(false);
  grand_child1_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;
  child_raw->SetHasRenderSurface(true);
  grand_child2_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(grand_child2_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Add a mask layer to child.
  child_raw->test_properties()->SetMaskLayer(
      LayerImpl::Create(host_impl.active_tree(), 6));
  child_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(child_raw->test_properties()
                  ->mask_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->test_properties()->mask_layer);

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->test_properties()->mask_layer);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(child_raw->test_properties()
                  ->mask_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->test_properties()->mask_layer);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  child_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  // With nothing drawing, we should have no layers.
  grand_child2_raw->SetDrawsContent(false);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(child_raw->test_properties()
                   ->mask_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Child itself draws means that we should have the child and the mask in the
  // list.
  child_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_FALSE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(child_raw->test_properties()
                  ->mask_layer->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_FALSE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(child_raw);
  expected.insert(child_raw->test_properties()->mask_layer);
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  child_raw->test_properties()->SetMaskLayer(nullptr);
  child_raw->layer_tree_impl()->property_trees()->needs_rebuild = true;

  // Now everyone's a member!
  grand_parent_raw->SetDrawsContent(true);
  parent_raw->SetDrawsContent(true);
  child_raw->SetDrawsContent(true);
  grand_child1_raw->SetDrawsContent(true);
  grand_child2_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);

  EXPECT_TRUE(grand_parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(parent_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(child_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child1_raw->is_drawn_render_surface_layer_list_member());
  EXPECT_TRUE(grand_child2_raw->is_drawn_render_surface_layer_list_member());

  expected.clear();
  expected.insert(grand_parent_raw);
  expected.insert(parent_raw);
  expected.insert(child_raw);
  expected.insert(grand_child1_raw);
  expected.insert(grand_child2_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, DrawPropertyScales) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings = host()->GetSettings();
  settings.layer_transforms_should_scale_layer_contents = true;
  FakeLayerTreeHostImpl host_impl(settings, &task_runner_provider,
                                  &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root_layer = root.get();
  std::unique_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* child1_layer = child1.get();
  std::unique_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* child2_layer = child2.get();

  gfx::Transform scale_transform_child1, scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  root->SetBounds(gfx::Size(1, 1));
  root->SetDrawsContent(true);
  child1_layer->test_properties()->transform = scale_transform_child1;
  child1_layer->SetBounds(gfx::Size(1, 1));
  child1_layer->SetDrawsContent(true);

  child1_layer->test_properties()->SetMaskLayer(
      LayerImpl::Create(host_impl.active_tree(), 4));

  root->test_properties()->AddChild(std::move(child1));
  root->test_properties()->AddChild(std::move(child2));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.active_tree()->SetElementIdsForTesting();

  ExecuteCalculateDrawProperties(root_layer);

  TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host_impl.animation_host()->AddAnimationTimeline(timeline);

  child2_layer->test_properties()->transform = scale_transform_child2;
  child2_layer->SetBounds(gfx::Size(1, 1));
  child2_layer->SetDrawsContent(true);
  AddAnimatedTransformToElementWithPlayer(child2_layer->element_id(), timeline,
                                          1.0, TransformOperations(), scale);

  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_layer);

  EXPECT_FLOAT_EQ(1.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(3.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(
      3.f,
      child1_layer->test_properties()->mask_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(5.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(child1_layer));
  EXPECT_FLOAT_EQ(8.f, GetMaximumAnimationScale(child2_layer));

  // Changing page-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  float page_scale_factor = 3.f;
  float device_scale_factor = 1.0f;
  std::vector<LayerImpl*> render_surface_layer_list;
  gfx::Size device_viewport_size =
      gfx::Size(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, device_viewport_size, &render_surface_layer_list);

  inputs.page_scale_factor = page_scale_factor;
  inputs.can_adjust_raster_scales = true;
  inputs.page_scale_layer = root_layer;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  EXPECT_FLOAT_EQ(3.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(9.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(
      9.f,
      child1_layer->test_properties()->mask_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(15.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(child1_layer));
  EXPECT_FLOAT_EQ(24.f, GetMaximumAnimationScale(child2_layer));

  // Changing device-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  device_scale_factor = 4.0f;
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  EXPECT_FLOAT_EQ(12.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(36.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(
      36.f,
      child1_layer->test_properties()->mask_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(60.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(child1_layer));
  EXPECT_FLOAT_EQ(96.f, GetMaximumAnimationScale(child2_layer));
}

TEST_F(LayerTreeHostCommonTest, AnimationScales) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings = host()->GetSettings();
  settings.layer_transforms_should_scale_layer_contents = true;
  FakeLayerTreeHostImpl host_impl(settings, &task_runner_provider,
                                  &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root_layer = root.get();
  std::unique_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* child1_layer = child1.get();
  std::unique_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* child2_layer = child2.get();

  root->test_properties()->AddChild(std::move(child1));
  child1_layer->test_properties()->AddChild(std::move(child2));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));

  host_impl.active_tree()->SetElementIdsForTesting();

  gfx::Transform scale_transform_child1, scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  root_layer->SetBounds(gfx::Size(1, 1));
  child1_layer->test_properties()->transform = scale_transform_child1;
  child1_layer->SetBounds(gfx::Size(1, 1));
  child2_layer->test_properties()->transform = scale_transform_child2;
  child2_layer->SetBounds(gfx::Size(1, 1));

  TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host_impl.animation_host()->AddAnimationTimeline(timeline);

  AddAnimatedTransformToElementWithPlayer(child2_layer->element_id(), timeline,
                                          1.0, TransformOperations(), scale);

  // Correctly computes animation scale when rebuilding property trees.
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_layer);

  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(child1_layer));
  EXPECT_FLOAT_EQ(24.f, GetMaximumAnimationScale(child2_layer));

  EXPECT_FLOAT_EQ(0.f, GetStartingAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetStartingAnimationScale(child1_layer));
  EXPECT_FLOAT_EQ(3.f, GetStartingAnimationScale(child2_layer));

  // Correctly updates animation scale when layer property changes.
  child1_layer->test_properties()->transform = gfx::Transform();
  root_layer->layer_tree_impl()
      ->property_trees()
      ->transform_tree.OnTransformAnimated(gfx::Transform(),
                                           child1_layer->transform_tree_index(),
                                           root_layer->layer_tree_impl());
  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = false;
  ExecuteCalculateDrawProperties(root_layer);
  EXPECT_FLOAT_EQ(8.f, GetMaximumAnimationScale(child2_layer));
  EXPECT_FLOAT_EQ(1.f, GetStartingAnimationScale(child2_layer));

  // Do not update animation scale if already updated.
  host_impl.active_tree()->property_trees()->SetAnimationScalesForTesting(
      child2_layer->transform_tree_index(), 100.f, 100.f);
  EXPECT_FLOAT_EQ(100.f, GetMaximumAnimationScale(child2_layer));
  EXPECT_FLOAT_EQ(100.f, GetStartingAnimationScale(child2_layer));
}

TEST_F(LayerTreeHostCommonTest,
       AnimationScaleWhenLayerTransformShouldNotScaleLayerBounds) {
  // Returns empty scale if layer_transforms_should_scale_layer_contents is
  // false.
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings = host()->GetSettings();
  settings.layer_transforms_should_scale_layer_contents = false;
  FakeLayerTreeHostImpl host_impl(settings, &task_runner_provider,
                                  &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root_layer = root.get();
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* child_layer = child.get();

  root->test_properties()->AddChild(std::move(child));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));

  host_impl.active_tree()->SetElementIdsForTesting();

  gfx::Transform scale_transform_child;
  scale_transform_child.Scale(4, 5);

  root_layer->SetBounds(gfx::Size(1, 1));
  child_layer->test_properties()->transform = scale_transform_child;
  child_layer->SetBounds(gfx::Size(1, 1));

  TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host_impl.animation_host()->AddAnimationTimeline(timeline);

  AddAnimatedTransformToElementWithPlayer(child_layer->element_id(), timeline,
                                          1.0, TransformOperations(), scale);

  root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_layer);

  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetMaximumAnimationScale(child_layer));

  EXPECT_FLOAT_EQ(0.f, GetStartingAnimationScale(root_layer));
  EXPECT_FLOAT_EQ(0.f, GetStartingAnimationScale(child_layer));
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectInChildRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip = AddChild<LayerImpl>(root);
  LayerImpl* content = AddChild<LayerImpl>(clip);

  root->SetBounds(gfx::Size(768 / 2, 3000));
  root->SetDrawsContent(true);
  clip->SetBounds(gfx::Size(768 / 2, 10000));
  clip->SetMasksToBounds(true);
  content->SetBounds(gfx::Size(768 / 2, 10000));
  content->SetDrawsContent(true);
  content->test_properties()->force_render_surface = true;

  gfx::Size device_viewport_size(768, 582);
  LayerImplList render_surface_layer_list_impl;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, device_viewport_size, gfx::Transform(),
      &render_surface_layer_list_impl);
  inputs.device_scale_factor = 2.f;
  inputs.page_scale_factor = 1.f;
  inputs.page_scale_layer = nullptr;
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  // Layers in the root render surface have their visible content rect clipped
  // by the viewport.
  EXPECT_EQ(gfx::Rect(768 / 2, 582 / 2), root->visible_layer_rect());

  // Layers drawing to a child render surface should still have their visible
  // content rect clipped by the viewport.
  EXPECT_EQ(gfx::Rect(768 / 2, 582 / 2), content->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, BoundsDeltaAffectVisibleContentRect) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  // Set two layers: the root layer clips it's child,
  // the child draws its content.

  gfx::Size root_size = gfx::Size(300, 500);

  // Sublayer should be bigger than the root enlarged by bounds_delta.
  gfx::Size sublayer_size = gfx::Size(300, 1000);

  // Device viewport accomidated the root and the browser controls.
  gfx::Size device_viewport_size = gfx::Size(300, 600);

  host_impl.SetViewportSize(device_viewport_size);
  host_impl.active_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl.active_tree(), 1));

  LayerImpl* root = host_impl.active_tree()->root_layer_for_testing();
  root->SetBounds(root_size);
  root->SetMasksToBounds(true);

  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl.active_tree(), 2));

  LayerImpl* sublayer = root->test_properties()->children[0];
  sublayer->SetBounds(sublayer_size);
  sublayer->SetDrawsContent(true);

  host_impl.active_tree()->BuildPropertyTreesForTesting();

  LayerImplList layer_impl_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, device_viewport_size, &layer_impl_list);

  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  EXPECT_EQ(gfx::Rect(root_size), sublayer->visible_layer_rect());

  root->SetBoundsDelta(gfx::Vector2dF(0.0, 50.0));
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  gfx::Rect affected_by_delta(0, 0, root_size.width(),
                              root_size.height() + 50);
  EXPECT_EQ(affected_by_delta, sublayer->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, NodesAffectedByBoundsDeltaGetUpdated) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> inner_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

  root->AddChild(inner_viewport_container_layer);
  inner_viewport_container_layer->AddChild(inner_viewport_scroll_layer);
  inner_viewport_scroll_layer->AddChild(outer_viewport_container_layer);
  outer_viewport_container_layer->AddChild(outer_viewport_scroll_layer);

  inner_viewport_scroll_layer->SetScrollClipLayerId(
      inner_viewport_container_layer->id());
  outer_viewport_scroll_layer->SetScrollClipLayerId(
      outer_viewport_container_layer->id());

  inner_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  outer_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);
  host()->GetLayerTree()->RegisterViewportLayers(
      nullptr, root, inner_viewport_scroll_layer, outer_viewport_scroll_layer);

  scoped_refptr<Layer> fixed_to_inner = Layer::Create();
  scoped_refptr<Layer> fixed_to_outer = Layer::Create();

  inner_viewport_scroll_layer->AddChild(fixed_to_inner);
  outer_viewport_scroll_layer->AddChild(fixed_to_outer);

  LayerPositionConstraint fixed_to_right;
  fixed_to_right.set_is_fixed_position(true);
  fixed_to_right.set_is_fixed_to_right_edge(true);

  fixed_to_inner->SetPositionConstraint(fixed_to_right);
  fixed_to_outer->SetPositionConstraint(fixed_to_right);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());

  TransformTree& transform_tree = host()->property_trees()->transform_tree;
  EXPECT_TRUE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_TRUE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());

  LayerPositionConstraint fixed_to_left;
  fixed_to_left.set_is_fixed_position(true);
  fixed_to_inner->SetPositionConstraint(fixed_to_left);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_TRUE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());

  fixed_to_outer->SetPositionConstraint(fixed_to_left);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectForAnimatedLayer) {
  host_impl()->CreatePendingTree();
  std::unique_ptr<LayerImpl> pending_root =
      LayerImpl::Create(host_impl()->pending_tree(), 1);
  LayerImpl* root = pending_root.get();
  host_impl()->pending_tree()->SetRootLayerForTesting(std::move(pending_root));
  std::unique_ptr<LayerImpl> animated_ptr =
      LayerImpl::Create(host_impl()->pending_tree(), 2);
  LayerImpl* animated = animated_ptr.get();
  root->test_properties()->AddChild(std::move(animated_ptr));

  animated->SetDrawsContent(true);
  host_impl()->pending_tree()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  root->SetMasksToBounds(true);
  root->test_properties()->force_render_surface = true;
  animated->test_properties()->opacity = 0.f;
  animated->SetBounds(gfx::Size(20, 20));

  AddOpacityTransitionToElementWithPlayer(
      animated->element_id(), timeline_impl(), 10.0, 0.f, 1.f, false);
  animated->test_properties()->opacity_can_animate = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  EXPECT_FALSE(animated->visible_layer_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleContentRectForAnimatedLayerWithSingularTransform) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip = AddChild<LayerImpl>(root);
  LayerImpl* animated = AddChild<LayerImpl>(clip);
  LayerImpl* surface = AddChild<LayerImpl>(animated);
  LayerImpl* descendant_of_animation = AddChild<LayerImpl>(surface);

  SetElementIdsForTesting();

  animated->SetDrawsContent(true);
  surface->SetDrawsContent(true);
  descendant_of_animation->SetDrawsContent(true);

  gfx::Transform uninvertible_matrix;
  uninvertible_matrix.Scale3d(6.f, 6.f, 0.f);

  root->SetBounds(gfx::Size(100, 100));
  clip->SetBounds(gfx::Size(10, 10));
  clip->SetMasksToBounds(true);
  animated->test_properties()->transform = uninvertible_matrix;
  animated->SetBounds(gfx::Size(120, 120));
  surface->SetBounds(gfx::Size(100, 100));
  surface->test_properties()->force_render_surface = true;
  descendant_of_animation->SetBounds(gfx::Size(200, 200));

  TransformOperations start_transform_operations;
  start_transform_operations.AppendMatrix(uninvertible_matrix);
  TransformOperations end_transform_operations;

  AddAnimatedTransformToElementWithPlayer(
      animated->element_id(), timeline_impl(), 10.0, start_transform_operations,
      end_transform_operations);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root);

  // The animated layer has a singular transform and maps to a non-empty rect in
  // clipped target space, so is treated as fully visible.
  EXPECT_EQ(gfx::Rect(120, 120), animated->visible_layer_rect());

  // The singular transform on |animated| is flattened when inherited by
  // |surface|, and this happens to make it invertible.
  EXPECT_EQ(gfx::Rect(2, 2), surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(2, 2), descendant_of_animation->visible_layer_rect());

  gfx::Transform zero_matrix;
  zero_matrix.Scale3d(0.f, 0.f, 0.f);
  root->layer_tree_impl()->property_trees()->transform_tree.OnTransformAnimated(
      zero_matrix, animated->transform_tree_index(), root->layer_tree_impl());
  // While computing visible rects by combining clips in screen space, we set
  // the entire layer as visible if the screen space transform is singular. This
  // is not always true when we combine clips in target space because if the
  // intersection of combined_clip in taret space with layer_rect projected to
  // target space is empty, we set it to an empty rect.
  bool skip_verify_visible_rect_calculations = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(
      root, skip_verify_visible_rect_calculations);

  // The animated layer maps to the empty rect in clipped target space, so is
  // treated as having an empty visible rect.
  EXPECT_EQ(gfx::Rect(), animated->visible_layer_rect());

  // The animated layer will be treated as fully visible when we combine clips
  // in screen space.
  gfx::Rect visible_rect = draw_property_utils::ComputeLayerVisibleRectDynamic(
      root->layer_tree_impl()->property_trees(), animated);
  EXPECT_EQ(gfx::Rect(120, 120), visible_rect);

  // This time, flattening does not make |animated|'s transform invertible. This
  // means the clip cannot be projected into |surface|'s space, so we treat
  // |surface| and layers that draw into it as having empty visible rect.
  EXPECT_EQ(gfx::Rect(), surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(), descendant_of_animation->visible_layer_rect());
}

// Verify that having animated opacity but current opacity 1 still creates
// a render surface.
TEST_F(LayerTreeHostCommonTest, AnimatedOpacityCreatesRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grandchild = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);
  grandchild->SetBounds(gfx::Size(50, 50));
  grandchild->SetDrawsContent(true);

  SetElementIdsForTesting();
  AddOpacityTransitionToElementWithPlayer(child->element_id(), timeline_impl(),
                                          10.0, 1.f, 0.2f, false);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(1.f, child->Opacity());
  EXPECT_TRUE(root->has_render_surface());
  EXPECT_TRUE(child->has_render_surface());
  EXPECT_FALSE(grandchild->has_render_surface());
}

// Verify that having an animated filter (but no current filter, as these
// are mutually exclusive) correctly creates a render surface.
TEST_F(LayerTreeHostCommonTest, AnimatedFilterCreatesRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grandchild = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  grandchild->SetBounds(gfx::Size(50, 50));

  SetElementIdsForTesting();
  AddAnimatedFilterToElementWithPlayer(child->element_id(), timeline_impl(),
                                       10.0, 0.1f, 0.2f);
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->has_render_surface());
  EXPECT_TRUE(child->has_render_surface());
  EXPECT_FALSE(grandchild->has_render_surface());

  EXPECT_TRUE(root->render_surface()->Filters().IsEmpty());
  EXPECT_TRUE(child->render_surface()->Filters().IsEmpty());

  EXPECT_FALSE(root->FilterIsAnimating());
  EXPECT_TRUE(child->FilterIsAnimating());
  EXPECT_FALSE(grandchild->FilterIsAnimating());
}

// Verify that having a filter animation with a delayed start time creates a
// render surface.
TEST_F(LayerTreeHostCommonTest, DelayedFilterAnimationCreatesRenderSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grandchild = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  grandchild->SetBounds(gfx::Size(50, 50));

  SetElementIdsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(0.1f));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(0.3f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(
      base::TimeDelta::FromMilliseconds(100), end_filters, nullptr));
  std::unique_ptr<Animation> animation =
      Animation::Create(std::move(curve), 0, 1, TargetProperty::FILTER);
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));

  AddAnimationToElementWithPlayer(child->element_id(), timeline_impl(),
                                  std::move(animation));
  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->has_render_surface());
  EXPECT_TRUE(child->has_render_surface());
  EXPECT_FALSE(grandchild->has_render_surface());

  EXPECT_TRUE(root->render_surface()->Filters().IsEmpty());
  EXPECT_TRUE(child->render_surface()->Filters().IsEmpty());

  EXPECT_FALSE(root->FilterIsAnimating());
  EXPECT_FALSE(root->HasPotentiallyRunningFilterAnimation());
  EXPECT_FALSE(child->FilterIsAnimating());
  EXPECT_TRUE(child->HasPotentiallyRunningFilterAnimation());
  EXPECT_FALSE(grandchild->FilterIsAnimating());
  EXPECT_FALSE(grandchild->HasPotentiallyRunningFilterAnimation());
}

// Ensures that the property tree code accounts for offsets between fixed
// position layers and their respective containers.
TEST_F(LayerTreeHostCommonTest, PropertyTreesAccountForFixedParentOffset) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grandchild = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  root->SetMasksToBounds(true);
  root->test_properties()->is_container_for_fixed_position_layers = true;
  child->SetPosition(gfx::PointF(1000, 1000));
  child->SetBounds(gfx::Size(50, 50));
  grandchild->SetPosition(gfx::PointF(-1000, -1000));
  grandchild->SetBounds(gfx::Size(50, 50));
  grandchild->SetDrawsContent(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  grandchild->test_properties()->position_constraint = constraint;

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), grandchild->visible_layer_rect());
}

// Ensures that the property tree code accounts for offsets between fixed
// position containers and their transform tree parents, when a fixed position
// layer's container is its layer tree parent, but this parent doesn't have its
// own transform tree node.
TEST_F(LayerTreeHostCommonTest,
       PropertyTreesAccountForFixedParentOffsetWhenContainerIsParent) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grandchild = AddChild<LayerImpl>(child);

  root->SetBounds(gfx::Size(50, 50));
  root->SetMasksToBounds(true);
  root->test_properties()->is_container_for_fixed_position_layers = true;
  child->SetPosition(gfx::PointF(1000, 1000));
  child->SetBounds(gfx::Size(50, 50));
  child->test_properties()->is_container_for_fixed_position_layers = true;
  grandchild->SetPosition(gfx::PointF(-1000, -1000));
  grandchild->SetBounds(gfx::Size(50, 50));
  grandchild->SetDrawsContent(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  grandchild->test_properties()->position_constraint = constraint;

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), grandchild->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, OnlyApplyFixedPositioningOnce) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* frame_clip = AddChild<LayerImpl>(root);
  LayerImpl* fixed = AddChild<LayerImpl>(frame_clip);
  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  root->SetBounds(gfx::Size(800, 800));
  root->test_properties()->is_container_for_fixed_position_layers = true;

  frame_clip->SetPosition(gfx::PointF(500, 100));
  frame_clip->SetBounds(gfx::Size(100, 100));
  frame_clip->SetMasksToBounds(true);

  fixed->SetBounds(gfx::Size(1000, 1000));
  fixed->SetDrawsContent(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->test_properties()->position_constraint = constraint;

  ExecuteCalculateDrawProperties(root);

  gfx::Rect expected(0, 0, 100, 100);
  EXPECT_EQ(expected, fixed->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, FixedClipsShouldBeAssociatedWithTheRightNode) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* frame_clip = AddChild<LayerImpl>(root);
  LayerImpl* scroller = AddChild<LayerImpl>(frame_clip);
  LayerImpl* fixed = AddChild<LayerImpl>(scroller);

  root->SetBounds(gfx::Size(800, 800));
  root->SetDrawsContent(true);
  root->test_properties()->is_container_for_fixed_position_layers = true;
  frame_clip->SetPosition(gfx::PointF(500, 100));
  frame_clip->SetBounds(gfx::Size(100, 100));
  frame_clip->SetMasksToBounds(true);
  frame_clip->SetDrawsContent(true);
  scroller->SetBounds(gfx::Size(1000, 1000));
  scroller->SetCurrentScrollOffset(gfx::ScrollOffset(100, 100));
  scroller->SetScrollClipLayer(frame_clip->id());
  scroller->SetDrawsContent(true);
  fixed->SetPosition(gfx::PointF(100, 100));
  fixed->SetBounds(gfx::Size(50, 50));
  fixed->SetMasksToBounds(true);
  fixed->SetDrawsContent(true);
  fixed->test_properties()->force_render_surface = true;

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->test_properties()->position_constraint = constraint;

  ExecuteCalculateDrawProperties(root);

  gfx::Rect expected(0, 0, 50, 50);
  EXPECT_EQ(expected, fixed->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, ChangingAxisAlignmentTriggersRebuild) {
  gfx::Transform translate;
  gfx::Transform rotate;

  translate.Translate(10, 10);
  rotate.Rotate(45);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(800, 800));
  root->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(translate);
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(rotate);
  EXPECT_TRUE(host()->property_trees()->needs_rebuild);
}

TEST_F(LayerTreeHostCommonTest, ChangeTransformOrigin) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2.f, 2.f);

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  child->test_properties()->transform = scale_matrix;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());

  child->test_properties()->transform_origin = gfx::Point3F(10.f, 10.f, 10.f);

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(5, 5, 5, 5), child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, UpdateScrollChildPosition) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* scroll_parent = AddChild<LayerImpl>(root);
  LayerImpl* scroll_child = AddChild<LayerImpl>(scroll_parent);

  gfx::Transform scale;
  scale.Scale(2.f, 2.f);

  root->SetBounds(gfx::Size(50, 50));
  scroll_child->test_properties()->transform = scale;
  scroll_child->SetBounds(gfx::Size(40, 40));
  scroll_child->SetDrawsContent(true);
  scroll_parent->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetDrawsContent(true);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(25, 25), scroll_child->visible_layer_rect());

  scroll_child->SetPosition(gfx::PointF(0, -10.f));
  scroll_parent->SetCurrentScrollOffset(gfx::ScrollOffset(0.f, 10.f));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 5, 25, 25), scroll_child->visible_layer_rect());
}

static void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {}

TEST_F(LayerTreeHostCommonTest, NumCopyRequestsInTargetSubtree) {
  // If the layer has a node in effect_tree, the return value of
  // num_copy_requests_in_target_subtree()  must be equal to the actual number
  // of copy requests in the sub-layer_tree; Otherwise, the number is expected
  // to be the value of its nearest ancestor that owns an effect node and
  // greater than or equal to the actual number of copy requests in the
  // sub-layer_tree.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grandchild = Layer::Create();
  scoped_refptr<Layer> greatgrandchild = Layer::Create();

  root->AddChild(child1);
  root->AddChild(child2);
  child1->AddChild(grandchild);
  grandchild->AddChild(greatgrandchild);
  host()->SetRootLayer(root);

  child1->RequestCopyOfOutput(
      CopyOutputRequest::CreateBitmapRequest(base::Bind(&CopyOutputCallback)));
  greatgrandchild->RequestCopyOfOutput(
      CopyOutputRequest::CreateBitmapRequest(base::Bind(&CopyOutputCallback)));
  child2->SetOpacity(0.f);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());

  EXPECT_EQ(root->num_copy_requests_in_target_subtree(), 2);
  EXPECT_EQ(child1->num_copy_requests_in_target_subtree(), 2);
  EXPECT_EQ(child2->num_copy_requests_in_target_subtree(), 0);
  EXPECT_EQ(grandchild->num_copy_requests_in_target_subtree(), 2);
  EXPECT_EQ(greatgrandchild->num_copy_requests_in_target_subtree(), 1);
}

TEST_F(LayerTreeHostCommonTest, SkippingSubtreeMain) {
  scoped_refptr<Layer> root = Layer::Create();
  FakeContentLayerClient client;
  client.set_bounds(root->bounds());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> grandchild =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<FakePictureLayer> greatgrandchild(
      FakePictureLayer::Create(&client));

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  grandchild->SetBounds(gfx::Size(10, 10));
  greatgrandchild->SetBounds(gfx::Size(10, 10));

  root->AddChild(child);
  child->AddChild(grandchild);
  grandchild->AddChild(greatgrandchild);
  host()->SetRootLayer(root);
  host()->GetLayerTree()->SetElementIdsForTesting();

  // Check the non-skipped case.
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  const LayerList* update_list = GetUpdateLayerList();
  EXPECT_TRUE(VerifyLayerInList(grandchild, update_list));

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  gfx::Transform singular;
  singular.matrix().set(0, 0, 0);

  child->SetTransform(singular);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_FALSE(VerifyLayerInList(grandchild, update_list));
  child->SetTransform(gfx::Transform());

  child->SetHideLayerAndSubtree(true);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_FALSE(VerifyLayerInList(grandchild, update_list));
  child->SetHideLayerAndSubtree(false);

  gfx::Transform zero_z_scale;
  zero_z_scale.Scale3d(1, 1, 0);
  child->SetTransform(zero_z_scale);

  // Add a transform animation with a start delay. Now, even though |child| has
  // a singular transform, the subtree should still get processed.
  int animation_id = 0;
  std::unique_ptr<Animation> animation = Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)),
      animation_id, 1, TargetProperty::TRANSFORM);
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));
  AddAnimationToElementWithPlayer(child->element_id(), timeline(),
                                  std::move(animation));
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_TRUE(VerifyLayerInList(grandchild, update_list));

  RemoveAnimationFromElementWithExistingPlayer(child->element_id(), timeline(),
                                               animation_id);
  child->SetTransform(gfx::Transform());
  child->SetOpacity(0.f);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_FALSE(VerifyLayerInList(grandchild, update_list));

  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild->RequestCopyOfOutput(
      CopyOutputRequest::CreateBitmapRequest(base::Bind(&CopyOutputCallback)));
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_TRUE(VerifyLayerInList(grandchild, update_list));

  // Add an opacity animation with a start delay.
  animation_id = 1;
  animation = Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      animation_id, 1, TargetProperty::OPACITY);
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));
  AddAnimationToElementWithExistingPlayer(child->element_id(), timeline(),
                                          std::move(animation));
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  update_list = GetUpdateLayerList();
  EXPECT_TRUE(VerifyLayerInList(grandchild, update_list));
}

TEST_F(LayerTreeHostCommonTest, SkippingLayerImpl) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.active_tree(), 2);
  std::unique_ptr<LayerImpl> grandchild =
      LayerImpl::Create(host_impl.active_tree(), 3);

  std::unique_ptr<FakePictureLayerImpl> greatgrandchild(
      FakePictureLayerImpl::Create(host_impl.active_tree(), 4));

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  grandchild->SetBounds(gfx::Size(10, 10));
  grandchild->SetDrawsContent(true);
  greatgrandchild->SetDrawsContent(true);

  LayerImpl* root_ptr = root.get();
  LayerImpl* child_ptr = child.get();
  LayerImpl* grandchild_ptr = grandchild.get();

  child->test_properties()->AddChild(std::move(grandchild));
  root->test_properties()->AddChild(std::move(child));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));

  host_impl.active_tree()->SetElementIdsForTesting();

  // Check the non-skipped case.
  // ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  // EXPECT_EQ(gfx::Rect(10, 10), grandchild_ptr->visible_layer_rect());

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  grandchild_ptr->set_visible_layer_rect(gfx::Rect());

  gfx::Transform singular;
  singular.matrix().set(0, 0, 0);
  // This line is used to make the results of skipping and not skipping layers
  // different.
  singular.matrix().set(0, 1, 1);

  gfx::Transform rotate;
  rotate.Rotate(90);

  gfx::Transform rotate_back_and_translate;
  rotate_back_and_translate.RotateAboutYAxis(180);
  rotate_back_and_translate.Translate(-10, 0);

  child_ptr->test_properties()->transform = singular;
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());
  child_ptr->test_properties()->transform = gfx::Transform();

  child_ptr->test_properties()->hide_layer_and_subtree = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());
  child_ptr->test_properties()->hide_layer_and_subtree = false;

  child_ptr->layer_tree_impl()->property_trees()->effect_tree.OnOpacityAnimated(
      0.f, child_ptr->effect_tree_index(), child_ptr->layer_tree_impl());
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());
  child_ptr->test_properties()->opacity = 1.f;

  root_ptr->test_properties()->transform = singular;
  // Force transform tree to have a node for child, so that ancestor's
  // invertible transform can be tested.
  child_ptr->test_properties()->transform = rotate;
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());
  root_ptr->test_properties()->transform = gfx::Transform();
  child_ptr->test_properties()->transform = gfx::Transform();

  root_ptr->test_properties()->opacity = 0.f;
  child_ptr->test_properties()->opacity = 0.7f;
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());
  root_ptr->test_properties()->opacity = 1.f;

  child_ptr->test_properties()->opacity = 0.f;
  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild_ptr->test_properties()->copy_requests.push_back(
      CopyOutputRequest::CreateEmptyRequest());
  root_ptr->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(10, 10), grandchild_ptr->visible_layer_rect());

  host_impl.active_tree()->property_trees()->effect_tree.ClearCopyRequests();
  child_ptr->test_properties()->opacity = 1.f;

  // A double sided render surface with backface visible should not be skipped
  grandchild_ptr->set_visible_layer_rect(gfx::Rect());
  child_ptr->SetHasRenderSurface(true);
  child_ptr->test_properties()->double_sided = true;
  child_ptr->test_properties()->transform = rotate_back_and_translate;
  root_ptr->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(10, 10), grandchild_ptr->visible_layer_rect());
  child_ptr->test_properties()->transform = gfx::Transform();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operation, nullptr));
  std::unique_ptr<Animation> transform_animation(
      Animation::Create(std::move(curve), 3, 3, TargetProperty::TRANSFORM));
  scoped_refptr<AnimationPlayer> player(AnimationPlayer::Create(1));
  host_impl.animation_host()->RegisterPlayerForElement(root_ptr->element_id(),
                                                       player.get());
  player->AddAnimation(std::move(transform_animation));
  grandchild_ptr->set_visible_layer_rect(gfx::Rect());
  child_ptr->SetScrollClipLayer(root_ptr->id());
  root_ptr->test_properties()->transform = singular;
  child_ptr->test_properties()->transform = singular;
  root_ptr->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(0, 0), grandchild_ptr->visible_layer_rect());

  host_impl.animation_host()->UnregisterPlayerForElement(root_ptr->element_id(),
                                                         player.get());
}

TEST_F(LayerTreeHostCommonTest, LayerSkippingInSubtreeOfSingularTransform) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  LayerImpl* grand_child = AddChild<LayerImpl>(child);

  SetElementIdsForTesting();

  gfx::Transform singular;
  singular.matrix().set(0, 0, 0);
  singular.matrix().set(0, 1, 1);

  root->SetBounds(gfx::Size(10, 10));
  child->test_properties()->transform = singular;
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operation, nullptr));
  std::unique_ptr<Animation> transform_animation(
      Animation::Create(std::move(curve), 3, 3, TargetProperty::TRANSFORM));
  scoped_refptr<AnimationPlayer> player(AnimationPlayer::Create(1));
  host_impl()->animation_host()->RegisterPlayerForElement(
      grand_child->element_id(), player.get());
  player->AddAnimation(std::move(transform_animation));

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 0), grand_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_layer_rect());

  host_impl()->animation_host()->UnregisterPlayerForElement(
      grand_child->element_id(), player.get());
}

TEST_F(LayerTreeHostCommonTest, SkippingPendingLayerImpl) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);

  host_impl.CreatePendingTree();
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.pending_tree(), 1);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl.pending_tree(), 2);
  std::unique_ptr<LayerImpl> grandchild =
      LayerImpl::Create(host_impl.pending_tree(), 3);

  std::unique_ptr<FakePictureLayerImpl> greatgrandchild(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 4));

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  grandchild->SetBounds(gfx::Size(10, 10));
  grandchild->SetDrawsContent(true);
  greatgrandchild->SetDrawsContent(true);

  LayerImpl* root_ptr = root.get();
  LayerImpl* grandchild_ptr = grandchild.get();

  child->test_properties()->AddChild(std::move(grandchild));
  root->test_properties()->AddChild(std::move(child));

  host_impl.pending_tree()->SetRootLayerForTesting(std::move(root));
  host_impl.pending_tree()->SetElementIdsForTesting();

  // Check the non-skipped case.
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(10, 10), grandchild_ptr->visible_layer_rect());

  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  std::unique_ptr<TimingFunction> func =
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE);
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta(), 0.9f, std::move(func)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 0.3f, nullptr));
  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 3, 3, TargetProperty::OPACITY));
  scoped_refptr<AnimationPlayer> player(AnimationPlayer::Create(1));
  host_impl.animation_host()->RegisterPlayerForElement(root_ptr->element_id(),
                                                       player.get());
  player->AddAnimation(std::move(animation));
  root_ptr->test_properties()->opacity = 0.f;
  grandchild_ptr->set_visible_layer_rect(gfx::Rect());
  root_ptr->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root_ptr);
  EXPECT_EQ(gfx::Rect(10, 10), grandchild_ptr->visible_layer_rect());

  host_impl.animation_host()->UnregisterPlayerForElement(root_ptr->element_id(),
                                                         player.get());
}

TEST_F(LayerTreeHostCommonTest, SkippingLayer) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());
  child->set_visible_layer_rect(gfx::Rect());

  child->test_properties()->hide_layer_and_subtree = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_layer_rect());
  child->test_properties()->hide_layer_and_subtree = false;

  child->SetBounds(gfx::Size());
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_layer_rect());
  child->SetBounds(gfx::Size(10, 10));

  gfx::Transform rotate;
  child->test_properties()->double_sided = false;
  rotate.RotateAboutXAxis(180.f);
  child->test_properties()->transform = rotate;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_layer_rect());
  child->test_properties()->double_sided = true;
  child->test_properties()->transform = gfx::Transform();

  child->test_properties()->opacity = 0.f;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, LayerTreeRebuildTest) {
  // Ensure that the treewalk in LayerTreeHostCommom::
  // PreCalculateMetaInformation happens when its required.
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  root->SetBounds(gfx::Size(100, 100));
  parent->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetClipParent(root.get());

  root->AddChild(parent);
  parent->AddChild(child);
  host()->SetRootLayer(root);

  child->RequestCopyOfOutput(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_EQ(parent->num_unclipped_descendants(), 1u);

  EXPECT_GT(root->num_copy_requests_in_target_subtree(), 0);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_GT(root->num_copy_requests_in_target_subtree(), 0);
}

TEST_F(LayerTreeHostCommonTest, ResetPropertyTreeIndices) {
  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(800, 800));

  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  scoped_refptr<Layer> child = Layer::Create();
  child->SetTransform(translate_z);
  child->SetBounds(gfx::Size(100, 100));

  root->AddChild(child);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_NE(-1, child->transform_tree_index());

  child->RemoveFromParent();

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_EQ(-1, child->transform_tree_index());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceClipsSubtree) {
  // Ensure that a Clip Node is added when a render surface applies clip.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* significant_transform = AddChildToRoot<LayerImpl>();
  LayerImpl* layer_clips_subtree = AddChild<LayerImpl>(significant_transform);
  LayerImpl* render_surface = AddChild<LayerImpl>(layer_clips_subtree);
  LayerImpl* test_layer = AddChild<LayerImpl>(render_surface);

  // This transform should be a significant one so that a transform node is
  // formed for it.
  gfx::Transform transform1;
  transform1.RotateAboutYAxis(45);
  transform1.RotateAboutXAxis(30);
  // This transform should be a 3d transform as we want the render surface
  // to flatten the transform
  gfx::Transform transform2;
  transform2.Translate3d(10, 10, 10);

  root->SetBounds(gfx::Size(30, 30));
  significant_transform->test_properties()->transform = transform1;
  significant_transform->SetBounds(gfx::Size(30, 30));
  layer_clips_subtree->SetBounds(gfx::Size(30, 30));
  layer_clips_subtree->SetMasksToBounds(true);
  layer_clips_subtree->test_properties()->force_render_surface = true;
  render_surface->test_properties()->transform = transform2;
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->test_properties()->force_render_surface = true;
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  float device_scale_factor = 1.f;
  float page_scale_factor = 1.f;
  LayerImpl* page_scale_layer = nullptr;
  LayerImpl* inner_viewport_scroll_layer = nullptr;
  LayerImpl* outer_viewport_scroll_layer = nullptr;
  // Visible rects computed by combining clips in target space and root space
  // don't match because of rotation transforms. So, we skip
  // verify_visible_rect_calculations.
  bool skip_verify_visible_rect_calculations = true;
  ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                 page_scale_layer, inner_viewport_scroll_layer,
                                 outer_viewport_scroll_layer,
                                 skip_verify_visible_rect_calculations);

  TransformTree& transform_tree =
      root->layer_tree_impl()->property_trees()->transform_tree;
  TransformNode* transform_node =
      transform_tree.Node(significant_transform->transform_tree_index());
  EXPECT_EQ(transform_node->owner_id, significant_transform->id());

  EXPECT_TRUE(root->has_render_surface());
  EXPECT_FALSE(significant_transform->has_render_surface());
  EXPECT_TRUE(layer_clips_subtree->has_render_surface());
  EXPECT_TRUE(render_surface->has_render_surface());
  EXPECT_FALSE(test_layer->has_render_surface());

  ClipTree& clip_tree = root->layer_tree_impl()->property_trees()->clip_tree;
  ClipNode* clip_node = clip_tree.Node(render_surface->clip_tree_index());
  EXPECT_EQ(clip_node->clip_type, ClipNode::ClipType::NONE);
  EXPECT_EQ(gfx::Rect(20, 20), test_layer->visible_layer_rect());

  // Also test the visible rects computed by combining clips in root space.
  gfx::Rect visible_rect = draw_property_utils::ComputeLayerVisibleRectDynamic(
      root->layer_tree_impl()->property_trees(), test_layer);
  EXPECT_EQ(gfx::Rect(30, 20), visible_rect);
}

TEST_F(LayerTreeHostCommonTest, TransformOfParentClipNodeAncestorOfTarget) {
  // Ensure that when parent clip node's transform is an ancestor of current
  // clip node's target, clip is 'projected' from parent space to current
  // target space and visible rects are calculated correctly.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_layer = AddChild<LayerImpl>(root);
  LayerImpl* target_layer = AddChild<LayerImpl>(clip_layer);
  LayerImpl* test_layer = AddChild<LayerImpl>(target_layer);

  gfx::Transform transform;
  transform.RotateAboutYAxis(45);

  root->SetBounds(gfx::Size(30, 30));
  clip_layer->test_properties()->transform = transform;
  clip_layer->SetBounds(gfx::Size(30, 30));
  clip_layer->SetMasksToBounds(true);
  target_layer->test_properties()->transform = transform;
  target_layer->SetBounds(gfx::Size(30, 30));
  target_layer->SetMasksToBounds(true);
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(30, 30), test_layer->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceWithUnclippedDescendantsClipsSubtree) {
  // Ensure clip rect is calculated correctly when render surface has unclipped
  // descendants.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* between_clip_parent_and_child = AddChild<LayerImpl>(clip_parent);
  LayerImpl* render_surface =
      AddChild<LayerImpl>(between_clip_parent_and_child);
  LayerImpl* test_layer = AddChild<LayerImpl>(render_surface);

  gfx::Transform translate;
  translate.Translate(2.0, 2.0);

  root->SetBounds(gfx::Size(30, 30));
  clip_parent->test_properties()->transform = translate;
  clip_parent->SetBounds(gfx::Size(30, 30));
  clip_parent->SetMasksToBounds(true);
  between_clip_parent_and_child->test_properties()->transform = translate;
  between_clip_parent_and_child->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->test_properties()->force_render_surface = true;
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  render_surface->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(render_surface);

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(test_layer->is_clipped());
  EXPECT_FALSE(test_layer->render_target()->is_clipped());
  EXPECT_EQ(gfx::Rect(-2, -2, 30, 30), test_layer->clip_rect());
  EXPECT_EQ(gfx::Rect(28, 28), test_layer->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceWithUnclippedDescendantsButDoesntApplyOwnClip) {
  // Ensure that the visible layer rect of a descendant of a surface with
  // unclipped descendants is computed correctly, when the surface doesn't apply
  // a clip.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface);
  LayerImpl* child = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(30, 10));
  clip_parent->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(10, 15));
  render_surface->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(10, 10));
  clip_child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(40, 40));
  child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(30, 10), child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceClipsSubtreeAndHasUnclippedDescendants) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* test_layer1 = AddChild<LayerImpl>(render_surface);
  LayerImpl* clip_child = AddChild<LayerImpl>(test_layer1);
  LayerImpl* test_layer2 = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(30, 30));
  root->SetMasksToBounds(true);
  clip_parent->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetMasksToBounds(true);
  render_surface->SetDrawsContent(true);
  render_surface->test_properties()->force_render_surface = true;
  test_layer1->SetBounds(gfx::Size(50, 50));
  test_layer1->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);
  test_layer2->SetBounds(gfx::Size(50, 50));
  test_layer2->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(30, 30), render_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), test_layer1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), clip_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), test_layer2->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, UnclippedClipParent) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(50, 50));
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetDrawsContent(true);
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->test_properties()->force_render_surface = true;
  render_surface->SetMasksToBounds(true);
  render_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);

  // The clip child should inherit its clip parent's clipping state, not its
  // tree parent's clipping state.
  EXPECT_FALSE(clip_parent->is_clipped());
  EXPECT_TRUE(render_surface->is_clipped());
  EXPECT_FALSE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceContentRectWithMultipleSurfaces) {
  // Tests the value of render surface content rect when we have multiple types
  // of surfaces : unclipped surfaces, surfaces with unclipped surfaces and
  // clipped surfaces.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* unclipped_surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_parent = AddChild<LayerImpl>(unclipped_surface);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* unclipped_desc_surface2 =
      AddChild<LayerImpl>(unclipped_desc_surface);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface2);
  LayerImpl* clipped_surface = AddChild<LayerImpl>(clip_child);

  root->SetBounds(gfx::Size(80, 80));
  unclipped_surface->SetBounds(gfx::Size(50, 50));
  unclipped_surface->SetMasksToBounds(true);
  unclipped_surface->SetDrawsContent(true);
  unclipped_surface->test_properties()->force_render_surface = true;
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetMasksToBounds(true);
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetMasksToBounds(true);
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;
  unclipped_desc_surface2->SetBounds(gfx::Size(60, 60));
  unclipped_desc_surface2->SetDrawsContent(true);
  unclipped_desc_surface2->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(100, 100));
  clipped_surface->SetBounds(gfx::Size(70, 70));
  clipped_surface->SetDrawsContent(true);
  clipped_surface->test_properties()->force_render_surface = true;

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(50, 50),
            unclipped_surface->render_surface()->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            unclipped_desc_surface->render_surface()->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            unclipped_desc_surface2->render_surface()->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            clipped_surface->render_surface()->content_rect());
}

TEST_F(LayerTreeHostCommonTest, ClipBetweenClipChildTargetAndClipParentTarget) {
  // Tests the value of render surface content rect when we have a layer that
  // clips between the clip parent's target and clip child's target.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_layer = AddChild<LayerImpl>(surface);
  LayerImpl* clip_parent = AddChild<LayerImpl>(clip_layer);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface);

  gfx::Transform translate;
  translate.Translate(10, 10);

  gfx::Transform rotate;
  rotate.RotateAboutXAxis(10);

  root->SetBounds(gfx::Size(100, 100));
  surface->SetBounds(gfx::Size(100, 100));
  surface->SetMasksToBounds(true);
  surface->test_properties()->force_render_surface = true;
  surface->test_properties()->transform = rotate;
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_layer->SetMasksToBounds(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->test_properties()->transform = translate;
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(10, 10),
            unclipped_desc_surface->render_surface()->content_rect());
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForDescendantOfScaledSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* surface = AddChildToRoot<LayerImpl>();
  LayerImpl* clip_layer = AddChild<LayerImpl>(surface);
  LayerImpl* clip_parent = AddChild<LayerImpl>(clip_layer);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface);


  gfx::Transform scale;
  scale.Scale(2, 2);

  root->SetBounds(gfx::Size(100, 100));
  surface->test_properties()->transform = scale;
  surface->SetBounds(gfx::Size(100, 100));
  surface->SetMasksToBounds(true);
  surface->test_properties()->force_render_surface = true;
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_layer->SetMasksToBounds(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(20, 20), clip_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, LayerWithInputHandlerAndZeroOpacity) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* test_layer = AddChild<LayerImpl>(render_surface);

  gfx::Transform translation;
  translation.Translate(10, 10);

  root->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->SetMasksToBounds(true);
  render_surface->test_properties()->force_render_surface = true;
  test_layer->test_properties()->transform = translation;
  test_layer->SetBounds(gfx::Size(20, 20));
  test_layer->SetDrawsContent(true);
  test_layer->SetTouchEventHandlerRegion(gfx::Rect(0, 0, 20, 20));
  test_layer->test_properties()->opacity = 0.f;

  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(translation,
                                  test_layer->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, ClipParentDrawsIntoScaledRootSurface) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_layer = AddChild<LayerImpl>(root);
  LayerImpl* clip_parent = AddChild<LayerImpl>(clip_layer);
  LayerImpl* unclipped_desc_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(unclipped_desc_surface);

  root->SetBounds(gfx::Size(100, 100));
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_layer->SetMasksToBounds(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  float device_scale_factor = 1.f;
  ExecuteCalculateDrawProperties(root, device_scale_factor);
  EXPECT_EQ(gfx::Rect(20, 20), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(20, 20), clip_child->visible_layer_rect());

  device_scale_factor = 2.f;
  ExecuteCalculateDrawProperties(root, device_scale_factor);
  EXPECT_EQ(gfx::Rect(40, 40), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(20, 20), clip_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, ClipChildVisibleRect) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* clip_parent = AddChildToRoot<LayerImpl>();
  LayerImpl* render_surface = AddChild<LayerImpl>(clip_parent);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(30, 30));
  clip_parent->SetBounds(gfx::Size(40, 40));
  clip_parent->SetMasksToBounds(true);
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetMasksToBounds(true);
  render_surface->SetDrawsContent(true);
  render_surface->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);

  clip_child->test_properties()->clip_parent = clip_parent;
  clip_parent->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  clip_parent->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);
  EXPECT_EQ(gfx::Rect(30, 30), clip_child->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       LayerClipRectLargerThanClippingRenderSurfaceRect) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* test_layer = AddChild<LayerImpl>(render_surface);

  root->SetBounds(gfx::Size(30, 30));
  root->SetMasksToBounds(true);
  root->SetDrawsContent(true);
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetMasksToBounds(true);
  render_surface->SetDrawsContent(true);
  render_surface->test_properties()->force_render_surface = true;
  test_layer->SetBounds(gfx::Size(50, 50));
  test_layer->SetMasksToBounds(true);
  test_layer->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(30, 30), root->clip_rect());
  EXPECT_EQ(gfx::Rect(50, 50), render_surface->clip_rect());
  EXPECT_EQ(gfx::Rect(50, 50), test_layer->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, SubtreeIsHiddenTest) {
  // Tests that subtree is hidden is updated.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* hidden = AddChild<LayerImpl>(root);
  LayerImpl* test = AddChild<LayerImpl>(hidden);

  root->SetBounds(gfx::Size(30, 30));
  hidden->SetBounds(gfx::Size(30, 30));
  hidden->test_properties()->force_render_surface = true;
  hidden->test_properties()->hide_layer_and_subtree = true;
  test->SetBounds(gfx::Size(30, 30));
  test->test_properties()->force_render_surface = true;

  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(test->IsHidden());

  hidden->test_properties()->hide_layer_and_subtree = false;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_FALSE(test->IsHidden());
}

TEST_F(LayerTreeHostCommonTest, TwoUnclippedRenderSurfaces) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(root);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* clip_child = AddChild<LayerImpl>(render_surface2);

  root->SetBounds(gfx::Size(30, 30));
  root->SetMasksToBounds(true);
  render_surface1->SetPosition(gfx::PointF(10, 10));
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->SetDrawsContent(true);
  render_surface1->test_properties()->force_render_surface = true;
  render_surface2->SetBounds(gfx::Size(30, 30));
  render_surface2->SetDrawsContent(true);
  render_surface2->test_properties()->force_render_surface = true;
  clip_child->SetBounds(gfx::Size(30, 30));

  clip_child->test_properties()->clip_parent = root;
  root->test_properties()->clip_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  root->test_properties()->clip_children->insert(clip_child);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::Rect(-10, -10, 30, 30), render_surface2->clip_rect());
  // A clip node is created for every render surface and for layers that have
  // local clip. So, here it should be craeted for every layer.
  EXPECT_EQ(root->layer_tree_impl()->property_trees()->clip_tree.size(), 5u);
}

TEST_F(LayerTreeHostCommonTest, MaskLayerDrawProperties) {
  // Tests that a mask layer's draw properties are computed correctly.
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* child = AddChild<LayerImpl>(root);
  child->test_properties()->SetMaskLayer(
      LayerImpl::Create(root->layer_tree_impl(), 100));
  LayerImpl* mask = child->test_properties()->mask_layer;

  gfx::Transform transform;
  transform.Translate(10, 10);

  root->SetBounds(gfx::Size(40, 40));
  root->SetDrawsContent(true);
  child->test_properties()->transform = transform;
  child->SetBounds(gfx::Size(30, 30));
  child->SetDrawsContent(false);
  mask->SetBounds(gfx::Size(20, 20));
  ExecuteCalculateDrawProperties(root);

  // The render surface created for the mask has no contributing content, so the
  // mask isn't a drawn RSLL member. This means it has an empty visible rect,
  // but its screen space transform can still be computed correctly on-demand.
  EXPECT_FALSE(mask->is_drawn_render_surface_layer_list_member());
  EXPECT_EQ(gfx::Rect(), mask->visible_layer_rect());
  EXPECT_TRANSFORMATION_MATRIX_EQ(transform, mask->ScreenSpaceTransform());

  // Make the child's render surface have contributing content.
  child->SetDrawsContent(true);
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRUE(mask->is_drawn_render_surface_layer_list_member());
  EXPECT_EQ(gfx::Rect(20, 20), mask->visible_layer_rect());
  EXPECT_TRANSFORMATION_MATRIX_EQ(transform, mask->ScreenSpaceTransform());

  transform.Translate(10, 10);
  child->test_properties()->transform = transform;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root);
  EXPECT_TRANSFORMATION_MATRIX_EQ(transform, mask->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Rect(20, 20), mask->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       SublayerScaleWithTransformNodeBetweenTwoTargets) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(root);
  LayerImpl* between_targets = AddChild<LayerImpl>(render_surface1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(between_targets);
  LayerImpl* test_layer = AddChild<LayerImpl>(render_surface2);

  gfx::Transform scale;
  scale.Scale(2.f, 2.f);

  root->SetBounds(gfx::Size(30, 30));
  render_surface1->test_properties()->transform = scale;
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->test_properties()->force_render_surface = true;
  between_targets->SetBounds(gfx::Size(30, 30));
  render_surface2->SetBounds(gfx::Size(30, 30));
  render_surface2->test_properties()->force_render_surface = true;
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  // We want layer between the two targets to create a clip node and effect
  // node but it shouldn't create a render surface.
  between_targets->SetMasksToBounds(true);
  between_targets->test_properties()->opacity = 0.5f;

  ExecuteCalculateDrawProperties(root);

  EffectTree& tree = root->layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = tree.Node(render_surface1->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2.f, 2.f));

  node = tree.Node(between_targets->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1.f, 1.f));

  node = tree.Node(render_surface2->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2.f, 2.f));

  EXPECT_EQ(gfx::Rect(15, 15), test_layer->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, NoisyTransform) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* scroll_child = AddChild<LayerImpl>(render_surface);
  LayerImpl* scroll_clip = AddChild<LayerImpl>(render_surface);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_clip);

  scroll_child->test_properties()->scroll_parent = scroll_parent;
  scroll_parent->test_properties()->scroll_children =
      base::MakeUnique<std::set<LayerImpl*>>();
  scroll_parent->test_properties()->scroll_children->insert(scroll_child);
  scroll_parent->SetScrollClipLayer(scroll_clip->id());

  scroll_parent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);
  render_surface->test_properties()->force_render_surface = true;

  // A noisy transform that's invertible.
  gfx::Transform transform;
  transform.matrix().setDouble(0, 0, 6.12323e-17);
  transform.matrix().setDouble(0, 2, 1);
  transform.matrix().setDouble(2, 2, 6.12323e-17);
  transform.matrix().setDouble(2, 0, -1);

  scroll_child->test_properties()->transform = transform;
  render_surface->test_properties()->transform = transform;

  root->SetBounds(gfx::Size(30, 30));
  scroll_child->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetBounds(gfx::Size(30, 30));
  ExecuteCalculateDrawProperties(root);

  gfx::Transform expected;
  expected.matrix().setDouble(0, 0, 3.749395e-33);
  expected.matrix().setDouble(0, 2, 6.12323e-17);
  expected.matrix().setDouble(2, 0, -1);
  expected.matrix().setDouble(2, 2, 6.12323e-17);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected,
                                  scroll_child->ScreenSpaceTransform());
}

TEST_F(LayerTreeHostCommonTest, LargeTransformTest) {
  LayerImpl* root = root_layer_for_testing();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);

  child->SetDrawsContent(true);
  child->SetMasksToBounds(true);

  gfx::Transform large_transform;
  large_transform.Scale(99999999999999999999.f, 99999999999999999999.f);
  large_transform.Scale(9999999999999999999.f, 9999999999999999999.f);
  EXPECT_TRUE(std::isinf(large_transform.matrix().get(0, 0)));
  EXPECT_TRUE(std::isinf(large_transform.matrix().get(1, 1)));

  root->SetBounds(gfx::Size(30, 30));
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->test_properties()->force_render_surface = true;

  // TODO(sunxd): we make child have no render surface, because if the
  // child has one, the large transform applied to child will result in NaNs in
  // the draw_transform of the render_surface, thus make draw property updates
  // skip the child layer. We need further investigation into this to know
  // what exactly happens here.
  child->test_properties()->transform = large_transform;
  child->SetBounds(gfx::Size(30, 30));

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(gfx::RectF(),
            render_surface1->render_surface()->DrawableContentRect());

  bool is_inf_or_nan = std::isinf(child->DrawTransform().matrix().get(0, 0)) ||
                       std::isnan(child->DrawTransform().matrix().get(0, 0));
  EXPECT_TRUE(is_inf_or_nan);

  is_inf_or_nan = std::isinf(child->DrawTransform().matrix().get(1, 1)) ||
                  std::isnan(child->DrawTransform().matrix().get(1, 1));
  EXPECT_TRUE(is_inf_or_nan);

  // The root layer should be in the RenderSurfaceLayerListImpl.
  const auto* rsll = render_surface_layer_list_impl();
  EXPECT_NE(std::find(rsll->begin(), rsll->end(), root), rsll->end());
}

TEST_F(LayerTreeHostCommonTest, PropertyTreesRebuildWithOpacityChanges) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(child);
  host()->SetRootLayer(root);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(20, 20));
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());

  // Changing the opacity from 1 to non-1 value should trigger rebuild of
  // property trees as a new effect node will be created.
  child->SetOpacity(0.5f);
  PropertyTrees* property_trees = host()->property_trees();
  EXPECT_TRUE(property_trees->needs_rebuild);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_NE(property_trees->effect_id_to_index_map.find(child->id()),
            property_trees->effect_id_to_index_map.end());

  // child already has an effect node. Changing its opacity shouldn't trigger
  // a property trees rebuild.
  child->SetOpacity(0.8f);
  property_trees = host()->property_trees();
  EXPECT_FALSE(property_trees->needs_rebuild);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_NE(property_trees->effect_id_to_index_map.find(child->id()),
            property_trees->effect_id_to_index_map.end());

  // Changing the opacity from non-1 value to 1 should trigger a rebuild of
  // property trees as the effect node may no longer be needed.
  child->SetOpacity(1.f);
  property_trees = host()->property_trees();
  EXPECT_TRUE(property_trees->needs_rebuild);

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());
  EXPECT_EQ(property_trees->effect_id_to_index_map.find(child->id()),
            property_trees->effect_id_to_index_map.end());
}

TEST_F(LayerTreeHostCommonTest, OpacityAnimationsTrackingTest) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> animated =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(animated);
  host()->SetRootLayer(root);
  host()->GetLayerTree()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  root->SetForceRenderSurfaceForTesting(true);
  animated->SetBounds(gfx::Size(20, 20));
  animated->SetOpacity(0.f);

  scoped_refptr<AnimationPlayer> player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline()->AttachPlayer(player);

  player->AttachElement(animated->element_id());

  int animation_id = 0;
  std::unique_ptr<Animation> animation = Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      animation_id, 1, TargetProperty::OPACITY);
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));
  Animation* animation_ptr = animation.get();
  AddAnimationToElementWithExistingPlayer(animated->element_id(), timeline(),
                                          std::move(animation));

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());

  EffectTree& tree = root->GetLayerTree()->property_trees()->effect_tree;
  EffectNode* node = tree.Node(animated->effect_tree_index());
  EXPECT_FALSE(node->is_currently_animating_opacity);
  EXPECT_TRUE(node->has_potential_opacity_animation);

  animation_ptr->set_time_offset(base::TimeDelta::FromMilliseconds(0));
  host()->AnimateLayers(
      base::TimeTicks::FromInternalValue(std::numeric_limits<int64_t>::max()));
  node = tree.Node(animated->effect_tree_index());
  EXPECT_TRUE(node->is_currently_animating_opacity);
  EXPECT_TRUE(node->has_potential_opacity_animation);

  player->AbortAnimations(TargetProperty::OPACITY, false /*needs_completion*/);
  node = tree.Node(animated->effect_tree_index());
  EXPECT_FALSE(node->is_currently_animating_opacity);
  EXPECT_FALSE(node->has_potential_opacity_animation);
}

TEST_F(LayerTreeHostCommonTest, TransformAnimationsTrackingTest) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> animated =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(animated);
  host()->SetRootLayer(root);
  host()->GetLayerTree()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  root->SetForceRenderSurfaceForTesting(true);
  animated->SetBounds(gfx::Size(20, 20));

  scoped_refptr<AnimationPlayer> player =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  timeline()->AttachPlayer(player);
  player->AttachElement(animated->element_id());

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operation, nullptr));
  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 3, 3, TargetProperty::TRANSFORM));
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));
  Animation* animation_ptr = animation.get();
  AddAnimationToElementWithExistingPlayer(animated->element_id(), timeline(),
                                          std::move(animation));

  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root.get());

  TransformTree& tree = root->GetLayerTree()->property_trees()->transform_tree;
  TransformNode* node = tree.Node(animated->transform_tree_index());
  EXPECT_FALSE(node->is_currently_animating);
  EXPECT_TRUE(node->has_potential_animation);

  animation_ptr->set_time_offset(base::TimeDelta::FromMilliseconds(0));
  host()->AnimateLayers(
      base::TimeTicks::FromInternalValue(std::numeric_limits<int64_t>::max()));
  node = tree.Node(animated->transform_tree_index());
  EXPECT_TRUE(node->is_currently_animating);
  EXPECT_TRUE(node->has_potential_animation);

  player->AbortAnimations(TargetProperty::TRANSFORM,
                          false /*needs_completion*/);
  node = tree.Node(animated->transform_tree_index());
  EXPECT_FALSE(node->is_currently_animating);
  EXPECT_FALSE(node->has_potential_animation);
}

TEST_F(LayerTreeHostCommonTest, ScrollTreeBuilderTest) {
  // Test the behavior of scroll tree builder
  // Topology:
  // +root1(1)[inner_viewport_container_layer]
  // +-page_scale_layer
  // +----parent2(2)[kHasBackgroundAttachmentFixedObjects|kScrollbarScrolling &
  // scrollable, inner_viewport_scroll_layer]
  // +------child6(6)[kScrollbarScrolling]
  // +--------grand_child10(10)[kScrollbarScrolling]
  // +----parent3(3)
  // +------child7(7)[scrollable]
  // +------child8(8)[scroll_parent=7]
  // +--------grand_child11(11)[scrollable]
  // +----parent4(4)
  // +------child9(9)
  // +--------grand_child12(12)
  // +----parent5(5)[contains_non_fast_scrollable_region]
  //
  // Expected scroll tree topology:
  // +property_tree_root---owner:-1
  // +--root---owner:1, id:1
  // +----node---owner:2, id:2
  // +------node---owner:6, id:3
  // +----node---owner:7, id:4
  // +------node---owner:11, id:5
  // +----node---owner:5, id:6
  //
  // Extra check:
  //   scroll_tree_index() of:
  //     grand_child10:3
  //     parent3:1
  //     child8:4
  //     parent4:1
  //     child9:1
  //     grand_child12:1
  scoped_refptr<Layer> root1 = Layer::Create();
  scoped_refptr<Layer> page_scale_layer = Layer::Create();
  scoped_refptr<Layer> parent2 = Layer::Create();
  scoped_refptr<Layer> parent3 = Layer::Create();
  scoped_refptr<Layer> parent4 = Layer::Create();
  scoped_refptr<Layer> parent5 = Layer::Create();
  scoped_refptr<Layer> child6 = Layer::Create();
  scoped_refptr<Layer> child7 = Layer::Create();
  scoped_refptr<Layer> child8 = Layer::Create();
  scoped_refptr<Layer> child9 = Layer::Create();
  scoped_refptr<Layer> grand_child10 = Layer::Create();
  scoped_refptr<Layer> grand_child11 = Layer::Create();
  scoped_refptr<Layer> grand_child12 = Layer::Create();

  root1->AddChild(page_scale_layer);
  page_scale_layer->AddChild(parent2);
  page_scale_layer->AddChild(parent3);
  page_scale_layer->AddChild(parent4);
  page_scale_layer->AddChild(parent5);
  parent2->AddChild(child6);
  parent3->AddChild(child7);
  parent3->AddChild(child8);
  parent4->AddChild(child9);
  child6->AddChild(grand_child10);
  child8->AddChild(grand_child11);
  child9->AddChild(grand_child12);
  host()->SetRootLayer(root1);

  parent2->AddMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  parent2->AddMainThreadScrollingReasons(
      MainThreadScrollingReason::kScrollbarScrolling);
  parent2->SetScrollClipLayerId(root1->id());
  child6->AddMainThreadScrollingReasons(
      MainThreadScrollingReason::kScrollbarScrolling);
  grand_child10->AddMainThreadScrollingReasons(
      MainThreadScrollingReason::kScrollbarScrolling);

  child7->SetScrollClipLayerId(parent3->id());

  child8->SetScrollParent(child7.get());
  grand_child11->SetScrollClipLayerId(parent3->id());

  parent5->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
  parent5->SetBounds(gfx::Size(10, 10));

  host()->GetLayerTree()->RegisterViewportLayers(nullptr, page_scale_layer,
                                                 parent2, nullptr);
  ExecuteCalculateDrawPropertiesAndSaveUpdateLayerList(root1.get());

  const int kInvalidPropertyTreeNodeId = -1;
  const int kRootPropertyTreeNodeId = 0;

  // Property tree root
  ScrollTree& scroll_tree = host()->property_trees()->scroll_tree;
  PropertyTrees property_trees;
  property_trees.is_main_thread = true;
  property_trees.is_active = false;
  ScrollTree& expected_scroll_tree = property_trees.scroll_tree;
  ScrollNode* property_tree_root = expected_scroll_tree.Node(0);
  property_tree_root->id = kRootPropertyTreeNodeId;
  property_tree_root->parent_id = kInvalidPropertyTreeNodeId;
  property_tree_root->owner_id = kInvalidPropertyTreeNodeId;
  property_tree_root->scrollable = false;
  property_tree_root->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  property_tree_root->contains_non_fast_scrollable_region = false;
  property_tree_root->transform_id = kRootPropertyTreeNodeId;

  // The node owned by root1
  ScrollNode scroll_root1;
  scroll_root1.id = 1;
  scroll_root1.owner_id = root1->id();
  scroll_root1.user_scrollable_horizontal = true;
  scroll_root1.user_scrollable_vertical = true;
  scroll_root1.transform_id = root1->transform_tree_index();
  expected_scroll_tree.Insert(scroll_root1, 0);

  // The node owned by parent2
  ScrollNode scroll_parent2;
  scroll_parent2.id = 2;
  scroll_parent2.owner_id = parent2->id();
  scroll_parent2.scrollable = true;
  scroll_parent2.main_thread_scrolling_reasons =
      parent2->main_thread_scrolling_reasons();
  scroll_parent2.scroll_clip_layer_bounds = root1->bounds();
  scroll_parent2.bounds = parent2->bounds();
  scroll_parent2.max_scroll_offset_affected_by_page_scale = true;
  scroll_parent2.is_inner_viewport_scroll_layer = true;
  scroll_parent2.user_scrollable_horizontal = true;
  scroll_parent2.user_scrollable_vertical = true;
  scroll_parent2.transform_id = parent2->transform_tree_index();
  expected_scroll_tree.Insert(scroll_parent2, 1);

  // The node owned by child6
  ScrollNode scroll_child6;
  scroll_child6.id = 3;
  scroll_child6.owner_id = child6->id();
  scroll_child6.main_thread_scrolling_reasons =
      child6->main_thread_scrolling_reasons();
  scroll_child6.should_flatten = true;
  scroll_child6.user_scrollable_horizontal = true;
  scroll_child6.user_scrollable_vertical = true;
  scroll_child6.transform_id = child6->transform_tree_index();
  expected_scroll_tree.Insert(scroll_child6, 2);

  // The node owned by child7, child7 also owns a transform node
  ScrollNode scroll_child7;
  scroll_child7.id = 4;
  scroll_child7.owner_id = child7->id();
  scroll_child7.scrollable = true;
  scroll_child7.scroll_clip_layer_bounds = parent3->bounds();
  scroll_child7.bounds = child7->bounds();
  scroll_child7.user_scrollable_horizontal = true;
  scroll_child7.user_scrollable_vertical = true;
  scroll_child7.transform_id = child7->transform_tree_index();
  expected_scroll_tree.Insert(scroll_child7, 1);

  // The node owned by grand_child11, grand_child11 also owns a transform node
  ScrollNode scroll_grand_child11;
  scroll_grand_child11.id = 5;
  scroll_grand_child11.owner_id = grand_child11->id();
  scroll_grand_child11.scrollable = true;
  scroll_grand_child11.user_scrollable_horizontal = true;
  scroll_grand_child11.user_scrollable_vertical = true;
  scroll_grand_child11.transform_id = grand_child11->transform_tree_index();
  expected_scroll_tree.Insert(scroll_grand_child11, 4);

  // The node owned by parent5
  ScrollNode scroll_parent5;
  scroll_parent5.id = 8;
  scroll_parent5.owner_id = parent5->id();
  scroll_parent5.contains_non_fast_scrollable_region = true;
  scroll_parent5.bounds = gfx::Size(10, 10);
  scroll_parent5.should_flatten = true;
  scroll_parent5.user_scrollable_horizontal = true;
  scroll_parent5.user_scrollable_vertical = true;
  scroll_parent5.transform_id = parent5->transform_tree_index();
  expected_scroll_tree.Insert(scroll_parent5, 1);

  expected_scroll_tree.SetScrollOffset(parent2->id(), gfx::ScrollOffset(0, 0));
  expected_scroll_tree.SetScrollOffset(child7->id(), gfx::ScrollOffset(0, 0));
  expected_scroll_tree.SetScrollOffset(grand_child11->id(),
                                       gfx::ScrollOffset(0, 0));
  expected_scroll_tree.set_needs_update(false);

  EXPECT_EQ(expected_scroll_tree, scroll_tree);

  // Check other layers' scroll_tree_index
  EXPECT_EQ(scroll_root1.id, page_scale_layer->scroll_tree_index());
  EXPECT_EQ(scroll_child6.id, grand_child10->scroll_tree_index());
  EXPECT_EQ(scroll_root1.id, parent3->scroll_tree_index());
  EXPECT_EQ(scroll_child7.id, child8->scroll_tree_index());
  EXPECT_EQ(scroll_root1.id, parent4->scroll_tree_index());
  EXPECT_EQ(scroll_root1.id, child9->scroll_tree_index());
  EXPECT_EQ(scroll_root1.id, grand_child12->scroll_tree_index());
}

}  // namespace
}  // namespace cc
