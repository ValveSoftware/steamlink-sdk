// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSCOLOR_ADDITIONS_H_
#define UI_BASE_COCOA_NSCOLOR_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

@interface NSColor (ChromeAdditions)

// Returns an autoreleased CGColor, just like -[NSColor CGColor] in 10.8 SDK.
- (CGColorRef)cr_CGColor;

@end

#endif  // UI_BASE_COCOA_NSCOLOR_ADDITIONS_H_
