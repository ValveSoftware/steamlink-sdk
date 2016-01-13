// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILED_LAYER_H_
#define CC_LAYERS_TILED_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/contents_scaling_layer.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/resources/resource_format.h"

namespace cc {
class LayerUpdater;
class PrioritizedResourceManager;
class PrioritizedResource;
class UpdatableTile;

class CC_EXPORT TiledLayer : public ContentsScalingLayer {
 public:
  enum TilingOption {
    ALWAYS_TILE,
    NEVER_TILE,
    AUTO_TILE,
  };

  // Layer implementation.
  virtual void SetIsMask(bool is_mask) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual void ReduceMemoryUsage() OVERRIDE;
  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* layer_tree_host) OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual Region VisibleContentOpaqueRegion() const OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual void OnOutputSurfaceCreated() OVERRIDE;

 protected:
  TiledLayer();
  virtual ~TiledLayer();

  void UpdateTileSizeAndTilingOption();
  void UpdateBounds();

  // Exposed to subclasses for testing.
  void SetTileSize(const gfx::Size& size);
  void SetTextureFormat(ResourceFormat texture_format) {
    texture_format_ = texture_format;
  }
  void SetBorderTexelOption(LayerTilingData::BorderTexelOption option);
  size_t NumPaintedTiles() { return tiler_->tiles().size(); }

  virtual LayerUpdater* Updater() const = 0;
  virtual void CreateUpdaterIfNeeded() = 0;

  // Set invalidations to be potentially repainted during Update().
  void InvalidateContentRect(const gfx::Rect& content_rect);

  // Reset state on tiles that will be used for updating the layer.
  void ResetUpdateState();

  // After preparing an update, returns true if more painting is needed.
  bool NeedsIdlePaint();
  gfx::Rect IdlePaintRect();

  bool SkipsDraw() const { return skips_draw_; }

  // Virtual for testing
  virtual PrioritizedResourceManager* ResourceManager();
  const LayerTilingData* TilerForTesting() const { return tiler_.get(); }
  const PrioritizedResource* ResourceAtForTesting(int i, int j) const;

 private:
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  void CreateTilerIfNeeded();
  void set_tiling_option(TilingOption tiling_option) {
    tiling_option_ = tiling_option;
  }

  bool TileOnlyNeedsPartialUpdate(UpdatableTile* tile);
  bool TileNeedsBufferedUpdate(UpdatableTile* tile);

  void MarkOcclusionsAndRequestTextures(
      int left,
      int top,
      int right,
      int bottom,
      const OcclusionTracker<Layer>* occlusion);

  bool UpdateTiles(int left,
                   int top,
                   int right,
                   int bottom,
                   ResourceUpdateQueue* queue,
                   const OcclusionTracker<Layer>* occlusion,
                   bool* did_paint);
  bool HaveTexturesForTiles(int left,
                            int top,
                            int right,
                            int bottom,
                            bool ignore_occlusions);
  void MarkTilesForUpdate(gfx::Rect* update_rect,
                          gfx::Rect* paint_rect,
                          int left,
                          int top,
                          int right,
                          int bottom,
                          bool ignore_occlusions);
  void UpdateTileTextures(const gfx::Rect& update_rect,
                          const gfx::Rect& paint_rect,
                          int left,
                          int top,
                          int right,
                          int bottom,
                          ResourceUpdateQueue* queue,
                          const OcclusionTracker<Layer>* occlusion);
  void UpdateScrollPrediction();

  UpdatableTile* TileAt(int i, int j) const;
  UpdatableTile* CreateTile(int i, int j);

  bool IsSmallAnimatedLayer() const;

  ResourceFormat texture_format_;
  bool skips_draw_;
  bool failed_update_;

  // Used for predictive painting.
  gfx::Vector2d predicted_scroll_;
  gfx::Rect predicted_visible_rect_;
  gfx::Rect previous_visible_rect_;
  gfx::Size previous_content_bounds_;

  TilingOption tiling_option_;
  scoped_ptr<LayerTilingData> tiler_;

  DISALLOW_COPY_AND_ASSIGN(TiledLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_TILED_LAYER_H_
