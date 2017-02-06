// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

namespace views {
namespace {

const int kScrollbarWidth = 10;
const int kThumbInsetInside = 3;
const int kThumbInsetFromEdge = 1;
const int kThumbCornerRadius = 2;
const int kThumbMinimumSize = kScrollbarWidth;
const int kThumbHoverAlpha = 128;
const int kThumbDefaultAlpha = 64;

class OverlayScrollBarThumb : public BaseScrollBarThumb,
                              public gfx::AnimationDelegate {
 public:
  explicit OverlayScrollBarThumb(BaseScrollBar* scroll_bar);
  ~OverlayScrollBarThumb() override;

 protected:
  // View overrides:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  double animation_opacity_;
  DISALLOW_COPY_AND_ASSIGN(OverlayScrollBarThumb);
};

OverlayScrollBarThumb::OverlayScrollBarThumb(BaseScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar),
      animation_opacity_(0.0) {
  // This is necessary, otherwise the thumb will be rendered below the views if
  // those views paint to their own layers.
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

OverlayScrollBarThumb::~OverlayScrollBarThumb() {
}

gfx::Size OverlayScrollBarThumb::GetPreferredSize() const {
  return gfx::Size(kThumbMinimumSize, kThumbMinimumSize);
}

void OverlayScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect local_bounds(GetLocalBounds());
  SkPaint paint;
  int alpha = kThumbDefaultAlpha * animation_opacity_;
  if (GetState() == CustomButton::STATE_HOVERED) {
    alpha = kThumbHoverAlpha * animation_opacity_;
  } else if(GetState() == CustomButton::STATE_PRESSED) {
    // If we are in pressed state, no need to worry about animation,
    // just display the deeper color.
    alpha = kThumbHoverAlpha;
  }

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SkColorSetARGB(alpha, 0, 0, 0));
  canvas->DrawRoundRect(local_bounds, kThumbCornerRadius, paint);
}

void OverlayScrollBarThumb::AnimationProgressed(
    const gfx::Animation* animation) {
  animation_opacity_ = animation->GetCurrentValue();
  SchedulePaint();
}

}  // namespace

OverlayScrollBar::OverlayScrollBar(bool horizontal)
    : BaseScrollBar(horizontal, new OverlayScrollBarThumb(this)),
      animation_(static_cast<OverlayScrollBarThumb*>(GetThumb())) {
  set_notify_enter_exit_on_child(true);
}

OverlayScrollBar::~OverlayScrollBar() {
}

gfx::Rect OverlayScrollBar::GetTrackBounds() const {
  gfx::Rect local_bounds(GetLocalBounds());
  if (IsHorizontal()) {
    local_bounds.Inset(kThumbInsetFromEdge, kThumbInsetInside,
                       kThumbInsetFromEdge, kThumbInsetFromEdge);
  } else {
    local_bounds.Inset(kThumbInsetInside, kThumbInsetFromEdge,
                       kThumbInsetFromEdge, kThumbInsetFromEdge);
  }
  gfx::Size track_size = local_bounds.size();
  track_size.SetToMax(GetThumb()->size());
  local_bounds.set_size(track_size);
  return local_bounds;
}

int OverlayScrollBar::GetLayoutSize() const {
  return 0;
}

int OverlayScrollBar::GetContentOverlapSize() const {
  return kScrollbarWidth;
}

void OverlayScrollBar::OnMouseEnteredScrollView(const ui::MouseEvent& event) {
  animation_.Show();
}

void OverlayScrollBar::OnMouseExitedScrollView(const ui::MouseEvent& event) {
  animation_.Hide();
}

void OverlayScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      animation_.Show();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_END:
      animation_.Hide();
      break;
    default:
      break;
  }
  BaseScrollBar::OnGestureEvent(event);
}

gfx::Size OverlayScrollBar::GetPreferredSize() const {
  return gfx::Size();
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

void OverlayScrollBar::OnPaint(gfx::Canvas* canvas) {
}

}  // namespace views
