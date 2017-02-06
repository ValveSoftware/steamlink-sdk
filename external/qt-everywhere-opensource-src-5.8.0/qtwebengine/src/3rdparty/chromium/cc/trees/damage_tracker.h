// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DAMAGE_TRACKER_H_
#define CC_TREES_DAMAGE_TRACKER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_collections.h"
#include "ui/gfx/geometry/rect.h"

class SkImageFilter;

namespace gfx {
class Rect;
}

namespace cc {

class FilterOperations;
class LayerImpl;
class RenderSurfaceImpl;

// Computes the region where pixels have actually changed on a
// RenderSurfaceImpl. This region is used to scissor what is actually drawn to
// the screen to save GPU computation and bandwidth.
class CC_EXPORT DamageTracker {
 public:
  static std::unique_ptr<DamageTracker> Create();
  ~DamageTracker();

  void DidDrawDamagedArea() { current_damage_rect_ = gfx::Rect(); }
  void AddDamageNextUpdate(const gfx::Rect& dmg) {
    current_damage_rect_.Union(dmg);
  }
  void UpdateDamageTrackingState(
      const LayerImplList& layer_list,
      const RenderSurfaceImpl* target_surface,
      bool target_surface_property_changed_only_from_descendant,
      const gfx::Rect& target_surface_content_rect,
      LayerImpl* target_surface_mask_layer,
      const FilterOperations& filters);

  gfx::Rect current_damage_rect() { return current_damage_rect_; }

 private:
  DamageTracker();

  gfx::Rect TrackDamageFromActiveLayers(
      const LayerImplList& layer_list,
      const RenderSurfaceImpl* target_surface);
  gfx::Rect TrackDamageFromSurfaceMask(LayerImpl* target_surface_mask_layer);
  gfx::Rect TrackDamageFromLeftoverRects();

  void PrepareRectHistoryForUpdate();

  // These helper functions are used only in TrackDamageFromActiveLayers().
  void ExtendDamageForLayer(LayerImpl* layer, gfx::Rect* target_damage_rect);
  void ExtendDamageForRenderSurface(RenderSurfaceImpl* render_surface,
                                    gfx::Rect* target_damage_rect);

  struct LayerRectMapData {
    LayerRectMapData() : layer_id_(0), mailboxId_(0) {}
    explicit LayerRectMapData(int layer_id)
        : layer_id_(layer_id), mailboxId_(0) {}
    void Update(const gfx::Rect& rect, unsigned int mailboxId) {
      mailboxId_ = mailboxId;
      rect_ = rect;
    }

    bool operator<(const LayerRectMapData& other) const {
      return layer_id_ < other.layer_id_;
    }

    int layer_id_;
    unsigned int mailboxId_;
    gfx::Rect rect_;
  };

  struct SurfaceRectMapData {
    SurfaceRectMapData() : surface_id_(0), mailboxId_(0) {}
    explicit SurfaceRectMapData(int surface_id)
        : surface_id_(surface_id), mailboxId_(0) {}
    void Update(const gfx::Rect& rect, unsigned int mailboxId) {
      mailboxId_ = mailboxId;
      rect_ = rect;
    }

    bool operator<(const SurfaceRectMapData& other) const {
      return surface_id_ < other.surface_id_;
    }

    int surface_id_;
    unsigned int mailboxId_;
    gfx::Rect rect_;
  };
  typedef std::vector<LayerRectMapData> SortedRectMapForLayers;
  typedef std::vector<SurfaceRectMapData> SortedRectMapForSurfaces;

  LayerRectMapData& RectDataForLayer(int layer_id, bool* layer_is_new);
  SurfaceRectMapData& RectDataForSurface(int layer_id, bool* layer_is_new);

  SortedRectMapForLayers rect_history_for_layers_;
  SortedRectMapForSurfaces rect_history_for_surfaces_;

  unsigned int mailboxId_;
  gfx::Rect current_damage_rect_;

  DISALLOW_COPY_AND_ASSIGN(DamageTracker);
};

}  // namespace cc

#endif  // CC_TREES_DAMAGE_TRACKER_H_
