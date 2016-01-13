// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

#import <objc/runtime.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/ui_base_switches.h"

@interface NSWindow (UndocumentedAPI)
// Normally, punching a hole in a window by painting a subview with a
// transparent color causes the shadow for that area to also not be present.
// That feature is "content has shadow", which means that shadows are effective
// even in the content area of the window. If, however, "content has shadow" is
// turned off, then the transparent area of the content casts a shadow. The one
// tricky part is that even if "content has shadow" is turned off, "the content"
// is defined as being the scanline from the leftmost opaque part to the
// rightmost opaque part.  Therefore, to force the entire window to have a
// shadow, make sure that for the entire content region, there is an opaque area
// on the right and left edge of the window.
- (void)_setContentHasShadow:(BOOL)shadow;
@end

@interface OpaqueView : NSView
@end

namespace {

bool CoreAnimationIsEnabled() {
  static bool is_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableCoreAnimation);
  return is_enabled;
}

NSComparisonResult OpaqueViewsOnTop(id view1, id view2, void* context) {
  BOOL view_1_is_opaque_view = [view1 isKindOfClass:[OpaqueView class]];
  BOOL view_2_is_opaque_view = [view2 isKindOfClass:[OpaqueView class]];
  if (view_1_is_opaque_view && view_2_is_opaque_view)
    return NSOrderedSame;
  if (view_1_is_opaque_view)
    return NSOrderedDescending;
  if (view_2_is_opaque_view)
    return NSOrderedAscending;
  return NSOrderedSame;
}

}  // namespace

@implementation OpaqueView

- (void)drawRect:(NSRect)r {
  [[NSColor blackColor] set];
  NSRectFill(r);
}

- (void)resetCursorRects {
  // When a view is moved relative to its peers, its cursor rects are reset.
  // (This is an undocumented side-effect.) At that time, verify that any
  // OpaqueViews are z-ordered in the front, and enforce it if need be.

  NSView* rootView = [self superview];
  DCHECK_EQ((NSView*)nil, [rootView superview]);
  NSArray* subviews = [rootView subviews];

  // If a window has any opaques, it will have exactly two.
  DCHECK_EQ(2U, [[subviews indexesOfObjectsPassingTest:
      ^(id el, NSUInteger i, BOOL *stop) {
        return [el isKindOfClass:[OpaqueView class]];
      }] count]);

  NSUInteger count = [subviews count];
  if (count < 2)
    return;

  if (![[subviews objectAtIndex:count - 1] isKindOfClass:[OpaqueView class]] ||
      ![[subviews objectAtIndex:count - 2] isKindOfClass:[OpaqueView class]]) {
    // Do not sort the subviews array here and call -[NSView setSubviews:] as
    // that causes a crash on 10.6.
    [rootView sortSubviewsUsingFunction:OpaqueViewsOnTop context:NULL];
  }
}

@end

@implementation UnderlayOpenGLHostingWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  // It is invalid to create windows with zero width or height. It screws things
  // up royally:
  // - It causes console spew: <http://crbug.com/78973>
  // - It breaks Expose: <http://sourceforge.net/projects/heat-meteo/forums/forum/268087/topic/4582610>
  //
  // This is a banned practice
  // <http://www.chromium.org/developers/coding-style/cocoa-dos-and-donts>. Do
  // not do this. Use kWindowSizeDeterminedLater in
  // ui/base/cocoa/window_size_constants.h instead.
  //
  // (This is checked here because UnderlayOpenGLHostingWindow is the base of
  // most Chromium windows, not because this is related to its functionality.)
  DCHECK(!NSIsEmptyRect(contentRect));
  if ((self = [super initWithContentRect:contentRect
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    if (CoreAnimationIsEnabled()) {
      // If CoreAnimation is used, then the hole punching technique won't be
      // used. Bail now and don't play any special games with the shadow.
      // TODO(avi): Rip all this shadow code out once CoreAnimation can't be
      // turned off. http://crbug.com/336554
      return self;
    }

    // OpenGL-accelerated content works by punching holes in windows. Therefore
    // all windows hosting OpenGL content must not be opaque.
    [self setOpaque:NO];

    if (windowStyle & NSTitledWindowMask) {
      // Only fiddle with shadows if the window is a proper window with a
      // title bar and all.
      [self _setContentHasShadow:NO];

      NSView* rootView = [[self contentView] superview];
      const NSRect rootBounds = [rootView bounds];

      // On 10.7/8, the bottom corners of the window are rounded by magic at a
      // deeper level than the NSThemeFrame, so it is OK to have the opaques
      // go all the way to the bottom.
      const CGFloat kTopEdgeInset = 16;
      const CGFloat kAlphaValueJustOpaqueEnough = 0.005;

      base::scoped_nsobject<NSView> leftOpaque([[OpaqueView alloc]
          initWithFrame:NSMakeRect(NSMinX(rootBounds),
                                   NSMinY(rootBounds),
                                   1,
                                   NSHeight(rootBounds) - kTopEdgeInset)]);
      [leftOpaque setAutoresizingMask:NSViewMaxXMargin |
                                      NSViewHeightSizable];
      [leftOpaque setAlphaValue:kAlphaValueJustOpaqueEnough];
      [rootView addSubview:leftOpaque];

      base::scoped_nsobject<NSView> rightOpaque([[OpaqueView alloc]
          initWithFrame:NSMakeRect(NSMaxX(rootBounds) - 1,
                                   NSMinY(rootBounds),
                                   1,
                                   NSHeight(rootBounds) - kTopEdgeInset)]);
      [rightOpaque setAutoresizingMask:NSViewMinXMargin |
                                       NSViewHeightSizable];
      [rightOpaque setAlphaValue:kAlphaValueJustOpaqueEnough];
      [rootView addSubview:rightOpaque];
    }
  }

  return self;
}

@end
