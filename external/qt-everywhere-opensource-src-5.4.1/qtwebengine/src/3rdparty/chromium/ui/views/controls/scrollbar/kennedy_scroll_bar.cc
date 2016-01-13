// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/kennedy_scroll_bar.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

namespace views {
namespace {

const int kScrollbarWidth = 10;
const int kThumbMinimumSize = kScrollbarWidth * 2;
const SkColor kBorderColor = SkColorSetARGB(32, 0, 0, 0);
const SkColor kThumbHoverColor = SkColorSetARGB(128, 0, 0, 0);
const SkColor kThumbDefaultColor = SkColorSetARGB(64, 0, 0, 0);
const SkColor kTrackHoverColor = SkColorSetARGB(32, 0, 0, 0);

class KennedyScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit KennedyScrollBarThumb(BaseScrollBar* scroll_bar);
  virtual ~KennedyScrollBarThumb();

 protected:
  // View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(KennedyScrollBarThumb);
};

KennedyScrollBarThumb::KennedyScrollBarThumb(BaseScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar) {
}

KennedyScrollBarThumb::~KennedyScrollBarThumb() {
}

gfx::Size KennedyScrollBarThumb::GetPreferredSize() const {
  return gfx::Size(kThumbMinimumSize, kThumbMinimumSize);
}

void KennedyScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect local_bounds(GetLocalBounds());
  canvas->FillRect(local_bounds,
                   (GetState() == CustomButton::STATE_HOVERED ||
                    GetState() == CustomButton::STATE_PRESSED) ?
                   kThumbHoverColor : kThumbDefaultColor);
  canvas->DrawRect(local_bounds, kBorderColor);
}

}  // namespace

KennedyScrollBar::KennedyScrollBar(bool horizontal)
    : BaseScrollBar(horizontal, new KennedyScrollBarThumb(this)) {
  set_notify_enter_exit_on_child(true);
}

KennedyScrollBar::~KennedyScrollBar() {
}

gfx::Rect KennedyScrollBar::GetTrackBounds() const {
  gfx::Rect local_bounds(GetLocalBounds());
  gfx::Size track_size = local_bounds.size();
  track_size.SetToMax(GetThumb()->size());
  local_bounds.set_size(track_size);
  return local_bounds;
}

int KennedyScrollBar::GetLayoutSize() const {
  return kScrollbarWidth;
}

gfx::Size KennedyScrollBar::GetPreferredSize() const {
  return GetTrackBounds().size();
}

void KennedyScrollBar::Layout() {
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

void KennedyScrollBar::OnPaint(gfx::Canvas* canvas) {
  CustomButton::ButtonState state = GetThumbTrackState();
  if ((state == CustomButton::STATE_HOVERED) ||
      (state == CustomButton::STATE_PRESSED)) {
    gfx::Rect local_bounds(GetLocalBounds());
    canvas->FillRect(local_bounds, kTrackHoverColor);
    canvas->DrawRect(local_bounds, kBorderColor);
  }
}

}  // namespace views
