// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_
#define UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class PlatformWindowCast : public PlatformWindow {
 public:
  PlatformWindowCast(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);
  ~PlatformWindowCast() override {}

  // PlatformWindow implementation:
  gfx::Rect GetBounds() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetTitle(const base::string16& title) override;
  void Show() override {}
  void Hide() override {}
  void Close() override {}
  void SetCapture() override {}
  void ReleaseCapture() override {}
  void ToggleFullscreen() override {}
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void SetCursor(PlatformCursor cursor) override {}
  void MoveCursorTo(const gfx::Point& location) override {}
  void ConfineCursorToBounds(const gfx::Rect& bounds) override {}
  PlatformImeController* GetPlatformImeController() override;

 private:
  PlatformWindowDelegate* delegate_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_
