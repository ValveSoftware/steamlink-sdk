// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PRIORITIZED_TILE_H_
#define CC_TILES_PRIORITIZED_TILE_H_

#include "cc/base/cc_export.h"
#include "cc/playback/raster_source.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"

namespace cc {
class RasterSource;
class PictureLayerTiling;

class CC_EXPORT PrioritizedTile {
 public:
  // This class is constructed and returned by a |PictureLayerTiling|, and
  // represents a tile and its priority.
  PrioritizedTile();
  PrioritizedTile(const PrioritizedTile& other);
  ~PrioritizedTile();

  Tile* tile() const { return tile_; }
  const scoped_refptr<RasterSource>& raster_source() const {
    return raster_source_;
  }
  const TilePriority& priority() const { return priority_; }
  bool is_occluded() const { return is_occluded_; }

  void AsValueInto(base::trace_event::TracedValue* value) const;

 private:
  friend class PictureLayerTiling;

  PrioritizedTile(Tile* tile,
                  scoped_refptr<RasterSource> raster_source,
                  const TilePriority priority,
                  bool is_occluded);

  Tile* tile_;
  scoped_refptr<RasterSource> raster_source_;
  TilePriority priority_;
  bool is_occluded_;
};

}  // namespace cc

#endif  // CC_TILES_PRIORITIZED_TILE_H_
