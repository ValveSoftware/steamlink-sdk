// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#include "ui/events/event.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

namespace mojo {
namespace services {
namespace {

gfx::Rect GetWindowBoundsForClientBounds(DWORD style, DWORD ex_style,
                                         const gfx::Rect& bounds) {
  RECT wr;
  wr.left = bounds.x();
  wr.top = bounds.y();
  wr.right = bounds.x() + bounds.width();
  wr.bottom = bounds.y() + bounds.height();
  AdjustWindowRectEx(&wr, style, FALSE, ex_style);

  // Make sure to keep the window onscreen, as AdjustWindowRectEx() may have
  // moved part of it offscreen.
  gfx::Rect window_bounds(wr.left, wr.top,
                          wr.right - wr.left, wr.bottom - wr.top);
  window_bounds.set_x(std::max(0, window_bounds.x()));
  window_bounds.set_y(std::max(0, window_bounds.y()));
  return window_bounds;
}

}

class NativeViewportWin : public gfx::WindowImpl,
                          public NativeViewport {
 public:
  explicit NativeViewportWin(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }
  virtual ~NativeViewportWin() {
    if (IsWindow(hwnd()))
      DestroyWindow(hwnd());
  }

 private:
  // Overridden from NativeViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE {
    gfx::Rect window_bounds = GetWindowBoundsForClientBounds(
        WS_OVERLAPPEDWINDOW, window_ex_style(), bounds);
    gfx::WindowImpl::Init(NULL, window_bounds);
    SetWindowText(hwnd(), L"native_viewport::NativeViewportWin!");
  }

  virtual void Show() OVERRIDE {
    ShowWindow(hwnd(), SW_SHOWNORMAL);
  }

  virtual void Hide() OVERRIDE {
    ShowWindow(hwnd(), SW_HIDE);
  }

  virtual void Close() OVERRIDE {
    DestroyWindow(hwnd());
  }

  virtual gfx::Size GetSize() OVERRIDE {
    RECT cr;
    GetClientRect(hwnd(), &cr);
    return gfx::Size(cr.right - cr.left, cr.bottom - cr.top);
  }

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    gfx::Rect window_bounds = GetWindowBoundsForClientBounds(
        GetWindowLong(hwnd(), GWL_STYLE),
        GetWindowLong(hwnd(), GWL_EXSTYLE),
        bounds);
    SetWindowPos(hwnd(), NULL, window_bounds.x(), window_bounds.y(),
                 window_bounds.width(), window_bounds.height(),
                 SWP_NOREPOSITION);
  }

  virtual void SetCapture() OVERRIDE {
    DCHECK(::GetCapture() != hwnd());
    ::SetCapture(hwnd());
  }

  virtual void ReleaseCapture() OVERRIDE {
    if (::GetCapture() == hwnd())
      ::ReleaseCapture();
  }

  CR_BEGIN_MSG_MAP_EX(NativeViewportWin)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)

    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_IME_CHAR, OnKeyEvent)

    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  CR_END_MSG_MAP()

  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param) {
    MSG msg = { hwnd(), message, w_param, l_param, 0,
                { CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param) } };
    ui::MouseEvent event(msg);
    SetMsgHandled(delegate_->OnEvent(&event));
    return 0;
  }
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param) {
    MSG msg = { hwnd(), message, w_param, l_param };
    ui::KeyEvent event(msg, message == WM_CHAR);
    SetMsgHandled(delegate_->OnEvent(&event));
    return 0;
  }
  LRESULT OnCreate(CREATESTRUCT* create_struct) {
    delegate_->OnAcceleratedWidgetAvailable(hwnd());
    return 0;
  }
  void OnDestroy() {
    delegate_->OnDestroyed();
  }
  void OnPaint(HDC) {
    RECT cr;
    GetClientRect(hwnd(), &cr);

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd(), &ps);
    HBRUSH red_brush = CreateSolidBrush(RGB(255, 0, 0));
    HGDIOBJ old_object = SelectObject(dc, red_brush);
    Rectangle(dc, cr.left, cr.top, cr.right, cr.bottom);
    SelectObject(dc, old_object);
    DeleteObject(red_brush);
    EndPaint(hwnd(), &ps);
  }
  void OnWindowPosChanged(WINDOWPOS* window_pos) {
    if (!(window_pos->flags & SWP_NOSIZE) ||
        !(window_pos->flags & SWP_NOMOVE)) {
      RECT cr;
      GetClientRect(hwnd(), &cr);
      delegate_->OnBoundsChanged(
          gfx::Rect(window_pos->x, window_pos->y,
                    cr.right - cr.left, cr.bottom - cr.top));
    }
  }

  NativeViewportDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportWin);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportWin(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
