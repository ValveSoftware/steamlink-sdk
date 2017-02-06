// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nscolor_additions.h"

#include "base/mac/foundation_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"

@implementation NSColor (ChromeAdditions)

- (CGColorRef)cr_CGColor {
  // Redirect to -[NSColor CGColor] if available because it caches.
  static BOOL redirect =
      [NSColor instancesRespondToSelector:@selector(CGColor)];
  if (redirect)
    return [self CGColor];

  const NSInteger numberOfComponents = [self numberOfComponents];
  std::vector<CGFloat> components(numberOfComponents, 0.0);
  [self getComponents:components.data()];
  auto color = CGColorCreate([[self colorSpace] CGColorSpace],
                             components.data());
  base::mac::CFTypeRefToNSObjectAutorelease(color);
  return color;
}

@end
