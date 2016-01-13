// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/render_surface_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/trees/damage_tracker.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

RenderSurfaceImpl::RenderSurfaceImpl(LayerImpl* owning_layer)
    : owning_layer_(owning_layer),
      surface_property_changed_(false),
      draw_opacity_is_animating_(false),
      target_surface_transforms_are_animating_(false),
      screen_space_transforms_are_animating_(false),
      is_clipped_(false),
      contributes_to_drawn_surface_(false),
      draw_opacity_(1),
      nearest_occlusion_immune_ancestor_(NULL),
      target_render_surface_layer_index_history_(0),
      current_layer_index_history_(0) {
  damage_tracker_ = DamageTracker::Create();
}

RenderSurfaceImpl::~RenderSurfaceImpl() {}

gfx::RectF RenderSurfaceImpl::DrawableContentRect() const {
  gfx::RectF drawable_content_rect =
      MathUtil::MapClippedRect(draw_transform_, content_rect_);
  if (owning_layer_->has_replica()) {
    drawable_content_rect.Union(
        MathUtil::MapClippedRect(replica_draw_transform_, content_rect_));
  }

  return drawable_content_rect;
}

int RenderSurfaceImpl::OwningLayerId() const {
  return owning_layer_ ? owning_layer_->id() : 0;
}


void RenderSurfaceImpl::SetClipRect(const gfx::Rect& clip_rect) {
  if (clip_rect_ == clip_rect)
    return;

  surface_property_changed_ = true;
  clip_rect_ = clip_rect;
}

bool RenderSurfaceImpl::ContentsChanged() const {
  return !damage_tracker_->current_damage_rect().IsEmpty();
}

void RenderSurfaceImpl::SetContentRect(const gfx::Rect& content_rect) {
  if (content_rect_ == content_rect)
    return;

  surface_property_changed_ = true;
  content_rect_ = content_rect;
}

bool RenderSurfaceImpl::SurfacePropertyChanged() const {
  // Surface property changes are tracked as follows:
  //
  // - surface_property_changed_ is flagged when the clip_rect or content_rect
  //   change. As of now, these are the only two properties that can be affected
  //   by descendant layers.
  //
  // - all other property changes come from the owning layer (or some ancestor
  //   layer that propagates its change to the owning layer).
  //
  DCHECK(owning_layer_);
  return surface_property_changed_ || owning_layer_->LayerPropertyChanged();
}

bool RenderSurfaceImpl::SurfacePropertyChangedOnlyFromDescendant() const {
  return surface_property_changed_ && !owning_layer_->LayerPropertyChanged();
}

void RenderSurfaceImpl::AddContributingDelegatedRenderPassLayer(
    LayerImpl* layer) {
  DCHECK(std::find(layer_list_.begin(), layer_list_.end(), layer) !=
         layer_list_.end());
  DelegatedRendererLayerImpl* delegated_renderer_layer =
      static_cast<DelegatedRendererLayerImpl*>(layer);
  contributing_delegated_render_pass_layer_list_.push_back(
      delegated_renderer_layer);
}

void RenderSurfaceImpl::ClearLayerLists() {
  layer_list_.clear();
  contributing_delegated_render_pass_layer_list_.clear();
}

RenderPass::Id RenderSurfaceImpl::RenderPassId() {
  int layer_id = owning_layer_->id();
  int sub_id = 0;
  DCHECK_GT(layer_id, 0);
  return RenderPass::Id(layer_id, sub_id);
}

void RenderSurfaceImpl::AppendRenderPasses(RenderPassSink* pass_sink) {
  for (size_t i = 0;
       i < contributing_delegated_render_pass_layer_list_.size();
       ++i) {
    DelegatedRendererLayerImpl* delegated_renderer_layer =
        contributing_delegated_render_pass_layer_list_[i];
    delegated_renderer_layer->AppendContributingRenderPasses(pass_sink);
  }

  scoped_ptr<RenderPass> pass = RenderPass::Create(layer_list_.size());
  pass->SetNew(RenderPassId(),
               content_rect_,
               gfx::IntersectRects(content_rect_,
                                   damage_tracker_->current_damage_rect()),
               screen_space_transform_);
  pass_sink->AppendRenderPass(pass.Pass());
}

void RenderSurfaceImpl::AppendQuads(QuadSink* quad_sink,
                                    AppendQuadsData* append_quads_data,
                                    bool for_replica,
                                    RenderPass::Id render_pass_id) {
  DCHECK(!for_replica || owning_layer_->has_replica());

  const gfx::Transform& draw_transform =
      for_replica ? replica_draw_transform_ : draw_transform_;
  gfx::Rect visible_content_rect =
      quad_sink->UnoccludedContributingSurfaceContentRect(content_rect_,
                                                          draw_transform);
  if (visible_content_rect.IsEmpty())
    return;

  SharedQuadState* shared_quad_state = quad_sink->CreateSharedQuadState();
  shared_quad_state->SetAll(draw_transform,
                            content_rect_.size(),
                            content_rect_,
                            clip_rect_,
                            is_clipped_,
                            draw_opacity_,
                            owning_layer_->blend_mode(),
                            owning_layer_->sorting_context_id());

  if (owning_layer_->ShowDebugBorders()) {
    SkColor color = for_replica ?
                    DebugColors::SurfaceReplicaBorderColor() :
                    DebugColors::SurfaceBorderColor();
    float width = for_replica ?
                  DebugColors::SurfaceReplicaBorderWidth(
                      owning_layer_->layer_tree_impl()) :
                  DebugColors::SurfaceBorderWidth(
                      owning_layer_->layer_tree_impl());
    scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
        DebugBorderDrawQuad::Create();
    debug_border_quad->SetNew(
        shared_quad_state, content_rect_, visible_content_rect, color, width);
    quad_sink->Append(debug_border_quad.PassAs<DrawQuad>());
  }

  // TODO(shawnsingh): By using the same RenderSurfaceImpl for both the content
  // and its reflection, it's currently not possible to apply a separate mask to
  // the reflection layer or correctly handle opacity in reflections (opacity
  // must be applied after drawing both the layer and its reflection). The
  // solution is to introduce yet another RenderSurfaceImpl to draw the layer
  // and its reflection in. For now we only apply a separate reflection mask if
  // the contents don't have a mask of their own.
  LayerImpl* mask_layer = owning_layer_->mask_layer();
  if (mask_layer &&
      (!mask_layer->DrawsContent() || mask_layer->bounds().IsEmpty()))
    mask_layer = NULL;

  if (!mask_layer && for_replica) {
    mask_layer = owning_layer_->replica_layer()->mask_layer();
    if (mask_layer &&
        (!mask_layer->DrawsContent() || mask_layer->bounds().IsEmpty()))
      mask_layer = NULL;
  }

  gfx::RectF mask_uv_rect(0.f, 0.f, 1.f, 1.f);
  if (mask_layer) {
    gfx::Vector2dF owning_layer_draw_scale =
        MathUtil::ComputeTransform2dScaleComponents(
            owning_layer_->draw_transform(), 1.f);
    gfx::SizeF unclipped_mask_target_size = gfx::ScaleSize(
        owning_layer_->content_bounds(),
        owning_layer_draw_scale.x(),
        owning_layer_draw_scale.y());

    float uv_scale_x =
        content_rect_.width() / unclipped_mask_target_size.width();
    float uv_scale_y =
        content_rect_.height() / unclipped_mask_target_size.height();

    mask_uv_rect = gfx::RectF(
        uv_scale_x * content_rect_.x() / content_rect_.width(),
        uv_scale_y * content_rect_.y() / content_rect_.height(),
        uv_scale_x,
        uv_scale_y);
  }

  ResourceProvider::ResourceId mask_resource_id =
      mask_layer ? mask_layer->ContentsResourceId() : 0;
  gfx::Rect contents_changed_since_last_frame =
      ContentsChanged() ? content_rect_ : gfx::Rect();

  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               content_rect_,
               visible_content_rect,
               render_pass_id,
               for_replica,
               mask_resource_id,
               contents_changed_since_last_frame,
               mask_uv_rect,
               owning_layer_->filters(),
               owning_layer_->background_filters());
  quad_sink->Append(quad.PassAs<DrawQuad>());
}

}  // namespace cc
