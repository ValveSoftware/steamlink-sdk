// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TileManagerTest : public testing::TestWithParam<bool>,
                        public TileManagerClient {
 public:
  typedef std::vector<scoped_refptr<Tile> > TileVector;

  TileManagerTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(0),
        ready_to_activate_(false) {}

  void Initialize(int max_tiles,
                  TileMemoryLimitPolicy memory_limit_policy,
                  TreePriority tree_priority) {
    output_surface_ = FakeOutputSurface::Create3d();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
        false);
    resource_pool_ = ResourcePool::Create(
        resource_provider_.get(), GL_TEXTURE_2D, RGBA_8888);
    tile_manager_ =
        make_scoped_ptr(new FakeTileManager(this, resource_pool_.get()));

    memory_limit_policy_ = memory_limit_policy;
    max_tiles_ = max_tiles;
    picture_pile_ = FakePicturePileImpl::CreateInfiniteFilledPile();

    SetTreePriority(tree_priority);
  }

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;

    if (UsingMemoryLimit()) {
      state.soft_memory_limit_in_bytes =
          max_tiles_ * 4 * tile_size.width() * tile_size.height();
      state.num_resources_limit = 100;
    } else {
      state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
      state.num_resources_limit = max_tiles_;
    }
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;

    global_state_ = state;
    resource_pool_->SetResourceUsageLimits(state.soft_memory_limit_in_bytes,
                                           state.soft_memory_limit_in_bytes,
                                           state.num_resources_limit);
    tile_manager_->SetGlobalStateForTesting(state);
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;

    testing::Test::TearDown();
  }

  // TileManagerClient implementation.
  virtual const std::vector<PictureLayerImpl*>& GetPictureLayers() OVERRIDE {
    return picture_layers_;
  }
  virtual void NotifyReadyToActivate() OVERRIDE { ready_to_activate_ = true; }
  virtual void NotifyTileStateChanged(const Tile* tile) OVERRIDE {}

  TileVector CreateTilesWithSize(int count,
                                 TilePriority active_priority,
                                 TilePriority pending_priority,
                                 const gfx::Size& tile_size) {
    TileVector tiles;
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile = tile_manager_->CreateTile(picture_pile_.get(),
                                                           tile_size,
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           0);
      tile->SetPriority(ACTIVE_TREE, active_priority);
      tile->SetPriority(PENDING_TREE, pending_priority);
      tiles.push_back(tile);
    }
    return tiles;
  }

  TileVector CreateTiles(int count,
                         TilePriority active_priority,
                         TilePriority pending_priority) {
    return CreateTilesWithSize(
        count, active_priority, pending_priority, settings_.default_tile_size);
  }

  FakeTileManager* tile_manager() { return tile_manager_.get(); }

  int AssignedMemoryCount(const TileVector& tiles) {
    int has_memory_count = 0;
    for (TileVector::const_iterator it = tiles.begin(); it != tiles.end();
         ++it) {
      if (tile_manager_->HasBeenAssignedMemory(*it))
        ++has_memory_count;
    }
    return has_memory_count;
  }

  bool ready_to_activate() const { return ready_to_activate_; }

  // The parametrization specifies whether the max tile limit should
  // be applied to memory or resources.
  bool UsingResourceLimit() { return !GetParam(); }
  bool UsingMemoryLimit() { return GetParam(); }

 protected:
  GlobalStateThatImpactsTilePriority global_state_;

 private:
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> resource_pool_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_tiles_;
  bool ready_to_activate_;
  std::vector<PictureLayerImpl*> picture_layers_;
};

TEST_P(TileManagerTest, EnoughMemoryAllowAnything) {
  // A few tiles of each type of priority, with enough memory for all tiles.

  Initialize(10, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(3, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryAllowPrepaintOnly) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never bin.

  Initialize(10, ALLOW_PREPAINT_ONLY, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(3, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryPendingLowResAllowAbsoluteMinimum) {
  // A few low-res tiles required for activation, with enough memory for all
  // tiles.

  Initialize(5, ALLOW_ABSOLUTE_MINIMUM, SAME_PRIORITY_FOR_BOTH_TREES);
  TileVector pending_low_res =
      CreateTiles(5, TilePriority(), TilePriorityLowRes());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, AssignedMemoryCount(pending_low_res));
}

TEST_P(TileManagerTest, EnoughMemoryAllowAbsoluteMinimum) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never and soon bins.

  Initialize(10, ALLOW_ABSOLUTE_MINIMUM, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(0, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryAllowNothing) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // but allow nothing should not assign any memory.

  Initialize(10, ALLOW_NOTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(0, AssignedMemoryCount(active_now));
  EXPECT_EQ(0, AssignedMemoryCount(pending_now));
  EXPECT_EQ(0, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, PartialOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 8 tiles. The result
  // is all pending tree tiles get memory, and 3 of the active tree tiles
  // get memory. None of these tiles is needed to avoid calimity (flickering or
  // raster-on-demand) so the soft memory limit is used.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());
  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(3, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(5, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, PartialOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. The result is all active tree tiles
  // get memory, and 3 of the pending tree tiles get memory.
  // The pending tiles are not needed to avoid calimity (flickering or
  // raster-on-demand) and the active tiles fit, so the soft limit is used.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TotalOOMMemoryToPending) {
  // 10 tiles on active tree eventually bin, 10 tiles on pending tree that are
  // required for activation, but only enough tiles for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Pending tiles are now required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(8, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMActiveSoonMemoryToPending) {
  // 10 tiles on active tree soon bin, 10 tiles on pending tree that are
  // required for activation, but only enough tiles for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForSoonBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Pending tiles are now required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(8, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMMemoryToActive) {
  // 10 tiles on active tree eventually bin, 10 tiles on pending tree now bin,
  // but only enough memory for 4 tiles. The result is 4 active tree tiles
  // get memory, and none of the pending tree tiles get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Active tiles are required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(8, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMMemoryToNewContent) {
  // 10 tiles on active tree now bin, 10 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. Any tile missing would cause
  // a calamity (flickering or raster-on-demand). Depending on mode,
  // we should use varying amounts of the higher hard memory limit.
  if (UsingResourceLimit())
    return;

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityForNowBin());

  // Active tiles are required to avoid calimity. The hard-limit is used and all
  // active-tiles fit. No pending tiles are needed to avoid calamity so only 10
  // tiles total are used.
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(10, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  // Even the hard-limit won't save us now. All tiles are required to avoid
  // a clamity but we only have 16. The tiles will be distribted randomly
  // given they are identical, in practice depending on their screen location.
  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(16,
            AssignedMemoryCount(active_tree_tiles) +
                AssignedMemoryCount(pending_tree_tiles));

  // The pending tree is now more important. Active tiles will take higher
  // priority if they are ready-to-draw in practice. Importantly though,
  // pending tiles also utilize the hard-limit.
  SetTreePriority(NEW_CONTENT_TAKES_PRIORITY);
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(10, AssignedMemoryCount(pending_tree_tiles));
}

// If true, the max tile limit should be applied as bytes; if false,
// as num_resources_limit.
INSTANTIATE_TEST_CASE_P(TileManagerTests,
                        TileManagerTest,
                        ::testing::Values(true, false));

class TileManagerTileIteratorTest : public testing::Test {
 public:
  TileManagerTileIteratorTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(10000),
        ready_to_activate_(false),
        id_(7),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_) {}

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
        state.soft_memory_limit_in_bytes,
        state.soft_memory_limit_in_bytes,
        state.num_resources_limit);
    host_impl_.tile_manager()->SetGlobalStateForTesting(state);
  }

  virtual void SetUp() OVERRIDE {
    InitializeRenderer();
    SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  }

  virtual void InitializeRenderer() {
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
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
    pending_layer_ = NULL;
    active_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size) {
    SetupDefaultTrees(layer_bounds);
    pending_layer_->set_fixed_tile_size(tile_size);
    active_layer_->set_fixed_tile_size(tile_size);
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

    pending_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));
    pending_layer_->DoPostCommitInitializationIfNeeded();
  }

  void CreateHighLowResAndSetAllTilesVisible() {
    // Active layer must get updated first so pending layer can share from it.
    active_layer_->CreateDefaultTilingsAndTiles();
    active_layer_->SetAllTilesVisible();
    pending_layer_->CreateDefaultTilingsAndTiles();
    pending_layer_->SetAllTilesVisible();
  }

  TileManager* tile_manager() { return host_impl_.tile_manager(); }

 protected:
  GlobalStateThatImpactsTilePriority global_state_;

  TestSharedBitmapManager shared_bitmap_manager_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_tiles_;
  bool ready_to_activate_;
  int id_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerImpl* pending_layer_;
  FakePictureLayerImpl* active_layer_;
};

TEST_F(TileManagerTileIteratorTest, PairedPictureLayers) {
  host_impl_.CreatePendingTree();
  host_impl_.ActivatePendingTree();
  host_impl_.CreatePendingTree();

  LayerTreeImpl* active_tree = host_impl_.active_tree();
  LayerTreeImpl* pending_tree = host_impl_.pending_tree();
  EXPECT_NE(active_tree, pending_tree);

  scoped_ptr<FakePictureLayerImpl> active_layer =
      FakePictureLayerImpl::Create(active_tree, 10);
  scoped_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, 10);

  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);

  EXPECT_EQ(2u, paired_layers.size());
  if (paired_layers[0].active_layer) {
    EXPECT_EQ(active_layer.get(), paired_layers[0].active_layer);
    EXPECT_EQ(NULL, paired_layers[0].pending_layer);
  } else {
    EXPECT_EQ(pending_layer.get(), paired_layers[0].pending_layer);
    EXPECT_EQ(NULL, paired_layers[0].active_layer);
  }

  if (paired_layers[1].active_layer) {
    EXPECT_EQ(active_layer.get(), paired_layers[1].active_layer);
    EXPECT_EQ(NULL, paired_layers[1].pending_layer);
  } else {
    EXPECT_EQ(pending_layer.get(), paired_layers[1].pending_layer);
    EXPECT_EQ(NULL, paired_layers[1].active_layer);
  }

  active_layer->set_twin_layer(pending_layer.get());
  pending_layer->set_twin_layer(active_layer.get());

  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  EXPECT_EQ(active_layer.get(), paired_layers[0].active_layer);
  EXPECT_EQ(pending_layer.get(), paired_layers[0].pending_layer);
}

TEST_F(TileManagerTileIteratorTest, RasterTileIterator) {
  SetupDefaultTrees(gfx::Size(1000, 1000));
  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  active_layer_->CreateDefaultTilingsAndTiles();
  pending_layer_->CreateDefaultTilingsAndTiles();

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  TileManager::RasterTileIterator it(tile_manager,
                                     SAME_PRIORITY_FOR_BOTH_TREES);
  EXPECT_TRUE(it);

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  for (; it; ++it) {
    EXPECT_TRUE(*it);
    all_tiles.insert(*it);
    ++tile_count;
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(17u, tile_count);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  for (TileManager::RasterTileIterator it(tile_manager,
                                          SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    smoothness_tiles.insert(tile);
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  active_layer_->ResetAllTilesPriorities();
  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer_->HighResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  pending_layer_->LowResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  active_layer_->HighResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);
  active_layer_->LowResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_low_res_tiles.size(); ++i)
    all_tiles.insert(pending_low_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  size_t increasing_distance_tiles = 0u;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  for (TileManager::RasterTileIterator it(tile_manager,
                                          SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_LE(last_tile->priority(ACTIVE_TREE).priority_bin,
              tile->priority(ACTIVE_TREE).priority_bin);
    if (last_tile->priority(ACTIVE_TREE).priority_bin ==
        tile->priority(ACTIVE_TREE).priority_bin) {
      increasing_distance_tiles +=
          last_tile->priority(ACTIVE_TREE).distance_to_visible <=
          tile->priority(ACTIVE_TREE).distance_to_visible;
    }

    if (tile->priority(ACTIVE_TREE).priority_bin == TilePriority::NOW &&
        last_tile->priority(ACTIVE_TREE).resolution !=
            tile->priority(ACTIVE_TREE).resolution) {
      // Low resolution should come first.
      EXPECT_EQ(LOW_RESOLUTION, last_tile->priority(ACTIVE_TREE).resolution);
    }

    last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(increasing_distance_tiles, 3 * tile_count / 4);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  increasing_distance_tiles = 0u;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  for (TileManager::RasterTileIterator it(tile_manager,
                                          NEW_CONTENT_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_LE(last_tile->priority(PENDING_TREE).priority_bin,
              tile->priority(PENDING_TREE).priority_bin);
    if (last_tile->priority(PENDING_TREE).priority_bin ==
        tile->priority(PENDING_TREE).priority_bin) {
      increasing_distance_tiles +=
          last_tile->priority(PENDING_TREE).distance_to_visible <=
          tile->priority(PENDING_TREE).distance_to_visible;
    }

    if (tile->priority(PENDING_TREE).priority_bin == TilePriority::NOW &&
        last_tile->priority(PENDING_TREE).resolution !=
            tile->priority(PENDING_TREE).resolution) {
      // High resolution should come first.
      EXPECT_EQ(HIGH_RESOLUTION, last_tile->priority(PENDING_TREE).resolution);
    }

    last_tile = tile;
    new_content_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(increasing_distance_tiles, 3 * tile_count / 4);
}

TEST_F(TileManagerTileIteratorTest, EvictionTileIterator) {
  SetupDefaultTrees(gfx::Size(1000, 1000));
  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  active_layer_->CreateDefaultTilingsAndTiles();
  pending_layer_->CreateDefaultTilingsAndTiles();

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  TileManager::EvictionTileIterator empty_it(tile_manager,
                                             SAME_PRIORITY_FOR_BOTH_TREES);
  EXPECT_FALSE(empty_it);
  std::set<Tile*> all_tiles;
  size_t tile_count = 0;

  for (TileManager::RasterTileIterator raster_it(tile_manager,
                                                 SAME_PRIORITY_FOR_BOTH_TREES);
       raster_it;
       ++raster_it) {
    ++tile_count;
    EXPECT_TRUE(*raster_it);
    all_tiles.insert(*raster_it);
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(17u, tile_count);

  tile_manager->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  TileManager::EvictionTileIterator it(tile_manager, SMOOTHNESS_TAKES_PRIORITY);
  EXPECT_TRUE(it);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  for (; it; ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    EXPECT_TRUE(tile->HasResources());
    smoothness_tiles.insert(tile);
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  tile_manager->ReleaseTileResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  active_layer_->ResetAllTilesPriorities();
  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer_->HighResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  pending_layer_->LowResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  active_layer_->HighResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);
  active_layer_->LowResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_low_res_tiles.size(); ++i)
    all_tiles.insert(pending_low_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  tile_manager->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  for (TileManager::EvictionTileIterator it(tile_manager,
                                            SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_TRUE(tile->HasResources());

    if (!last_tile)
      last_tile = tile;

    EXPECT_GE(last_tile->priority(ACTIVE_TREE).priority_bin,
              tile->priority(ACTIVE_TREE).priority_bin);
    if (last_tile->priority(ACTIVE_TREE).priority_bin ==
        tile->priority(ACTIVE_TREE).priority_bin) {
      EXPECT_GE(last_tile->priority(ACTIVE_TREE).distance_to_visible,
                tile->priority(ACTIVE_TREE).distance_to_visible);
    }

    last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  for (TileManager::EvictionTileIterator it(tile_manager,
                                            NEW_CONTENT_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_GE(last_tile->priority(PENDING_TREE).priority_bin,
              tile->priority(PENDING_TREE).priority_bin);
    if (last_tile->priority(PENDING_TREE).priority_bin ==
        tile->priority(PENDING_TREE).priority_bin) {
      EXPECT_GE(last_tile->priority(PENDING_TREE).distance_to_visible,
                tile->priority(PENDING_TREE).distance_to_visible);
    }

    last_tile = tile;
    new_content_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}
}  // namespace
}  // namespace cc
