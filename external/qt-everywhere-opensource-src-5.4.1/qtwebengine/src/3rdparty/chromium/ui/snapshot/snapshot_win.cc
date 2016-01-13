// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot_win.h"

#include "base/callback.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/snapshot/snapshot.h"

namespace {

gfx::Rect GetWindowBounds(HWND window_handle) {
  RECT content_rect = {0, 0, 0, 0};
  if (window_handle) {
    ::GetWindowRect(window_handle, &content_rect);
  } else {
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfo(MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY),
                       &monitor_info)) {
      content_rect = monitor_info.rcMonitor;
    }
  }
  content_rect.right++;  // Match what PrintWindow wants.

  return gfx::Rect(content_rect.right - content_rect.left,
                   content_rect.bottom - content_rect.top);
}

}  // namespace

namespace ui {

namespace internal {

bool GrabHwndSnapshot(HWND window_handle,
                      const gfx::Rect& snapshot_bounds,
                      std::vector<unsigned char>* png_representation) {
  DCHECK(snapshot_bounds.right() <= GetWindowBounds(window_handle).right());
  DCHECK(snapshot_bounds.bottom() <= GetWindowBounds(window_handle).bottom());

  // Create a memory DC that's compatible with the window.
  HDC window_hdc = GetWindowDC(window_handle);
  base::win::ScopedCreateDC mem_hdc(CreateCompatibleDC(window_hdc));

  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(snapshot_bounds.width(),
                          snapshot_bounds.height(),
                          &hdr);
  unsigned char *bit_ptr = NULL;
  base::win::ScopedBitmap bitmap(
      CreateDIBSection(mem_hdc,
                       reinterpret_cast<BITMAPINFO*>(&hdr),
                       DIB_RGB_COLORS,
                       reinterpret_cast<void **>(&bit_ptr),
                       NULL, 0));

  base::win::ScopedSelectObject select_bitmap(mem_hdc, bitmap);
  // Clear the bitmap to white (so that rounded corners on windows
  // show up on a white background, and strangely-shaped windows
  // look reasonable). Not capturing an alpha mask saves a
  // bit of space.
  PatBlt(mem_hdc, 0, 0, snapshot_bounds.width(), snapshot_bounds.height(),
         WHITENESS);
  // Grab a copy of the window
  // First, see if PrintWindow is defined (it's not in Windows 2000).
  typedef BOOL (WINAPI *PrintWindowPointer)(HWND, HDC, UINT);
  PrintWindowPointer print_window =
      reinterpret_cast<PrintWindowPointer>(
          GetProcAddress(GetModuleHandle(L"User32.dll"), "PrintWindow"));

  // If PrintWindow is defined, use it.  It will work on partially
  // obscured windows, and works better for out of process sub-windows.
  // Otherwise grab the bits we can get with BitBlt; it's better
  // than nothing and will work fine in the average case (window is
  // completely on screen).  Always BitBlt when grabbing the whole screen.
  if (snapshot_bounds.origin() == gfx::Point() && print_window && window_handle)
    (*print_window)(window_handle, mem_hdc, 0);
  else
    BitBlt(mem_hdc, 0, 0, snapshot_bounds.width(), snapshot_bounds.height(),
           window_hdc, snapshot_bounds.x(), snapshot_bounds.y(), SRCCOPY);

  // We now have a copy of the window contents in a DIB, so
  // encode it into a useful format for posting to the bug report
  // server.
  gfx::PNGCodec::Encode(bit_ptr, gfx::PNGCodec::FORMAT_BGRA,
                        snapshot_bounds.size(),
                        snapshot_bounds.width() * 4, true,
                        std::vector<gfx::PNGCodec::Comment>(),
                        png_representation);

  ReleaseDC(window_handle, window_hdc);

  return true;
}

}  // namespace internal

#if !defined(USE_AURA)

bool GrabViewSnapshot(gfx::NativeView view_handle,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(view_handle, png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window_handle,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  DCHECK(window_handle);
  return internal::GrabHwndSnapshot(window_handle, snapshot_bounds,
                                    png_representation);
}

void GrapWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    GrabWindowSnapshotAsyncCallback callback) {
  callback.Run(gfx::Image());
}

void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  callback.Run(scoped_refptr<base::RefCountedBytes>());
}


void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  callback.Run(scoped_refptr<base::RefCountedBytes>());
}

#endif  // !defined(USE_AURA)

}  // namespace ui
