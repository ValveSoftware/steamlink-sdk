// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/legacy_render_widget_host_win.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_switches.h"
#include "ui/base/touch/touch_enabled.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/internal_constants.h"
#include "ui/base/win/window_event_target.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/dpi.h"

namespace content {

// A custom MSAA object id used to determine if a screen reader or some
// other client is listening on MSAA events - if so, we enable full web
// accessibility support.
const int kIdScreenReaderHoneyPot = 1;

LegacyRenderWidgetHostHWND::~LegacyRenderWidgetHostHWND() {
  ::DestroyWindow(hwnd());
}

// static
scoped_ptr<LegacyRenderWidgetHostHWND> LegacyRenderWidgetHostHWND::Create(
    HWND parent) {
  // content_unittests passes in the desktop window as the parent. We allow
  // the LegacyRenderWidgetHostHWND instance to be created in this case for
  // these tests to pass.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLegacyIntermediateWindow) ||
      (!GetWindowEventTarget(parent) && parent != ::GetDesktopWindow()))
    return scoped_ptr<LegacyRenderWidgetHostHWND>();

  scoped_ptr<LegacyRenderWidgetHostHWND> legacy_window_instance;
  legacy_window_instance.reset(new LegacyRenderWidgetHostHWND(parent));
  // If we failed to create the child, or if the switch to disable the legacy
  // window is passed in, then return NULL.
  if (!::IsWindow(legacy_window_instance->hwnd()))
    return scoped_ptr<LegacyRenderWidgetHostHWND>();

  legacy_window_instance->Init();
  return legacy_window_instance.Pass();
}

void LegacyRenderWidgetHostHWND::UpdateParent(HWND parent) {
  ::SetParent(hwnd(), parent);
  // If the new parent is the desktop Window, then we disable the child window
  // to ensure that it does not receive any input events. It should not because
  // of WS_EX_TRANSPARENT. This is only for safety.
  if (parent == ::GetDesktopWindow()) {
    ::EnableWindow(hwnd(), FALSE);
  } else {
    ::EnableWindow(hwnd(), TRUE);
  }
}

HWND LegacyRenderWidgetHostHWND::GetParent() {
  return ::GetParent(hwnd());
}

void LegacyRenderWidgetHostHWND::OnManagerDeleted() {
  manager_ = NULL;
}

void LegacyRenderWidgetHostHWND::Show() {
  ::ShowWindow(hwnd(), SW_SHOW);
}

void LegacyRenderWidgetHostHWND::Hide() {
  ::ShowWindow(hwnd(), SW_HIDE);
}

void LegacyRenderWidgetHostHWND::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect bounds_in_pixel = gfx::win::DIPToScreenRect(bounds);
  ::SetWindowPos(hwnd(), NULL, bounds_in_pixel.x(), bounds_in_pixel.y(),
                 bounds_in_pixel.width(), bounds_in_pixel.height(), 0);
}

void LegacyRenderWidgetHostHWND::OnFinalMessage(HWND hwnd) {
  if (manager_)
    manager_->OnAccessibleHwndDeleted();
}

LegacyRenderWidgetHostHWND::LegacyRenderWidgetHostHWND(HWND parent)
    : manager_(NULL),
      mouse_tracking_enabled_(false) {
  RECT rect = {0};
  Base::Create(parent, rect, L"Chrome Legacy Window",
               WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
               WS_EX_TRANSPARENT);
}

bool LegacyRenderWidgetHostHWND::Init() {
  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      ui::AreTouchEventsEnabled())
    RegisterTouchWindow(hwnd(), TWF_WANTPALM);

  HRESULT hr = ::CreateStdAccessibleObject(
      hwnd(), OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void **>(window_accessible_.Receive()));
  DCHECK(SUCCEEDED(hr));

  if (!BrowserAccessibilityState::GetInstance()->IsAccessibleBrowser()) {
    // Attempt to detect screen readers or other clients who want full
    // accessibility support, by seeing if they respond to this event.
    NotifyWinEvent(EVENT_SYSTEM_ALERT, hwnd(), kIdScreenReaderHoneyPot,
                   CHILDID_SELF);
  }

  return !!SUCCEEDED(hr);
}

// static
ui::WindowEventTarget* LegacyRenderWidgetHostHWND::GetWindowEventTarget(
    HWND parent) {
  return reinterpret_cast<ui::WindowEventTarget*>(ui::ViewProp::GetValue(
      parent, ui::WindowEventTarget::kWin32InputEventTarget));
}

LRESULT LegacyRenderWidgetHostHWND::OnEraseBkGnd(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  return 1;
}

LRESULT LegacyRenderWidgetHostHWND::OnGetObject(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  // Only the lower 32 bits of l_param are valid when checking the object id
  // because it sometimes gets sign-extended incorrectly (but not always).
  DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(l_param));

  if (kIdScreenReaderHoneyPot == obj_id) {
    // When an MSAA client has responded to our fake event on this id,
    // enable screen reader support.
    BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
    return static_cast<LRESULT>(0L);
  }

  if (OBJID_CLIENT != obj_id || !manager_)
    return static_cast<LRESULT>(0L);

  base::win::ScopedComPtr<IAccessible> root(
      manager_->GetRoot()->ToBrowserAccessibilityWin());
  return LresultFromObject(IID_IAccessible, w_param,
      static_cast<IAccessible*>(root.Detach()));
}

// We send keyboard/mouse/touch messages to the parent window via SendMessage.
// While this works, this has the side effect of converting input messages into
// sent messages which changes their priority and could technically result
// in these messages starving other messages in the queue. Additionally
// keyboard/mouse hooks would not see these messages. The alternative approach
// is to set and release capture as needed on the parent to ensure that it
// receives all mouse events. However that was shelved due to possible issues
// with capture changes.
LRESULT LegacyRenderWidgetHostHWND::OnKeyboardRange(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param,
                                                    BOOL& handled) {
  if (GetWindowEventTarget(GetParent())) {
    return GetWindowEventTarget(GetParent())->HandleKeyboardMessage(
        message, w_param, l_param);
  }
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseRange(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param,
                                                 BOOL& handled) {
  if (message == WM_MOUSEMOVE) {
    if (!mouse_tracking_enabled_) {
      mouse_tracking_enabled_ = true;
      TRACKMOUSEEVENT tme;
      tme.cbSize = sizeof(tme);
      tme.dwFlags = TME_LEAVE;
      tme.hwndTrack = hwnd();
      tme.dwHoverTime = 0;
      TrackMouseEvent(&tme);
    }
  }
  // The offsets for WM_NCXXX and WM_MOUSEWHEEL and WM_MOUSEHWHEEL messages are
  // in screen coordinates. We should not be converting them to parent
  // coordinates.
  if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) &&
      (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL)) {
    POINT mouse_coords;
    mouse_coords.x = GET_X_LPARAM(l_param);
    mouse_coords.y = GET_Y_LPARAM(l_param);
    ::MapWindowPoints(hwnd(), GetParent(), &mouse_coords, 1);
    l_param = MAKELPARAM(mouse_coords.x, mouse_coords.y);
  }
  if (GetWindowEventTarget(GetParent())) {
    return GetWindowEventTarget(GetParent())->HandleMouseMessage(
        message, w_param, l_param);
  }
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseLeave(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  mouse_tracking_enabled_ = false;
  if ((::GetCapture() != GetParent()) && GetWindowEventTarget(GetParent())) {
    // We should send a WM_MOUSELEAVE to the parent window only if the mouse
    // has moved outside the bounds of the parent.
    POINT cursor_pos;
    ::GetCursorPos(&cursor_pos);
    if (::WindowFromPoint(cursor_pos) != GetParent()) {
      return GetWindowEventTarget(GetParent())->HandleMouseMessage(
          message, w_param, l_param);
    }
  }
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseActivate(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  // Don't pass this to DefWindowProc. That results in the WM_MOUSEACTIVATE
  // message going all the way to the parent which then messes up state
  // related to focused views, etc. This is because it treats this as if
  // it lost activation.
  // Our dummy window should not interfere with focus and activation in
  // the parent. Return MA_ACTIVATE here ensures that focus state in the parent
  // is preserved. The only exception is if the parent was created with the
  // WS_EX_NOACTIVATE style.
  if (::GetWindowLong(GetParent(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)
    return MA_NOACTIVATE;
  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (::GetProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow)) {
    ::RemoveProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow);
    return MA_NOACTIVATE;
  }
  return MA_ACTIVATE;
}

LRESULT LegacyRenderWidgetHostHWND::OnTouch(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  if (GetWindowEventTarget(GetParent())) {
    return GetWindowEventTarget(GetParent())->HandleTouchMessage(
        message, w_param, l_param);
  }
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnScroll(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  if (GetWindowEventTarget(GetParent())) {
    return GetWindowEventTarget(GetParent())->HandleScrollMessage(
        message, w_param, l_param);
  }
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCHitTest(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  if (GetWindowEventTarget(GetParent())) {
    LRESULT hit_test = GetWindowEventTarget(
        GetParent())->HandleNcHitTestMessage(message, w_param, l_param);
    // If the parent returns HTNOWHERE which can happen for popup windows, etc
    // we return HTCLIENT.
    if (hit_test == HTNOWHERE)
      hit_test = HTCLIENT;
    return hit_test;
  }
  return HTNOWHERE;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCPaint(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnPaint(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  PAINTSTRUCT ps = {0};
  ::BeginPaint(hwnd(), &ps);
  ::EndPaint(hwnd(), &ps);
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnSetCursor(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCCalcSize(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  // Prevent scrollbars, etc from drawing.
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnSize(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param) {
  // Certain trackpad drivers on Windows have bugs where in they don't generate
  // WM_MOUSEWHEEL messages for the trackpoint and trackpad scrolling gestures
  // unless there is an entry for Chrome with the class name of the Window.
  // Additionally others check if the window WS_VSCROLL/WS_HSCROLL styles and
  // generate the legacy WM_VSCROLL/WM_HSCROLL messages.
  // We add these styles to ensure that trackpad/trackpoint scrolling
  // work.
  long current_style = ::GetWindowLong(hwnd(), GWL_STYLE);
  ::SetWindowLong(hwnd(), GWL_STYLE,
                  current_style | WS_VSCROLL | WS_HSCROLL);
  return 0;
}

}  // namespace content
