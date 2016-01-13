// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/trees/layer_tree_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class FakeRasterizerImpl : public Rasterizer, public RasterizerTaskClient {
 public:
  // Overridden from Rasterizer:
  virtual void SetClient(RasterizerClient* client) OVERRIDE {}
  virtual void Shutdown() OVERRIDE {}
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE {
    for (RasterTaskQueue::Item::Vector::const_iterator it =
             queue->items.begin();
         it != queue->items.end();
         ++it) {
      RasterTask* task = it->task;

      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();

      completed_tasks_.push_back(task);
    }
  }
  virtual void CheckForCompletedTasks() OVERRIDE {
    for (RasterTask::Vector::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end();
         ++it) {
      RasterTask* task = it->get();

      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      task->RunReplyOnOriginThread();
    }
    completed_tasks_.clear();
  }

  // Overridden from RasterizerTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(RasterTask* task) OVERRIDE {
    return NULL;
  }
  virtual void ReleaseCanvasForRaster(RasterTask* task) OVERRIDE {}

 private:
  RasterTask::Vector completed_tasks_;
};
base::LazyInstance<FakeRasterizerImpl> g_fake_rasterizer =
    LAZY_INSTANCE_INITIALIZER;

class TileManagerPerfTest : public testing::Test {
 public:
  TileManagerPerfTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(10000),
        id_(7),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(10000),
                   &proxy_,
                   &shared_bitmap_manager_),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size(256, 256);

    state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
    state.num_resources_limit = max_tiles_;
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;

    global_state_ = state;
    host_impl_.resource_pool()->SetResourceUsageLimits(
        state.soft_memory_limit_in_bytes, 0, state.num_resources_limit);
    host_impl_.tile_manager()->SetGlobalStateForTesting(state);
  }

  virtual void SetUp() OVERRIDE {
    picture_pile_ = FakePicturePileImpl::CreateInfiniteFilledPile();
    InitializeRenderer();
    SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  }

  virtual void InitializeRenderer() {
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
    tile_manager()->SetRasterizerForTesting(g_fake_rasterizer.Pointer());
  }

  void SetupDefaultTrees(const gfx::Size& layer_bounds) {
    gfx::Size tile_size(100, 100);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

    SetupTrees(pending_pile, active_pile);
  }

  void ActivateTree() {
    host_impl_.ActivatePendingTree();
    CHECK(!host_impl_.pending_tree());
    pending_root_layer_ = NULL;
    active_root_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size) {
    SetupDefaultTrees(layer_bounds);
    pending_root_layer_->set_fixed_tile_size(tile_size);
    active_root_layer_->set_fixed_tile_size(tile_size);
  }

  void SetupTrees(scoped_refptr<PicturePileImpl> pending_pile,
                  scoped_refptr<PicturePileImpl> active_pile) {
    SetupPendingTree(active_pile);
    ActivateTree();
    SetupPendingTree(pending_pile);
  }

  void SetupPendingTree(scoped_refptr<PicturePileImpl> pile) {
    host_impl_.CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pending_tree();
    // Clear recycled tree.
    pending_tree->DetachLayerTree();

    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::CreateWithPile(pending_tree, id_, pile);
    pending_layer->SetDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());

    pending_root_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));
    pending_root_layer_->DoPostCommitInitializationIfNeeded();
  }

  void CreateHighLowResAndSetAllTilesVisible() {
    // Active layer must get updated first so pending layer can share from it.
    active_root_layer_->CreateDefaultTilingsAndTiles();
    active_root_layer_->SetAllTilesVisible();
    pending_root_layer_->CreateDefaultTilingsAndTiles();
    pending_root_layer_->SetAllTilesVisible();
  }

  void RunRasterIteratorTest(const std::string& test_name,
                             unsigned tile_count) {
    timer_.Reset();
    do {
      int count = tile_count;
      for (TileManager::RasterTileIterator it(tile_manager(),
                                              SAME_PRIORITY_FOR_BOTH_TREES);
           it && count;
           ++it) {
        --count;
      }
      ASSERT_EQ(0, count);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tile_manager_raster_tile_iterator",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  std::vector<LayerImpl*> CreateLayers(int layer_count,
                                       int tiles_per_layer_count) {
    // Compute the width/height required for high res to get
    // tiles_per_layer_count tiles.
    float width = std::sqrt(static_cast<float>(tiles_per_layer_count));
    float height = tiles_per_layer_count / width;

    // Adjust the width and height to account for the fact that tiles
    // are bigger than 1x1. Also, account for the fact that that we
    // will be creating one high res and one low res tiling. That is,
    // width and height should be smaller by sqrt(1 + low_res_scale).
    // This gives us _approximately_ correct counts.
    width *= settings_.default_tile_size.width() /
             std::sqrt(1 + settings_.low_res_contents_scale_factor);
    height *= settings_.default_tile_size.height() /
              std::sqrt(1 + settings_.low_res_contents_scale_factor);

    // Ensure that we start with blank trees and no tiles.
    host_impl_.ResetTreesForTesting();
    tile_manager()->CleanUpReleasedTilesForTesting();

    gfx::Size layer_bounds(width, height);
    gfx::Size viewport(width / 5, height / 5);
    host_impl_.SetViewportSize(viewport);
    SetupDefaultTreesWithFixedTileSize(layer_bounds,
                                       settings_.default_tile_size);

    active_root_layer_->CreateDefaultTilingsAndTiles();
    pending_root_layer_->CreateDefaultTilingsAndTiles();

    std::vector<LayerImpl*> layers;

    // Pending layer counts as one layer.
    layers.push_back(pending_root_layer_);
    int next_id = id_ + 1;

    // Create the rest of the layers as children of the root layer.
    while (static_cast<int>(layers.size()) < layer_count) {
      scoped_ptr<FakePictureLayerImpl> layer =
          FakePictureLayerImpl::CreateWithPile(
              host_impl_.pending_tree(), next_id, picture_pile_);
      layer->SetBounds(layer_bounds);
      layers.push_back(layer.get());
      pending_root_layer_->AddChild(layer.PassAs<LayerImpl>());

      FakePictureLayerImpl* fake_layer =
          static_cast<FakePictureLayerImpl*>(layers.back());

      fake_layer->SetDrawsContent(true);
      fake_layer->DoPostCommitInitializationIfNeeded();
      fake_layer->CreateDefaultTilingsAndTiles();
      ++next_id;
    }

    return layers;
  }

  GlobalStateThatImpactsTilePriority GlobalStateForTest() {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.soft_memory_limit_in_bytes =
        10000u * 4u *
        static_cast<size_t>(tile_size.width() * tile_size.height());
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes;
    state.num_resources_limit = 10000;
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    return state;
  }

  void RunManageTilesTest(const std::string& test_name,
                          int layer_count,
                          int approximate_tile_count_per_layer) {
    std::vector<LayerImpl*> layers =
        CreateLayers(layer_count, approximate_tile_count_per_layer);
    timer_.Reset();
    do {
      host_impl_.UpdateCurrentFrameTime();
      for (unsigned i = 0; i < layers.size(); ++i)
        layers[i]->UpdateTiles();

      GlobalStateThatImpactsTilePriority global_state(GlobalStateForTest());
      tile_manager()->ManageTiles(global_state);
      tile_manager()->UpdateVisibleTiles();
      timer_.NextLap();
      host_impl_.ResetCurrentFrameTimeForNextFrame();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult(
        "manage_tiles", "", test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  TileManager* tile_manager() { return host_impl_.tile_manager(); }

 protected:
  GlobalStateThatImpactsTilePriority global_state_;

  TestSharedBitmapManager shared_bitmap_manager_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_tiles_;
  int id_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerImpl* pending_root_layer_;
  FakePictureLayerImpl* active_root_layer_;
  LapTimer timer_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  LayerTreeSettings settings_;
};

TEST_F(TileManagerPerfTest, ManageTiles) {
  RunManageTilesTest("1_100", 1, 100);
  RunManageTilesTest("1_500", 1, 500);
  RunManageTilesTest("1_1000", 1, 1000);
  RunManageTilesTest("5_100", 5, 100);
  RunManageTilesTest("5_500", 5, 500);
  RunManageTilesTest("5_1000", 5, 1000);
  RunManageTilesTest("10_100", 10, 100);
  RunManageTilesTest("10_500", 10, 500);
  RunManageTilesTest("10_1000", 10, 1000);
  RunManageTilesTest("100_100", 100, 100);
  RunManageTilesTest("100_500", 100, 500);
  RunManageTilesTest("100_1000", 100, 1000);
}

TEST_F(TileManagerPerfTest, RasterTileIterator) {
  SetupDefaultTrees(gfx::Size(10000, 10000));
  active_root_layer_->CreateDefaultTilingsAndTiles();
  pending_root_layer_->CreateDefaultTilingsAndTiles();

  RunRasterIteratorTest("2_16", 16);
  RunRasterIteratorTest("2_32", 32);
  RunRasterIteratorTest("2_64", 64);
  RunRasterIteratorTest("2_128", 128);
}

}  // namespace

}  // namespace cc
