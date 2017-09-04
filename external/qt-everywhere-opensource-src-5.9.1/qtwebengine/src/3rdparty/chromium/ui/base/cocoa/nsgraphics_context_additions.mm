// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsgraphics_context_additions.h"

@implementation NSGraphicsContext (CrAdditions)

- (void)cr_setPatternPhase:(NSPoint)phase
                   forView:(NSView*)view {
  NSView* ancestorWithLayer = view;
  while (ancestorWithLayer && ![ancestorWithLayer layer])
    ancestorWithLayer = [ancestorWithLayer superview];
  if (ancestorWithLayer) {
    NSPoint bottomLeft = NSZeroPoint;
    if ([ancestorWithLayer isFlipped])
      bottomLeft.y = NSMaxY([ancestorWithLayer bounds]);
    NSPoint offset = [ancestorWithLayer convertPoint:bottomLeft toView:nil];
    phase.x -= offset.x;
    phase.y -= offset.y;
  }
  [self setPatternPhase:phase];
}

@end
