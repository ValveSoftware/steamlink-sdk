// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_LAYER_TILING_H_
#define CC_RESOURCES_PICTURE_LAYER_TILING_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/base/tiling_data.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/rect.h"

namespace cc {

class PictureLayerTiling;

class CC_EXPORT PictureLayerTilingClient {
 public:
  // Create a tile at the given content_rect (in the contents scale of the
  // tiling) This might return null if the client cannot create such a tile.
  virtual scoped_refptr<Tile> CreateTile(
    PictureLayerTiling* tiling,
    const gfx::Rect& content_rect) = 0;
  virtual void UpdatePile(Tile* tile) = 0;
  virtual gfx::Size CalculateTileSize(
    const gfx::Size& content_bounds) const = 0;
  virtual const Region* GetInvalidation() = 0;
  virtual const PictureLayerTiling* GetTwinTiling(
      const PictureLayerTiling* tiling) const = 0;
  virtual size_t GetMaxTilesForInterestArea() const = 0;
  virtual float GetSkewportTargetTimeInSeconds() const = 0;
  virtual int GetSkewportExtrapolationLimitInContentPixels() const = 0;

 protected:
  virtual ~PictureLayerTilingClient() {}
};

class CC_EXPORT PictureLayerTiling {
 public:
  class CC_EXPORT TilingRasterTileIterator {
   public:
    TilingRasterTileIterator();
    TilingRasterTileIterator(PictureLayerTiling* tiling, WhichTree tree);
    ~TilingRasterTileIterator();

    operator bool() const { return !!current_tile_; }
    Tile* operator*() { return current_tile_; }
    TilePriority::PriorityBin get_type() const { return type_; }

    TilingRasterTileIterator& operator++();

    gfx::Rect TileBounds() const {
      DCHECK(*this);
      if (type_ == TilePriority::NOW) {
        return tiling_->tiling_data_.TileBounds(visible_iterator_.index_x(),
                                                visible_iterator_.index_y());
      }
      return tiling_->tiling_data_.TileBounds(spiral_iterator_.index_x(),
                                              spiral_iterator_.index_y());
    }

   private:
    void AdvancePhase();
    bool TileNeedsRaster(Tile* tile) const {
      RasterMode mode = tile->DetermineRasterModeForTree(tree_);
      return tile->NeedsRasterForMode(mode);
    }

    PictureLayerTiling* tiling_;

    TilePriority::PriorityBin type_;
    gfx::Rect visible_rect_in_content_space_;
    gfx::Rect skewport_in_content_space_;
    gfx::Rect eventually_rect_in_content_space_;
    gfx::Rect soon_border_rect_in_content_space_;
    WhichTree tree_;

    Tile* current_tile_;
    TilingData::Iterator visible_iterator_;
    TilingData::SpiralDifferenceIterator spiral_iterator_;
    bool skewport_processed_;
  };

  class CC_EXPORT TilingEvictionTileIterator {
   public:
    TilingEvictionTileIterator();
    TilingEvictionTileIterator(PictureLayerTiling* tiling,
                               TreePriority tree_priority);
    ~TilingEvictionTileIterator();

    operator bool();
    Tile* operator*();
    TilingEvictionTileIterator& operator++();
    TilePriority::PriorityBin get_type() {
      DCHECK(*this);
      const TilePriority& priority =
          (*tile_iterator_)->priority_for_tree_priority(tree_priority_);
      return priority.priority_bin;
    }

   private:
    void Initialize();
    bool IsValid() const { return is_valid_; }

    bool is_valid_;
    PictureLayerTiling* tiling_;
    TreePriority tree_priority_;
    std::vector<Tile*>::iterator tile_iterator_;
  };

  ~PictureLayerTiling();

  // Create a tiling with no tiles.  CreateTiles must be called to add some.
  static scoped_ptr<PictureLayerTiling> Create(
      float contents_scale,
      const gfx::Size& layer_bounds,
      PictureLayerTilingClient* client);
  gfx::Size layer_bounds() const { return layer_bounds_; }
  void SetLayerBounds(const gfx::Size& layer_bounds);
  void Invalidate(const Region& layer_region);
  void RemoveTilesInRegion(const Region& layer_region);
  void CreateMissingTilesInLiveTilesRect();

  void SetClient(PictureLayerTilingClient* client);
  void set_resolution(TileResolution resolution) { resolution_ = resolution; }
  TileResolution resolution() const { return resolution_; }

  gfx::Rect TilingRect() const;
  gfx::Rect live_tiles_rect() const { return live_tiles_rect_; }
  gfx::Size tile_size() const { return tiling_data_.max_texture_size(); }
  float contents_scale() const { return contents_scale_; }

  Tile* TileAt(int i, int j) const {
    TileMap::const_iterator iter = tiles_.find(TileMapKey(i, j));
    return (iter == tiles_.end()) ? NULL : iter->second.get();
  }

  void CreateAllTilesForTesting() {
    SetLiveTilesRect(tiling_data_.tiling_rect());
  }

  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> all_tiles;
    for (TileMap::const_iterator it = tiles_.begin();
         it != tiles_.end(); ++it)
      all_tiles.push_back(it->second.get());
    return all_tiles;
  }

  // Iterate over all tiles to fill content_rect.  Even if tiles are invalid
  // (i.e. no valid resource) this tiling should still iterate over them.
  // The union of all geometry_rect calls for each element iterated over should
  // exactly equal content_rect and no two geometry_rects should intersect.
  class CC_EXPORT CoverageIterator {
   public:
    CoverageIterator();
    CoverageIterator(const PictureLayerTiling* tiling,
        float dest_scale,
        const gfx::Rect& rect);
    ~CoverageIterator();

    // Visible rect (no borders), always in the space of content_rect,
    // regardless of the contents scale of the tiling.
    gfx::Rect geometry_rect() const;
    // Texture rect (in texels) for geometry_rect
    gfx::RectF texture_rect() const;
    gfx::Size texture_size() const;

    // Full rect (including borders) of the current tile, always in the space
    // of content_rect, regardless of the contents scale of the tiling.
    gfx::Rect full_tile_geometry_rect() const;

    Tile* operator->() const { return current_tile_; }
    Tile* operator*() const { return current_tile_; }

    CoverageIterator& operator++();
    operator bool() const { return tile_j_ <= bottom_; }

    int i() const { return tile_i_; }
    int j() const { return tile_j_; }

   private:
    const PictureLayerTiling* tiling_;
    gfx::Rect dest_rect_;
    float dest_to_content_scale_;

    Tile* current_tile_;
    gfx::Rect current_geometry_rect_;
    int tile_i_;
    int tile_j_;
    int left_;
    int top_;
    int right_;
    int bottom_;

    friend class PictureLayerTiling;
  };

  void Reset();

  void UpdateTilePriorities(WhichTree tree,
                            const gfx::Rect& visible_layer_rect,
                            float layer_contents_scale,
                            double current_frame_time_in_seconds);

  // Copies the src_tree priority into the dst_tree priority for all tiles.
  // The src_tree priority is reset to the lowest priority possible.  This
  // also updates the pile on each tile to be the current client's pile.
  void DidBecomeActive();

  // Resets the active priority for all tiles in a tiling, when an active
  // tiling is becoming recycled. This may include some tiles which are
  // not in the the pending tiling (due to invalidations). This must
  // be called before DidBecomeActive, as it resets the active priority
  // while DidBecomeActive promotes pending priority on a similar set of tiles.
  void DidBecomeRecycled();

  void UpdateTilesToCurrentPile();

  bool NeedsUpdateForFrameAtTime(double frame_time_in_seconds) {
    return frame_time_in_seconds != last_impl_frame_time_in_seconds_;
  }

  scoped_ptr<base::Value> AsValue() const;
  size_t GPUMemoryUsageInBytes() const;

  struct RectExpansionCache {
    RectExpansionCache();

    gfx::Rect previous_start;
    gfx::Rect previous_bounds;
    gfx::Rect previous_result;
    int64 previous_target;
  };

  static
  gfx::Rect ExpandRectEquallyToAreaBoundedBy(
      const gfx::Rect& starting_rect,
      int64 target_area,
      const gfx::Rect& bounding_rect,
      RectExpansionCache* cache);

  bool has_ever_been_updated() const {
    return last_impl_frame_time_in_seconds_ != 0.0;
  }

 protected:
  friend class CoverageIterator;
  friend class TilingRasterTileIterator;
  friend class TilingEvictionTileIterator;

  typedef std::pair<int, int> TileMapKey;
  typedef base::hash_map<TileMapKey, scoped_refptr<Tile> > TileMap;

  PictureLayerTiling(float contents_scale,
                     const gfx::Size& layer_bounds,
                     PictureLayerTilingClient* client);
  void SetLiveTilesRect(const gfx::Rect& live_tiles_rect);
  Tile* CreateTile(int i, int j, const PictureLayerTiling* twin_tiling);

  // Computes a skewport. The calculation extrapolates the last visible
  // rect and the current visible rect to expand the skewport to where it
  // would be in |skewport_target_time| seconds. Note that the skewport
  // is guaranteed to contain the current visible rect.
  gfx::Rect ComputeSkewport(double current_frame_time_in_seconds,
                            const gfx::Rect& visible_rect_in_content_space)
      const;

  void UpdateEvictionCacheIfNeeded(TreePriority tree_priority);
  void DoInvalidate(const Region& layer_region, bool recreate_tiles);

  // Given properties.
  float contents_scale_;
  gfx::Size layer_bounds_;
  TileResolution resolution_;
  PictureLayerTilingClient* client_;

  // Internal data.
  TilingData tiling_data_;
  TileMap tiles_;  // It is not legal to have a NULL tile in the tiles_ map.
  gfx::Rect live_tiles_rect_;

  // State saved for computing velocities based upon finite differences.
  double last_impl_frame_time_in_seconds_;
  gfx::Rect last_visible_rect_in_content_space_;

  gfx::Rect current_visible_rect_in_content_space_;
  gfx::Rect current_skewport_;
  gfx::Rect current_eventually_rect_;
  gfx::Rect current_soon_border_rect_;

  std::vector<Tile*> eviction_tiles_cache_;
  bool eviction_tiles_cache_valid_;
  TreePriority eviction_cache_tree_priority_;

 private:
  DISALLOW_ASSIGN(PictureLayerTiling);

  RectExpansionCache expansion_cache_;
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_LAYER_TILING_H_
