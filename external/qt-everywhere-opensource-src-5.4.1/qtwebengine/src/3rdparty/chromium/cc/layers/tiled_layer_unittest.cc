// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer.h"

#include <limits>
#include <vector>

#include "base/run_loop.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestOcclusionTracker : public OcclusionTracker<Layer> {
 public:
  TestOcclusionTracker() : OcclusionTracker(gfx::Rect(0, 0, 1000, 1000)) {
    stack_.push_back(StackObject());
  }

  void SetRenderTarget(Layer* render_target) {
    stack_.back().target = render_target;
  }

  void SetOcclusion(const Region& occlusion) {
    stack_.back().occlusion_from_inside_target = occlusion;
  }
};

class SynchronousOutputSurfaceLayerTreeHost : public LayerTreeHost {
 public:
  static scoped_ptr<SynchronousOutputSurfaceLayerTreeHost> Create(
      LayerTreeHostClient* client,
      SharedBitmapManager* manager,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
    return make_scoped_ptr(new SynchronousOutputSurfaceLayerTreeHost(
        client, manager, settings, impl_task_runner));
  }

  virtual ~SynchronousOutputSurfaceLayerTreeHost() {}

  bool EnsureOutputSurfaceCreated() {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        run_loop_.QuitClosure(),
        base::TimeDelta::FromSeconds(5));
    run_loop_.Run();
    return output_surface_created_;
  }

  virtual void OnCreateAndInitializeOutputSurfaceAttempted(
      bool success) OVERRIDE {
    LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted(success);
    output_surface_created_ = success;
    run_loop_.Quit();
  }

 private:
  SynchronousOutputSurfaceLayerTreeHost(
      LayerTreeHostClient* client,
      SharedBitmapManager* manager,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
      : LayerTreeHost(client, manager, settings),
        output_surface_created_(false) {
    LayerTreeHost::InitializeThreaded(impl_task_runner);
  }

  bool output_surface_created_;
  base::RunLoop run_loop_;
};

class TiledLayerTest : public testing::Test {
 public:
  TiledLayerTest()
      : proxy_(NULL),
        output_surface_(FakeOutputSurface::Create3d()),
        queue_(make_scoped_ptr(new ResourceUpdateQueue)),
        impl_thread_("ImplThread"),
        fake_layer_tree_host_client_(FakeLayerTreeHostClient::DIRECT_3D),
        occlusion_(NULL) {
    settings_.max_partial_texture_updates = std::numeric_limits<size_t>::max();
    settings_.layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetUp() {
    impl_thread_.Start();
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    layer_tree_host_ = SynchronousOutputSurfaceLayerTreeHost::Create(
        &fake_layer_tree_host_client_,
        shared_bitmap_manager_.get(),
        settings_,
        impl_thread_.message_loop_proxy());
    proxy_ = layer_tree_host_->proxy();
    resource_manager_ = PrioritizedResourceManager::Create(proxy_);
    layer_tree_host_->SetLayerTreeHostClientReady();
    CHECK(layer_tree_host_->EnsureOutputSurfaceCreated());
    layer_tree_host_->SetRootLayer(Layer::Create());

    CHECK(output_surface_->BindToClient(&output_surface_client_));

    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(proxy_);
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
        false);
    host_impl_ = make_scoped_ptr(
        new FakeLayerTreeHostImpl(proxy_, shared_bitmap_manager_.get()));
  }

  virtual ~TiledLayerTest() {
    ResourceManagerClearAllMemory(resource_manager_.get(),
                                  resource_provider_.get());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(proxy_);
    resource_provider_.reset();
    host_impl_.reset();
  }

  void ResourceManagerClearAllMemory(
      PrioritizedResourceManager* resource_manager,
      ResourceProvider* resource_provider) {
    {
      DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(proxy_);
      resource_manager->ClearAllMemory(resource_provider);
      resource_manager->ReduceMemory(resource_provider);
    }
    resource_manager->UnlinkAndClearEvictedBackings();
  }

  void UpdateTextures() {
    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(proxy_);
    DCHECK(queue_);
    scoped_ptr<ResourceUpdateController> update_controller =
        ResourceUpdateController::Create(NULL,
                                         proxy_->ImplThreadTaskRunner(),
                                         queue_.Pass(),
                                         resource_provider_.get());
    update_controller->Finalize();
    queue_ = make_scoped_ptr(new ResourceUpdateQueue);
  }

  void LayerPushPropertiesTo(FakeTiledLayer* layer,
                             FakeTiledLayerImpl* layer_impl) {
    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(proxy_);
    layer->PushPropertiesTo(layer_impl);
    layer->ResetNumDependentsNeedPushProperties();
  }

  void LayerUpdate(FakeTiledLayer* layer, TestOcclusionTracker* occluded) {
    DebugScopedSetMainThread main_thread(proxy_);
    layer->Update(queue_.get(), occluded);
  }

  void CalcDrawProps(RenderSurfaceLayerList* render_surface_layer_list) {
    if (occlusion_)
      occlusion_->SetRenderTarget(layer_tree_host_->root_layer());

    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        layer_tree_host_->root_layer(),
        layer_tree_host_->device_viewport_size(),
        render_surface_layer_list);
    inputs.device_scale_factor = layer_tree_host_->device_scale_factor();
    inputs.max_texture_size =
        layer_tree_host_->GetRendererCapabilities().max_texture_size;
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  bool UpdateAndPush(const scoped_refptr<FakeTiledLayer>& layer1,
                     const scoped_ptr<FakeTiledLayerImpl>& layer_impl1) {
    scoped_refptr<FakeTiledLayer> layer2;
    scoped_ptr<FakeTiledLayerImpl> layer_impl2;
    return UpdateAndPush(layer1, layer_impl1, layer2, layer_impl2);
  }

  bool UpdateAndPush(const scoped_refptr<FakeTiledLayer>& layer1,
                     const scoped_ptr<FakeTiledLayerImpl>& layer_impl1,
                     const scoped_refptr<FakeTiledLayer>& layer2,
                     const scoped_ptr<FakeTiledLayerImpl>& layer_impl2) {
    // Get textures
    resource_manager_->ClearPriorities();
    if (layer1.get())
      layer1->SetTexturePriorities(priority_calculator_);
    if (layer2.get())
      layer2->SetTexturePriorities(priority_calculator_);
    resource_manager_->PrioritizeTextures();

    // Save paint properties
    if (layer1.get())
      layer1->SavePaintProperties();
    if (layer2.get())
      layer2->SavePaintProperties();

    // Update content
    if (layer1.get())
      layer1->Update(queue_.get(), occlusion_);
    if (layer2.get())
      layer2->Update(queue_.get(), occlusion_);

    bool needs_update = false;
    if (layer1.get())
      needs_update |= layer1->NeedsIdlePaint();
    if (layer2.get())
      needs_update |= layer2->NeedsIdlePaint();

    // Update textures and push.
    UpdateTextures();
    if (layer1.get())
      LayerPushPropertiesTo(layer1.get(), layer_impl1.get());
    if (layer2.get())
      LayerPushPropertiesTo(layer2.get(), layer_impl2.get());

    return needs_update;
  }

 public:
  Proxy* proxy_;
  LayerTreeSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourceUpdateQueue> queue_;
  PriorityCalculator priority_calculator_;
  base::Thread impl_thread_;
  FakeLayerTreeHostClient fake_layer_tree_host_client_;
  scoped_ptr<SynchronousOutputSurfaceLayerTreeHost> layer_tree_host_;
  scoped_ptr<FakeLayerTreeHostImpl> host_impl_;
  scoped_ptr<PrioritizedResourceManager> resource_manager_;
  TestOcclusionTracker* occlusion_;
};

TEST_F(TiledLayerTest, PushDirtyTiles) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  layer->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  UpdateAndPush(layer, layer_impl);

  // We should have both tiles on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));

  // Invalidates both tiles, but then only update one of them.
  layer->InvalidateContentRect(gfx::Rect(0, 0, 100, 200));
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
  UpdateAndPush(layer, layer_impl);

  // We should only have the first tile since the other tile was invalidated but
  // not painted.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, PushOccludedDirtyTiles) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;
  layer_tree_host_->SetViewportSize(gfx::Size(1000, 1000));

  layer_tree_host_->root_layer()->AddChild(layer);

  {
    RenderSurfaceLayerList render_surface_layer_list;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    CalcDrawProps(&render_surface_layer_list);
    UpdateAndPush(layer, layer_impl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));
  }

  {
    RenderSurfaceLayerList render_surface_layer_list;

    // Invalidates part of the top tile...
    layer->InvalidateContentRect(gfx::Rect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.SetOcclusion(gfx::Rect(0, 0, 50, 50));
    CalcDrawProps(&render_surface_layer_list);
    UpdateAndPush(layer, layer_impl);

    // We should still have both tiles, as part of the top tile is still
    // unoccluded.
    EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));
  }
}

TEST_F(TiledLayerTest, PushDeletedTiles) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  layer->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  UpdateAndPush(layer, layer_impl);

  // We should have both tiles on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));

  resource_manager_->ClearPriorities();
  ResourceManagerClearAllMemory(resource_manager_.get(),
                                resource_provider_.get());
  resource_manager_->SetMaxMemoryLimitBytes(4 * 1024 * 1024);

  // This should drop the tiles on the impl thread.
  LayerPushPropertiesTo(layer.get(), layer_impl.get());

  // We should now have no textures on the impl thread.
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 1));

  // This should recreate and update one of the deleted textures.
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
  UpdateAndPush(layer, layer_impl);

  // We should have one tiles on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, PushIdlePaintTiles) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the
  // center.  This paints 1 visible of the 25 invalid tiles.
  layer->SetBounds(gfx::Size(500, 500));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(200, 200, 100, 100);
  bool needs_update = UpdateAndPush(layer, layer_impl);
  // We should need idle-painting for surrounding tiles.
  EXPECT_TRUE(needs_update);

  // We should have one tile on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(2, 2));

  // For the next four updates, we should detect we still need idle painting.
  for (int i = 0; i < 4; i++) {
    needs_update = UpdateAndPush(layer, layer_impl);
    EXPECT_TRUE(needs_update);
  }

  // We should always finish painting eventually.
  for (int i = 0; i < 20; i++)
    needs_update = UpdateAndPush(layer, layer_impl);

  // We should have pre-painted all of the surrounding tiles.
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++)
      EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(i, j));
  }

  EXPECT_FALSE(needs_update);
}

TEST_F(TiledLayerTest, PredictivePainting) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));

  layer_tree_host_->root_layer()->AddChild(layer);

  // Prepainting should occur in the scroll direction first, and the
  // visible rect should be extruded only along the dominant axis.
  gfx::Vector2d directions[6] = { gfx::Vector2d(-10, 0), gfx::Vector2d(10, 0),
                                  gfx::Vector2d(0, -10), gfx::Vector2d(0, 10),
                                  gfx::Vector2d(10, 20),
                                  gfx::Vector2d(-20, 10) };
  // We should push all tiles that touch the extruded visible rect.
  gfx::Rect pushed_visible_tiles[6] = {
    gfx::Rect(2, 2, 2, 1), gfx::Rect(1, 2, 2, 1), gfx::Rect(2, 2, 1, 2),
    gfx::Rect(2, 1, 1, 2), gfx::Rect(2, 1, 1, 2), gfx::Rect(2, 2, 2, 1)
  };
  // The first pre-paint should also paint first in the scroll
  // direction so we should find one additional tile in the scroll direction.
  gfx::Rect pushed_prepaint_tiles[6] = {
    gfx::Rect(2, 2, 3, 1), gfx::Rect(0, 2, 3, 1), gfx::Rect(2, 2, 1, 3),
    gfx::Rect(2, 0, 1, 3), gfx::Rect(2, 0, 1, 3), gfx::Rect(2, 2, 3, 1)
  };
  for (int k = 0; k < 6; k++) {
    // The tile size is 100x100. Setup 5x5 tiles with one visible tile
    // in the center.
    gfx::Size bounds = gfx::Size(500, 500);
    gfx::Rect visible_rect = gfx::Rect(200, 200, 100, 100);
    gfx::Rect previous_visible_rect =
        gfx::Rect(visible_rect.origin() + directions[k], visible_rect.size());
    gfx::Rect next_visible_rect =
        gfx::Rect(visible_rect.origin() - directions[k], visible_rect.size());

    // Setup. Use the previous_visible_rect to setup the prediction for next
    // frame.
    layer->SetBounds(bounds);

    RenderSurfaceLayerList render_surface_layer_list;
    CalcDrawProps(&render_surface_layer_list);
    layer->draw_properties().visible_content_rect = previous_visible_rect;
    bool needs_update = UpdateAndPush(layer, layer_impl);

    // Invalidate and move the visible_rect in the scroll direction.
    // Check that the correct tiles have been painted in the visible pass.
    layer->SetNeedsDisplay();
    layer->draw_properties().visible_content_rect = visible_rect;
    needs_update = UpdateAndPush(layer, layer_impl);
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++)
        EXPECT_EQ(layer_impl->HasResourceIdForTileAt(i, j),
                  pushed_visible_tiles[k].Contains(i, j));
    }

    // Move the transform in the same direction without invalidating.
    // Check that non-visible pre-painting occured in the correct direction.
    // Ignore diagonal scrolls here (k > 3) as these have new visible content
    // now.
    if (k <= 3) {
      layer->draw_properties().visible_content_rect = next_visible_rect;
      needs_update = UpdateAndPush(layer, layer_impl);
      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++)
          EXPECT_EQ(layer_impl->HasResourceIdForTileAt(i, j),
                    pushed_prepaint_tiles[k].Contains(i, j));
      }
    }

    // We should always finish painting eventually.
    for (int i = 0; i < 20; i++)
      needs_update = UpdateAndPush(layer, layer_impl);
    EXPECT_FALSE(needs_update);
  }
}

TEST_F(TiledLayerTest, PushTilesAfterIdlePaintFailed) {
  // Start with 2mb of memory, but the test is going to try to use just more
  // than 1mb, so we reduce to 1mb later.
  resource_manager_->SetMaxMemoryLimitBytes(2 * 1024 * 1024);
  scoped_refptr<FakeTiledLayer> layer1 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl1 =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  scoped_refptr<FakeTiledLayer> layer2 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl2 =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 2));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer1);
  layer_tree_host_->root_layer()->AddChild(layer2);

  // For this test we have two layers. layer1 exhausts most texture memory,
  // leaving room for 2 more tiles from layer2, but not all three tiles. First
  // we paint layer1, and one tile from layer2. Then when we idle paint layer2,
  // we will fail on the third tile of layer2, and this should not leave the
  // second tile in a bad state.

  // This uses 960000 bytes, leaving 88576 bytes of memory left, which is enough
  // for 2 tiles only in the other layer.
  gfx::Rect layer1_rect(0, 0, 100, 2400);

  // This requires 4*30000 bytes of memory.
  gfx::Rect layer2_rect(0, 0, 100, 300);

  // Paint a single tile in layer2 so that it will idle paint.
  layer1->SetBounds(layer1_rect.size());
  layer2->SetBounds(layer2_rect.size());
  CalcDrawProps(&render_surface_layer_list);
  layer1->draw_properties().visible_content_rect = layer1_rect;
  layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
  bool needs_update = UpdateAndPush(layer1, layer_impl1, layer2, layer_impl2);
  // We should need idle-painting for both remaining tiles in layer2.
  EXPECT_TRUE(needs_update);

  // Reduce our memory limits to 1mb.
  resource_manager_->SetMaxMemoryLimitBytes(1024 * 1024);

  // Now idle paint layer2. We are going to run out of memory though!
  // Oh well, commit the frame and push.
  for (int i = 0; i < 4; i++) {
    needs_update = UpdateAndPush(layer1, layer_impl1, layer2, layer_impl2);
  }

  // Sanity check, we should have textures for the big layer.
  EXPECT_TRUE(layer_impl1->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl1->HasResourceIdForTileAt(0, 23));

  // We should only have the first two tiles from layer2 since
  // it failed to idle update the last tile.
  EXPECT_TRUE(layer_impl2->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl2->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl2->HasResourceIdForTileAt(0, 1));
  EXPECT_TRUE(layer_impl2->HasResourceIdForTileAt(0, 1));

  EXPECT_FALSE(needs_update);
  EXPECT_FALSE(layer_impl2->HasResourceIdForTileAt(0, 2));
}

TEST_F(TiledLayerTest, PushIdlePaintedOccludedTiles) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates one occluded tile, culls it
  // during paint, but prepaints it.
  occluded.SetOcclusion(gfx::Rect(0, 0, 100, 100));

  layer->SetBounds(gfx::Size(100, 100));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
  UpdateAndPush(layer, layer_impl);

  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
}

TEST_F(TiledLayerTest, PushTilesMarkedDirtyDuringPaint) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  // However, during the paint, we invalidate one of the tiles. This should
  // not prevent the tile from being pushed.
  layer->fake_layer_updater()->SetRectToInvalidate(
      gfx::Rect(0, 50, 100, 50), layer.get());
  layer->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  UpdateAndPush(layer, layer_impl);

  // We should have both tiles on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, PushTilesLayerMarkedDirtyDuringPaintOnNextLayer) {
  scoped_refptr<FakeTiledLayer> layer1 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_refptr<FakeTiledLayer> layer2 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer1_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  scoped_ptr<FakeTiledLayerImpl> layer2_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 2));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer1);
  layer_tree_host_->root_layer()->AddChild(layer2);

  // Invalidate a tile on layer1, during update of layer 2.
  layer2->fake_layer_updater()->SetRectToInvalidate(
      gfx::Rect(0, 50, 100, 50), layer1.get());
  layer1->SetBounds(gfx::Size(100, 200));
  layer2->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  layer1->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  UpdateAndPush(layer1, layer1_impl, layer2, layer2_impl);

  // We should have both tiles on the impl side for all layers.
  EXPECT_TRUE(layer1_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer1_impl->HasResourceIdForTileAt(0, 1));
  EXPECT_TRUE(layer2_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer2_impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, PushTilesLayerMarkedDirtyDuringPaintOnPreviousLayer) {
  scoped_refptr<FakeTiledLayer> layer1 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_refptr<FakeTiledLayer> layer2 =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer1_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  scoped_ptr<FakeTiledLayerImpl> layer2_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 2));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer1);
  layer_tree_host_->root_layer()->AddChild(layer2);

  layer1->fake_layer_updater()->SetRectToInvalidate(
      gfx::Rect(0, 50, 100, 50), layer2.get());
  layer1->SetBounds(gfx::Size(100, 200));
  layer2->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  layer1->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  UpdateAndPush(layer1, layer1_impl, layer2, layer2_impl);

  // We should have both tiles on the impl side for all layers.
  EXPECT_TRUE(layer1_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer1_impl->HasResourceIdForTileAt(0, 1));
  EXPECT_TRUE(layer2_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer2_impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, PaintSmallAnimatedLayersImmediately) {
  // Create a LayerTreeHost that has the right viewportsize,
  // so the layer is considered small enough.
  bool run_out_of_memory[2] = { false, true };
  for (int i = 0; i < 2; i++) {
    // Create a layer with 5x5 tiles, with 4x4 size viewport.
    int viewport_width = 4 * FakeTiledLayer::tile_size().width();
    int viewport_height = 4 * FakeTiledLayer::tile_size().width();
    int layer_width = 5 * FakeTiledLayer::tile_size().width();
    int layer_height = 5 * FakeTiledLayer::tile_size().height();
    int memory_for_layer = layer_width * layer_height * 4;
    layer_tree_host_->SetViewportSize(
        gfx::Size(viewport_width, viewport_height));

    // Use 10x5 tiles to run out of memory.
    if (run_out_of_memory[i])
      layer_width *= 2;

    resource_manager_->SetMaxMemoryLimitBytes(memory_for_layer);

    scoped_refptr<FakeTiledLayer> layer =
        make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    RenderSurfaceLayerList render_surface_layer_list;

    layer_tree_host_->root_layer()->AddChild(layer);

    // Full size layer with half being visible.
    layer->SetBounds(gfx::Size(layer_width, layer_height));
    gfx::Rect visible_rect(0, 0, layer_width / 2, layer_height);
    CalcDrawProps(&render_surface_layer_list);

    // Pretend the layer is animating.
    layer->draw_properties().target_space_transform_is_animating = true;
    layer->draw_properties().visible_content_rect = visible_rect;
    layer->SetLayerTreeHost(layer_tree_host_.get());

    // The layer should paint its entire contents on the first paint
    // if it is close to the viewport size and has the available memory.
    layer->SetTexturePriorities(priority_calculator_);
    resource_manager_->PrioritizeTextures();
    layer->SavePaintProperties();
    layer->Update(queue_.get(), NULL);
    UpdateTextures();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());

    // We should have all the tiles for the small animated layer.
    // We should still have the visible tiles when we didn't
    // have enough memory for all the tiles.
    if (!run_out_of_memory[i]) {
      for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j)
          EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(i, j));
      }
    } else {
      for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j)
          EXPECT_EQ(layer_impl->HasResourceIdForTileAt(i, j), i < 5);
      }
    }

    layer->RemoveFromParent();
  }
}

TEST_F(TiledLayerTest, IdlePaintOutOfMemory) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // We have enough memory for only the visible rect, so we will run out of
  // memory in first idle paint.
  int memory_limit = 4 * 100 * 100;  // 1 tiles, 4 bytes per pixel.
  resource_manager_->SetMaxMemoryLimitBytes(memory_limit);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  bool needs_update = false;
  layer->SetBounds(gfx::Size(300, 300));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(100, 100, 100, 100);
  for (int i = 0; i < 2; i++)
    needs_update = UpdateAndPush(layer, layer_impl);

  // Idle-painting should see no more priority tiles for painting.
  EXPECT_FALSE(needs_update);

  // We should have one tile on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, IdlePaintZeroSizedLayer) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));

  layer_tree_host_->root_layer()->AddChild(layer);

  bool animating[2] = { false, true };
  for (int i = 0; i < 2; i++) {
    // Pretend the layer is animating.
    layer->draw_properties().target_space_transform_is_animating = animating[i];

    // The layer's bounds are empty.
    // Empty layers don't paint or idle-paint.
    layer->SetBounds(gfx::Size());

    RenderSurfaceLayerList render_surface_layer_list;
    CalcDrawProps(&render_surface_layer_list);
    layer->draw_properties().visible_content_rect = gfx::Rect();
    bool needs_update = UpdateAndPush(layer, layer_impl);

    // Empty layers don't have tiles.
    EXPECT_EQ(0u, layer->NumPaintedTiles());

    // Empty layers don't need prepaint.
    EXPECT_FALSE(needs_update);

    // Empty layers don't have tiles.
    EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 0));
  }
}

TEST_F(TiledLayerTest, IdlePaintNonVisibleLayers) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));

  // Alternate between not visible and visible.
  gfx::Rect v(0, 0, 100, 100);
  gfx::Rect nv(0, 0, 0, 0);
  gfx::Rect visible_rect[10] = { nv, nv, v, v, nv, nv, v, v, nv, nv };
  bool invalidate[10] = { true, true, true, true, true, true, true, true, false,
                          false };

  // We should not have any tiles except for when the layer was visible
  // or after the layer was visible and we didn't invalidate.
  bool have_tile[10] = { false, false, true, true, false, false, true, true,
                         true, true };

  layer_tree_host_->root_layer()->AddChild(layer);

  for (int i = 0; i < 10; i++) {
    layer->SetBounds(gfx::Size(100, 100));

    RenderSurfaceLayerList render_surface_layer_list;
    CalcDrawProps(&render_surface_layer_list);
    layer->draw_properties().visible_content_rect = visible_rect[i];

    if (invalidate[i])
      layer->InvalidateContentRect(gfx::Rect(0, 0, 100, 100));
    bool needs_update = UpdateAndPush(layer, layer_impl);

    // We should never signal idle paint, as we painted the entire layer
    // or the layer was not visible.
    EXPECT_FALSE(needs_update);
    EXPECT_EQ(layer_impl->HasResourceIdForTileAt(0, 0), have_tile[i]);
  }
}

TEST_F(TiledLayerTest, InvalidateFromPrepare) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  layer->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
  UpdateAndPush(layer, layer_impl);

  // We should have both tiles on the impl side.
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));

  layer->fake_layer_updater()->ClearPrepareCount();
  // Invoke update again. As the layer is valid update shouldn't be invoked on
  // the LayerUpdater.
  UpdateAndPush(layer, layer_impl);
  EXPECT_EQ(0, layer->fake_layer_updater()->prepare_count());

  // SetRectToInvalidate triggers InvalidateContentRect() being invoked from
  // update.
  layer->fake_layer_updater()->SetRectToInvalidate(
      gfx::Rect(25, 25, 50, 50), layer.get());
  layer->fake_layer_updater()->ClearPrepareCount();
  layer->InvalidateContentRect(gfx::Rect(0, 0, 50, 50));
  UpdateAndPush(layer, layer_impl);
  EXPECT_EQ(1, layer->fake_layer_updater()->prepare_count());
  layer->fake_layer_updater()->ClearPrepareCount();

  // The layer should still be invalid as update invoked invalidate.
  UpdateAndPush(layer, layer_impl);  // visible
  EXPECT_EQ(1, layer->fake_layer_updater()->prepare_count());
}

TEST_F(TiledLayerTest, VerifyUpdateRectWhenContentBoundsAreScaled) {
  // The update rect (that indicates what was actually painted) should be in
  // layer space, not the content space.
  scoped_refptr<FakeTiledLayerWithScaledBounds> layer = make_scoped_refptr(
      new FakeTiledLayerWithScaledBounds(resource_manager_.get()));

  layer_tree_host_->root_layer()->AddChild(layer);

  gfx::Rect layer_bounds(0, 0, 300, 200);
  gfx::Rect content_bounds(0, 0, 200, 250);

  layer->SetBounds(layer_bounds.size());
  layer->SetContentBounds(content_bounds.size());
  layer->draw_properties().visible_content_rect = content_bounds;

  // On first update, the update_rect includes all tiles, even beyond the
  // boundaries of the layer.
  // However, it should still be in layer space, not content space.
  layer->InvalidateContentRect(content_bounds);

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 300, 300 * 0.8), layer->update_rect());
  UpdateTextures();

  // After the tiles are updated once, another invalidate only needs to update
  // the bounds of the layer.
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->InvalidateContentRect(content_bounds);
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(layer_bounds), layer->update_rect());
  UpdateTextures();

  // Partial re-paint should also be represented by the update rect in layer
  // space, not content space.
  gfx::Rect partial_damage(30, 100, 10, 10);
  layer->InvalidateContentRect(partial_damage);
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(45, 80, 15, 8), layer->update_rect());
}

TEST_F(TiledLayerTest, VerifyInvalidationWhenContentsScaleChanges) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  scoped_ptr<FakeTiledLayerImpl> layer_impl =
      make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // Create a layer with one tile.
  layer->SetBounds(gfx::Size(100, 100));
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
  layer->Update(queue_.get(), NULL);
  UpdateTextures();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100),
                       layer->last_needs_display_rect());

  // Push the tiles to the impl side and check that there is exactly one.
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  UpdateTextures();
  LayerPushPropertiesTo(layer.get(), layer_impl.get());
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 1));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(1, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(1, 1));

  layer->SetNeedsDisplayRect(gfx::Rect());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(), layer->last_needs_display_rect());

  // Change the contents scale.
  layer->UpdateContentsScale(2.f);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 200, 200);

  // The impl side should get 2x2 tiles now.
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  UpdateTextures();
  LayerPushPropertiesTo(layer.get(), layer_impl.get());
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(0, 1));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(1, 0));
  EXPECT_TRUE(layer_impl->HasResourceIdForTileAt(1, 1));

  // Verify that changing the contents scale caused invalidation, and
  // that the layer-space rectangle requiring painting is not scaled.
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100),
                       layer->last_needs_display_rect());

  // Invalidate the entire layer again, but do not paint. All tiles should be
  // gone now from the impl side.
  layer->SetNeedsDisplay();
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();

  LayerPushPropertiesTo(layer.get(), layer_impl.get());
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(0, 1));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(1, 0));
  EXPECT_FALSE(layer_impl->HasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, SkipsDrawGetsReset) {
  // Create two 300 x 300 tiled layers.
  gfx::Size content_bounds(300, 300);
  gfx::Rect content_rect(content_bounds);

  // We have enough memory for only one of the two layers.
  int memory_limit = 4 * 300 * 300;  // 4 bytes per pixel.

  scoped_refptr<FakeTiledLayer> root_layer = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
  scoped_refptr<FakeTiledLayer> child_layer = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
  root_layer->AddChild(child_layer);

  root_layer->SetBounds(content_bounds);
  root_layer->draw_properties().visible_content_rect = content_rect;
  root_layer->SetPosition(gfx::PointF(0, 0));
  child_layer->SetBounds(content_bounds);
  child_layer->draw_properties().visible_content_rect = content_rect;
  child_layer->SetPosition(gfx::PointF(0, 0));
  root_layer->InvalidateContentRect(content_rect);
  child_layer->InvalidateContentRect(content_rect);

  layer_tree_host_->SetRootLayer(root_layer);
  layer_tree_host_->SetViewportSize(gfx::Size(300, 300));
  layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(
      memory_limit);

  layer_tree_host_->UpdateLayers(queue_.get());

  // We'll skip the root layer.
  EXPECT_TRUE(root_layer->SkipsDraw());
  EXPECT_FALSE(child_layer->SkipsDraw());

  layer_tree_host_->CommitComplete();

  // Remove the child layer.
  root_layer->RemoveAllChildren();

  layer_tree_host_->UpdateLayers(queue_.get());
  EXPECT_FALSE(root_layer->SkipsDraw());

  ResourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(),
                                resource_provider_.get());
  layer_tree_host_->SetRootLayer(NULL);
}

TEST_F(TiledLayerTest, ResizeToSmaller) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));

  layer_tree_host_->root_layer()->AddChild(layer);

  layer->SetBounds(gfx::Size(700, 700));
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
  layer->InvalidateContentRect(gfx::Rect(0, 0, 700, 700));

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);

  layer->SetBounds(gfx::Size(200, 200));
  layer->InvalidateContentRect(gfx::Rect(0, 0, 200, 200));
}

TEST_F(TiledLayerTest, HugeLayerUpdateCrash) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));

  layer_tree_host_->root_layer()->AddChild(layer);

  int size = 1 << 30;
  layer->SetBounds(gfx::Size(size, size));
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
  layer->InvalidateContentRect(gfx::Rect(0, 0, size, size));

  // Ensure no crash for bounds where size * size would overflow an int.
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
}

class TiledLayerPartialUpdateTest : public TiledLayerTest {
 public:
  TiledLayerPartialUpdateTest() { settings_.max_partial_texture_updates = 4; }
};

TEST_F(TiledLayerPartialUpdateTest, PartialUpdates) {
  // Create one 300 x 200 tiled layer with 3 x 2 tiles.
  gfx::Size content_bounds(300, 200);
  gfx::Rect content_rect(content_bounds);

  scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
  layer->SetBounds(content_bounds);
  layer->SetPosition(gfx::PointF(0, 0));
  layer->draw_properties().visible_content_rect = content_rect;
  layer->InvalidateContentRect(content_rect);

  layer_tree_host_->SetRootLayer(layer);
  layer_tree_host_->SetViewportSize(gfx::Size(300, 200));

  // Full update of all 6 tiles.
  layer_tree_host_->UpdateLayers(queue_.get());
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    EXPECT_EQ(6u, queue_->FullUploadSize());
    EXPECT_EQ(0u, queue_->PartialUploadSize());
    UpdateTextures();
    EXPECT_EQ(6, layer->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());
    layer->fake_layer_updater()->ClearUpdateCount();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  // Full update of 3 tiles and partial update of 3 tiles.
  layer->InvalidateContentRect(gfx::Rect(0, 0, 300, 150));
  layer_tree_host_->UpdateLayers(queue_.get());
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    EXPECT_EQ(3u, queue_->FullUploadSize());
    EXPECT_EQ(3u, queue_->PartialUploadSize());
    UpdateTextures();
    EXPECT_EQ(6, layer->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());
    layer->fake_layer_updater()->ClearUpdateCount();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  // Partial update of 6 tiles.
  layer->InvalidateContentRect(gfx::Rect(50, 50, 200, 100));
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    layer_tree_host_->UpdateLayers(queue_.get());
    EXPECT_EQ(2u, queue_->FullUploadSize());
    EXPECT_EQ(4u, queue_->PartialUploadSize());
    UpdateTextures();
    EXPECT_EQ(6, layer->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());
    layer->fake_layer_updater()->ClearUpdateCount();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  // Checkerboard all tiles.
  layer->InvalidateContentRect(gfx::Rect(0, 0, 300, 200));
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  // Partial update of 6 checkerboard tiles.
  layer->InvalidateContentRect(gfx::Rect(50, 50, 200, 100));
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    layer_tree_host_->UpdateLayers(queue_.get());
    EXPECT_EQ(6u, queue_->FullUploadSize());
    EXPECT_EQ(0u, queue_->PartialUploadSize());
    UpdateTextures();
    EXPECT_EQ(6, layer->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());
    layer->fake_layer_updater()->ClearUpdateCount();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  // Partial update of 4 tiles.
  layer->InvalidateContentRect(gfx::Rect(50, 50, 100, 100));
  {
    scoped_ptr<FakeTiledLayerImpl> layer_impl =
        make_scoped_ptr(new FakeTiledLayerImpl(host_impl_->active_tree(), 1));
    layer_tree_host_->UpdateLayers(queue_.get());
    EXPECT_EQ(0u, queue_->FullUploadSize());
    EXPECT_EQ(4u, queue_->PartialUploadSize());
    UpdateTextures();
    EXPECT_EQ(4, layer->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());
    layer->fake_layer_updater()->ClearUpdateCount();
    LayerPushPropertiesTo(layer.get(), layer_impl.get());
  }
  layer_tree_host_->CommitComplete();

  ResourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(),
                                resource_provider_.get());
  layer_tree_host_->SetRootLayer(NULL);
}

TEST_F(TiledLayerTest, TilesPaintedWithoutOcclusion) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles.
  layer->SetBounds(gfx::Size(100, 200));
  CalcDrawProps(&render_surface_layer_list);

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), NULL);
  EXPECT_EQ(2, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, TilesPaintedWithOcclusion) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100.

  layer_tree_host_->SetViewportSize(gfx::Size(600, 600));
  layer->SetBounds(gfx::Size(600, 600));
  CalcDrawProps(&render_surface_layer_list);

  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 100));
  layer->draw_properties().drawable_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->draw_properties().visible_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(36 - 3, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();

  occluded.SetOcclusion(gfx::Rect(250, 200, 300, 100));
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(36 - 2, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();

  occluded.SetOcclusion(gfx::Rect(250, 250, 300, 100));
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(36, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, TilesPaintedWithOcclusionAndVisiblityConstraints) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100.

  layer_tree_host_->SetViewportSize(gfx::Size(600, 600));
  layer->SetBounds(gfx::Size(600, 600));
  CalcDrawProps(&render_surface_layer_list);

  // The partially occluded tiles (by the 150 occlusion height) are visible
  // beyond the occlusion, so not culled.
  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 150));
  layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 360);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 360);
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(24 - 3, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();

  // Now the visible region stops at the edge of the occlusion so the partly
  // visible tiles become fully occluded.
  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 150));
  layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 350);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 350);
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(24 - 6, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();

  // Now the visible region is even smaller than the occlusion, it should have
  // the same result.
  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 150));
  layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 340);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 340);
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(24 - 6, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, TilesNotPaintedWithoutInvalidation) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100.

  layer_tree_host_->SetViewportSize(gfx::Size(600, 600));
  layer->SetBounds(gfx::Size(600, 600));
  CalcDrawProps(&render_surface_layer_list);

  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 100));
  layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 600);
  layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 600);
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(36 - 3, layer->fake_layer_updater()->update_count());
  UpdateTextures();

  layer->fake_layer_updater()->ClearUpdateCount();
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();

  // Repaint without marking it dirty. The 3 culled tiles will be pre-painted
  // now.
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(3, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, TilesPaintedWithOcclusionAndTransforms) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100.

  // This makes sure the painting works when the occluded region (in screen
  // space) is transformed differently than the layer.
  layer_tree_host_->SetViewportSize(gfx::Size(600, 600));
  layer->SetBounds(gfx::Size(600, 600));
  CalcDrawProps(&render_surface_layer_list);
  gfx::Transform screen_transform;
  screen_transform.Scale(0.5, 0.5);
  layer->draw_properties().screen_space_transform = screen_transform;
  layer->draw_properties().target_space_transform = screen_transform;

  occluded.SetOcclusion(gfx::Rect(100, 100, 150, 50));
  layer->draw_properties().drawable_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->draw_properties().visible_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  EXPECT_EQ(36 - 3, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, TilesPaintedWithOcclusionAndScaling) {
  scoped_refptr<FakeTiledLayer> layer =
      new FakeTiledLayer(resource_manager_.get());
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;

  scoped_refptr<FakeTiledLayer> scale_layer =
      new FakeTiledLayer(resource_manager_.get());
  gfx::Transform scale_transform;
  scale_transform.Scale(2.0, 2.0);
  scale_layer->SetTransform(scale_transform);

  layer_tree_host_->root_layer()->AddChild(scale_layer);

  // The tile size is 100x100.

  // This makes sure the painting works when the content space is scaled to
  // a different layer space.
  layer_tree_host_->SetViewportSize(gfx::Size(600, 600));
  layer->SetBounds(gfx::Size(300, 300));
  scale_layer->AddChild(layer);
  CalcDrawProps(&render_surface_layer_list);
  EXPECT_FLOAT_EQ(2.f, layer->contents_scale_x());
  EXPECT_FLOAT_EQ(2.f, layer->contents_scale_y());
  EXPECT_EQ(gfx::Size(600, 600).ToString(),
            layer->content_bounds().ToString());

  // No tiles are covered by the 300x50 occlusion.
  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 50));
  layer->draw_properties().drawable_content_rect =
      gfx::Rect(layer->bounds());
  layer->draw_properties().visible_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  int visible_tiles1 = 6 * 6;
  EXPECT_EQ(visible_tiles1, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();

  // The occlusion of 300x100 will be cover 3 tiles as tiles are 100x100 still.
  occluded.SetOcclusion(gfx::Rect(200, 200, 300, 100));
  layer->draw_properties().drawable_content_rect =
      gfx::Rect(layer->bounds());
  layer->draw_properties().visible_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  int visible_tiles2 = 6 * 6 - 3;
  EXPECT_EQ(visible_tiles2, layer->fake_layer_updater()->update_count());

  layer->fake_layer_updater()->ClearUpdateCount();

  // This makes sure content scaling and transforms work together.
  // When the tiles are scaled down by half, they are 50x50 each in the
  // screen.
  gfx::Transform screen_transform;
  screen_transform.Scale(0.5, 0.5);
  layer->draw_properties().screen_space_transform = screen_transform;
  layer->draw_properties().target_space_transform = screen_transform;

  // An occlusion of 150x100 will cover 3*2 = 6 tiles.
  occluded.SetOcclusion(gfx::Rect(100, 100, 150, 100));

  gfx::Rect layer_bounds_rect(layer->bounds());
  layer->draw_properties().drawable_content_rect =
      gfx::ScaleToEnclosingRect(layer_bounds_rect, 0.5f);
  layer->draw_properties().visible_content_rect =
      gfx::Rect(layer->content_bounds());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  int visible_tiles3 = 6 * 6 - 6;
  EXPECT_EQ(visible_tiles3, layer->fake_layer_updater()->update_count());
}

TEST_F(TiledLayerTest, VisibleContentOpaqueRegion) {
  scoped_refptr<FakeTiledLayer> layer =
      make_scoped_refptr(new FakeTiledLayer(resource_manager_.get()));
  RenderSurfaceLayerList render_surface_layer_list;
  TestOcclusionTracker occluded;
  occlusion_ = &occluded;
  layer_tree_host_->SetViewportSize(gfx::Size(1000, 1000));

  layer_tree_host_->root_layer()->AddChild(layer);

  // The tile size is 100x100, so this invalidates and then paints two tiles in
  // various ways.

  gfx::Rect opaque_paint_rect;
  Region opaque_contents;

  gfx::Rect content_bounds = gfx::Rect(0, 0, 100, 200);
  gfx::Rect visible_bounds = gfx::Rect(0, 0, 100, 150);

  layer->SetBounds(content_bounds.size());
  CalcDrawProps(&render_surface_layer_list);
  layer->draw_properties().drawable_content_rect = visible_bounds;
  layer->draw_properties().visible_content_rect = visible_bounds;

  // If the layer doesn't paint opaque content, then the
  // VisibleContentOpaqueRegion should be empty.
  layer->fake_layer_updater()->SetOpaquePaintRect(gfx::Rect());
  layer->InvalidateContentRect(content_bounds);
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  opaque_contents = layer->VisibleContentOpaqueRegion();
  EXPECT_TRUE(opaque_contents.IsEmpty());

  // VisibleContentOpaqueRegion should match the visible part of what is painted
  // opaque.
  opaque_paint_rect = gfx::Rect(10, 10, 90, 190);
  layer->fake_layer_updater()->SetOpaquePaintRect(opaque_paint_rect);
  layer->InvalidateContentRect(content_bounds);
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  UpdateTextures();
  opaque_contents = layer->VisibleContentOpaqueRegion();
  EXPECT_EQ(gfx::IntersectRects(opaque_paint_rect, visible_bounds).ToString(),
            opaque_contents.ToString());

  // If we paint again without invalidating, the same stuff should be opaque.
  layer->fake_layer_updater()->SetOpaquePaintRect(gfx::Rect());
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  UpdateTextures();
  opaque_contents = layer->VisibleContentOpaqueRegion();
  EXPECT_EQ(gfx::IntersectRects(opaque_paint_rect, visible_bounds).ToString(),
            opaque_contents.ToString());

  // If we repaint a non-opaque part of the tile, then it shouldn't lose its
  // opaque-ness. And other tiles should not be affected.
  layer->fake_layer_updater()->SetOpaquePaintRect(gfx::Rect());
  layer->InvalidateContentRect(gfx::Rect(0, 0, 1, 1));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  UpdateTextures();
  opaque_contents = layer->VisibleContentOpaqueRegion();
  EXPECT_EQ(gfx::IntersectRects(opaque_paint_rect, visible_bounds).ToString(),
            opaque_contents.ToString());

  // If we repaint an opaque part of the tile, then it should lose its
  // opaque-ness. But other tiles should still not be affected.
  layer->fake_layer_updater()->SetOpaquePaintRect(gfx::Rect());
  layer->InvalidateContentRect(gfx::Rect(10, 10, 1, 1));
  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();
  layer->Update(queue_.get(), &occluded);
  UpdateTextures();
  opaque_contents = layer->VisibleContentOpaqueRegion();
  EXPECT_EQ(gfx::IntersectRects(gfx::Rect(10, 100, 90, 100),
                                visible_bounds).ToString(),
            opaque_contents.ToString());
}

TEST_F(TiledLayerTest, DontAllocateContentsWhenTargetSurfaceCantBeAllocated) {
  // Tile size is 100x100.
  gfx::Rect root_rect(0, 0, 300, 200);
  gfx::Rect child_rect(0, 0, 300, 100);
  gfx::Rect child2_rect(0, 100, 300, 100);

  scoped_refptr<FakeTiledLayer> root = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
  scoped_refptr<Layer> surface = Layer::Create();
  scoped_refptr<FakeTiledLayer> child = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
  scoped_refptr<FakeTiledLayer> child2 = make_scoped_refptr(
      new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));

  root->SetBounds(root_rect.size());
  root->draw_properties().drawable_content_rect = root_rect;
  root->draw_properties().visible_content_rect = root_rect;
  root->AddChild(surface);

  surface->SetForceRenderSurface(true);
  surface->SetOpacity(0.5);
  surface->AddChild(child);
  surface->AddChild(child2);

  child->SetBounds(child_rect.size());
  child->SetPosition(child_rect.origin());
  child->draw_properties().visible_content_rect = child_rect;
  child->draw_properties().drawable_content_rect = root_rect;

  child2->SetBounds(child2_rect.size());
  child2->SetPosition(child2_rect.origin());
  child2->draw_properties().visible_content_rect = child2_rect;
  child2->draw_properties().drawable_content_rect = root_rect;

  layer_tree_host_->SetRootLayer(root);
  layer_tree_host_->SetViewportSize(root_rect.size());

  // With a huge memory limit, all layers should update and push their textures.
  root->InvalidateContentRect(root_rect);
  child->InvalidateContentRect(child_rect);
  child2->InvalidateContentRect(child2_rect);
  layer_tree_host_->UpdateLayers(queue_.get());
  {
    UpdateTextures();
    EXPECT_EQ(6, root->fake_layer_updater()->update_count());
    EXPECT_EQ(3, child->fake_layer_updater()->update_count());
    EXPECT_EQ(3, child2->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());

    root->fake_layer_updater()->ClearUpdateCount();
    child->fake_layer_updater()->ClearUpdateCount();
    child2->fake_layer_updater()->ClearUpdateCount();

    scoped_ptr<FakeTiledLayerImpl> root_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), root->id()));
    scoped_ptr<FakeTiledLayerImpl> child_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child->id()));
    scoped_ptr<FakeTiledLayerImpl> child2_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child2->id()));
    LayerPushPropertiesTo(child2.get(), child2_impl.get());
    LayerPushPropertiesTo(child.get(), child_impl.get());
    LayerPushPropertiesTo(root.get(), root_impl.get());

    for (unsigned i = 0; i < 3; ++i) {
      for (unsigned j = 0; j < 2; ++j)
        EXPECT_TRUE(root_impl->HasResourceIdForTileAt(i, j));
      EXPECT_TRUE(child_impl->HasResourceIdForTileAt(i, 0));
      EXPECT_TRUE(child2_impl->HasResourceIdForTileAt(i, 0));
    }
  }
  layer_tree_host_->CommitComplete();

  // With a memory limit that includes only the root layer (3x2 tiles) and half
  // the surface that the child layers draw into, the child layers will not be
  // allocated. If the surface isn't accounted for, then one of the children
  // would fit within the memory limit.
  root->InvalidateContentRect(root_rect);
  child->InvalidateContentRect(child_rect);
  child2->InvalidateContentRect(child2_rect);

  size_t memory_limit = (3 * 2 + 3 * 1) * (100 * 100) * 4;
  layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(
      memory_limit);
  layer_tree_host_->UpdateLayers(queue_.get());
  {
    UpdateTextures();
    EXPECT_EQ(6, root->fake_layer_updater()->update_count());
    EXPECT_EQ(0, child->fake_layer_updater()->update_count());
    EXPECT_EQ(0, child2->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());

    root->fake_layer_updater()->ClearUpdateCount();
    child->fake_layer_updater()->ClearUpdateCount();
    child2->fake_layer_updater()->ClearUpdateCount();

    scoped_ptr<FakeTiledLayerImpl> root_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), root->id()));
    scoped_ptr<FakeTiledLayerImpl> child_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child->id()));
    scoped_ptr<FakeTiledLayerImpl> child2_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child2->id()));
    LayerPushPropertiesTo(child2.get(), child2_impl.get());
    LayerPushPropertiesTo(child.get(), child_impl.get());
    LayerPushPropertiesTo(root.get(), root_impl.get());

    for (unsigned i = 0; i < 3; ++i) {
      for (unsigned j = 0; j < 2; ++j)
        EXPECT_TRUE(root_impl->HasResourceIdForTileAt(i, j));
      EXPECT_FALSE(child_impl->HasResourceIdForTileAt(i, 0));
      EXPECT_FALSE(child2_impl->HasResourceIdForTileAt(i, 0));
    }
  }
  layer_tree_host_->CommitComplete();

  // With a memory limit that includes only half the root layer, no contents
  // will be allocated. If render surface memory wasn't accounted for, there is
  // enough space for one of the children layers, but they draw into a surface
  // that can't be allocated.
  root->InvalidateContentRect(root_rect);
  child->InvalidateContentRect(child_rect);
  child2->InvalidateContentRect(child2_rect);

  memory_limit = (3 * 1) * (100 * 100) * 4;
  layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(
      memory_limit);
  layer_tree_host_->UpdateLayers(queue_.get());
  {
    UpdateTextures();
    EXPECT_EQ(0, root->fake_layer_updater()->update_count());
    EXPECT_EQ(0, child->fake_layer_updater()->update_count());
    EXPECT_EQ(0, child2->fake_layer_updater()->update_count());
    EXPECT_FALSE(queue_->HasMoreUpdates());

    root->fake_layer_updater()->ClearUpdateCount();
    child->fake_layer_updater()->ClearUpdateCount();
    child2->fake_layer_updater()->ClearUpdateCount();

    scoped_ptr<FakeTiledLayerImpl> root_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), root->id()));
    scoped_ptr<FakeTiledLayerImpl> child_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child->id()));
    scoped_ptr<FakeTiledLayerImpl> child2_impl = make_scoped_ptr(
        new FakeTiledLayerImpl(host_impl_->active_tree(), child2->id()));
    LayerPushPropertiesTo(child2.get(), child2_impl.get());
    LayerPushPropertiesTo(child.get(), child_impl.get());
    LayerPushPropertiesTo(root.get(), root_impl.get());

    for (unsigned i = 0; i < 3; ++i) {
      for (unsigned j = 0; j < 2; ++j)
        EXPECT_FALSE(root_impl->HasResourceIdForTileAt(i, j));
      EXPECT_FALSE(child_impl->HasResourceIdForTileAt(i, 0));
      EXPECT_FALSE(child2_impl->HasResourceIdForTileAt(i, 0));
    }
  }
  layer_tree_host_->CommitComplete();

  ResourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(),
                                resource_provider_.get());
  layer_tree_host_->SetRootLayer(NULL);
}

class TrackingLayerPainter : public LayerPainter {
 public:
  static scoped_ptr<TrackingLayerPainter> Create() {
    return make_scoped_ptr(new TrackingLayerPainter());
  }

  virtual void Paint(SkCanvas* canvas,
                     const gfx::Rect& content_rect,
                     gfx::RectF* opaque) OVERRIDE {
    painted_rect_ = content_rect;
  }

  gfx::Rect PaintedRect() const { return painted_rect_; }
  void ResetPaintedRect() { painted_rect_ = gfx::Rect(); }

 private:
  gfx::Rect painted_rect_;
};

class UpdateTrackingTiledLayer : public FakeTiledLayer {
 public:
  explicit UpdateTrackingTiledLayer(PrioritizedResourceManager* manager)
      : FakeTiledLayer(manager) {
    scoped_ptr<TrackingLayerPainter> painter(TrackingLayerPainter::Create());
    tracking_layer_painter_ = painter.get();
    layer_updater_ =
        BitmapContentLayerUpdater::Create(painter.PassAs<LayerPainter>(),
                                          &stats_instrumentation_,
                                          0);
  }

  TrackingLayerPainter* tracking_layer_painter() const {
    return tracking_layer_painter_;
  }

 private:
  virtual LayerUpdater* Updater() const OVERRIDE {
    return layer_updater_.get();
  }
  virtual ~UpdateTrackingTiledLayer() {}

  TrackingLayerPainter* tracking_layer_painter_;
  scoped_refptr<BitmapContentLayerUpdater> layer_updater_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
};

TEST_F(TiledLayerTest, NonIntegerContentsScaleIsNotDistortedDuringPaint) {
  scoped_refptr<UpdateTrackingTiledLayer> layer =
      make_scoped_refptr(new UpdateTrackingTiledLayer(resource_manager_.get()));

  layer_tree_host_->root_layer()->AddChild(layer);

  gfx::Rect layer_rect(0, 0, 30, 31);
  layer->SetPosition(layer_rect.origin());
  layer->SetBounds(layer_rect.size());
  layer->UpdateContentsScale(1.5f);

  gfx::Rect content_rect(0, 0, 45, 47);
  EXPECT_EQ(content_rect.size(), layer->content_bounds());
  layer->draw_properties().visible_content_rect = content_rect;
  layer->draw_properties().drawable_content_rect = content_rect;

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();

  // Update the whole tile.
  layer->Update(queue_.get(), NULL);
  layer->tracking_layer_painter()->ResetPaintedRect();

  EXPECT_RECT_EQ(gfx::Rect(), layer->tracking_layer_painter()->PaintedRect());
  UpdateTextures();

  // Invalidate the entire layer in content space. When painting, the rect given
  // to webkit should match the layer's bounds.
  layer->InvalidateContentRect(content_rect);
  layer->Update(queue_.get(), NULL);

  EXPECT_RECT_EQ(layer_rect, layer->tracking_layer_painter()->PaintedRect());
}

TEST_F(TiledLayerTest,
       NonIntegerContentsScaleIsNotDistortedDuringInvalidation) {
  scoped_refptr<UpdateTrackingTiledLayer> layer =
      make_scoped_refptr(new UpdateTrackingTiledLayer(resource_manager_.get()));

  layer_tree_host_->root_layer()->AddChild(layer);

  gfx::Rect layer_rect(0, 0, 30, 31);
  layer->SetPosition(layer_rect.origin());
  layer->SetBounds(layer_rect.size());
  layer->UpdateContentsScale(1.3f);

  gfx::Rect content_rect(layer->content_bounds());
  layer->draw_properties().visible_content_rect = content_rect;
  layer->draw_properties().drawable_content_rect = content_rect;

  layer->SetTexturePriorities(priority_calculator_);
  resource_manager_->PrioritizeTextures();
  layer->SavePaintProperties();

  // Update the whole tile.
  layer->Update(queue_.get(), NULL);
  layer->tracking_layer_painter()->ResetPaintedRect();

  EXPECT_RECT_EQ(gfx::Rect(), layer->tracking_layer_painter()->PaintedRect());
  UpdateTextures();

  // Invalidate the entire layer in layer space. When painting, the rect given
  // to webkit should match the layer's bounds.
  layer->SetNeedsDisplayRect(layer_rect);
  layer->Update(queue_.get(), NULL);

  EXPECT_RECT_EQ(layer_rect, layer->tracking_layer_painter()->PaintedRect());
}

}  // namespace
}  // namespace cc
