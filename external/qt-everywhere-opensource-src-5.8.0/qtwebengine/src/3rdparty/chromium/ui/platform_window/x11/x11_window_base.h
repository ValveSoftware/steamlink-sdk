// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window_export.h"

typedef struct _XDisplay XDisplay;
typedef unsigned long XID;
typedef union _XEvent XEvent;

namespace ui {

// Abstract base implementation for a X11 based PlatformWindow. Methods that
// are platform specific are left unimplemented.
class X11_WINDOW_EXPORT X11WindowBase : public PlatformWindow {
 public:
  explicit X11WindowBase(PlatformWindowDelegate* delegate);
  ~X11WindowBase() override;

  // Creates new underlying XWindow. Does not map XWindow.
  void Create();

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

 protected:
  void Destroy();

  PlatformWindowDelegate* delegate() { return delegate_; }
  XDisplay* xdisplay() { return xdisplay_; }
  XID xwindow() const { return xwindow_; }

  // Checks if XEvent is for this XWindow.
  bool IsEventForXWindow(const XEvent& xev) const;

  // Processes events for this XWindow.
  void ProcessXWindowEvent(XEvent* xev);

 private:
  PlatformWindowDelegate* delegate_;

  XDisplay* xdisplay_;
  XID xwindow_;
  XID xroot_window_;
  X11AtomCache atom_cache_;

  base::string16 window_title_;

  // Setting the bounds is an asynchronous operation in X11. |requested_bounds_|
  // is the bounds requested using XConfigureWindow, and |confirmed_bounds_| is
  // the bounds the X11 server has set on the window.
  gfx::Rect requested_bounds_;
  gfx::Rect confirmed_bounds_;

  bool window_mapped_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11WindowBase);
};

namespace test {

// Sets the value of the |override_redirect| flag when creating an X11 window.
// It is necessary to set this flag on for various tests, otherwise the call to
// X11WindowBase::Show() blocks because it never receives the MapNotify event.
// It is
// unclear why this is necessary, but might be related to calls to
// XInitThreads().
X11_WINDOW_EXPORT void SetUseOverrideRedirectWindowByDefault(
    bool override_redirect);

}  // namespace test

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_
