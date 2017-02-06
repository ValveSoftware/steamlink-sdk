// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "cc/output/copy_output_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/snapshot/snapshot_async.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(
      view->GetWindowAndroid(), png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  // Not supported in Android.  Callers should fall back to the async version.
  return false;
}

static void MakeAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const cc::CopyOutputRequest::CopyOutputRequestCallback& callback) {
  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(callback);

  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Rect source_rect_in_pixel =
      gfx::ScaleToEnclosingRect(source_rect, device_scale_factor);

  // Account for the toolbar offset.
  gfx::Vector2dF offset = window->content_offset();
  gfx::Rect adjusted_source_rect(gfx::ToRoundedPoint(
      gfx::PointF(source_rect_in_pixel.x() + offset.x(),
                  source_rect_in_pixel.y() + offset.y())),
      source_rect_in_pixel.size());

  request->set_area(adjusted_source_rect);
  window->GetCompositor()->RequestCopyOfOutputOnRootLayer(std::move(request));
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback) {
  MakeAsyncCopyRequest(window,
                       source_rect,
                       base::Bind(&SnapshotAsync::ScaleCopyOutputResult,
                                  callback,
                                  target_size,
                                  background_task_runner));
}

void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  MakeAsyncCopyRequest(window,
                       source_rect,
                       base::Bind(&SnapshotAsync::EncodeCopyOutputResult,
                                  callback,
                                  background_task_runner));
}

void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  GrabWindowSnapshotAsync(
      view->GetWindowAndroid(), source_rect, background_task_runner, callback);
}

}  // namespace ui
