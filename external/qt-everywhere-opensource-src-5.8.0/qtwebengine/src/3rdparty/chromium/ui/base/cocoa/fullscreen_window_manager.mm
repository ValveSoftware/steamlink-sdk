// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/fullscreen_window_manager.h"

namespace {

// Get the screen with the menu bar.
NSScreen* GetMenuBarScreen() {
  // Documentation in NSScreen says that the first object in
  // +[NSScreen screens] is the menu bar screen.
  NSArray *screens = [NSScreen screens];
  if ([screens count])
    return [screens objectAtIndex:0];
  return nil;
}

// Get the screen with the dock.
NSScreen* GetDockScreen() {
  NSArray *screens = [NSScreen screens];
  NSUInteger count = [screens count];
  if (count == 0)
    return NULL;
  if (count == 1)
    return [screens objectAtIndex:0];

  for (NSUInteger i = 1; i < count; ++i) {
    NSScreen* screen = [screens objectAtIndex:i];
    // This screen is not the menu bar screen since it's not index 0. Therefore,
    // the only reason that the frame would not match the visible frame is if
    // the dock is on the screen.
    if (!NSEqualRects([screen frame], [screen visibleFrame]))
      return screen;
  }
  return [screens objectAtIndex:0];
}

}  // namespace

@interface FullscreenWindowManager()
- (void)onScreenChanged:(NSNotification*)note;
- (void)update;
@end

@implementation FullscreenWindowManager

- (id)initWithWindow:(NSWindow*)window
       desiredScreen:(NSScreen*)desiredScreen {
  if ((self = [super init])) {
    window_.reset([window retain]);
    desiredScreen_.reset([desiredScreen retain]);
    fullscreenMode_ = base::mac::kFullScreenModeNormal;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onScreenChanged:)
               name:NSApplicationDidChangeScreenParametersNotification
             object:NSApp];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self exitFullscreenMode];
  [super dealloc];
}

- (void)enterFullscreenMode {
  if (fullscreenActive_)
    return;
  fullscreenActive_ = true;
  [self update];
}

- (void)exitFullscreenMode {
  if (!fullscreenActive_)
    return;
  fullscreenActive_ = false;
  [self update];
}

- (void)onScreenChanged:(NSNotification*)note {
  [self update];
}

- (void)update {
  // From OS X 10.10, NSApplicationDidChangeScreenParametersNotification is sent
  // when displaying a fullscreen window, which should normally only be sent if
  // the monitor resolution has changed or new display is detected.
  if (![[NSScreen screens] containsObject:desiredScreen_])
    desiredScreen_.reset([[window_ screen] retain]);

  base::mac::FullScreenMode newMode;
  if (!fullscreenActive_)
    newMode = base::mac::kFullScreenModeNormal;
  else if ([desiredScreen_ isEqual:GetMenuBarScreen()])
    newMode = base::mac::kFullScreenModeHideAll;
  else if ([desiredScreen_ isEqual:GetDockScreen()])
    newMode = base::mac::kFullScreenModeHideDock;
  else
    newMode = base::mac::kFullScreenModeNormal;

  if (fullscreenMode_ != newMode) {
    if (fullscreenMode_ != base::mac::kFullScreenModeNormal)
      base::mac::ReleaseFullScreen(fullscreenMode_);
    if (newMode != base::mac::kFullScreenModeNormal)
      base::mac::RequestFullScreen(newMode);
    fullscreenMode_ = newMode;
  }

  if (fullscreenActive_)
    [window_ setFrame:[desiredScreen_ frame] display:YES];
}

@end
