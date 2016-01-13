// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_win.h"

#include "base/logging.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {

static const unsigned long kDefaultScrollLinesPerWheelDelta = 3;
static const unsigned long kDefaultScrollCharsPerWheelDelta = 1;

static bool IsKeyDown(WPARAM wparam) {
  return (GetKeyState(wparam) & 0x8000) != 0;
}

static int GetLocationModifier(WPARAM wparam, LPARAM lparam) {
  int modifier = 0;
  switch (wparam) {
  case VK_RETURN:
    if ((lparam >> 16) & KF_EXTENDED)
      modifier = WebInputEvent::IsKeyPad;
    break;
  case VK_INSERT:
  case VK_DELETE:
  case VK_HOME:
  case VK_END:
  case VK_PRIOR:
  case VK_NEXT:
  case VK_UP:
  case VK_DOWN:
  case VK_LEFT:
  case VK_RIGHT:
    if (!((lparam >> 16) & KF_EXTENDED))
      modifier = WebInputEvent::IsKeyPad;
    break;
  case VK_NUMLOCK:
  case VK_NUMPAD0:
  case VK_NUMPAD1:
  case VK_NUMPAD2:
  case VK_NUMPAD3:
  case VK_NUMPAD4:
  case VK_NUMPAD5:
  case VK_NUMPAD6:
  case VK_NUMPAD7:
  case VK_NUMPAD8:
  case VK_NUMPAD9:
  case VK_DIVIDE:
  case VK_MULTIPLY:
  case VK_SUBTRACT:
  case VK_ADD:
  case VK_DECIMAL:
  case VK_CLEAR:
    modifier = WebInputEvent::IsKeyPad;
    break;
  case VK_SHIFT:
    if (IsKeyDown(VK_LSHIFT))
      modifier = WebInputEvent::IsLeft;
    else if (IsKeyDown(VK_RSHIFT))
      modifier = WebInputEvent::IsRight;
    break;
  case VK_CONTROL:
    if (IsKeyDown(VK_LCONTROL))
      modifier = WebInputEvent::IsLeft;
    else if (IsKeyDown(VK_RCONTROL))
      modifier = WebInputEvent::IsRight;
    break;
  case VK_MENU:
    if (IsKeyDown(VK_LMENU))
      modifier = WebInputEvent::IsLeft;
    else if (IsKeyDown(VK_RMENU))
      modifier = WebInputEvent::IsRight;
    break;
  case VK_LWIN:
    modifier = WebInputEvent::IsLeft;
    break;
  case VK_RWIN:
    modifier = WebInputEvent::IsRight;
    break;
  }

  DCHECK(!modifier
         || modifier == WebInputEvent::IsKeyPad
         || modifier == WebInputEvent::IsLeft
         || modifier == WebInputEvent::IsRight);
  return modifier;
}

// Loads the state for toggle keys into the event.
static void SetToggleKeyState(WebInputEvent* event) {
  // Low bit set from GetKeyState indicates "toggled".
  if (::GetKeyState(VK_NUMLOCK) & 1)
    event->modifiers |= WebInputEvent::NumLockOn;
  if (::GetKeyState(VK_CAPITAL) & 1)
    event->modifiers |= WebInputEvent::CapsLockOn;
}

WebKeyboardEvent WebKeyboardEventBuilder::Build(HWND hwnd,
                                                UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam,
                                                DWORD time_ms) {
  WebKeyboardEvent result;

  DCHECK(time_ms);
  result.timeStampSeconds = time_ms / 1000.0;

  result.windowsKeyCode = static_cast<int>(wparam);
  // Record the scan code (along with other context bits) for this key event.
  result.nativeKeyCode = static_cast<int>(lparam);

  switch (message) {
  case WM_SYSKEYDOWN:
    result.isSystemKey = true;
  case WM_KEYDOWN:
    result.type = WebInputEvent::RawKeyDown;
    break;
  case WM_SYSKEYUP:
    result.isSystemKey = true;
  case WM_KEYUP:
    result.type = WebInputEvent::KeyUp;
    break;
  case WM_IME_CHAR:
    result.type = WebInputEvent::Char;
    break;
  case WM_SYSCHAR:
    result.isSystemKey = true;
    result.type = WebInputEvent::Char;
  case WM_CHAR:
    result.type = WebInputEvent::Char;
    break;
  default:
    NOTREACHED();
  }

  if (result.type == WebInputEvent::Char
   || result.type == WebInputEvent::RawKeyDown) {
    result.text[0] = result.windowsKeyCode;
    result.unmodifiedText[0] = result.windowsKeyCode;
  }
  if (result.type != WebInputEvent::Char) {
    UpdateWindowsKeyCodeAndKeyIdentifier(
        &result,
        static_cast<ui::KeyboardCode>(result.windowsKeyCode));
  }

  if (::GetKeyState(VK_SHIFT) & 0x8000)
    result.modifiers |= WebInputEvent::ShiftKey;
  if (::GetKeyState(VK_CONTROL) & 0x8000)
    result.modifiers |= WebInputEvent::ControlKey;
  if (::GetKeyState(VK_MENU) & 0x8000)
    result.modifiers |= WebInputEvent::AltKey;
  // NOTE: There doesn't seem to be a way to query the mouse button state in
  // this case.

  if (LOWORD(lparam) > 1)
    result.modifiers |= WebInputEvent::IsAutoRepeat;

  result.modifiers |= GetLocationModifier(wparam, lparam);

  SetToggleKeyState(&result);
  return result;
}

// WebMouseEvent --------------------------------------------------------------

static int g_last_click_count = 0;
static double g_last_click_time = 0;

static LPARAM GetRelativeCursorPos(HWND hwnd) {
  POINT pos = {-1, -1};
  GetCursorPos(&pos);
  ScreenToClient(hwnd, &pos);
  return MAKELPARAM(pos.x, pos.y);
}

WebMouseEvent WebMouseEventBuilder::Build(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          DWORD time_ms) {
  WebMouseEvent result;

  switch (message) {
  case WM_MOUSEMOVE:
    result.type = WebInputEvent::MouseMove;
    if (wparam & MK_LBUTTON)
      result.button = WebMouseEvent::ButtonLeft;
    else if (wparam & MK_MBUTTON)
      result.button = WebMouseEvent::ButtonMiddle;
    else if (wparam & MK_RBUTTON)
      result.button = WebMouseEvent::ButtonRight;
    else
      result.button = WebMouseEvent::ButtonNone;
    break;
  case WM_MOUSELEAVE:
    result.type = WebInputEvent::MouseLeave;
    result.button = WebMouseEvent::ButtonNone;
    // set the current mouse position (relative to the client area of the
    // current window) since none is specified for this event
    lparam = GetRelativeCursorPos(hwnd);
    break;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
    result.type = WebInputEvent::MouseDown;
    result.button = WebMouseEvent::ButtonLeft;
    break;
  case WM_MBUTTONDOWN:
  case WM_MBUTTONDBLCLK:
    result.type = WebInputEvent::MouseDown;
    result.button = WebMouseEvent::ButtonMiddle;
    break;
  case WM_RBUTTONDOWN:
  case WM_RBUTTONDBLCLK:
    result.type = WebInputEvent::MouseDown;
    result.button = WebMouseEvent::ButtonRight;
    break;
  case WM_LBUTTONUP:
    result.type = WebInputEvent::MouseUp;
    result.button = WebMouseEvent::ButtonLeft;
    break;
  case WM_MBUTTONUP:
    result.type = WebInputEvent::MouseUp;
    result.button = WebMouseEvent::ButtonMiddle;
    break;
  case WM_RBUTTONUP:
    result.type = WebInputEvent::MouseUp;
    result.button = WebMouseEvent::ButtonRight;
    break;
  default:
    NOTREACHED();
  }

  DCHECK(time_ms);
  result.timeStampSeconds = time_ms / 1000.0;

  // set position fields:

  result.x = static_cast<short>(LOWORD(lparam));
  result.y = static_cast<short>(HIWORD(lparam));
  result.windowX = result.x;
  result.windowY = result.y;

  POINT global_point = { result.x, result.y };
  ClientToScreen(hwnd, &global_point);

  result.globalX = global_point.x;
  result.globalY = global_point.y;

  // calculate number of clicks:

  // This differs slightly from the WebKit code in WebKit/win/WebView.cpp
  // where their original code looks buggy.
  static int last_click_position_x;
  static int last_click_position_y;
  static WebMouseEvent::Button last_click_button = WebMouseEvent::ButtonLeft;

  double current_time = result.timeStampSeconds;
  bool cancel_previous_click =
      (abs(last_click_position_x - result.x) >
          (::GetSystemMetrics(SM_CXDOUBLECLK) / 2))
      || (abs(last_click_position_y - result.y) >
          (::GetSystemMetrics(SM_CYDOUBLECLK) / 2))
      || ((current_time - g_last_click_time) * 1000.0 > ::GetDoubleClickTime());

  if (result.type == WebInputEvent::MouseDown) {
    if (!cancel_previous_click && (result.button == last_click_button)) {
      ++g_last_click_count;
    } else {
      g_last_click_count = 1;
      last_click_position_x = result.x;
      last_click_position_y = result.y;
    }
    g_last_click_time = current_time;
    last_click_button = result.button;
  } else if (result.type == WebInputEvent::MouseMove
          || result.type == WebInputEvent::MouseLeave) {
    if (cancel_previous_click) {
      g_last_click_count = 0;
      last_click_position_x = 0;
      last_click_position_y = 0;
      g_last_click_time = 0;
    }
  }
  result.clickCount = g_last_click_count;

  // set modifiers:

  if (wparam & MK_CONTROL)
    result.modifiers |= WebInputEvent::ControlKey;
  if (wparam & MK_SHIFT)
    result.modifiers |= WebInputEvent::ShiftKey;
  if (::GetKeyState(VK_MENU) & 0x8000)
    result.modifiers |= WebInputEvent::AltKey;
  if (wparam & MK_LBUTTON)
    result.modifiers |= WebInputEvent::LeftButtonDown;
  if (wparam & MK_MBUTTON)
    result.modifiers |= WebInputEvent::MiddleButtonDown;
  if (wparam & MK_RBUTTON)
    result.modifiers |= WebInputEvent::RightButtonDown;

  SetToggleKeyState(&result);
  return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(HWND hwnd,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam,
                                                    DWORD time_ms) {
  WebMouseWheelEvent result;

  result.type = WebInputEvent::MouseWheel;

  DCHECK(time_ms);
  result.timeStampSeconds = time_ms / 1000.0;

  result.button = WebMouseEvent::ButtonNone;

  // Get key state, coordinates, and wheel delta from event.
  typedef SHORT (WINAPI *GetKeyStateFunction)(int key);
  GetKeyStateFunction get_key_state_func;
  UINT key_state;
  float wheel_delta;
  bool horizontal_scroll = false;
  if ((message == WM_VSCROLL) || (message == WM_HSCROLL)) {
    // Synthesize mousewheel event from a scroll event.  This is needed to
    // simulate middle mouse scrolling in some laptops.  Use GetAsyncKeyState
    // for key state since we are synthesizing the input event.
    get_key_state_func = GetAsyncKeyState;
    key_state = 0;
    if (get_key_state_func(VK_SHIFT) & 0x8000)
      key_state |= MK_SHIFT;
    if (get_key_state_func(VK_CONTROL) & 0x8000)
      key_state |= MK_CONTROL;
    // NOTE: There doesn't seem to be a way to query the mouse button state
    // in this case.

    POINT cursor_position = {0};
    GetCursorPos(&cursor_position);
    result.globalX = cursor_position.x;
    result.globalY = cursor_position.y;

    switch (LOWORD(wparam)) {
      case SB_LINEUP:    // == SB_LINELEFT
        wheel_delta = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:  // == SB_LINERIGHT
        wheel_delta = -WHEEL_DELTA;
        break;
      case SB_PAGEUP:
        wheel_delta = 1;
        result.scrollByPage = true;
        break;
      case SB_PAGEDOWN:
        wheel_delta = -1;
        result.scrollByPage = true;
        break;
      default:  // We don't supoprt SB_THUMBPOSITION or SB_THUMBTRACK here.
        wheel_delta = 0;
        break;
    }

    if (message == WM_HSCROLL)
      horizontal_scroll = true;
  } else {
    // Non-synthesized event; we can just read data off the event.
    get_key_state_func = ::GetKeyState;
    key_state = GET_KEYSTATE_WPARAM(wparam);

    result.globalX = static_cast<short>(LOWORD(lparam));
    result.globalY = static_cast<short>(HIWORD(lparam));

    wheel_delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam));
    if (message == WM_MOUSEHWHEEL) {
      horizontal_scroll = true;
      wheel_delta = -wheel_delta;  // Windows is <- -/+ ->, WebKit <- +/- ->.
    }
  }
  if (key_state & MK_SHIFT)
    horizontal_scroll = true;

  // Set modifiers based on key state.
  if (key_state & MK_SHIFT)
    result.modifiers |= WebInputEvent::ShiftKey;
  if (key_state & MK_CONTROL)
    result.modifiers |= WebInputEvent::ControlKey;
  if (get_key_state_func(VK_MENU) & 0x8000)
    result.modifiers |= WebInputEvent::AltKey;
  if (key_state & MK_LBUTTON)
    result.modifiers |= WebInputEvent::LeftButtonDown;
  if (key_state & MK_MBUTTON)
    result.modifiers |= WebInputEvent::MiddleButtonDown;
  if (key_state & MK_RBUTTON)
    result.modifiers |= WebInputEvent::RightButtonDown;

  SetToggleKeyState(&result);

  // Set coordinates by translating event coordinates from screen to client.
  POINT client_point = { result.globalX, result.globalY };
  MapWindowPoints(0, hwnd, &client_point, 1);
  result.x = client_point.x;
  result.y = client_point.y;
  result.windowX = result.x;
  result.windowY = result.y;

  // Convert wheel delta amount to a number of pixels to scroll.
  //
  // How many pixels should we scroll per line?  Gecko uses the height of the
  // current line, which means scroll distance changes as you go through the
  // page or go to different pages.  IE 8 is ~60 px/line, although the value
  // seems to vary slightly by page and zoom level.  Also, IE defaults to
  // smooth scrolling while Firefox doesn't, so it can get away with somewhat
  // larger scroll values without feeling as jerky.  Here we use 100 px per
  // three lines (the default scroll amount is three lines per wheel tick).
  // Even though we have smooth scrolling, we don't make this as large as IE
  // because subjectively IE feels like it scrolls farther than you want while
  // reading articles.
  static const float kScrollbarPixelsPerLine = 100.0f / 3.0f;
  wheel_delta /= WHEEL_DELTA;
  float scroll_delta = wheel_delta;
  if (horizontal_scroll) {
    unsigned long scroll_chars = kDefaultScrollCharsPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scroll_chars, 0);
    // TODO(pkasting): Should probably have a different multiplier
    // scrollbarPixelsPerChar here.
    scroll_delta *= static_cast<float>(scroll_chars) * kScrollbarPixelsPerLine;
  } else {
    unsigned long scroll_lines = kDefaultScrollLinesPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);
    if (scroll_lines == WHEEL_PAGESCROLL)
      result.scrollByPage = true;
    if (!result.scrollByPage) {
      scroll_delta *=
          static_cast<float>(scroll_lines) * kScrollbarPixelsPerLine;
    }
  }

  // Set scroll amount based on above calculations.  WebKit expects positive
  // deltaY to mean "scroll up" and positive deltaX to mean "scroll left".
  if (horizontal_scroll) {
    result.deltaX = scroll_delta;
    result.wheelTicksX = wheel_delta;
  } else {
    result.deltaY = scroll_delta;
    result.wheelTicksY = wheel_delta;
  }

  return result;
}

}  // namespace content
