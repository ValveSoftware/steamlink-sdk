// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task_runner_util.h"
#include "cc/output/copy_output_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/snapshot/snapshot_async.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(view, png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  // Not supported in Aura.  Callers should fall back to the async version.
  return false;
}

static void MakeAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const cc::CopyOutputRequest::CopyOutputRequestCallback& callback) {
  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(callback);
  request->set_area(source_rect);
  window->layer()->RequestCopyOfOutput(std::move(request));
}

static void FinishedAsyncCopyRequest(
    std::unique_ptr<aura::WindowTracker> tracker,
    const gfx::Rect& source_rect,
    const cc::CopyOutputRequest::CopyOutputRequestCallback& callback,
    int retry_count,
    std::unique_ptr<cc::CopyOutputResult> result) {
  static const int kMaxRetries = 5;
  // Retry the copy request if the previous one failed for some reason.
  if (!tracker->windows().empty() && (retry_count < kMaxRetries) &&
      result->IsEmpty()) {
    // Look up window before calling MakeAsyncRequest. Otherwise, due
    // to undefined (favorably right to left) argument evaluation
    // order, the tracker might have been passed and set to NULL
    // before the window is looked up which results in a NULL pointer
    // dereference.
    gfx::NativeWindow window = tracker->windows()[0];
    MakeAsyncCopyRequest(
        window, source_rect,
        base::Bind(&FinishedAsyncCopyRequest, base::Passed(&tracker),
                   source_rect, callback, retry_count + 1));
    return;
  }

  callback.Run(std::move(result));
}

static void MakeInitialAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const cc::CopyOutputRequest::CopyOutputRequestCallback& callback) {
  auto tracker = base::MakeUnique<aura::WindowTracker>();
  tracker->Add(window);
  MakeAsyncCopyRequest(
      window, source_rect,
      base::Bind(&FinishedAsyncCopyRequest, base::Passed(&tracker), source_rect,
                 callback, 0));
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback) {
  MakeInitialAsyncCopyRequest(
      window, source_rect,
      base::Bind(&SnapshotAsync::ScaleCopyOutputResult, callback, target_size,
                 background_task_runner));
}

void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  MakeInitialAsyncCopyRequest(window, source_rect,
                              base::Bind(&SnapshotAsync::EncodeCopyOutputResult,
                                         callback, background_task_runner));
}

void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  GrabWindowSnapshotAsync(view, source_rect, background_task_runner, callback);
}


}  // namespace ui
