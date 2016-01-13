// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PRIORITIZED_TILE_SET_H_
#define CC_RESOURCES_PRIORITIZED_TILE_SET_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/resources/managed_tile_state.h"

namespace cc {
class Tile;

class CC_EXPORT PrioritizedTileSet {
 public:
  PrioritizedTileSet();
  ~PrioritizedTileSet();

  void InsertTile(Tile* tile, ManagedTileBin bin);
  void Clear();

  class CC_EXPORT Iterator {
   public:
    Iterator(PrioritizedTileSet* set, bool use_priority_ordering);

    ~Iterator();

    void DisablePriorityOrdering();

    Iterator& operator++();
    Tile* operator->() { return *(*this); }
    Tile* operator*();
    operator bool() const {
      return iterator_ != tile_set_->tiles_[current_bin_].end();
    }

   private:
    void AdvanceList();

    PrioritizedTileSet* tile_set_;
    ManagedTileBin current_bin_;
    std::vector<Tile*>::iterator iterator_;
    bool use_priority_ordering_;
  };

 private:
  friend class Iterator;

  void SortBinIfNeeded(ManagedTileBin bin);

  std::vector<Tile*> tiles_[NUM_BINS];
  bool bin_sorted_[NUM_BINS];
};

}  // namespace cc

#endif  // CC_RESOURCES_PRIORITIZED_TILE_SET_H_
