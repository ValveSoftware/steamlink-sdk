// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_WIN_H_
#define UI_AURA_WINDOW_TREE_HOST_WIN_H_

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/win/window_impl.h"

namespace aura {

class AURA_EXPORT WindowTreeHostWin : public WindowTreeHost,
                                      public ui::EventSource,
                                      public gfx::WindowImpl {
 public:
  explicit WindowTreeHostWin(const gfx::Rect& bounds);
  virtual ~WindowTreeHostWin();
  // WindowTreeHost:
  virtual ui::EventSource* GetEventSource() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

 private:
  CR_BEGIN_MSG_MAP_EX(WindowTreeHostWin)
    // Range handlers must go first!
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE,
                                WM_NCXBUTTONDBLCLK,
                                OnMouseRange)

    // Mouse capture events.
    CR_MESSAGE_HANDLER_EX(WM_CAPTURECHANGED, OnCaptureChanged)

    // Key events.
    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_IME_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)

    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_MOVE(OnMove)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_SIZE(OnSize)
  CR_END_MSG_MAP()

  void OnClose();
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnCaptureChanged(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  void OnMove(const gfx::Point& point);
  void OnPaint(HDC dc);
  void OnSize(UINT param, const gfx::Size& size);

  bool has_capture_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWin);
};

namespace test {

// Set true to let WindowTreeHostWin use a popup window
// with no frame/title so that the window size and test's
// expectations matches.
AURA_EXPORT void SetUsePopupAsRootWindowForTest(bool use);

}  // namespace

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_WIN_H_
