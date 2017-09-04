// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include <stddef.h>

#include "cc/output/compositor_frame_sink.h"
#include "cc/output/context_provider.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/test/layer_test_common.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void IgnoreCallback(const gpu::SyncToken& sync_token,
                    bool lost,
                    BlockingTaskRunner* main_thread_task_runner) {}

TEST(TextureLayerImplTest, VisibleOpaqueRegion) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect layer_rect(layer_bounds);
  const Region layer_region(layer_rect);

  LayerTestCommon::LayerImplTest impl;

  TextureLayerImpl* layer = impl.AddChildToRoot<TextureLayerImpl>();
  layer->SetBounds(layer_bounds);
  layer->draw_properties().visible_layer_rect = layer_rect;
  layer->SetBlendBackgroundColor(true);

  // Verify initial conditions.
  EXPECT_FALSE(layer->contents_opaque());
  EXPECT_EQ(0u, layer->background_color());
  EXPECT_EQ(Region().ToString(), layer->VisibleOpaqueRegion().ToString());

  // Opaque background.
  layer->SetBackgroundColor(SK_ColorWHITE);
  EXPECT_EQ(layer_region.ToString(), layer->VisibleOpaqueRegion().ToString());

  // Transparent background.
  layer->SetBackgroundColor(SkColorSetARGB(100, 255, 255, 255));
  EXPECT_EQ(Region().ToString(), layer->VisibleOpaqueRegion().ToString());
}

TEST(TextureLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  gpu::Mailbox mailbox;
  impl.compositor_frame_sink()
      ->context_provider()
      ->ContextGL()
      ->GenMailboxCHROMIUM(mailbox.name);
  TextureMailbox texture_mailbox(
      mailbox,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO, 0x123,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      GL_TEXTURE_2D);

  TextureLayerImpl* texture_layer_impl =
      impl.AddChildToRoot<TextureLayerImpl>();
  texture_layer_impl->SetBounds(layer_size);
  texture_layer_impl->SetDrawsContent(true);
  texture_layer_impl->SetTextureMailbox(
      texture_mailbox,
      SingleReleaseCallbackImpl::Create(base::Bind(&IgnoreCallback)));

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(texture_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsAreOccluded(
        impl.quad_list(), occluded, &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

TEST(TextureLayerImplTest, OutputIsSecure) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  gpu::Mailbox mailbox;
  impl.compositor_frame_sink()
      ->context_provider()
      ->ContextGL()
      ->GenMailboxCHROMIUM(mailbox.name);
  TextureMailbox texture_mailbox(
      mailbox,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO, 0x123,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      GL_TEXTURE_2D, layer_size, false, true);

  TextureLayerImpl* texture_layer_impl =
      impl.AddChildToRoot<TextureLayerImpl>();
  texture_layer_impl->SetBounds(layer_size);
  texture_layer_impl->SetDrawsContent(true);
  texture_layer_impl->SetTextureMailbox(
      texture_mailbox,
      SingleReleaseCallbackImpl::Create(base::Bind(&IgnoreCallback)));

  impl.CalcDrawProps(viewport_size);

  {
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    EXPECT_EQ(1u, impl.quad_list().size());
    ASSERT_EQ(DrawQuad::Material::TEXTURE_CONTENT,
              impl.quad_list().front()->material);
    const TextureDrawQuad* quad =
        TextureDrawQuad::MaterialCast(impl.quad_list().front());
    EXPECT_TRUE(quad->secure_output_only);
  }
}

TEST(TextureLayerImplTest, ResourceNotFreedOnGpuRasterToggle) {
  LayerTreeSettings settings;
  settings.gpu_rasterization_enabled = true;
  LayerTestCommon::LayerImplTest impl(settings);
  impl.host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));

  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  gpu::Mailbox mailbox;
  impl.compositor_frame_sink()
      ->context_provider()
      ->ContextGL()
      ->GenMailboxCHROMIUM(mailbox.name);
  TextureMailbox texture_mailbox(
      mailbox,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO, 0x123,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      GL_TEXTURE_2D);

  TextureLayerImpl* texture_layer_impl =
      impl.AddChildToRoot<TextureLayerImpl>();
  texture_layer_impl->SetBounds(layer_size);
  texture_layer_impl->SetDrawsContent(true);
  bool released = false;
  texture_layer_impl->SetTextureMailbox(
      texture_mailbox,
      SingleReleaseCallbackImpl::Create(base::Bind(
          [](bool* released, const gpu::SyncToken& sync_token, bool lost,
             BlockingTaskRunner* main_thread_task_runner) { *released = true; },
          base::Unretained(&released))));

  impl.CalcDrawProps(viewport_size);

  // Gpu rasterization is disabled by default.
  EXPECT_FALSE(impl.host_impl()->use_gpu_rasterization());
  EXPECT_FALSE(released);
  // Toggling the gpu rasterization clears all tilings on both trees.
  impl.host_impl()->SetHasGpuRasterizationTrigger(true);
  impl.host_impl()->SetContentIsSuitableForGpuRasterization(true);
  impl.host_impl()->CommitComplete();
  EXPECT_TRUE(impl.host_impl()->use_gpu_rasterization());
  EXPECT_FALSE(released);
}

}  // namespace
}  // namespace cc
