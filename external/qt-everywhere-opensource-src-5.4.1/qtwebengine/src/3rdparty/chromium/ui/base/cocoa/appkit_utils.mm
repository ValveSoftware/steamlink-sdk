// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/appkit_utils.h"

#include "ui/base/resource/resource_bundle.h"

namespace {

// Gets an NSImage given an image id.
NSImage* GetImage(int image_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(image_id)
      .ToNSImage();
}

}  // namespace

namespace ui {

void DrawNinePartImage(NSRect frame,
                       const NinePartImageIds& image_ids,
                       NSCompositingOperation operation,
                       CGFloat alpha,
                       BOOL flipped) {
  NSDrawNinePartImage(frame,
                      GetImage(image_ids.top_left),
                      GetImage(image_ids.top),
                      GetImage(image_ids.top_right),
                      GetImage(image_ids.left),
                      GetImage(image_ids.center),
                      GetImage(image_ids.right),
                      GetImage(image_ids.bottom_left),
                      GetImage(image_ids.bottom),
                      GetImage(image_ids.bottom_right),
                      operation,
                      alpha,
                      flipped);
}

}  // namespace ui
