// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_FULLSCREEN_WINDOW_MANAGER_H_
#define UI_BASE_COCOA_FULLSCREEN_WINDOW_MANAGER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/ui_base_export.h"

// A utility class to manage the fullscreen mode for a given window. This class
// also updates the window frame if the screen changes.
UI_BASE_EXPORT
@interface FullscreenWindowManager : NSObject {
 @private
  base::scoped_nsobject<NSWindow> window_;
  // Explicitly keep track of the screen we want to position the window in.
  // This is better than using -[NSWindow screen] because that might change if
  // the screen changes to a low resolution.
  base::scoped_nsobject<NSScreen> desiredScreen_;
  base::mac::FullScreenMode fullscreenMode_;
  BOOL fullscreenActive_;
}

- (id)initWithWindow:(NSWindow*)window
       desiredScreen:(NSScreen*)desiredScreen;

// Enables fullscreen mode which causes the menubar and dock to be hidden as
// needed.
- (void)enterFullscreenMode;

// Exists fullscreen mode which stops hiding the menubar and dock.
- (void)exitFullscreenMode;

@end

#endif  // UI_BASE_COCOA_FULLSCREEN_WINDOW_MANAGER_H_
