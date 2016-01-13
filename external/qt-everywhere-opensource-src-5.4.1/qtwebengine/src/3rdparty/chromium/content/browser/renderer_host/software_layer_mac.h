// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_LAYER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_LAYER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/gfx/geometry/size.h"

@interface SoftwareLayer : CALayer
- (id)init;

- (void)setContentsToData:(const void *)data
             withRowBytes:(size_t)rowBytes
            withPixelSize:(gfx::Size)pixelSize
          withScaleFactor:(float)scaleFactor;
@end


#endif  // CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_LAYER_MAC_H_
