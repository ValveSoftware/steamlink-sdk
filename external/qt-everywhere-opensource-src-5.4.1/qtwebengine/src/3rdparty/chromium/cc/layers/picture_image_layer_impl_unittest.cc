// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestablePictureImageLayerImpl : public PictureImageLayerImpl {
 public:
  TestablePictureImageLayerImpl(LayerTreeImpl* tree_impl, int id)
      : PictureImageLayerImpl(tree_impl, id) {
  }
  using PictureLayerImpl::UpdateIdealScales;
  using PictureLayerImpl::MaximumTilingContentsScale;
  using PictureLayerImpl::DoPostCommitInitializationIfNeeded;

  PictureLayerTilingSet* tilings() { return tilings_.get(); }

  friend class PictureImageLayerImplTest;
};

class PictureImageLayerImplTest : public testing::Test {
 public:
  PictureImageLayerImplTest()
      : proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_) {
    tiling_client_.SetTileSize(ImplSidePaintingSettings().default_tile_size);
    host_impl_.CreatePendingTree();
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  }

  scoped_ptr<TestablePictureImageLayerImpl> CreateLayer(int id,
                                                        WhichTree which_tree) {
    LayerTreeImpl* tree = NULL;
    switch (which_tree) {
      case ACTIVE_TREE:
        tree = host_impl_.active_tree();
        break;
      case PENDING_TREE:
        tree = host_impl_.pending_tree();
        break;
      case NUM_TREES:
        NOTREACHED();
        break;
    }
    TestablePictureImageLayerImpl* layer =
        new TestablePictureImageLayerImpl(tree, id);
    layer->SetBounds(gfx::Size(100, 200));
    layer->SetContentBounds(gfx::Size(100, 200));
    layer->tilings_.reset(new PictureLayerTilingSet(&tiling_client_,
                                                    layer->bounds()));
    layer->pile_ = tiling_client_.pile();
    return make_scoped_ptr(layer);
  }

  void SetupDrawPropertiesAndUpdateTiles(TestablePictureImageLayerImpl* layer,
                                         float ideal_contents_scale,
                                         float device_scale_factor,
                                         float page_scale_factor,
                                         float maximum_animation_contents_scale,
                                         bool animating_transform_to_screen) {
    layer->draw_properties().ideal_contents_scale = ideal_contents_scale;
    layer->draw_properties().device_scale_factor = device_scale_factor;
    layer->draw_properties().page_scale_factor = page_scale_factor;
    layer->draw_properties().maximum_animation_contents_scale =
        maximum_animation_contents_scale;
    layer->draw_properties().screen_space_transform_is_animating =
        animating_transform_to_screen;
    layer->UpdateTiles();
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakePictureLayerTilingClient tiling_client_;
};

TEST_F(PictureImageLayerImplTest, CalculateContentsScale) {
  scoped_ptr<TestablePictureImageLayerImpl> layer(CreateLayer(1, PENDING_TREE));
  layer->SetDrawsContent(true);

  SetupDrawPropertiesAndUpdateTiles(layer.get(), 2.f, 3.f, 4.f, 1.f, false);

  EXPECT_FLOAT_EQ(1.f, layer->contents_scale_x());
  EXPECT_FLOAT_EQ(1.f, layer->contents_scale_y());
  EXPECT_FLOAT_EQ(1.f, layer->MaximumTilingContentsScale());
}

TEST_F(PictureImageLayerImplTest, IgnoreIdealContentScale) {
  scoped_ptr<TestablePictureImageLayerImpl> pending_layer(
      CreateLayer(1, PENDING_TREE));
  pending_layer->SetDrawsContent(true);

  // Set PictureLayerImpl::ideal_contents_scale_ to 2.f which is not equal
  // to the content scale used by PictureImageLayerImpl.
  const float suggested_ideal_contents_scale = 2.f;
  const float device_scale_factor = 3.f;
  const float page_scale_factor = 4.f;
  const float maximum_animation_contents_scale = 1.f;
  const bool animating_transform_to_screen = false;
  SetupDrawPropertiesAndUpdateTiles(pending_layer.get(),
                                    suggested_ideal_contents_scale,
                                    device_scale_factor,
                                    page_scale_factor,
                                    maximum_animation_contents_scale,
                                    animating_transform_to_screen);
  EXPECT_EQ(1.f, pending_layer->tilings()->tiling_at(0)->contents_scale());

  // Push to active layer.
  host_impl_.pending_tree()->SetRootLayer(pending_layer.PassAs<LayerImpl>());
  host_impl_.ActivatePendingTree();
  TestablePictureImageLayerImpl* active_layer =
      static_cast<TestablePictureImageLayerImpl*>(
          host_impl_.active_tree()->root_layer());
  SetupDrawPropertiesAndUpdateTiles(active_layer,
                                    suggested_ideal_contents_scale,
                                    device_scale_factor,
                                    page_scale_factor,
                                    maximum_animation_contents_scale,
                                    animating_transform_to_screen);
  EXPECT_EQ(1.f, active_layer->tilings()->tiling_at(0)->contents_scale());

  // Create tile and resource.
  active_layer->tilings()->tiling_at(0)->CreateAllTilesForTesting();
  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(
      active_layer->tilings()->tiling_at(0)->AllTilesForTesting());

  // Draw.
  active_layer->draw_properties().visible_content_rect =
      gfx::Rect(active_layer->bounds());
  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);
  AppendQuadsData data;
  active_layer->WillDraw(DRAW_MODE_SOFTWARE, NULL);
  active_layer->AppendQuads(&quad_culler, &data);
  active_layer->DidDraw(NULL);

  EXPECT_EQ(DrawQuad::TILED_CONTENT, quad_culler.quad_list()[0]->material);

  // Tiles are ready at correct scale, so should not set had_incomplete_tile.
  EXPECT_FALSE(data.had_incomplete_tile);
}

}  // namespace
}  // namespace cc
