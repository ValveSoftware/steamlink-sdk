// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

ScrollbarLayerImplBase::ScrollbarLayerImplBase(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar,
    bool is_overlay)
    : LayerImpl(tree_impl, id),
      scroll_layer_id_(Layer::INVALID_ID),
      is_overlay_scrollbar_(is_overlay),
      thumb_thickness_scale_factor_(1.f),
      current_pos_(0.f),
      clip_layer_length_(0.f),
      scroll_layer_length_(0.f),
      orientation_(orientation),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar),
      vertical_adjust_(0.f) {}

ScrollbarLayerImplBase::~ScrollbarLayerImplBase() {
  layer_tree_impl()->UnregisterScrollbar(this);
}

void ScrollbarLayerImplBase::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  DCHECK(layer->ToScrollbarLayer());
  layer->ToScrollbarLayer()->set_is_overlay_scrollbar(is_overlay_scrollbar_);
  layer->ToScrollbarLayer()->SetScrollLayerId(ScrollLayerId());
}

ScrollbarLayerImplBase* ScrollbarLayerImplBase::ToScrollbarLayer() {
  return this;
}

void ScrollbarLayerImplBase::SetScrollLayerId(int scroll_layer_id) {
  if (scroll_layer_id_ == scroll_layer_id)
    return;

  layer_tree_impl()->UnregisterScrollbar(this);

  scroll_layer_id_ = scroll_layer_id;

  layer_tree_impl()->RegisterScrollbar(this);
}

bool ScrollbarLayerImplBase::SetCurrentPos(float current_pos) {
  if (current_pos_ == current_pos)
    return false;
  current_pos_ = current_pos;
  NoteLayerPropertyChanged();
  return true;
}

bool ScrollbarLayerImplBase::CanScrollOrientation() const {
  LayerImpl* scroll_layer = layer_tree_impl()->LayerById(scroll_layer_id_);
  if (!scroll_layer)
    return false;
  return scroll_layer->user_scrollable(orientation()) &&
         clip_layer_length_ < scroll_layer_length_;
}

bool ScrollbarLayerImplBase::SetVerticalAdjust(float vertical_adjust) {
  if (vertical_adjust_ == vertical_adjust)
    return false;
  vertical_adjust_ = vertical_adjust;
  NoteLayerPropertyChanged();
  return true;
}

bool ScrollbarLayerImplBase::SetClipLayerLength(float clip_layer_length) {
  if (clip_layer_length_ == clip_layer_length)
    return false;
  clip_layer_length_ = clip_layer_length;
  NoteLayerPropertyChanged();
  return true;
}

bool ScrollbarLayerImplBase::SetScrollLayerLength(float scroll_layer_length) {
  if (scroll_layer_length_ == scroll_layer_length)
    return false;
  scroll_layer_length_ = scroll_layer_length;
  NoteLayerPropertyChanged();
  return true;
}

bool ScrollbarLayerImplBase::SetThumbThicknessScaleFactor(float factor) {
  if (thumb_thickness_scale_factor_ == factor)
    return false;
  thumb_thickness_scale_factor_ = factor;
  NoteLayerPropertyChanged();
  return true;
}

gfx::Rect ScrollbarLayerImplBase::ComputeThumbQuadRect() const {
  // Thumb extent is the length of the thumb in the scrolling direction, thumb
  // thickness is in the perpendicular direction. Here's an example of a
  // horizontal scrollbar - inputs are above the scrollbar, computed values
  // below:
  //
  //    |<------------------- track_length_ ------------------->|
  //
  // |--| <-- start_offset
  //
  // +--+----------------------------+------------------+-------+--+
  // |<||                            |##################|       ||>|
  // +--+----------------------------+------------------+-------+--+
  //
  //                                 |<- thumb_length ->|
  //
  // |<------- thumb_offset -------->|
  //
  // For painted, scrollbars, the length is fixed. For solid color scrollbars we
  // have to compute it. The ratio of the thumb's length to the track's length
  // is the same as that of the visible viewport to the total viewport, unless
  // that would make the thumb's length less than its thickness.
  //
  // vertical_adjust_ is used when the layer geometry from the main thread is
  // not in sync with what the user sees. For instance on Android scrolling the
  // top bar controls out of view reveals more of the page content. We want the
  // root layer scrollbars to reflect what the user sees even if we haven't
  // received new layer geometry from the main thread.  If the user has scrolled
  // down by 50px and the initial viewport size was 950px the geometry would
  // look something like this:
  //
  // vertical_adjust_ = 50, scroll position 0, visible ratios 99%
  // Layer geometry:             Desired thumb positions:
  // +--------------------+-+   +----------------------+   <-- 0px
  // |                    |v|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // |                    |t|   |                     #|
  // |                    |i|   |                     #|
  // |                    |c|   |                     #|
  // |                    |a|   |                     #|
  // |                    |l|   |                     #|
  // |                    | |   |                     #|
  // |                    |l|   |                     #|
  // |                    |a|   |                     #|
  // |                    |y|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // +--------------------+-+   |                     #|
  // | horizontal  layer  | |   |                     #|
  // +--------------------+-+   |                     #|  <-- 950px
  // |                      |   |                     #|
  // |                      |   |##################### |
  // +----------------------+   +----------------------+  <-- 1000px
  //
  // The layer geometry is set up for a 950px tall viewport, but the user can
  // actually see down to 1000px. Thus we have to move the quad for the
  // horizontal scrollbar down by the vertical_adjust_ factor and lay the
  // vertical thumb out on a track lengthed by the vertical_adjust_ factor. This
  // means the quads may extend outside the layer's bounds.

  // With the length known, we can compute the thumb's position.
  float track_length = TrackLength();
  int thumb_length = ThumbLength();
  int thumb_thickness = ThumbThickness();
  float maximum = scroll_layer_length_ - clip_layer_length_;

  // With the length known, we can compute the thumb's position.
  float clamped_current_pos = std::min(std::max(current_pos_, 0.f), maximum);

  int thumb_offset = TrackStart();
  if (maximum > 0) {
    float ratio = clamped_current_pos / maximum;
    float max_offset = track_length - thumb_length;
    thumb_offset += static_cast<int>(ratio * max_offset);
  }

  float thumb_thickness_adjustment =
      thumb_thickness * (1.f - thumb_thickness_scale_factor_);

  gfx::RectF thumb_rect;
  if (orientation_ == HORIZONTAL) {
    thumb_rect = gfx::RectF(thumb_offset,
                            vertical_adjust_ + thumb_thickness_adjustment,
                            thumb_length,
                            thumb_thickness - thumb_thickness_adjustment);
  } else {
    thumb_rect = gfx::RectF(
        is_left_side_vertical_scrollbar_
            ? bounds().width() - thumb_thickness
            : thumb_thickness_adjustment,
        thumb_offset,
        thumb_thickness - thumb_thickness_adjustment,
        thumb_length);
  }

  return gfx::ToEnclosingRect(thumb_rect);
}

}  // namespace cc
