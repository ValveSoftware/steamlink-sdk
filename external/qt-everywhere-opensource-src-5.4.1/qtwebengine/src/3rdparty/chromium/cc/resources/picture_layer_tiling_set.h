// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_LAYER_TILING_SET_H_
#define CC_RESOURCES_PICTURE_LAYER_TILING_SET_H_

#include "cc/base/region.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/resources/picture_layer_tiling.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT PictureLayerTilingSet {
 public:
  PictureLayerTilingSet(PictureLayerTilingClient* client,
                        const gfx::Size& layer_bounds);
  ~PictureLayerTilingSet();

  void SetClient(PictureLayerTilingClient* client);
  const PictureLayerTilingClient* client() const { return client_; }

  // Make this set of tilings match the same set of content scales from |other|.
  // Delete any tilings that don't meet |minimum_contents_scale|.  Recreate
  // any tiles that intersect |layer_invalidation|.  Update the size of all
  // tilings to |new_layer_bounds|.
  // Returns true if we had at least one high res tiling synced.
  bool SyncTilings(const PictureLayerTilingSet& other,
                   const gfx::Size& new_layer_bounds,
                   const Region& layer_invalidation,
                   float minimum_contents_scale);

  void RemoveTilesInRegion(const Region& region);

  gfx::Size layer_bounds() const { return layer_bounds_; }

  PictureLayerTiling* AddTiling(float contents_scale);
  size_t num_tilings() const { return tilings_.size(); }
  int NumHighResTilings() const;
  PictureLayerTiling* tiling_at(size_t idx) { return tilings_[idx]; }
  const PictureLayerTiling* tiling_at(size_t idx) const {
    return tilings_[idx];
  }

  PictureLayerTiling* TilingAtScale(float scale) const;

  // Remove all tilings.
  void RemoveAllTilings();

  // Remove one tiling.
  void Remove(PictureLayerTiling* tiling);

  // Remove all tiles; keep all tilings.
  void RemoveAllTiles();

  void DidBecomeActive();
  void DidBecomeRecycled();

  // For a given rect, iterates through tiles that can fill it.  If no
  // set of tiles with resources can fill the rect, then it will iterate
  // through null tiles with valid geometry_rect() until the rect is full.
  // If all tiles have resources, the union of all geometry_rects will
  // exactly fill rect with no overlap.
  class CC_EXPORT CoverageIterator {
   public:
    CoverageIterator(const PictureLayerTilingSet* set,
      float contents_scale,
      const gfx::Rect& content_rect,
      float ideal_contents_scale);
    ~CoverageIterator();

    // Visible rect (no borders), always in the space of rect,
    // regardless of the relative contents scale of the tiling.
    gfx::Rect geometry_rect() const;
    // Texture rect (in texels) for geometry_rect
    gfx::RectF texture_rect() const;
    // Texture size in texels
    gfx::Size texture_size() const;

    Tile* operator->() const;
    Tile* operator*() const;

    CoverageIterator& operator++();
    operator bool() const;

    PictureLayerTiling* CurrentTiling();

   private:
    int NextTiling() const;

    const PictureLayerTilingSet* set_;
    float contents_scale_;
    float ideal_contents_scale_;
    PictureLayerTiling::CoverageIterator tiling_iter_;
    int current_tiling_;
    int ideal_tiling_;

    Region current_region_;
    Region missing_region_;
    Region::Iterator region_iter_;
  };

  scoped_ptr<base::Value> AsValue() const;
  size_t GPUMemoryUsageInBytes() const;

 private:
  PictureLayerTilingClient* client_;
  gfx::Size layer_bounds_;
  ScopedPtrVector<PictureLayerTiling> tilings_;

  friend class Iterator;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerTilingSet);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_LAYER_TILING_SET_H_
