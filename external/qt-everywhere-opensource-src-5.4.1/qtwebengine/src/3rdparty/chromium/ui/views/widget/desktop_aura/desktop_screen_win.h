// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_

#include "ui/gfx/screen_win.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopScreenWin : public gfx::ScreenWin {
public:
  DesktopScreenWin();
  virtual ~DesktopScreenWin();

 private:
  // Overridden from gfx::ScreenWin:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual HWND GetHWNDFromNativeView(gfx::NativeView window) const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenWin);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
