// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nscolor_additions.h"

#include "base/mac/scoped_cftyperef.h"
#include "testing/platform_test.h"

typedef PlatformTest NSColorChromeAdditionsTest;

TEST_F(NSColorChromeAdditionsTest, ConvertColorWithTwoComponents) {
  NSColor* nsColor = [NSColor colorWithCalibratedWhite:0.5 alpha:0.5];
  base::ScopedCFTypeRef<CGColorRef> cgColor(CGColorCreateGenericGray(0.5, 0.5));
  EXPECT_TRUE(CFEqual(cgColor, [nsColor cr_CGColor]));
}

TEST_F(NSColorChromeAdditionsTest, ConvertColorWithFourComponents) {
  NSColor* nsColor =
      [NSColor colorWithCalibratedRed:0.1 green:0.2 blue:0.3 alpha:0.4];
  base::ScopedCFTypeRef<CGColorRef> cgColor(
      CGColorCreateGenericRGB(0.1, 0.2, 0.3, 0.4));
  EXPECT_TRUE(CFEqual(cgColor, [nsColor cr_CGColor]));
}
