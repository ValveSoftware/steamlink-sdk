// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "cc/resources/managed_tile_state.h"
#include "cc/resources/prioritized_tile_set.h"
#include "cc/resources/tile.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class BinComparator {
 public:
  bool operator()(const scoped_refptr<Tile>& a,
                  const scoped_refptr<Tile>& b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();

    if (ams.priority_bin != bms.priority_bin)
      return ams.priority_bin < bms.priority_bin;

    if (ams.required_for_activation != bms.required_for_activation)
      return ams.required_for_activation;

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    if (ams.distance_to_visible != bms.distance_to_visible)
      return ams.distance_to_visible < bms.distance_to_visible;

    gfx::Rect a_rect = a->content_rect();
    gfx::Rect b_rect = b->content_rect();
    if (a_rect.y() != b_rect.y())
      return a_rect.y() < b_rect.y();
    return a_rect.x() < b_rect.x();
  }
};

namespace {

class PrioritizedTileSetTest : public testing::Test {
 public:
  PrioritizedTileSetTest() {
    output_surface_ = FakeOutputSurface::Create3d().Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ =
        ResourceProvider::Create(
            output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
            false)
            .Pass();
    resource_pool_ = ResourcePool::Create(
        resource_provider_.get(), GL_TEXTURE_2D, RGBA_8888);
    tile_manager_.reset(
        new FakeTileManager(&tile_manager_client_, resource_pool_.get()));
    picture_pile_ = FakePicturePileImpl::CreateInfiniteFilledPile();
  }

  scoped_refptr<Tile> CreateTile() {
    return tile_manager_->CreateTile(picture_pile_.get(),
                                     settings_.default_tile_size,
                                     gfx::Rect(),
                                     gfx::Rect(),
                                     1.0,
                                     0,
                                     0,
                                     0);
  }

 private:
  LayerTreeSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> resource_pool_;
  FakeTileManagerClient tile_manager_client_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
};

TEST_F(PrioritizedTileSetTest, EmptyIterator) {
  // Creating an iterator to an empty set should work (but create iterator that
  // isn't valid).

  PrioritizedTileSet set;

  PrioritizedTileSet::Iterator it(&set, true);
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, NonEmptyIterator) {
  PrioritizedTileSet set;
  scoped_refptr<Tile> tile = CreateTile();
  set.InsertTile(tile, NOW_BIN);

  PrioritizedTileSet::Iterator it(&set, true);
  EXPECT_TRUE(it);
  EXPECT_TRUE(*it == tile.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, NowAndReadyToDrawBin) {
  // Ensure that tiles in NOW_AND_READY_TO_DRAW_BIN aren't sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NOW_AND_READY_TO_DRAW_BIN);
    }
  }

  // Tiles should appear in the same order as inserted.
  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, NowBin) {
  // Ensure that tiles in NOW_BIN are sorted according to BinComparator.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NOW_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, SoonBin) {
  // Ensure that tiles in SOON_BIN are sorted according to BinComparator.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, SOON_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, SoonBinNoPriority) {
  // Ensure that when not using priority iterator, SOON_BIN tiles
  // are not sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, SOON_BIN);
    }
  }

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, false);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, EventuallyAndActiveBin) {
  // Ensure that EVENTUALLY_AND_ACTIVE_BIN tiles are sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, EVENTUALLY_AND_ACTIVE_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, EventuallyBin) {
  // Ensure that EVENTUALLY_BIN tiles are sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, EVENTUALLY_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, AtLastAndActiveBin) {
  // Ensure that AT_LAST_AND_ACTIVE_BIN tiles are sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, AT_LAST_AND_ACTIVE_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, AtLastBin) {
  // Ensure that AT_LAST_BIN tiles are sorted.

  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, AT_LAST_BIN);
    }
  }

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::Iterator it(&set, true);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, TilesForEachBin) {
  // Aggregate test with one tile for each of the bins, which
  // should appear in order of the bins.

  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> now_bin = CreateTile();
  scoped_refptr<Tile> soon_bin = CreateTile();
  scoped_refptr<Tile> eventually_and_active_bin = CreateTile();
  scoped_refptr<Tile> eventually_bin = CreateTile();
  scoped_refptr<Tile> at_last_bin = CreateTile();
  scoped_refptr<Tile> at_last_and_active_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(soon_bin, SOON_BIN);
  set.InsertTile(at_last_and_active_bin, AT_LAST_AND_ACTIVE_BIN);
  set.InsertTile(eventually_bin, EVENTUALLY_BIN);
  set.InsertTile(now_bin, NOW_BIN);
  set.InsertTile(eventually_and_active_bin, EVENTUALLY_AND_ACTIVE_BIN);
  set.InsertTile(at_last_bin, AT_LAST_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  // Tiles should appear in order.
  PrioritizedTileSet::Iterator it(&set, true);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == now_bin.get());
  ++it;
  EXPECT_TRUE(*it == soon_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_and_active_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_bin.get());
  ++it;
  EXPECT_TRUE(*it == at_last_and_active_bin.get());
  ++it;
  EXPECT_TRUE(*it == at_last_bin.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, ManyTilesForEachBin) {
  // Aggregate test with many tiles in each of the bins of various
  // priorities. Ensure that they are all returned in a sorted order.

  std::vector<scoped_refptr<Tile> > now_and_ready_to_draw_bins;
  std::vector<scoped_refptr<Tile> > now_bins;
  std::vector<scoped_refptr<Tile> > soon_bins;
  std::vector<scoped_refptr<Tile> > eventually_and_active_bins;
  std::vector<scoped_refptr<Tile> > eventually_bins;
  std::vector<scoped_refptr<Tile> > at_last_bins;
  std::vector<scoped_refptr<Tile> > at_last_and_active_bins;

  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  PrioritizedTileSet set;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);

      now_and_ready_to_draw_bins.push_back(tile);
      now_bins.push_back(tile);
      soon_bins.push_back(tile);
      eventually_and_active_bins.push_back(tile);
      eventually_bins.push_back(tile);
      at_last_bins.push_back(tile);
      at_last_and_active_bins.push_back(tile);

      set.InsertTile(tile, NOW_AND_READY_TO_DRAW_BIN);
      set.InsertTile(tile, NOW_BIN);
      set.InsertTile(tile, SOON_BIN);
      set.InsertTile(tile, EVENTUALLY_AND_ACTIVE_BIN);
      set.InsertTile(tile, EVENTUALLY_BIN);
      set.InsertTile(tile, AT_LAST_BIN);
      set.InsertTile(tile, AT_LAST_AND_ACTIVE_BIN);
    }
  }

  PrioritizedTileSet::Iterator it(&set, true);
  std::vector<scoped_refptr<Tile> >::iterator vector_it;

  // Now and ready are not sorted.
  for (vector_it = now_and_ready_to_draw_bins.begin();
       vector_it != now_and_ready_to_draw_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Now bins are sorted.
  std::sort(now_bins.begin(), now_bins.end(), BinComparator());
  for (vector_it = now_bins.begin(); vector_it != now_bins.end(); ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Soon bins are sorted.
  std::sort(soon_bins.begin(), soon_bins.end(), BinComparator());
  for (vector_it = soon_bins.begin(); vector_it != soon_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Eventually and active bins are sorted.
  std::sort(eventually_and_active_bins.begin(),
            eventually_and_active_bins.end(),
            BinComparator());
  for (vector_it = eventually_and_active_bins.begin();
       vector_it != eventually_and_active_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Eventually bins are sorted.
  std::sort(eventually_bins.begin(), eventually_bins.end(), BinComparator());
  for (vector_it = eventually_bins.begin(); vector_it != eventually_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // At last and active bins are sorted.
  std::sort(at_last_and_active_bins.begin(),
            at_last_and_active_bins.end(),
            BinComparator());
  for (vector_it = at_last_and_active_bins.begin();
       vector_it != at_last_and_active_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // At last bins are sorted.
  std::sort(at_last_bins.begin(), at_last_bins.end(), BinComparator());
  for (vector_it = at_last_bins.begin(); vector_it != at_last_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, ManyTilesForEachBinDisablePriority) {
  // Aggregate test with many tiles for each of the bins. Tiles should
  // appear in order, until DisablePriorityOrdering is called. After that
  // tiles should appear in the order they were inserted.

  std::vector<scoped_refptr<Tile> > now_and_ready_to_draw_bins;
  std::vector<scoped_refptr<Tile> > now_bins;
  std::vector<scoped_refptr<Tile> > soon_bins;
  std::vector<scoped_refptr<Tile> > eventually_and_active_bins;
  std::vector<scoped_refptr<Tile> > eventually_bins;
  std::vector<scoped_refptr<Tile> > at_last_bins;
  std::vector<scoped_refptr<Tile> > at_last_and_active_bins;

  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  PrioritizedTileSet set;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);

      now_and_ready_to_draw_bins.push_back(tile);
      now_bins.push_back(tile);
      soon_bins.push_back(tile);
      eventually_and_active_bins.push_back(tile);
      eventually_bins.push_back(tile);
      at_last_bins.push_back(tile);
      at_last_and_active_bins.push_back(tile);

      set.InsertTile(tile, NOW_AND_READY_TO_DRAW_BIN);
      set.InsertTile(tile, NOW_BIN);
      set.InsertTile(tile, SOON_BIN);
      set.InsertTile(tile, EVENTUALLY_AND_ACTIVE_BIN);
      set.InsertTile(tile, EVENTUALLY_BIN);
      set.InsertTile(tile, AT_LAST_BIN);
      set.InsertTile(tile, AT_LAST_AND_ACTIVE_BIN);
    }
  }

  PrioritizedTileSet::Iterator it(&set, true);
  std::vector<scoped_refptr<Tile> >::iterator vector_it;

  // Now and ready are not sorted.
  for (vector_it = now_and_ready_to_draw_bins.begin();
       vector_it != now_and_ready_to_draw_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Now bins are sorted.
  std::sort(now_bins.begin(), now_bins.end(), BinComparator());
  for (vector_it = now_bins.begin(); vector_it != now_bins.end(); ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Soon bins are sorted.
  std::sort(soon_bins.begin(), soon_bins.end(), BinComparator());
  for (vector_it = soon_bins.begin(); vector_it != soon_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // After we disable priority ordering, we already have sorted the next vector.
  it.DisablePriorityOrdering();

  // Eventually and active bins are sorted.
  std::sort(eventually_and_active_bins.begin(),
            eventually_and_active_bins.end(),
            BinComparator());
  for (vector_it = eventually_and_active_bins.begin();
       vector_it != eventually_and_active_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // Eventually bins are not sorted.
  for (vector_it = eventually_bins.begin(); vector_it != eventually_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // At last and active bins are not sorted.
  for (vector_it = at_last_and_active_bins.begin();
       vector_it != at_last_and_active_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  // At last bins are not sorted.
  for (vector_it = at_last_bins.begin(); vector_it != at_last_bins.end();
       ++vector_it) {
    EXPECT_TRUE(*vector_it == *it);
    ++it;
  }

  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, TilesForFirstAndLastBins) {
  // Make sure that if we have empty lists between two non-empty lists,
  // we just get two tiles from the iterator.

  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> at_last_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(at_last_bin, AT_LAST_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  // Only two tiles should appear and they should appear in order.
  PrioritizedTileSet::Iterator it(&set, true);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == at_last_bin.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, MultipleIterators) {
  // Ensure that multiple iterators don't interfere with each other.

  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> now_bin = CreateTile();
  scoped_refptr<Tile> soon_bin = CreateTile();
  scoped_refptr<Tile> eventually_bin = CreateTile();
  scoped_refptr<Tile> at_last_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(soon_bin, SOON_BIN);
  set.InsertTile(eventually_bin, EVENTUALLY_BIN);
  set.InsertTile(now_bin, NOW_BIN);
  set.InsertTile(at_last_bin, AT_LAST_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  // Tiles should appear in order.
  PrioritizedTileSet::Iterator it(&set, true);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == now_bin.get());
  ++it;
  EXPECT_TRUE(*it == soon_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_bin.get());
  ++it;
  EXPECT_TRUE(*it == at_last_bin.get());
  ++it;
  EXPECT_FALSE(it);

  // Creating multiple iterators shouldn't affect old iterators.
  PrioritizedTileSet::Iterator second_it(&set, true);
  EXPECT_TRUE(second_it);
  EXPECT_FALSE(it);

  ++second_it;
  EXPECT_TRUE(second_it);
  ++second_it;
  EXPECT_TRUE(second_it);
  EXPECT_FALSE(it);

  PrioritizedTileSet::Iterator third_it(&set, true);
  EXPECT_TRUE(third_it);
  ++second_it;
  ++second_it;
  EXPECT_TRUE(second_it);
  EXPECT_TRUE(third_it);
  EXPECT_FALSE(it);

  ++third_it;
  ++third_it;
  EXPECT_TRUE(third_it);
  EXPECT_TRUE(*third_it == soon_bin.get());
  EXPECT_TRUE(second_it);
  EXPECT_TRUE(*second_it == at_last_bin.get());
  EXPECT_FALSE(it);

  ++second_it;
  EXPECT_TRUE(third_it);
  EXPECT_FALSE(second_it);
  EXPECT_FALSE(it);

  set.Clear();

  PrioritizedTileSet::Iterator empty_it(&set, true);
  EXPECT_FALSE(empty_it);
}

}  // namespace
}  // namespace cc

