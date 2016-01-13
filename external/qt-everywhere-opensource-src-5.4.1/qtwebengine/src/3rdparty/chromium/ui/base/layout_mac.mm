// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/sdk_forward_declarations.h"

namespace {

float GetScaleFactorScaleForNativeView(gfx::NativeView view) {
  if (NSWindow* window = [view window]) {
    if ([window respondsToSelector:@selector(backingScaleFactor)])
      return [window backingScaleFactor];
    return [window userSpaceScaleFactor];
  }

  NSArray* screens = [NSScreen screens];
  if (![screens count])
    return 1.0f;

  NSScreen* screen = [screens objectAtIndex:0];
  if ([screen respondsToSelector:@selector(backingScaleFactor)])
    return [screen backingScaleFactor];
  return [screen userSpaceScaleFactor];
}

}  // namespace

namespace ui {

float GetScaleFactorForNativeView(gfx::NativeView view) {
  return GetScaleFactorScaleForNativeView(view);
}

}  // namespace ui
