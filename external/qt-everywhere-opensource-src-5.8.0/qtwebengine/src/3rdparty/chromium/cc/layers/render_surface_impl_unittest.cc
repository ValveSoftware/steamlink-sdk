// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/render_surface_impl.h"

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/test/fake_mask_layer_impl.h"
#include "cc/test/layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(RenderSurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  LayerImpl* owning_layer_impl = impl.AddChildToRoot<LayerImpl>();
  owning_layer_impl->SetBounds(layer_size);
  owning_layer_impl->SetDrawsContent(true);
  owning_layer_impl->test_properties()->force_render_surface = true;

  impl.CalcDrawProps(viewport_size);

  RenderSurfaceImpl* render_surface_impl = owning_layer_impl->render_surface();
  ASSERT_TRUE(render_surface_impl);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(owning_layer_impl->visible_layer_rect());
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsAreOccluded(
        impl.quad_list(), occluded, &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

TEST(RenderSurfaceLayerImplTest, AppendQuadsWithScaledMask) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(impl.host_impl()->active_tree(), 2);
  root->SetHasRenderSurface(true);
  std::unique_ptr<LayerImpl> surface =
      LayerImpl::Create(impl.host_impl()->active_tree(), 3);
  surface->SetBounds(layer_size);
  surface->SetHasRenderSurface(true);

  gfx::Transform scale;
  scale.Scale(2, 2);
  surface->SetTransform(scale);

  surface->test_properties()->SetMaskLayer(
      FakeMaskLayerImpl::Create(impl.host_impl()->active_tree(), 4));
  surface->test_properties()->mask_layer->SetDrawsContent(true);
  surface->test_properties()->mask_layer->SetBounds(layer_size);

  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(impl.host_impl()->active_tree(), 5);
  child->SetDrawsContent(true);
  child->SetBounds(layer_size);

  surface->test_properties()->AddChild(std::move(child));
  root->test_properties()->AddChild(std::move(surface));
  impl.host_impl()->active_tree()->SetRootLayerForTesting(std::move(root));

  impl.host_impl()->SetViewportSize(viewport_size);
  impl.host_impl()->active_tree()->BuildLayerListAndPropertyTreesForTesting();
  impl.host_impl()->active_tree()->UpdateDrawProperties(false);

  LayerImpl* surface_raw = impl.host_impl()
                               ->active_tree()
                               ->root_layer_for_testing()
                               ->test_properties()
                               ->children[0];
  RenderSurfaceImpl* render_surface_impl = surface_raw->render_surface();
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData append_quads_data;
  render_surface_impl->AppendQuads(
      render_pass.get(), render_surface_impl->draw_transform(), Occlusion(),
      SK_ColorBLACK, 1.f, render_surface_impl->MaskLayer(), &append_quads_data,
      RenderPassId(1, 1));

  const RenderPassDrawQuad* quad =
      RenderPassDrawQuad::MaterialCast(render_pass->quad_list.front());
  EXPECT_EQ(gfx::Vector2dF(0.0005f, 0.0005f), quad->mask_uv_scale);
  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f), quad->filters_scale);
}

}  // namespace
}  // namespace cc
