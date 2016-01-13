// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RENDER_SURFACE_IMPL_H_
#define CC_LAYERS_RENDER_SURFACE_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_lists.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

class DamageTracker;
class DelegatedRendererLayerImpl;
class QuadSink;
class RenderPassSink;
class LayerImpl;
template <typename LayerType>
class LayerIterator;

struct AppendQuadsData;

class CC_EXPORT RenderSurfaceImpl {
 public:
  explicit RenderSurfaceImpl(LayerImpl* owning_layer);
  virtual ~RenderSurfaceImpl();

  gfx::PointF ContentRectCenter() const {
    return gfx::RectF(content_rect_).CenterPoint();
  }

  // Returns the rect that encloses the RenderSurfaceImpl including any
  // reflection.
  gfx::RectF DrawableContentRect() const;

  void SetDrawOpacity(float opacity) { draw_opacity_ = opacity; }
  float draw_opacity() const { return draw_opacity_; }

  void SetNearestOcclusionImmuneAncestor(RenderSurfaceImpl* surface) {
    nearest_occlusion_immune_ancestor_ = surface;
  }
  const RenderSurfaceImpl* nearest_occlusion_immune_ancestor() const {
    return nearest_occlusion_immune_ancestor_;
  }

  void SetDrawOpacityIsAnimating(bool draw_opacity_is_animating) {
    draw_opacity_is_animating_ = draw_opacity_is_animating;
  }
  bool draw_opacity_is_animating() const { return draw_opacity_is_animating_; }

  void SetDrawTransform(const gfx::Transform& draw_transform) {
    draw_transform_ = draw_transform;
  }
  const gfx::Transform& draw_transform() const { return draw_transform_; }

  void SetScreenSpaceTransform(const gfx::Transform& screen_space_transform) {
    screen_space_transform_ = screen_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return screen_space_transform_;
  }

  void SetReplicaDrawTransform(const gfx::Transform& replica_draw_transform) {
    replica_draw_transform_ = replica_draw_transform;
  }
  const gfx::Transform& replica_draw_transform() const {
    return replica_draw_transform_;
  }

  void SetReplicaScreenSpaceTransform(
      const gfx::Transform& replica_screen_space_transform) {
    replica_screen_space_transform_ = replica_screen_space_transform;
  }
  const gfx::Transform& replica_screen_space_transform() const {
    return replica_screen_space_transform_;
  }

  void SetTargetSurfaceTransformsAreAnimating(bool animating) {
    target_surface_transforms_are_animating_ = animating;
  }
  bool target_surface_transforms_are_animating() const {
    return target_surface_transforms_are_animating_;
  }
  void SetScreenSpaceTransformsAreAnimating(bool animating) {
    screen_space_transforms_are_animating_ = animating;
  }
  bool screen_space_transforms_are_animating() const {
    return screen_space_transforms_are_animating_;
  }

  void SetIsClipped(bool is_clipped) { is_clipped_ = is_clipped; }
  bool is_clipped() const { return is_clipped_; }

  void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect clip_rect() const { return clip_rect_; }

  // When false, the RenderSurface does not contribute to another target
  // RenderSurface that is being drawn for the current frame. It could still be
  // drawn to as a target, but its output will not be a part of any other
  // surface.
  bool contributes_to_drawn_surface() const {
    return contributes_to_drawn_surface_;
  }
  void set_contributes_to_drawn_surface(bool contributes_to_drawn_surface) {
    contributes_to_drawn_surface_ = contributes_to_drawn_surface;
  }

  bool ContentsChanged() const;

  void SetContentRect(const gfx::Rect& content_rect);
  gfx::Rect content_rect() const { return content_rect_; }

  LayerImplList& layer_list() { return layer_list_; }
  void AddContributingDelegatedRenderPassLayer(LayerImpl* layer);
  void ClearLayerLists();

  int OwningLayerId() const;

  void ResetPropertyChangedFlag() { surface_property_changed_ = false; }
  bool SurfacePropertyChanged() const;
  bool SurfacePropertyChangedOnlyFromDescendant() const;

  DamageTracker* damage_tracker() const { return damage_tracker_.get(); }

  RenderPass::Id RenderPassId();

  void AppendRenderPasses(RenderPassSink* pass_sink);
  void AppendQuads(QuadSink* quad_sink,
                   AppendQuadsData* append_quads_data,
                   bool for_replica,
                   RenderPass::Id render_pass_id);

 private:
  LayerImpl* owning_layer_;

  // Uses this surface's space.
  gfx::Rect content_rect_;
  bool surface_property_changed_ : 1;
  bool draw_opacity_is_animating_ : 1;
  bool target_surface_transforms_are_animating_ : 1;
  bool screen_space_transforms_are_animating_ : 1;

  bool is_clipped_ : 1;
  bool contributes_to_drawn_surface_ : 1;

  float draw_opacity_;
  gfx::Transform draw_transform_;
  gfx::Transform screen_space_transform_;
  gfx::Transform replica_draw_transform_;
  gfx::Transform replica_screen_space_transform_;

  // Uses the space of the surface's target surface.
  gfx::Rect clip_rect_;

  LayerImplList layer_list_;
  std::vector<DelegatedRendererLayerImpl*>
      contributing_delegated_render_pass_layer_list_;

  // The nearest ancestor target surface that will contain the contents of this
  // surface, and that ignores outside occlusion. This can point to itself.
  RenderSurfaceImpl* nearest_occlusion_immune_ancestor_;

  scoped_ptr<DamageTracker> damage_tracker_;

  // For LayerIteratorActions
  int target_render_surface_layer_index_history_;
  int current_layer_index_history_;

  friend class LayerIterator<LayerImpl>;

  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceImpl);
};

}  // namespace cc
#endif  // CC_LAYERS_RENDER_SURFACE_IMPL_H_
