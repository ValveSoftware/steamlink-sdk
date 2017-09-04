// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace views {
namespace {

// Total thickness of the thumb (matches visuals when hovered).
const int kThumbThickness = 11;
// When hovered, the thumb takes up the full width. Otherwise, it's a bit
// slimmer.
const int kThumbHoverOffset = 4;
const int kThumbStroke = 1;
const float kThumbHoverAlpha = 0.5f;
const float kThumbDefaultAlpha = 0.3f;

}  // namespace

OverlayScrollBar::Thumb::Thumb(OverlayScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {
  // |scroll_bar| isn't done being constructed; it's not safe to do anything
  // that might reference it yet.
}

OverlayScrollBar::Thumb::~Thumb() {}

void OverlayScrollBar::Thumb::Init() {
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  // Animate all changes to the layer except the first one.
  OnStateChanged();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size OverlayScrollBar::Thumb::GetPreferredSize() const {
  // The visual size of the thumb is kThumbThickness, but it slides back and
  // forth by kThumbHoverOffset. To make event targetting work well, expand the
  // width of the thumb such that it's always taking up the full width of the
  // track regardless of the offset.
  return gfx::Size(kThumbThickness + kThumbHoverOffset,
                   kThumbThickness + kThumbHoverOffset);
}

void OverlayScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  SkPaint fill_paint;
  fill_paint.setStyle(SkPaint::kFill_Style);
  fill_paint.setColor(SK_ColorBLACK);
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF(IsHorizontal() ? kThumbHoverOffset : 0,
                                 IsHorizontal() ? 0 : kThumbHoverOffset, 0, 0));
  fill_bounds.Inset(gfx::InsetsF(kThumbStroke, kThumbStroke,
                                 IsHorizontal() ? 0 : kThumbStroke,
                                 IsHorizontal() ? kThumbStroke : 0));
  canvas->DrawRect(fill_bounds, fill_paint);

  SkPaint stroke_paint;
  stroke_paint.setStyle(SkPaint::kStroke_Style);
  stroke_paint.setColor(SK_ColorWHITE);
  stroke_paint.setStrokeWidth(kThumbStroke);
  gfx::RectF stroke_bounds(fill_bounds);
  stroke_bounds.Inset(gfx::InsetsF(kThumbStroke / 2.f));
  // The stroke doesn't apply to the far edge of the thumb.
  SkPath path;
  path.moveTo(gfx::PointFToSkPoint(stroke_bounds.top_right()));
  path.lineTo(gfx::PointFToSkPoint(stroke_bounds.origin()));
  path.lineTo(gfx::PointFToSkPoint(stroke_bounds.bottom_left()));
  if (IsHorizontal()) {
    path.moveTo(gfx::PointFToSkPoint(stroke_bounds.bottom_right()));
    path.close();
  } else {
    path.lineTo(gfx::PointFToSkPoint(stroke_bounds.bottom_right()));
  }
  canvas->DrawPath(path, stroke_paint);
}

void OverlayScrollBar::Thumb::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  scroll_bar_->Show();
  // Don't start the hide countdown if the thumb is still hovered or pressed.
  if (GetState() == CustomButton::STATE_NORMAL)
    scroll_bar_->StartHideCountdown();
}

void OverlayScrollBar::Thumb::OnStateChanged() {
  if (GetState() == CustomButton::STATE_NORMAL) {
    gfx::Transform translation;
    translation.Translate(
        gfx::Vector2d(IsHorizontal() ? 0 : kThumbHoverOffset,
                      IsHorizontal() ? kThumbHoverOffset: 0));
    layer()->SetTransform(translation);
    layer()->SetOpacity(kThumbDefaultAlpha);

    if (GetWidget())
      scroll_bar_->StartHideCountdown();
  } else {
    layer()->SetTransform(gfx::Transform());
    layer()->SetOpacity(kThumbHoverAlpha);
  }
}

OverlayScrollBar::OverlayScrollBar(bool horizontal)
    : BaseScrollBar(horizontal), hide_timer_(false, false) {
  auto thumb = new Thumb(this);
  SetThumb(thumb);
  thumb->Init();
  set_notify_enter_exit_on_child(true);
  SetPaintToLayer(true);
  layer()->SetMasksToBounds(true);
  layer()->SetFillsBoundsOpaquely(false);
}

OverlayScrollBar::~OverlayScrollBar() {}

gfx::Rect OverlayScrollBar::GetTrackBounds() const {
  gfx::Rect local = GetLocalBounds();
  // The track has to be wide enough for the thumb.
  local.Inset(gfx::Insets(IsHorizontal() ? -kThumbHoverOffset : 0,
                          IsHorizontal() ? 0 : -kThumbHoverOffset, 0, 0));
  return local;
}

int OverlayScrollBar::GetLayoutSize() const {
  return 0;
}

int OverlayScrollBar::GetContentOverlapSize() const {
  return kThumbThickness;
}

void OverlayScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  Show();
}

void OverlayScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  StartHideCountdown();
}

void OverlayScrollBar::Show() {
  layer()->SetOpacity(1.0f);
  hide_timer_.Stop();
}

void OverlayScrollBar::Hide() {
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(200));
  layer()->SetOpacity(0.0f);
}

void OverlayScrollBar::StartHideCountdown() {
  if (IsMouseHovered())
    return;
  hide_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(1),
      base::Bind(&OverlayScrollBar::Hide, base::Unretained(this)));
}

void OverlayScrollBar::Layout() {
  gfx::Rect thumb_bounds = GetTrackBounds();
  BaseScrollBarThumb* thumb = GetThumb();
  if (IsHorizontal()) {
    thumb_bounds.set_x(thumb->x());
    thumb_bounds.set_width(thumb->width());
  } else {
    thumb_bounds.set_y(thumb->y());
    thumb_bounds.set_height(thumb->height());
  }
  thumb->SetBoundsRect(thumb_bounds);
}

}  // namespace views
