// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/fullscreen_handler.h"

#include "base/logging.h"
#include "base/win/win_util.h"
#include "ui/gfx/rect.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, public:

FullscreenHandler::FullscreenHandler()
    : hwnd_(NULL),
      fullscreen_(false),
      metro_snap_(false) {
}

FullscreenHandler::~FullscreenHandler() {
}

void FullscreenHandler::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;

  SetFullscreenImpl(fullscreen, false);
}

void FullscreenHandler::SetMetroSnap(bool metro_snap) {
  if (metro_snap_ == metro_snap)
    return;

  SetFullscreenImpl(metro_snap, true);
  metro_snap_ = metro_snap;
}

gfx::Rect FullscreenHandler::GetRestoreBounds() const {
  return gfx::Rect(saved_window_info_.window_rect);
}

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, private:

void FullscreenHandler::SetFullscreenImpl(bool fullscreen, bool for_metro) {
  ScopedFullscreenVisibility visibility(hwnd_);

  // Save current window state if not already fullscreen.
  if (!fullscreen_) {
    // Save current window information.  We force the window into restored mode
    // before going fullscreen because Windows doesn't seem to hide the
    // taskbar if the window is in the maximized state.
    saved_window_info_.maximized = !!::IsZoomed(hwnd_);
    if (saved_window_info_.maximized)
      ::SendMessage(hwnd_, WM_SYSCOMMAND, SC_RESTORE, 0);
    saved_window_info_.style = GetWindowLong(hwnd_, GWL_STYLE);
    saved_window_info_.ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    GetWindowRect(hwnd_, &saved_window_info_.window_rect);
  }

  fullscreen_ = fullscreen;

  if (fullscreen_) {
    // Set new window style and size.
    SetWindowLong(hwnd_, GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd_, GWL_EXSTYLE,
                  saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME |
                  WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    // On expand, if we're given a window_rect, grow to it, otherwise do
    // not resize.
    if (!for_metro) {
      MONITORINFO monitor_info;
      monitor_info.cbSize = sizeof(monitor_info);
      GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                     &monitor_info);
      gfx::Rect window_rect(monitor_info.rcMonitor);
      SetWindowPos(hwnd_, NULL, window_rect.x(), window_rect.y(),
                   window_rect.width(), window_rect.height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
  } else {
    // Reset original window style and size.  The multiple window size/moves
    // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
    // repainted.  Better-looking methods welcome.
    SetWindowLong(hwnd_, GWL_STYLE, saved_window_info_.style);
    SetWindowLong(hwnd_, GWL_EXSTYLE, saved_window_info_.ex_style);

    if (!for_metro) {
      // On restore, resize to the previous saved rect size.
      gfx::Rect new_rect(saved_window_info_.window_rect);
      SetWindowPos(hwnd_, NULL, new_rect.x(), new_rect.y(),
                   new_rect.width(), new_rect.height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    if (saved_window_info_.maximized)
      ::SendMessage(hwnd_, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  }
}

}  // namespace views
