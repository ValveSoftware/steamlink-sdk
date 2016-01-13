// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
    : client_(client),
      pile_(make_scoped_refptr(new PicturePile())),
      instrumentation_object_tracker_(id()),
      is_mask_(false),
      update_source_frame_number_(-1),
      can_use_lcd_text_last_frame_(can_use_lcd_text()) {
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::DrawsContent() const {
  return Layer::DrawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  if (layer_impl->bounds().IsEmpty()) {
    // Update may not get called for an empty layer, so resize here instead.
    // Using layer_impl because either bounds() or paint_properties().bounds
    // may disagree and either one could have been pushed to layer_impl.
    pile_->SetTilingRect(gfx::Rect());
  } else if (update_source_frame_number_ ==
             layer_tree_host()->source_frame_number()) {
    // TODO(ernstm): This DCHECK is only valid as long as the pile's tiling_rect
    // is identical to the layer_rect.
    // If update called, then pile size must match bounds pushed to impl layer.
    DCHECK_EQ(layer_impl->bounds().ToString(),
              pile_->tiling_rect().size().ToString());
  }

  layer_impl->SetIsMask(is_mask_);

  // Unlike other properties, invalidation must always be set on layer_impl.
  // See PictureLayerImpl::PushPropertiesTo for more details.
  layer_impl->invalidation_.Clear();
  layer_impl->invalidation_.Swap(&pile_invalidation_);
  layer_impl->pile_ = PicturePileImpl::CreateFromOther(pile_.get());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);
  if (host) {
    pile_->SetMinContentsScale(host->settings().minimum_contents_scale);
    pile_->SetTileGridSize(host->settings().default_tile_size);
    pile_->set_slow_down_raster_scale_factor(
        host->debug_state().slow_down_raster_scale_factor);
    pile_->set_show_debug_picture_borders(
        host->debug_state().show_picture_borders);
  }
}

void PictureLayer::SetNeedsDisplayRect(const gfx::RectF& layer_rect) {
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  if (!rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    rect.Intersect(gfx::Rect(bounds()));
    pending_invalidation_.Union(rect);
  }
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker<Layer>* occlusion) {
  update_source_frame_number_ = layer_tree_host()->source_frame_number();
  bool updated = Layer::Update(queue, occlusion);

  UpdateCanUseLCDText();

  gfx::Rect visible_layer_rect = gfx::ScaleToEnclosingRect(
      visible_content_rect(), 1.f / contents_scale_x());

  gfx::Rect layer_rect = gfx::Rect(paint_properties().bounds);

  if (last_updated_visible_content_rect_ == visible_content_rect() &&
      pile_->tiling_rect() == layer_rect && pending_invalidation_.IsEmpty()) {
    // Only early out if the visible content rect of this layer hasn't changed.
    return updated;
  }

  TRACE_EVENT1("cc", "PictureLayer::Update",
               "source_frame_number",
               layer_tree_host()->source_frame_number());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->id());

  pile_->SetTilingRect(layer_rect);

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pending_invalidation_.Swap(&pile_invalidation_);
  pending_invalidation_.Clear();

  if (layer_tree_host()->settings().record_full_layer) {
    // Workaround for http://crbug.com/235910 - to retain backwards compat
    // the full page content must always be provided in the picture layer.
    visible_layer_rect = gfx::Rect(bounds());
  }

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(client_);
  updated |=
      pile_->UpdateAndExpandInvalidation(client_,
                                         &pile_invalidation_,
                                         SafeOpaqueBackgroundColor(),
                                         contents_opaque(),
                                         client_->FillsBoundsCompletely(),
                                         visible_layer_rect,
                                         update_source_frame_number_,
                                         RecordingMode(),
                                         rendering_stats_instrumentation());
  last_updated_visible_content_rect_ = visible_content_rect();

  if (updated) {
    SetNeedsPushProperties();
  } else {
    // If this invalidation did not affect the pile, then it can be cleared as
    // an optimization.
    pile_invalidation_.Clear();
  }

  return updated;
}

void PictureLayer::SetIsMask(bool is_mask) {
  is_mask_ = is_mask;
}

Picture::RecordingMode PictureLayer::RecordingMode() const {
  switch (layer_tree_host()->settings().recording_mode) {
    case LayerTreeSettings::RecordNormally:
      return Picture::RECORD_NORMALLY;
    case LayerTreeSettings::RecordWithSkRecord:
      return Picture::RECORD_WITH_SKRECORD;
  }
  NOTREACHED();
  return Picture::RECORD_NORMALLY;
}

bool PictureLayer::SupportsLCDText() const {
  return true;
}

void PictureLayer::UpdateCanUseLCDText() {
  if (can_use_lcd_text_last_frame_ == can_use_lcd_text())
    return;

  can_use_lcd_text_last_frame_ = can_use_lcd_text();
  if (client_)
    client_->DidChangeLayerCanUseLCDText();
}

skia::RefPtr<SkPicture> PictureLayer::GetPicture() const {
  // We could either flatten the PicturePile into a single SkPicture,
  // or paint a fresh one depending on what we intend to do with the
  // picture. For now we just paint a fresh one to get consistent results.
  if (!DrawsContent())
    return skia::RefPtr<SkPicture>();

  int width = bounds().width();
  int height = bounds().height();
  gfx::RectF opaque;

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(width, height, NULL, 0);
  client_->PaintContents(canvas,
                         gfx::Rect(width, height),
                         &opaque,
                         ContentLayerClient::GRAPHICS_CONTEXT_ENABLED);
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());
  return picture;
}

bool PictureLayer::IsSuitableForGpuRasterization() const {
  return pile_->is_suitable_for_gpu_rasterization();
}

void PictureLayer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

}  // namespace cc
