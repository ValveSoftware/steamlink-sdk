// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_tile_set.h"

#include <algorithm>

#include "cc/resources/managed_tile_state.h"
#include "cc/resources/tile.h"

namespace cc {

class BinComparator {
 public:
  bool operator()(const Tile* a,
                  const Tile* b) const {
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

typedef std::vector<Tile*> TileVector;

void SortBinTiles(ManagedTileBin bin, TileVector* tiles) {
  switch (bin) {
    case NOW_AND_READY_TO_DRAW_BIN:
    case NEVER_BIN:
      break;
    case NOW_BIN:
    case SOON_BIN:
    case EVENTUALLY_AND_ACTIVE_BIN:
    case EVENTUALLY_BIN:
    case AT_LAST_AND_ACTIVE_BIN:
    case AT_LAST_BIN:
      std::sort(tiles->begin(), tiles->end(), BinComparator());
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

PrioritizedTileSet::PrioritizedTileSet() {
  for (int bin = 0; bin < NUM_BINS; ++bin)
    bin_sorted_[bin] = true;
}

PrioritizedTileSet::~PrioritizedTileSet() {}

void PrioritizedTileSet::InsertTile(Tile* tile, ManagedTileBin bin) {
  tiles_[bin].push_back(tile);
  bin_sorted_[bin] = false;
}

void PrioritizedTileSet::Clear() {
  for (int bin = 0; bin < NUM_BINS; ++bin) {
    tiles_[bin].clear();
    bin_sorted_[bin] = true;
  }
}

void PrioritizedTileSet::SortBinIfNeeded(ManagedTileBin bin) {
  if (!bin_sorted_[bin]) {
    SortBinTiles(bin, &tiles_[bin]);
    bin_sorted_[bin] = true;
  }
}

PrioritizedTileSet::Iterator::Iterator(
    PrioritizedTileSet* tile_set, bool use_priority_ordering)
    : tile_set_(tile_set),
      current_bin_(NOW_AND_READY_TO_DRAW_BIN),
      use_priority_ordering_(use_priority_ordering) {
  if (use_priority_ordering_)
    tile_set_->SortBinIfNeeded(current_bin_);
  iterator_ = tile_set->tiles_[current_bin_].begin();
  if (iterator_ == tile_set_->tiles_[current_bin_].end())
    AdvanceList();
}

PrioritizedTileSet::Iterator::~Iterator() {}

void PrioritizedTileSet::Iterator::DisablePriorityOrdering() {
  use_priority_ordering_ = false;
}

PrioritizedTileSet::Iterator&
PrioritizedTileSet::Iterator::operator++() {
  // We can't increment past the end of the tiles.
  DCHECK(iterator_ != tile_set_->tiles_[current_bin_].end());

  ++iterator_;
  if (iterator_ == tile_set_->tiles_[current_bin_].end())
    AdvanceList();
  return *this;
}

Tile* PrioritizedTileSet::Iterator::operator*() {
  DCHECK(iterator_ != tile_set_->tiles_[current_bin_].end());
  return *iterator_;
}

void PrioritizedTileSet::Iterator::AdvanceList() {
  DCHECK(iterator_ == tile_set_->tiles_[current_bin_].end());

  while (current_bin_ != NEVER_BIN) {
    current_bin_ = static_cast<ManagedTileBin>(current_bin_ + 1);

    if (use_priority_ordering_)
      tile_set_->SortBinIfNeeded(current_bin_);

    iterator_ = tile_set_->tiles_[current_bin_].begin();
    if (iterator_ != tile_set_->tiles_[current_bin_].end())
      break;
  }
}

}  // namespace cc
