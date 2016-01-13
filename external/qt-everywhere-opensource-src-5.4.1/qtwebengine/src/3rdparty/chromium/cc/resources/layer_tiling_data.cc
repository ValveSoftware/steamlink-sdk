// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_tiling_data.h"

#include <vector>

#include "base/logging.h"

namespace cc {

scoped_ptr<LayerTilingData> LayerTilingData::Create(const gfx::Size& tile_size,
                                                    BorderTexelOption border) {
  return make_scoped_ptr(new LayerTilingData(tile_size, border));
}

LayerTilingData::LayerTilingData(const gfx::Size& tile_size,
                                 BorderTexelOption border)
    : tiling_data_(tile_size, gfx::Rect(), border == HAS_BORDER_TEXELS) {
  SetTileSize(tile_size);
}

LayerTilingData::~LayerTilingData() {}

void LayerTilingData::SetTileSize(const gfx::Size& size) {
  if (tile_size() == size)
    return;

  reset();

  tiling_data_.SetMaxTextureSize(size);
}

gfx::Size LayerTilingData::tile_size() const {
  return tiling_data_.max_texture_size();
}

void LayerTilingData::SetBorderTexelOption(
    BorderTexelOption border_texel_option) {
  bool border_texels = border_texel_option == HAS_BORDER_TEXELS;
  if (has_border_texels() == border_texels)
    return;

  reset();
  tiling_data_.SetHasBorderTexels(border_texels);
}

const LayerTilingData& LayerTilingData::operator=
    (const LayerTilingData & tiler) {
  tiling_data_ = tiler.tiling_data_;

  return *this;
}

void LayerTilingData::AddTile(scoped_ptr<Tile> tile, int i, int j) {
  DCHECK(!TileAt(i, j));
  tile->move_to(i, j);
  tiles_.add(std::make_pair(i, j), tile.Pass());
}

scoped_ptr<LayerTilingData::Tile> LayerTilingData::TakeTile(int i, int j) {
  return tiles_.take_and_erase(std::make_pair(i, j));
}

LayerTilingData::Tile* LayerTilingData::TileAt(int i, int j) const {
  return tiles_.get(std::make_pair(i, j));
}

void LayerTilingData::ContentRectToTileIndices(const gfx::Rect& content_rect,
                                               int* left,
                                               int* top,
                                               int* right,
                                               int* bottom) const {
  // An empty rect doesn't result in an empty set of tiles, so don't pass an
  // empty rect.
  // TODO(enne): Possibly we should fill a vector of tiles instead, since the
  // normal use of this function is to enumerate some tiles.
  DCHECK(!content_rect.IsEmpty());

  *left = tiling_data_.TileXIndexFromSrcCoord(content_rect.x());
  *top = tiling_data_.TileYIndexFromSrcCoord(content_rect.y());
  *right = tiling_data_.TileXIndexFromSrcCoord(content_rect.right() - 1);
  *bottom = tiling_data_.TileYIndexFromSrcCoord(content_rect.bottom() - 1);
}

gfx::Rect LayerTilingData::TileRect(const Tile* tile) const {
  gfx::Rect tile_rect = tiling_data_.TileBoundsWithBorder(tile->i(), tile->j());
  tile_rect.set_size(tile_size());
  return tile_rect;
}

Region LayerTilingData::OpaqueRegionInContentRect(
    const gfx::Rect& content_rect) const {
  if (content_rect.IsEmpty())
    return Region();

  Region opaque_region;
  int left, top, right, bottom;
  ContentRectToTileIndices(content_rect, &left, &top, &right, &bottom);
  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      Tile* tile = TileAt(i, j);
      if (!tile)
        continue;

      gfx::Rect tile_opaque_rect =
          gfx::IntersectRects(content_rect, tile->opaque_rect());
      opaque_region.Union(tile_opaque_rect);
    }
  }
  return opaque_region;
}

void LayerTilingData::SetTilingRect(const gfx::Rect& tiling_rect) {
  tiling_data_.SetTilingRect(tiling_rect);
  if (tiling_rect.IsEmpty()) {
    tiles_.clear();
    return;
  }

  // Any tiles completely outside our new bounds are invalid and should be
  // dropped.
  int left, top, right, bottom;
  ContentRectToTileIndices(tiling_rect, &left, &top, &right, &bottom);
  std::vector<TileMapKey> invalid_tile_keys;
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    if (it->first.first > right || it->first.second > bottom)
      invalid_tile_keys.push_back(it->first);
  }
  for (size_t i = 0; i < invalid_tile_keys.size(); ++i)
    tiles_.erase(invalid_tile_keys[i]);
}

}  // namespace cc
