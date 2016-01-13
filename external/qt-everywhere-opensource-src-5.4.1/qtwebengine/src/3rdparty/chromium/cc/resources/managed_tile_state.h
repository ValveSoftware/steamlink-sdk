// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MANAGED_TILE_STATE_H_
#define CC_RESOURCES_MANAGED_TILE_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/raster_mode.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/resources/tile_priority.h"

namespace cc {

class TileManager;

// Tile manager classifying tiles into a few basic bins:
enum ManagedTileBin {
  NOW_AND_READY_TO_DRAW_BIN = 0,  // Ready to draw and within viewport.
  NOW_BIN = 1,                    // Needed ASAP.
  SOON_BIN = 2,                   // Impl-side version of prepainting.
  EVENTUALLY_AND_ACTIVE_BIN = 3,  // Nice to have, and has a task or resource.
  EVENTUALLY_BIN = 4,             // Nice to have, if we've got memory and time.
  AT_LAST_AND_ACTIVE_BIN = 5,     // Only do this after all other bins.
  AT_LAST_BIN = 6,                // Only do this after all other bins.
  NEVER_BIN = 7,                  // Dont bother.
  NUM_BINS = 8
  // NOTE: Be sure to update ManagedTileBinAsValue and kBinPolicyMap when adding
  // or reordering fields.
};
scoped_ptr<base::Value> ManagedTileBinAsValue(ManagedTileBin bin);

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  class CC_EXPORT TileVersion {
   public:
    enum Mode { RESOURCE_MODE, SOLID_COLOR_MODE, PICTURE_PILE_MODE };

    TileVersion();
    ~TileVersion();

    Mode mode() const { return mode_; }

    bool IsReadyToDraw() const;

    ResourceProvider::ResourceId get_resource_id() const {
      DCHECK(mode_ == RESOURCE_MODE);
      DCHECK(resource_);

      return resource_->id();
    }

    SkColor get_solid_color() const {
      DCHECK(mode_ == SOLID_COLOR_MODE);

      return solid_color_;
    }

    bool contents_swizzled() const {
      DCHECK(resource_);
      return !PlatformColor::SameComponentOrder(resource_->format());
    }

    bool requires_resource() const {
      return mode_ == RESOURCE_MODE || mode_ == PICTURE_PILE_MODE;
    }

    inline bool has_resource() const { return !!resource_; }

    size_t GPUMemoryUsageInBytes() const;

    void SetSolidColorForTesting(SkColor color) { set_solid_color(color); }
    void SetResourceForTesting(scoped_ptr<ScopedResource> resource) {
      resource_ = resource.Pass();
    }

   private:
    friend class TileManager;
    friend class PrioritizedTileSet;
    friend class Tile;
    friend class ManagedTileState;

    void set_use_resource() { mode_ = RESOURCE_MODE; }

    void set_solid_color(const SkColor& color) {
      mode_ = SOLID_COLOR_MODE;
      solid_color_ = color;
    }

    void set_rasterize_on_demand() { mode_ = PICTURE_PILE_MODE; }

    Mode mode_;
    SkColor solid_color_;
    scoped_ptr<ScopedResource> resource_;
    scoped_refptr<RasterTask> raster_task_;
  };

  ManagedTileState();
  ~ManagedTileState();

  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  TileVersion tile_versions[NUM_RASTER_MODES];
  RasterMode raster_mode;

  ManagedTileBin bin;

  TileResolution resolution;
  bool required_for_activation;
  TilePriority::PriorityBin priority_bin;
  float distance_to_visible;
  bool visible_and_ready_to_draw;

  // Priority for this state from the last time we assigned memory.
  unsigned scheduled_priority;
};

}  // namespace cc

#endif  // CC_RESOURCES_MANAGED_TILE_STATE_H_
