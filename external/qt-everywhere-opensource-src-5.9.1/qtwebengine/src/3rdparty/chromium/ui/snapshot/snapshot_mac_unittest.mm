// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace {

typedef PlatformTest GrabWindowSnapshotTest;

TEST_F(GrabWindowSnapshotTest, TestGrabWindowSnapshot) {
  // Launch a test window so we can take a snapshot.
  NSRect frame = NSMakeRect(0, 0, 400, 400);
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setBackgroundColor:[NSColor whiteColor]];
  [window makeKeyAndOrderFront:NSApp];

  std::unique_ptr<std::vector<unsigned char>> png_representation(
      new std::vector<unsigned char>);
  gfx::Rect bounds = gfx::Rect(0, 0, frame.size.width, frame.size.height);
  EXPECT_TRUE(ui::GrabWindowSnapshot(window, png_representation.get(),
                                           bounds));

  // Copy png back into NSData object so we can make sure we grabbed a png.
  base::scoped_nsobject<NSData> image_data(
      [[NSData alloc] initWithBytes:&(*png_representation)[0]
                             length:png_representation->size()]);
  NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:image_data.get()];
  EXPECT_TRUE([rep isKindOfClass:[NSBitmapImageRep class]]);
  CGFloat scaleFactor = 1.0f;
  if ([window respondsToSelector:@selector(backingScaleFactor)])
    scaleFactor = [window backingScaleFactor];
  EXPECT_EQ(400 * scaleFactor, CGImageGetWidth([rep CGImage]));
  NSColor* color = [rep colorAtX:200 * scaleFactor y:200 * scaleFactor];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  EXPECT_GE(red + green + blue, 3.0);
}

}  // namespace
}  // namespace ui
