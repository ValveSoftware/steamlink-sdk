// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer_impl.h"

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/quad_f.h"

namespace cc {

class DrawableTile : public LayerTilingData::Tile {
 public:
  static scoped_ptr<DrawableTile> Create() {
    return make_scoped_ptr(new DrawableTile());
  }

  ResourceProvider::ResourceId resource_id() const { return resource_id_; }
  void set_resource_id(ResourceProvider::ResourceId resource_id) {
    resource_id_ = resource_id;
  }
  bool contents_swizzled() { return contents_swizzled_; }
  void set_contents_swizzled(bool contents_swizzled) {
    contents_swizzled_ = contents_swizzled;
  }

 private:
  DrawableTile() : resource_id_(0), contents_swizzled_(false) {}

  ResourceProvider::ResourceId resource_id_;
  bool contents_swizzled_;

  DISALLOW_COPY_AND_ASSIGN(DrawableTile);
};

TiledLayerImpl::TiledLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id), skips_draw_(true) {}

TiledLayerImpl::~TiledLayerImpl() {
}

ResourceProvider::ResourceId TiledLayerImpl::ContentsResourceId() const {
  // This function is only valid for single texture layers, e.g. masks.
  DCHECK(tiler_);
  // It's possible the mask layer is created but has no size or otherwise
  // can't draw.
  if (tiler_->num_tiles_x() == 0 || tiler_->num_tiles_y() == 0)
    return 0;

  // Any other number of tiles other than 0 or 1 is incorrect for masks.
  DCHECK_EQ(tiler_->num_tiles_x(), 1);
  DCHECK_EQ(tiler_->num_tiles_y(), 1);

  DrawableTile* tile = TileAt(0, 0);
  ResourceProvider::ResourceId resource_id = tile ? tile->resource_id() : 0;
  return resource_id;
}

bool TiledLayerImpl::HasTileAt(int i, int j) const {
  return !!tiler_->TileAt(i, j);
}

bool TiledLayerImpl::HasResourceIdForTileAt(int i, int j) const {
  return HasTileAt(i, j) && TileAt(i, j)->resource_id();
}

DrawableTile* TiledLayerImpl::TileAt(int i, int j) const {
  return static_cast<DrawableTile*>(tiler_->TileAt(i, j));
}

DrawableTile* TiledLayerImpl::CreateTile(int i, int j) {
  scoped_ptr<DrawableTile> tile(DrawableTile::Create());
  DrawableTile* added_tile = tile.get();
  tiler_->AddTile(tile.PassAs<LayerTilingData::Tile>(), i, j);

  return added_tile;
}

void TiledLayerImpl::GetDebugBorderProperties(SkColor* color,
                                              float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

scoped_ptr<LayerImpl> TiledLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TiledLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void TiledLayerImpl::AsValueInto(base::DictionaryValue* state) const {
  LayerImpl::AsValueInto(state);
  state->Set("invalidation", MathUtil::AsValue(update_rect()).release());
}

size_t TiledLayerImpl::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  const size_t kMemoryUsagePerTileInBytes =
      4 * tiler_->tile_size().width() * tiler_->tile_size().height();
  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    const DrawableTile* tile = static_cast<DrawableTile*>(iter->second);
    if (!tile || !tile->resource_id())
      continue;
    amount += kMemoryUsagePerTileInBytes;
  }
  return amount;
}

void TiledLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  TiledLayerImpl* tiled_layer = static_cast<TiledLayerImpl*>(layer);

  tiled_layer->set_skips_draw(skips_draw_);
  tiled_layer->SetTilingData(*tiler_);

  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    int i = iter->first.first;
    int j = iter->first.second;
    DrawableTile* tile = static_cast<DrawableTile*>(iter->second);

    tiled_layer->PushTileProperties(i,
                                    j,
                                    tile->resource_id(),
                                    tile->opaque_rect(),
                                    tile->contents_swizzled());
  }
}

bool TiledLayerImpl::WillDraw(DrawMode draw_mode,
                              ResourceProvider* resource_provider) {
  if (!tiler_ || tiler_->has_empty_bounds() ||
      visible_content_rect().IsEmpty() ||
      draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void TiledLayerImpl::AppendQuads(QuadSink* quad_sink,
                                 AppendQuadsData* append_quads_data) {
  DCHECK(tiler_);
  DCHECK(!tiler_->has_empty_bounds());
  DCHECK(!visible_content_rect().IsEmpty());

  gfx::Rect content_rect = visible_content_rect();
  SharedQuadState* shared_quad_state = quad_sink->CreateSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      quad_sink, content_bounds(), shared_quad_state, append_quads_data);

  int left, top, right, bottom;
  tiler_->ContentRectToTileIndices(content_rect, &left, &top, &right, &bottom);

  if (ShowDebugBorders()) {
    for (int j = top; j <= bottom; ++j) {
      for (int i = left; i <= right; ++i) {
        DrawableTile* tile = TileAt(i, j);
        gfx::Rect tile_rect = tiler_->tile_bounds(i, j);
        gfx::Rect visible_tile_rect = tile_rect;
        SkColor border_color;
        float border_width;

        if (skips_draw_ || !tile || !tile->resource_id()) {
          border_color = DebugColors::MissingTileBorderColor();
          border_width = DebugColors::MissingTileBorderWidth(layer_tree_impl());
        } else {
          border_color = DebugColors::HighResTileBorderColor();
          border_width = DebugColors::HighResTileBorderWidth(layer_tree_impl());
        }
        scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
            DebugBorderDrawQuad::Create();
        debug_border_quad->SetNew(shared_quad_state,
                                  tile_rect,
                                  visible_tile_rect,
                                  border_color,
                                  border_width);
        quad_sink->Append(debug_border_quad.PassAs<DrawQuad>());
      }
    }
  }

  if (skips_draw_)
    return;

  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      DrawableTile* tile = TileAt(i, j);
      gfx::Rect tile_rect = tiler_->tile_bounds(i, j);
      gfx::Rect display_rect = tile_rect;
      tile_rect.Intersect(content_rect);

      // Skip empty tiles.
      if (tile_rect.IsEmpty())
        continue;

      gfx::Rect visible_tile_rect =
          quad_sink->UnoccludedContentRect(tile_rect, draw_transform());
      if (visible_tile_rect.IsEmpty())
        continue;

      if (!tile || !tile->resource_id()) {
        SkColor checker_color;
        if (ShowDebugBorders()) {
          checker_color =
              tile ? DebugColors::InvalidatedTileCheckerboardColor()
                   : DebugColors::EvictedTileCheckerboardColor();
        } else {
          checker_color = DebugColors::DefaultCheckerboardColor();
        }

        scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
            CheckerboardDrawQuad::Create();
        checkerboard_quad->SetNew(
            shared_quad_state, tile_rect, visible_tile_rect, checker_color);
        quad_sink->Append(checkerboard_quad.PassAs<DrawQuad>());
        append_quads_data->num_missing_tiles++;
        continue;
      }

      gfx::Rect tile_opaque_rect =
          contents_opaque() ? tile_rect : gfx::IntersectRects(
                                              tile->opaque_rect(), tile_rect);

      // Keep track of how the top left has moved, so the texture can be
      // offset the same amount.
      gfx::Vector2d display_offset = tile_rect.origin() - display_rect.origin();
      gfx::Vector2d texture_offset =
          tiler_->texture_offset(i, j) + display_offset;
      gfx::RectF tex_coord_rect = gfx::RectF(tile_rect.size()) + texture_offset;

      float tile_width = static_cast<float>(tiler_->tile_size().width());
      float tile_height = static_cast<float>(tiler_->tile_size().height());
      gfx::Size texture_size(tile_width, tile_height);

      scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
      quad->SetNew(shared_quad_state,
                   tile_rect,
                   tile_opaque_rect,
                   visible_tile_rect,
                   tile->resource_id(),
                   tex_coord_rect,
                   texture_size,
                   tile->contents_swizzled());
      quad_sink->Append(quad.PassAs<DrawQuad>());
    }
  }
}

void TiledLayerImpl::SetTilingData(const LayerTilingData& tiler) {
  if (tiler_) {
    tiler_->reset();
  } else {
    tiler_ = LayerTilingData::Create(tiler.tile_size(),
                                     tiler.has_border_texels()
                                         ? LayerTilingData::HAS_BORDER_TEXELS
                                         : LayerTilingData::NO_BORDER_TEXELS);
  }
  *tiler_ = tiler;
}

void TiledLayerImpl::PushTileProperties(
    int i,
    int j,
    ResourceProvider::ResourceId resource_id,
    const gfx::Rect& opaque_rect,
    bool contents_swizzled) {
  DrawableTile* tile = TileAt(i, j);
  if (!tile)
    tile = CreateTile(i, j);
  tile->set_resource_id(resource_id);
  tile->set_opaque_rect(opaque_rect);
  tile->set_contents_swizzled(contents_swizzled);
}

void TiledLayerImpl::PushInvalidTile(int i, int j) {
  DrawableTile* tile = TileAt(i, j);
  if (!tile)
    tile = CreateTile(i, j);
  tile->set_resource_id(0);
  tile->set_opaque_rect(gfx::Rect());
  tile->set_contents_swizzled(false);
}

Region TiledLayerImpl::VisibleContentOpaqueRegion() const {
  if (skips_draw_)
    return Region();
  if (contents_opaque())
    return visible_content_rect();
  return tiler_->OpaqueRegionInContentRect(visible_content_rect());
}

void TiledLayerImpl::ReleaseResources() {
  tiler_->reset();
}

const char* TiledLayerImpl::LayerTypeAsString() const {
  return "cc::TiledLayerImpl";
}

}  // namespace cc
