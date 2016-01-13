// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/skia_util.h"

namespace cc {

scoped_ptr<LayerImpl> PaintedScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(
      tree_impl, id(), scrollbar_->Orientation()).PassAs<LayerImpl>();
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::Create(
    scoped_ptr<Scrollbar> scrollbar,
    int scroll_layer_id) {
  return make_scoped_refptr(
      new PaintedScrollbarLayer(scrollbar.Pass(), scroll_layer_id));
}

PaintedScrollbarLayer::PaintedScrollbarLayer(scoped_ptr<Scrollbar> scrollbar,
                                             int scroll_layer_id)
    : scrollbar_(scrollbar.Pass()),
      scroll_layer_id_(scroll_layer_id),
      clip_layer_id_(Layer::INVALID_ID),
      thumb_thickness_(scrollbar_->ThumbThickness()),
      thumb_length_(scrollbar_->ThumbLength()),
      is_overlay_(scrollbar_->IsOverlay()),
      has_thumb_(scrollbar_->HasThumb()) {
  if (!scrollbar_->IsOverlay())
    SetShouldScrollOnMainThread(true);
}

PaintedScrollbarLayer::~PaintedScrollbarLayer() {}

int PaintedScrollbarLayer::ScrollLayerId() const {
  return scroll_layer_id_;
}

void PaintedScrollbarLayer::SetScrollLayer(int layer_id) {
  if (layer_id == scroll_layer_id_)
    return;

  scroll_layer_id_ = layer_id;
  SetNeedsFullTreeSync();
}

void PaintedScrollbarLayer::SetClipLayer(int layer_id) {
  if (layer_id == clip_layer_id_)
    return;

  clip_layer_id_ = layer_id;
  SetNeedsFullTreeSync();
}

bool PaintedScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return scrollbar_->IsOverlay();
}

ScrollbarOrientation PaintedScrollbarLayer::orientation() const {
  return scrollbar_->Orientation();
}

int PaintedScrollbarLayer::MaxTextureSize() {
  DCHECK(layer_tree_host());
  return layer_tree_host()->GetRendererCapabilities().max_texture_size;
}

float PaintedScrollbarLayer::ClampScaleToMaxTextureSize(float scale) {
  // If the scaled content_bounds() is bigger than the max texture size of the
  // device, we need to clamp it by rescaling, since content_bounds() is used
  // below to set the texture size.
  gfx::Size scaled_bounds = ComputeContentBoundsForScale(scale, scale);
  if (scaled_bounds.width() > MaxTextureSize() ||
      scaled_bounds.height() > MaxTextureSize()) {
    if (scaled_bounds.width() > scaled_bounds.height())
      return (MaxTextureSize() - 1) / static_cast<float>(bounds().width());
    else
      return (MaxTextureSize() - 1) / static_cast<float>(bounds().height());
  }
  return scale;
}

void PaintedScrollbarLayer::CalculateContentsScale(
    float ideal_contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  ContentsScalingLayer::CalculateContentsScale(
      ClampScaleToMaxTextureSize(ideal_contents_scale),
      device_scale_factor,
      page_scale_factor,
      maximum_animation_contents_scale,
      animating_transform_to_screen,
      contents_scale_x,
      contents_scale_y,
      content_bounds);
}

void PaintedScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ContentsScalingLayer::PushPropertiesTo(layer);

  PushScrollClipPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  if (orientation() == HORIZONTAL) {
    scrollbar_layer->SetTrackStart(
        track_rect_.x() - location_.x());
    scrollbar_layer->SetTrackLength(track_rect_.width());
  } else {
    scrollbar_layer->SetTrackStart(
        track_rect_.y() - location_.y());
    scrollbar_layer->SetTrackLength(track_rect_.height());
  }

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  if (thumb_resource_.get())
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());

  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_);
}

ScrollbarLayerInterface* PaintedScrollbarLayer::ToScrollbarLayer() {
  return this;
}

void PaintedScrollbarLayer::PushScrollClipPropertiesTo(LayerImpl* layer) {
  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollLayerById(scroll_layer_id_);
  scrollbar_layer->SetClipLayerById(clip_layer_id_);
}

void PaintedScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_resource_.reset();
    thumb_resource_.reset();
  }

  ContentsScalingLayer::SetLayerTreeHost(host);
}

gfx::Rect PaintedScrollbarLayer::ScrollbarLayerRectToContentRect(
    const gfx::Rect& layer_rect) const {
  // Don't intersect with the bounds as in LayerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::Rect expanded_rect = gfx::ScaleToEnclosingRect(
      layer_rect, contents_scale_x(), contents_scale_y());
  // We should never return a rect bigger than the content_bounds().
  gfx::Size clamped_size = expanded_rect.size();
  clamped_size.SetToMin(content_bounds());
  expanded_rect.set_size(clamped_size);
  return expanded_rect;
}

gfx::Rect PaintedScrollbarLayer::OriginThumbRect() const {
  gfx::Size thumb_size;
  if (orientation() == HORIZONTAL) {
    thumb_size =
        gfx::Size(scrollbar_->ThumbLength(), scrollbar_->ThumbThickness());
  } else {
    thumb_size =
        gfx::Size(scrollbar_->ThumbThickness(), scrollbar_->ThumbLength());
  }
  return gfx::Rect(thumb_size);
}

void PaintedScrollbarLayer::UpdateThumbAndTrackGeometry() {
  UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  UpdateProperty(scrollbar_->Location(), &location_);
  UpdateProperty(scrollbar_->IsOverlay(), &is_overlay_);
  UpdateProperty(scrollbar_->HasThumb(), &has_thumb_);
  if (has_thumb_) {
    UpdateProperty(scrollbar_->ThumbThickness(), &thumb_thickness_);
    UpdateProperty(scrollbar_->ThumbLength(), &thumb_length_);
  }
}

bool PaintedScrollbarLayer::Update(ResourceUpdateQueue* queue,
                                   const OcclusionTracker<Layer>* occlusion) {
  UpdateThumbAndTrackGeometry();

  gfx::Rect track_layer_rect = gfx::Rect(location_, bounds());
  gfx::Rect scaled_track_rect = ScrollbarLayerRectToContentRect(
      track_layer_rect);

  if (track_rect_.IsEmpty() || scaled_track_rect.IsEmpty())
    return false;

  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);
    ContentsScalingLayer::Update(queue, occlusion);
  }

  if (update_rect_.IsEmpty() && track_resource_)
    return false;

  track_resource_ = ScopedUIResource::Create(
      layer_tree_host(),
      RasterizeScrollbarPart(track_layer_rect, scaled_track_rect, TRACK));

  gfx::Rect thumb_layer_rect = OriginThumbRect();
  gfx::Rect scaled_thumb_rect =
      ScrollbarLayerRectToContentRect(thumb_layer_rect);
  if (has_thumb_ && !scaled_thumb_rect.IsEmpty()) {
    thumb_resource_ = ScopedUIResource::Create(
        layer_tree_host(),
        RasterizeScrollbarPart(thumb_layer_rect, scaled_thumb_rect, THUMB));
  }

  // UI resources changed so push properties is needed.
  SetNeedsPushProperties();
  return true;
}

UIResourceBitmap PaintedScrollbarLayer::RasterizeScrollbarPart(
    const gfx::Rect& layer_rect,
    const gfx::Rect& content_rect,
    ScrollbarPart part) {
  DCHECK(!content_rect.size().IsEmpty());
  DCHECK(!layer_rect.size().IsEmpty());

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(content_rect.width(), content_rect.height());
  SkCanvas skcanvas(skbitmap);

  float scale_x =
      content_rect.width() / static_cast<float>(layer_rect.width());
  float scale_y =
      content_rect.height() / static_cast<float>(layer_rect.height());

  skcanvas.scale(SkFloatToScalar(scale_x),
                 SkFloatToScalar(scale_y));
  skcanvas.translate(SkFloatToScalar(-layer_rect.x()),
                     SkFloatToScalar(-layer_rect.y()));

  SkRect layer_skrect = RectToSkRect(layer_rect);
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  skcanvas.drawRect(layer_skrect, paint);
  skcanvas.clipRect(layer_skrect);

  scrollbar_->PaintPart(&skcanvas, part, layer_rect);
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  return UIResourceBitmap(skbitmap);
}

}  // namespace cc
