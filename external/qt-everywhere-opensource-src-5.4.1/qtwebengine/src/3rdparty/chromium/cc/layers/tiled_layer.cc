// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "build/build_config.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/priority_calculator.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

// Maximum predictive expansion of the visible area.
static const int kMaxPredictiveTilesCount = 2;

// Number of rows/columns of tiles to pre-paint.
// We should increase these further as all textures are
// prioritized and we insure performance doesn't suffer.
static const int kPrepaintRows = 4;
static const int kPrepaintColumns = 2;

class UpdatableTile : public LayerTilingData::Tile {
 public:
  static scoped_ptr<UpdatableTile> Create(
      scoped_ptr<LayerUpdater::Resource> updater_resource) {
    return make_scoped_ptr(new UpdatableTile(updater_resource.Pass()));
  }

  LayerUpdater::Resource* updater_resource() { return updater_resource_.get(); }
  PrioritizedResource* managed_resource() {
    return updater_resource_->texture();
  }

  bool is_dirty() const { return !dirty_rect.IsEmpty(); }

  // Reset update state for the current frame. This should occur before painting
  // for all layers. Since painting one layer can invalidate another layer after
  // it has already painted, mark all non-dirty tiles as valid before painting
  // such that invalidations during painting won't prevent them from being
  // pushed.
  void ResetUpdateState() {
    update_rect = gfx::Rect();
    occluded = false;
    partial_update = false;
    valid_for_frame = !is_dirty();
  }

  // This promises to update the tile and therefore also guarantees the tile
  // will be valid for this frame. dirty_rect is copied into update_rect so we
  // can continue to track re-entrant invalidations that occur during painting.
  void MarkForUpdate() {
    valid_for_frame = true;
    update_rect = dirty_rect;
    dirty_rect = gfx::Rect();
  }

  gfx::Rect dirty_rect;
  gfx::Rect update_rect;
  bool partial_update;
  bool valid_for_frame;
  bool occluded;

 private:
  explicit UpdatableTile(scoped_ptr<LayerUpdater::Resource> updater_resource)
      : partial_update(false),
        valid_for_frame(false),
        occluded(false),
        updater_resource_(updater_resource.Pass()) {}

  scoped_ptr<LayerUpdater::Resource> updater_resource_;

  DISALLOW_COPY_AND_ASSIGN(UpdatableTile);
};

TiledLayer::TiledLayer()
    : ContentsScalingLayer(),
      texture_format_(RGBA_8888),
      skips_draw_(false),
      failed_update_(false),
      tiling_option_(AUTO_TILE) {
  tiler_ =
      LayerTilingData::Create(gfx::Size(), LayerTilingData::HAS_BORDER_TEXELS);
}

TiledLayer::~TiledLayer() {}

scoped_ptr<LayerImpl> TiledLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return TiledLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void TiledLayer::UpdateTileSizeAndTilingOption() {
  DCHECK(layer_tree_host());

  gfx::Size default_tile_size = layer_tree_host()->settings().default_tile_size;
  gfx::Size max_untiled_layer_size =
      layer_tree_host()->settings().max_untiled_layer_size;
  int layer_width = content_bounds().width();
  int layer_height = content_bounds().height();

  gfx::Size tile_size(std::min(default_tile_size.width(), layer_width),
                      std::min(default_tile_size.height(), layer_height));

  // Tile if both dimensions large, or any one dimension large and the other
  // extends into a second tile but the total layer area isn't larger than that
  // of the largest possible untiled layer. This heuristic allows for long
  // skinny layers (e.g. scrollbars) that are Nx1 tiles to minimize wasted
  // texture space but still avoids creating very large tiles.
  bool any_dimension_large = layer_width > max_untiled_layer_size.width() ||
                             layer_height > max_untiled_layer_size.height();
  bool any_dimension_one_tile =
      (layer_width <= default_tile_size.width() ||
       layer_height <= default_tile_size.height()) &&
      (layer_width * layer_height) <= (max_untiled_layer_size.width() *
                                       max_untiled_layer_size.height());
  bool auto_tiled = any_dimension_large && !any_dimension_one_tile;

  bool is_tiled;
  if (tiling_option_ == ALWAYS_TILE)
    is_tiled = true;
  else if (tiling_option_ == NEVER_TILE)
    is_tiled = false;
  else
    is_tiled = auto_tiled;

  gfx::Size requested_size = is_tiled ? tile_size : content_bounds();
  const int max_size =
      layer_tree_host()->GetRendererCapabilities().max_texture_size;
  requested_size.SetToMin(gfx::Size(max_size, max_size));
  SetTileSize(requested_size);
}

void TiledLayer::UpdateBounds() {
  gfx::Rect old_tiling_rect = tiler_->tiling_rect();
  gfx::Rect new_tiling_rect = gfx::Rect(content_bounds());
  if (old_tiling_rect == new_tiling_rect)
    return;
  tiler_->SetTilingRect(new_tiling_rect);

  // Invalidate any areas that the new bounds exposes.
  Region old_region = old_tiling_rect;
  Region new_region = new_tiling_rect;
  new_tiling_rect.Subtract(old_tiling_rect);
  for (Region::Iterator new_rects(new_tiling_rect); new_rects.has_rect();
       new_rects.next())
    InvalidateContentRect(new_rects.rect());
}

void TiledLayer::SetTileSize(const gfx::Size& size) {
  tiler_->SetTileSize(size);
}

void TiledLayer::SetBorderTexelOption(
    LayerTilingData::BorderTexelOption border_texel_option) {
  tiler_->SetBorderTexelOption(border_texel_option);
}

bool TiledLayer::DrawsContent() const {
  if (!ContentsScalingLayer::DrawsContent())
    return false;

  bool has_more_than_one_tile =
      tiler_->num_tiles_x() > 1 || tiler_->num_tiles_y() > 1;
  if (tiling_option_ == NEVER_TILE && has_more_than_one_tile)
    return false;

  return true;
}

void TiledLayer::ReduceMemoryUsage() {
  if (Updater())
    Updater()->ReduceMemoryUsage();
}

void TiledLayer::SetIsMask(bool is_mask) {
  set_tiling_option(is_mask ? NEVER_TILE : AUTO_TILE);
}

void TiledLayer::PushPropertiesTo(LayerImpl* layer) {
  ContentsScalingLayer::PushPropertiesTo(layer);

  TiledLayerImpl* tiled_layer = static_cast<TiledLayerImpl*>(layer);

  tiled_layer->set_skips_draw(skips_draw_);
  tiled_layer->SetTilingData(*tiler_);
  std::vector<UpdatableTile*> invalid_tiles;

  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    int i = iter->first.first;
    int j = iter->first.second;
    UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
    // TODO(enne): This should not ever be null.
    if (!tile)
      continue;

    if (!tile->managed_resource()->have_backing_texture()) {
      // Evicted tiles get deleted from both layers
      invalid_tiles.push_back(tile);
      continue;
    }

    if (!tile->valid_for_frame) {
      // Invalidated tiles are set so they can get different debug colors.
      tiled_layer->PushInvalidTile(i, j);
      continue;
    }

    tiled_layer->PushTileProperties(
        i,
        j,
        tile->managed_resource()->resource_id(),
        tile->opaque_rect(),
        tile->managed_resource()->contents_swizzled());
  }
  for (std::vector<UpdatableTile*>::const_iterator iter = invalid_tiles.begin();
       iter != invalid_tiles.end();
       ++iter)
    tiler_->TakeTile((*iter)->i(), (*iter)->j());

  // TiledLayer must push properties every frame, since viewport state and
  // occlusion from anywhere in the tree can change what the layer decides to
  // push to the impl tree.
  needs_push_properties_ = true;
}

PrioritizedResourceManager* TiledLayer::ResourceManager() {
  if (!layer_tree_host())
    return NULL;
  return layer_tree_host()->contents_texture_manager();
}

const PrioritizedResource* TiledLayer::ResourceAtForTesting(int i,
                                                            int j) const {
  UpdatableTile* tile = TileAt(i, j);
  if (!tile)
    return NULL;
  return tile->managed_resource();
}

void TiledLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host && host != layer_tree_host()) {
    for (LayerTilingData::TileMap::const_iterator
             iter = tiler_->tiles().begin();
         iter != tiler_->tiles().end();
         ++iter) {
      UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
      // TODO(enne): This should not ever be null.
      if (!tile)
        continue;
      tile->managed_resource()->SetTextureManager(
          host->contents_texture_manager());
    }
  }
  ContentsScalingLayer::SetLayerTreeHost(host);
}

UpdatableTile* TiledLayer::TileAt(int i, int j) const {
  return static_cast<UpdatableTile*>(tiler_->TileAt(i, j));
}

UpdatableTile* TiledLayer::CreateTile(int i, int j) {
  CreateUpdaterIfNeeded();

  scoped_ptr<UpdatableTile> tile(
      UpdatableTile::Create(Updater()->CreateResource(ResourceManager())));
  tile->managed_resource()->SetDimensions(tiler_->tile_size(), texture_format_);

  UpdatableTile* added_tile = tile.get();
  tiler_->AddTile(tile.PassAs<LayerTilingData::Tile>(), i, j);

  added_tile->dirty_rect = tiler_->TileRect(added_tile);

  // Temporary diagnostic crash.
  CHECK(added_tile);
  CHECK(TileAt(i, j));

  return added_tile;
}

void TiledLayer::SetNeedsDisplayRect(const gfx::RectF& dirty_rect) {
  InvalidateContentRect(LayerRectToContentRect(dirty_rect));
  ContentsScalingLayer::SetNeedsDisplayRect(dirty_rect);
}

void TiledLayer::InvalidateContentRect(const gfx::Rect& content_rect) {
  UpdateBounds();
  if (tiler_->is_empty() || content_rect.IsEmpty() || skips_draw_)
    return;

  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
    DCHECK(tile);
    // TODO(enne): This should not ever be null.
    if (!tile)
      continue;
    gfx::Rect bound = tiler_->TileRect(tile);
    bound.Intersect(content_rect);
    tile->dirty_rect.Union(bound);
  }
}

// Returns true if tile is dirty and only part of it needs to be updated.
bool TiledLayer::TileOnlyNeedsPartialUpdate(UpdatableTile* tile) {
  return !tile->dirty_rect.Contains(tiler_->TileRect(tile)) &&
         tile->managed_resource()->have_backing_texture();
}

bool TiledLayer::UpdateTiles(int left,
                             int top,
                             int right,
                             int bottom,
                             ResourceUpdateQueue* queue,
                             const OcclusionTracker<Layer>* occlusion,
                             bool* updated) {
  CreateUpdaterIfNeeded();

  bool ignore_occlusions = !occlusion;
  if (!HaveTexturesForTiles(left, top, right, bottom, ignore_occlusions)) {
    failed_update_ = true;
    return false;
  }

  gfx::Rect update_rect;
  gfx::Rect paint_rect;
  MarkTilesForUpdate(
    &update_rect, &paint_rect, left, top, right, bottom, ignore_occlusions);

  if (paint_rect.IsEmpty())
    return true;

  *updated = true;
  UpdateTileTextures(
      update_rect, paint_rect, left, top, right, bottom, queue, occlusion);
  return true;
}

void TiledLayer::MarkOcclusionsAndRequestTextures(
    int left,
    int top,
    int right,
    int bottom,
    const OcclusionTracker<Layer>* occlusion) {
  int occluded_tile_count = 0;
  bool succeeded = true;
  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      UpdatableTile* tile = TileAt(i, j);
      DCHECK(tile);  // Did SetTexturePriorities get skipped?
      // TODO(enne): This should not ever be null.
      if (!tile)
        continue;
      // Did ResetUpdateState get skipped? Are we doing more than one occlusion
      // pass?
      DCHECK(!tile->occluded);
      gfx::Rect visible_tile_rect = gfx::IntersectRects(
          tiler_->tile_bounds(i, j), visible_content_rect());
      if (!draw_transform_is_animating() && occlusion &&
          occlusion->Occluded(
              render_target(), visible_tile_rect, draw_transform())) {
        tile->occluded = true;
        occluded_tile_count++;
      } else {
        succeeded &= tile->managed_resource()->RequestLate();
      }
    }
  }
}

bool TiledLayer::HaveTexturesForTiles(int left,
                                      int top,
                                      int right,
                                      int bottom,
                                      bool ignore_occlusions) {
  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      UpdatableTile* tile = TileAt(i, j);
      DCHECK(tile);  // Did SetTexturePriorites get skipped?
                     // TODO(enne): This should not ever be null.
      if (!tile)
        continue;

      // Ensure the entire tile is dirty if we don't have the texture.
      if (!tile->managed_resource()->have_backing_texture())
        tile->dirty_rect = tiler_->TileRect(tile);

      // If using occlusion and the visible region of the tile is occluded,
      // don't reserve a texture or update the tile.
      if (tile->occluded && !ignore_occlusions)
        continue;

      if (!tile->managed_resource()->can_acquire_backing_texture())
        return false;
    }
  }
  return true;
}

void TiledLayer::MarkTilesForUpdate(gfx::Rect* update_rect,
                                    gfx::Rect* paint_rect,
                                    int left,
                                    int top,
                                    int right,
                                    int bottom,
                                    bool ignore_occlusions) {
  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      UpdatableTile* tile = TileAt(i, j);
      DCHECK(tile);  // Did SetTexturePriorites get skipped?
                     // TODO(enne): This should not ever be null.
      if (!tile)
        continue;
      if (tile->occluded && !ignore_occlusions)
        continue;

      // Prepare update rect from original dirty rects.
      update_rect->Union(tile->dirty_rect);

      // TODO(reveman): Decide if partial update should be allowed based on cost
      // of update. https://bugs.webkit.org/show_bug.cgi?id=77376
      if (tile->is_dirty() &&
          !layer_tree_host()->AlwaysUsePartialTextureUpdates()) {
        // If we get a partial update, we use the same texture, otherwise return
        // the current texture backing, so we don't update visible textures
        // non-atomically.  If the current backing is in-use, it won't be
        // deleted until after the commit as the texture manager will not allow
        // deletion or recycling of in-use textures.
        if (TileOnlyNeedsPartialUpdate(tile) &&
            layer_tree_host()->RequestPartialTextureUpdate()) {
          tile->partial_update = true;
        } else {
          tile->dirty_rect = tiler_->TileRect(tile);
          tile->managed_resource()->ReturnBackingTexture();
        }
      }

      paint_rect->Union(tile->dirty_rect);
      tile->MarkForUpdate();
    }
  }
}

void TiledLayer::UpdateTileTextures(const gfx::Rect& update_rect,
                                    const gfx::Rect& paint_rect,
                                    int left,
                                    int top,
                                    int right,
                                    int bottom,
                                    ResourceUpdateQueue* queue,
                                    const OcclusionTracker<Layer>* occlusion) {
  // The update_rect should be in layer space. So we have to convert the
  // paint_rect from content space to layer space.
  float width_scale =
      paint_properties().bounds.width() /
      static_cast<float>(content_bounds().width());
  float height_scale =
      paint_properties().bounds.height() /
      static_cast<float>(content_bounds().height());
  update_rect_ = gfx::ScaleRect(update_rect, width_scale, height_scale);

  // Calling PrepareToUpdate() calls into WebKit to paint, which may have the
  // side effect of disabling compositing, which causes our reference to the
  // texture updater to be deleted.  However, we can't free the memory backing
  // the SkCanvas until the paint finishes, so we grab a local reference here to
  // hold the updater alive until the paint completes.
  scoped_refptr<LayerUpdater> protector(Updater());
  gfx::Rect painted_opaque_rect;
  Updater()->PrepareToUpdate(paint_rect,
                             tiler_->tile_size(),
                             1.f / width_scale,
                             1.f / height_scale,
                             &painted_opaque_rect);

  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      UpdatableTile* tile = TileAt(i, j);
      DCHECK(tile);  // Did SetTexturePriorites get skipped?
                     // TODO(enne): This should not ever be null.
      if (!tile)
        continue;

      gfx::Rect tile_rect = tiler_->tile_bounds(i, j);

      // Use update_rect as the above loop copied the dirty rect for this frame
      // to update_rect.
      gfx::Rect dirty_rect = tile->update_rect;
      if (dirty_rect.IsEmpty())
        continue;

      // Save what was painted opaque in the tile. Keep the old area if the
      // paint didn't touch it, and didn't paint some other part of the tile
      // opaque.
      gfx::Rect tile_painted_rect = gfx::IntersectRects(tile_rect, paint_rect);
      gfx::Rect tile_painted_opaque_rect =
          gfx::IntersectRects(tile_rect, painted_opaque_rect);
      if (!tile_painted_rect.IsEmpty()) {
        gfx::Rect paint_inside_tile_opaque_rect =
            gfx::IntersectRects(tile->opaque_rect(), tile_painted_rect);
        bool paint_inside_tile_opaque_rect_is_non_opaque =
            !paint_inside_tile_opaque_rect.IsEmpty() &&
            !tile_painted_opaque_rect.Contains(paint_inside_tile_opaque_rect);
        bool opaque_paint_not_inside_tile_opaque_rect =
            !tile_painted_opaque_rect.IsEmpty() &&
            !tile->opaque_rect().Contains(tile_painted_opaque_rect);

        if (paint_inside_tile_opaque_rect_is_non_opaque ||
            opaque_paint_not_inside_tile_opaque_rect)
          tile->set_opaque_rect(tile_painted_opaque_rect);
      }

      // source_rect starts as a full-sized tile with border texels included.
      gfx::Rect source_rect = tiler_->TileRect(tile);
      source_rect.Intersect(dirty_rect);
      // Paint rect not guaranteed to line up on tile boundaries, so
      // make sure that source_rect doesn't extend outside of it.
      source_rect.Intersect(paint_rect);

      tile->update_rect = source_rect;

      if (source_rect.IsEmpty())
        continue;

      const gfx::Point anchor = tiler_->TileRect(tile).origin();

      // Calculate tile-space rectangle to upload into.
      gfx::Vector2d dest_offset = source_rect.origin() - anchor;
      CHECK_GE(dest_offset.x(), 0);
      CHECK_GE(dest_offset.y(), 0);

      // Offset from paint rectangle to this tile's dirty rectangle.
      gfx::Vector2d paint_offset = source_rect.origin() - paint_rect.origin();
      CHECK_GE(paint_offset.x(), 0);
      CHECK_GE(paint_offset.y(), 0);
      CHECK_LE(paint_offset.x() + source_rect.width(), paint_rect.width());
      CHECK_LE(paint_offset.y() + source_rect.height(), paint_rect.height());

      tile->updater_resource()->Update(
          queue, source_rect, dest_offset, tile->partial_update);
    }
  }
}

// This picks a small animated layer to be anything less than one viewport. This
// is specifically for page transitions which are viewport-sized layers. The
// extra tile of padding is due to these layers being slightly larger than the
// viewport in some cases.
bool TiledLayer::IsSmallAnimatedLayer() const {
  if (!draw_transform_is_animating() && !screen_space_transform_is_animating())
    return false;
  gfx::Size viewport_size =
      layer_tree_host() ? layer_tree_host()->device_viewport_size()
                        : gfx::Size();
  gfx::Rect content_rect(content_bounds());
  return content_rect.width() <=
         viewport_size.width() + tiler_->tile_size().width() &&
         content_rect.height() <=
         viewport_size.height() + tiler_->tile_size().height();
}

namespace {
// TODO(epenner): Remove this and make this based on distance once distance can
// be calculated for offscreen layers. For now, prioritize all small animated
// layers after 512 pixels of pre-painting.
void SetPriorityForTexture(const gfx::Rect& visible_rect,
                           const gfx::Rect& tile_rect,
                           bool draws_to_root,
                           bool is_small_animated_layer,
                           PrioritizedResource* texture) {
  int priority = PriorityCalculator::LowestPriority();
  if (!visible_rect.IsEmpty()) {
    priority = PriorityCalculator::PriorityFromDistance(
        visible_rect, tile_rect, draws_to_root);
  }

  if (is_small_animated_layer) {
    priority = PriorityCalculator::max_priority(
        priority, PriorityCalculator::SmallAnimatedLayerMinPriority());
  }

  if (priority != PriorityCalculator::LowestPriority())
    texture->set_request_priority(priority);
}
}  // namespace

void TiledLayer::SetTexturePriorities(const PriorityCalculator& priority_calc) {
  UpdateBounds();
  ResetUpdateState();
  UpdateScrollPrediction();

  if (tiler_->has_empty_bounds())
    return;

  bool draws_to_root = !render_target()->parent();
  bool small_animated_layer = IsSmallAnimatedLayer();

  // Minimally create the tiles in the desired pre-paint rect.
  gfx::Rect create_tiles_rect = IdlePaintRect();
  if (small_animated_layer)
    create_tiles_rect = gfx::Rect(content_bounds());
  if (!create_tiles_rect.IsEmpty()) {
    int left, top, right, bottom;
    tiler_->ContentRectToTileIndices(
        create_tiles_rect, &left, &top, &right, &bottom);
    for (int j = top; j <= bottom; ++j) {
      for (int i = left; i <= right; ++i) {
        if (!TileAt(i, j))
          CreateTile(i, j);
      }
    }
  }

  // Now update priorities on all tiles we have in the layer, no matter where
  // they are.
  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
    // TODO(enne): This should not ever be null.
    if (!tile)
      continue;
    gfx::Rect tile_rect = tiler_->TileRect(tile);
    SetPriorityForTexture(predicted_visible_rect_,
                          tile_rect,
                          draws_to_root,
                          small_animated_layer,
                          tile->managed_resource());
  }
}

Region TiledLayer::VisibleContentOpaqueRegion() const {
  if (skips_draw_)
    return Region();
  if (contents_opaque())
    return visible_content_rect();
  return tiler_->OpaqueRegionInContentRect(visible_content_rect());
}

void TiledLayer::ResetUpdateState() {
  skips_draw_ = false;
  failed_update_ = false;

  LayerTilingData::TileMap::const_iterator end = tiler_->tiles().end();
  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != end;
       ++iter) {
    UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
    // TODO(enne): This should not ever be null.
    if (!tile)
      continue;
    tile->ResetUpdateState();
  }
}

namespace {
gfx::Rect ExpandRectByDelta(const gfx::Rect& rect, const gfx::Vector2d& delta) {
  int width = rect.width() + std::abs(delta.x());
  int height = rect.height() + std::abs(delta.y());
  int x = rect.x() + ((delta.x() < 0) ? delta.x() : 0);
  int y = rect.y() + ((delta.y() < 0) ? delta.y() : 0);
  return gfx::Rect(x, y, width, height);
}
}

void TiledLayer::UpdateScrollPrediction() {
  // This scroll prediction is very primitive and should be replaced by a
  // a recursive calculation on all layers which uses actual scroll/animation
  // velocities. To insure this doesn't miss-predict, we only use it to predict
  // the visible_rect if:
  // - content_bounds() hasn't changed.
  // - visible_rect.size() hasn't changed.
  // These two conditions prevent rotations, scales, pinch-zooms etc. where
  // the prediction would be incorrect.
  gfx::Vector2d delta = visible_content_rect().CenterPoint() -
                        previous_visible_rect_.CenterPoint();
  predicted_scroll_ = -delta;
  predicted_visible_rect_ = visible_content_rect();
  if (previous_content_bounds_ == content_bounds() &&
      previous_visible_rect_.size() == visible_content_rect().size()) {
    // Only expand the visible rect in the major scroll direction, to prevent
    // massive paints due to diagonal scrolls.
    gfx::Vector2d major_scroll_delta =
        (std::abs(delta.x()) > std::abs(delta.y())) ?
        gfx::Vector2d(delta.x(), 0) :
        gfx::Vector2d(0, delta.y());
    predicted_visible_rect_ =
        ExpandRectByDelta(visible_content_rect(), major_scroll_delta);

    // Bound the prediction to prevent unbounded paints, and clamp to content
    // bounds.
    gfx::Rect bound = visible_content_rect();
    bound.Inset(-tiler_->tile_size().width() * kMaxPredictiveTilesCount,
                -tiler_->tile_size().height() * kMaxPredictiveTilesCount);
    bound.Intersect(gfx::Rect(content_bounds()));
    predicted_visible_rect_.Intersect(bound);
  }
  previous_content_bounds_ = content_bounds();
  previous_visible_rect_ = visible_content_rect();
}

bool TiledLayer::Update(ResourceUpdateQueue* queue,
                        const OcclusionTracker<Layer>* occlusion) {
  DCHECK(!skips_draw_ && !failed_update_);  // Did ResetUpdateState get skipped?

  // Tiled layer always causes commits to wait for activation, as it does
  // not support pending trees.
  SetNextCommitWaitsForActivation();

  bool updated = false;

  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);

    updated |= ContentsScalingLayer::Update(queue, occlusion);
    UpdateBounds();
  }

  if (tiler_->has_empty_bounds() || !DrawsContent())
    return false;

  // Animation pre-paint. If the layer is small, try to paint it all
  // immediately whether or not it is occluded, to avoid paint/upload
  // hiccups while it is animating.
  if (IsSmallAnimatedLayer()) {
    int left, top, right, bottom;
    tiler_->ContentRectToTileIndices(gfx::Rect(content_bounds()),
                                     &left,
                                     &top,
                                     &right,
                                     &bottom);
    UpdateTiles(left, top, right, bottom, queue, NULL, &updated);
    if (updated)
      return updated;
    // This was an attempt to paint the entire layer so if we fail it's okay,
    // just fallback on painting visible etc. below.
    failed_update_ = false;
  }

  if (predicted_visible_rect_.IsEmpty())
    return updated;

  // Visible painting. First occlude visible tiles and paint the non-occluded
  // tiles.
  int left, top, right, bottom;
  tiler_->ContentRectToTileIndices(
      predicted_visible_rect_, &left, &top, &right, &bottom);
  MarkOcclusionsAndRequestTextures(left, top, right, bottom, occlusion);
  skips_draw_ = !UpdateTiles(
      left, top, right, bottom, queue, occlusion, &updated);
  if (skips_draw_)
    tiler_->reset();
  if (skips_draw_ || updated)
    return true;

  // If we have already painting everything visible. Do some pre-painting while
  // idle.
  gfx::Rect idle_paint_content_rect = IdlePaintRect();
  if (idle_paint_content_rect.IsEmpty())
    return updated;

  // Prepaint anything that was occluded but inside the layer's visible region.
  if (!UpdateTiles(left, top, right, bottom, queue, NULL, &updated) ||
      updated)
    return updated;

  int prepaint_left, prepaint_top, prepaint_right, prepaint_bottom;
  tiler_->ContentRectToTileIndices(idle_paint_content_rect,
                                   &prepaint_left,
                                   &prepaint_top,
                                   &prepaint_right,
                                   &prepaint_bottom);

  // Then expand outwards one row/column at a time until we find a dirty
  // row/column to update. Increment along the major and minor scroll directions
  // first.
  gfx::Vector2d delta = -predicted_scroll_;
  delta = gfx::Vector2d(delta.x() == 0 ? 1 : delta.x(),
                        delta.y() == 0 ? 1 : delta.y());
  gfx::Vector2d major_delta =
      (std::abs(delta.x()) > std::abs(delta.y())) ? gfx::Vector2d(delta.x(), 0)
                                        : gfx::Vector2d(0, delta.y());
  gfx::Vector2d minor_delta =
      (std::abs(delta.x()) <= std::abs(delta.y())) ? gfx::Vector2d(delta.x(), 0)
                                         : gfx::Vector2d(0, delta.y());
  gfx::Vector2d deltas[4] = { major_delta, minor_delta, -major_delta,
                              -minor_delta };
  for (int i = 0; i < 4; i++) {
    if (deltas[i].y() > 0) {
      while (bottom < prepaint_bottom) {
        ++bottom;
        if (!UpdateTiles(
                left, bottom, right, bottom, queue, NULL, &updated) ||
            updated)
          return updated;
      }
    }
    if (deltas[i].y() < 0) {
      while (top > prepaint_top) {
        --top;
        if (!UpdateTiles(
                left, top, right, top, queue, NULL, &updated) ||
            updated)
          return updated;
      }
    }
    if (deltas[i].x() < 0) {
      while (left > prepaint_left) {
        --left;
        if (!UpdateTiles(
                left, top, left, bottom, queue, NULL, &updated) ||
            updated)
          return updated;
      }
    }
    if (deltas[i].x() > 0) {
      while (right < prepaint_right) {
        ++right;
        if (!UpdateTiles(
                right, top, right, bottom, queue, NULL, &updated) ||
            updated)
          return updated;
      }
    }
  }
  return updated;
}

void TiledLayer::OnOutputSurfaceCreated() {
  // Ensure that all textures are of the right format.
  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
    if (!tile)
      continue;
    PrioritizedResource* resource = tile->managed_resource();
    resource->SetDimensions(resource->size(), texture_format_);
  }
}

bool TiledLayer::NeedsIdlePaint() {
  // Don't trigger more paints if we failed (as we'll just fail again).
  if (failed_update_ || visible_content_rect().IsEmpty() ||
      tiler_->has_empty_bounds() || !DrawsContent())
    return false;

  gfx::Rect idle_paint_content_rect = IdlePaintRect();
  if (idle_paint_content_rect.IsEmpty())
    return false;

  int left, top, right, bottom;
  tiler_->ContentRectToTileIndices(
      idle_paint_content_rect, &left, &top, &right, &bottom);

  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      UpdatableTile* tile = TileAt(i, j);
      DCHECK(tile);  // Did SetTexturePriorities get skipped?
      if (!tile)
        continue;

      bool updated = !tile->update_rect.IsEmpty();
      bool can_acquire =
          tile->managed_resource()->can_acquire_backing_texture();
      bool dirty =
          tile->is_dirty() || !tile->managed_resource()->have_backing_texture();
      if (!updated && can_acquire && dirty)
        return true;
    }
  }
  return false;
}

gfx::Rect TiledLayer::IdlePaintRect() {
  // Don't inflate an empty rect.
  if (visible_content_rect().IsEmpty())
    return gfx::Rect();

  gfx::Rect prepaint_rect = visible_content_rect();
  prepaint_rect.Inset(-tiler_->tile_size().width() * kPrepaintColumns,
                      -tiler_->tile_size().height() * kPrepaintRows);
  gfx::Rect content_rect(content_bounds());
  prepaint_rect.Intersect(content_rect);

  return prepaint_rect;
}

}  // namespace cc
