// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void CheckDrawLayer(HeadsUpDisplayLayerImpl* layer,
                    ResourceProvider* resource_provider,
                    DrawMode draw_mode) {
  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);
  AppendQuadsData data;
  bool will_draw = layer->WillDraw(draw_mode, resource_provider);
  if (will_draw)
    layer->AppendQuads(&quad_culler, &data);
  layer->UpdateHudTexture(draw_mode, resource_provider);
  if (will_draw)
    layer->DidDraw(resource_provider);

  size_t expected_quad_list_size = will_draw ? 1 : 0;
  EXPECT_EQ(expected_quad_list_size, quad_culler.quad_list().size());
}

TEST(HeadsUpDisplayLayerImplTest, ResourcelessSoftwareDrawAfterResourceLoss) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  host_impl.CreatePendingTree();
  host_impl.InitializeRenderer(
      FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  scoped_ptr<HeadsUpDisplayLayerImpl> layer =
    HeadsUpDisplayLayerImpl::Create(host_impl.pending_tree(), 1);
  layer->SetContentBounds(gfx::Size(100, 100));

  // Check regular hardware draw is ok.
  CheckDrawLayer(
      layer.get(), host_impl.resource_provider(), DRAW_MODE_HARDWARE);

  // Simulate a resource loss on transitioning to resourceless software mode.
  layer->ReleaseResources();

  // Should skip resourceless software draw and not crash in UpdateHudTexture.
  CheckDrawLayer(layer.get(),
                 host_impl.resource_provider(),
                 DRAW_MODE_RESOURCELESS_SOFTWARE);
}

}  // namespace
}  // namespace cc
