// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/scrollbar/base_scroll_bar.h"

namespace views {

// The transparent scrollbar which overlays its contents.
class VIEWS_EXPORT OverlayScrollBar : public BaseScrollBar {
 public:
  explicit OverlayScrollBar(bool horizontal);
  virtual ~OverlayScrollBar();

 protected:
  // BaseScrollBar overrides:
  virtual gfx::Rect GetTrackBounds() const OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // ScrollBar overrides:
  virtual int GetLayoutSize() const OVERRIDE;
  virtual int GetContentOverlapSize() const OVERRIDE;
  virtual void OnMouseEnteredScrollView(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExitedScrollView(const ui::MouseEvent& event) OVERRIDE;

  // View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::SlideAnimation animation_;
  DISALLOW_COPY_AND_ASSIGN(OverlayScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_
