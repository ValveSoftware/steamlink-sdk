// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/software_layer_mac.h"

#include "base/debug/trace_event.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/base/cocoa/animation_utils.h"

@implementation SoftwareLayer

- (id)init {
  if (self = [super init]) {
    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
  }
  return self;
}

- (void)setContentsToData:(const void *)data
             withRowBytes:(size_t)rowBytes
            withPixelSize:(gfx::Size)pixelSize
          withScaleFactor:(float)scaleFactor {
  TRACE_EVENT0("browser", "-[SoftwareLayer setContentsToData]");

  // Disable animating the contents change or the scale factor change.
  ScopedCAActionDisabler disabler;

  // Set the contents of the software CALayer to be a CGImage with the provided
  // pixel data. Make a copy of the data before backing the image with them,
  // because the same buffer will be reused for the next frame.
  base::ScopedCFTypeRef<CFDataRef> dataCopy(
      CFDataCreate(NULL,
                   static_cast<const UInt8 *>(data),
                   rowBytes * pixelSize.height()));
  base::ScopedCFTypeRef<CGDataProviderRef> dataProvider(
      CGDataProviderCreateWithCFData(dataCopy));
  base::ScopedCFTypeRef<CGImageRef> image(
      CGImageCreate(pixelSize.width(),
                    pixelSize.height(),
                    8,
                    32,
                    rowBytes,
                    base::mac::GetSystemColorSpace(),
                    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                    dataProvider,
                    NULL,
                    false,
                    kCGRenderingIntentDefault));
  [self setContents:(id)image.get()];

  // Set the contents scale of the software CALayer.
  if ([self respondsToSelector:(@selector(contentsScale))] &&
      [self respondsToSelector:(@selector(setContentsScale:))] &&
      [self contentsScale] != scaleFactor) {
    [self setContentsScale:scaleFactor];
  }
}

@end
