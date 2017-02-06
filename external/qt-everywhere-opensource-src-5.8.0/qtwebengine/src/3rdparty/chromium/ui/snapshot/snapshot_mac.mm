// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  NSWindow* window = [view window];
  NSScreen* screen = [[NSScreen screens] firstObject];
  gfx::Rect screen_bounds = gfx::Rect(NSRectToCGRect([screen frame]));


  // Get the view bounds relative to the screen
  NSRect frame = [view convertRect:[view bounds] toView:nil];
  frame = [window convertRectToScreen:frame];

  gfx::Rect view_bounds = gfx::Rect(NSRectToCGRect(frame));

  // Flip window coordinates based on the primary screen.
  view_bounds.set_y(
      screen_bounds.height() - view_bounds.y() - view_bounds.height());

  // Convert snapshot bounds relative to window into bounds relative to
  // screen.
  gfx::Rect screen_snapshot_bounds = snapshot_bounds;
  screen_snapshot_bounds.Offset(view_bounds.OffsetFromOrigin());

  DCHECK_LE(screen_snapshot_bounds.right(), view_bounds.right());
  DCHECK_LE(screen_snapshot_bounds.bottom(), view_bounds.bottom());

  png_representation->clear();

  base::ScopedCFTypeRef<CGImageRef> windowSnapshot(
      CGWindowListCreateImage(screen_snapshot_bounds.ToCGRect(),
                              kCGWindowListOptionIncludingWindow,
                              [window windowNumber],
                              kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot) <= 0)
    return false;

  base::scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:windowSnapshot]);
  NSData* data = [rep representationUsingType:NSPNGFileType properties:@{}];
  const unsigned char* buf = static_cast<const unsigned char*>([data bytes]);
  NSUInteger length = [data length];
  if (buf == NULL || length == 0)
    return false;

  png_representation->assign(buf, buf + length);
  DCHECK(!png_representation->empty());

  return true;
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  return GrabViewSnapshot([[window contentView] superview], png_representation,
      snapshot_bounds);
}

void GrabWindowSnapshotAndScaleAsync(
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
  return GrabViewSnapshotAsync([[window contentView] superview], source_rect,
      background_task_runner, callback);
}

}  // namespace ui
