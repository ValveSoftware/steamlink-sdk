// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/child_window_surface_win.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/win/scoped_hdc.h"
#include "base/win/wrapped_window_proc.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

namespace {

ATOM g_window_class;

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  switch (message) {
    case WM_ERASEBKGND:
      // Prevent windows from erasing the background.
      return 1;
    case WM_PAINT:
      PAINTSTRUCT paint;
      if (BeginPaint(window, &paint)) {
        ChildWindowSurfaceWin* window_surface =
            reinterpret_cast<ChildWindowSurfaceWin*>(
                gfx::GetWindowUserData(window));
        DCHECK(window_surface);

        // Wait to clear the contents until a GL draw occurs, as otherwise an
        // unsightly black flash may happen if the GL contents are still
        // transparent.
        window_surface->InvalidateWindowRect(gfx::Rect(paint.rcPaint));
        EndPaint(window, &paint);
      }
      return 0;
    default:
      return DefWindowProc(window, message, w_param, l_param);
  }
}

void InitializeWindowClass() {
  if (g_window_class)
    return;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"Intermediate D3D Window",
      &base::win::WrappedWindowProc<IntermediateWindowProc>, CS_OWNDC, 0, 0,
      nullptr, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr,
      nullptr, nullptr, &intermediate_class);
  g_window_class = RegisterClassEx(&intermediate_class);
  if (!g_window_class) {
    LOG(ERROR) << "RegisterClass failed.";
    return;
  }
}
}

ChildWindowSurfaceWin::ChildWindowSurfaceWin(GpuChannelManager* manager,
                                             HWND parent_window)
    : gl::NativeViewGLSurfaceEGL(0),
      parent_window_(parent_window),
      manager_(manager),
      alpha_(true),
      first_swap_(true) {
  // Don't use EGL_ANGLE_window_fixed_size so that we can avoid recreating the
  // window surface, which can cause flicker on DirectComposition.
  enable_fixed_size_angle_ = false;
}

EGLConfig ChildWindowSurfaceWin::GetConfig() {
  if (!config_) {
    int alpha_size = alpha_ ? 8 : EGL_DONT_CARE;

    EGLint config_attribs[] = {EGL_ALPHA_SIZE,
                               alpha_size,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                               EGL_NONE};

    EGLDisplay display = GetHardwareDisplay();
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config_, 1, &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << ui::GetLastEGLErrorString();
      return NULL;
    }
  }

  return config_;
}

bool ChildWindowSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;
  InitializeWindowClass();
  DCHECK(g_window_class);

  RECT windowRect;
  GetClientRect(parent_window_, &windowRect);

  window_ = CreateWindowEx(
      WS_EX_NOPARENTNOTIFY, reinterpret_cast<wchar_t*>(g_window_class), L"",
      WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE, 0, 0,
      windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
      ui::GetHiddenWindow(), NULL, NULL, NULL);
  gfx::SetWindowUserData(window_, this);
  manager_->delegate()->SendAcceleratedSurfaceCreatedChildWindow(parent_window_,
                                                                 window_);
  return true;
}

bool ChildWindowSurfaceWin::Resize(const gfx::Size& size,
                                   float scale_factor,
                                   bool has_alpha) {
  if (!SupportsPostSubBuffer()) {
    if (!MoveWindow(window_, 0, 0, size.width(), size.height(), FALSE)) {
      return false;
    }
    alpha_ = has_alpha;
    return gl::NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
  } else {
    if (size == GetSize() && has_alpha == alpha_)
      return true;

    // Force a resize and redraw (but not a move, activate, etc.).
    if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                      SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                          SWP_NOOWNERZORDER | SWP_NOZORDER)) {
      return false;
    }
    size_ = size;
    if (has_alpha == alpha_) {
      // A 0-size PostSubBuffer doesn't swap but forces the swap chain to resize
      // to match the window.
      PostSubBuffer(0, 0, 0, 0);
    } else {
      alpha_ = has_alpha;
      config_ = nullptr;

      std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
      gl::GLContext* current_context = gl::GLContext::GetCurrent();
      bool was_current = current_context && current_context->IsCurrent(this);
      if (was_current) {
        scoped_make_current.reset(
            new ui::ScopedMakeCurrent(current_context, this));
        current_context->ReleaseCurrent(this);
      }

      Destroy();

      if (!Initialize()) {
        LOG(ERROR) << "Failed to resize window.";
        return false;
      }
    }
    return true;
  }
}

gfx::SwapResult ChildWindowSurfaceWin::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  // Force the driver to finish drawing before clearing the contents to
  // transparent, to reduce or eliminate the period of time where the contents
  // have flashed black.
  if (first_swap_) {
    glFinish();
    first_swap_ = false;
  }
  ClearInvalidContents();
  return result;
}

gfx::SwapResult ChildWindowSurfaceWin::PostSubBuffer(int x,
                                                     int y,
                                                     int width,
                                                     int height) {
  gfx::SwapResult result =
      NativeViewGLSurfaceEGL::PostSubBuffer(x, y, width, height);
  ClearInvalidContents();
  return result;
}

void ChildWindowSurfaceWin::InvalidateWindowRect(const gfx::Rect& rect) {
  rect_to_clear_.Union(rect);
}

void ChildWindowSurfaceWin::ClearInvalidContents() {
  if (!rect_to_clear_.IsEmpty()) {
    base::win::ScopedGetDC dc(window_);

    RECT rect = rect_to_clear_.ToRECT();

    // DirectComposition composites with the contents under the SwapChain,
    // so ensure that's cleared. GDI treats black as transparent.
    FillRect(dc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    rect_to_clear_ = gfx::Rect();
  }
}

ChildWindowSurfaceWin::~ChildWindowSurfaceWin() {
  gfx::SetWindowUserData(window_, nullptr);
  DestroyWindow(window_);
}

}  // namespace gpu
