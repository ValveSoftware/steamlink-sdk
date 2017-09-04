// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_H_
#define UI_SNAPSHOT_SNAPSHOT_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot_export.h"

namespace base {
class TaskRunner;
}

namespace gfx {
class Rect;
class Image;
class Size;
}

namespace ui {

// Grabs a snapshot of the window/view. No security checks are done. This is
// intended to be used for debugging purposes where no BrowserProcess instance
// is available (ie. tests). This function is synchronous, so it should NOT be
// used in a result of user action. Support for async vs synchronous
// GrabWindowSnapshot differs by platform.  To be most general, use the
// synchronous function first and if it returns false call the async one.
SNAPSHOT_EXPORT bool GrabWindowSnapshot(
    gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds);

SNAPSHOT_EXPORT bool GrabViewSnapshot(
    gfx::NativeView view,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds);


// These functions take a snapshot of |source_rect|, specified in layer space
// coordinates (DIP for desktop, physical pixels for Android), and scale the
// snapshot to |target_size| (in physical pixels), asynchronously.
typedef base::Callback<void(const gfx::Image& snapshot)>
    GrabWindowSnapshotAsyncCallback;
SNAPSHOT_EXPORT void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback);

typedef base::Callback<void(scoped_refptr<base::RefCountedBytes> png_data)>
    GrabWindowSnapshotAsyncPNGCallback;
SNAPSHOT_EXPORT void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback);
SNAPSHOT_EXPORT void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback);

}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_H_
