// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.h"

#import <AppKit/AppKit.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace {

// Draws an NSImage or an NSImageRep with a given size into a SkBitmap.
SkBitmap NSImageOrNSImageRepToSkBitmapWithColorSpace(
    NSImage* image,
    NSImageRep* image_rep,
    NSSize size,
    bool is_opaque,
    CGColorSpaceRef color_space) {
  // Only image or image_rep should be provided, not both.
  DCHECK((image != 0) ^ (image_rep != 0));

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(size.width, size.height, is_opaque))
    return bitmap;  // Return |bitmap| which should respond true to isNull().


  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      data,
      size.width,
      size.height,
      8,
      size.width * 4,
      color_space,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  // Something went really wrong. Best guess is that the bitmap data is invalid.
  DCHECK(context);

  [NSGraphicsContext saveGraphicsState];

  NSGraphicsContext* context_cocoa =
      [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
  [NSGraphicsContext setCurrentContext:context_cocoa];

  NSRect drawRect = NSMakeRect(0, 0, size.width, size.height);
  if (image) {
    [image drawInRect:drawRect
             fromRect:NSZeroRect
            operation:NSCompositeCopy
             fraction:1.0];
  } else {
    [image_rep drawInRect:drawRect
                 fromRect:NSZeroRect
                operation:NSCompositeCopy
                 fraction:1.0
           respectFlipped:NO
                    hints:nil];
  }

  [NSGraphicsContext restoreGraphicsState];

  return bitmap;
}

} // namespace

namespace skia {

CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix) {
  // CGAffineTransforms don't support perspective transforms, so make sure
  // we don't get those.
  DCHECK(matrix[SkMatrix::kMPersp0] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp1] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp2] == 1.0f);

  return CGAffineTransformMake(matrix[SkMatrix::kMScaleX],
                               matrix[SkMatrix::kMSkewY],
                               matrix[SkMatrix::kMSkewX],
                               matrix[SkMatrix::kMScaleY],
                               matrix[SkMatrix::kMTransX],
                               matrix[SkMatrix::kMTransY]);
}

SkRect CGRectToSkRect(const CGRect& rect) {
  SkRect sk_rect = {
    rect.origin.x, rect.origin.y, CGRectGetMaxX(rect), CGRectGetMaxY(rect)
  };
  return sk_rect;
}

CGRect SkIRectToCGRect(const SkIRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

CGRect SkRectToCGRect(const SkRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color) {
  DCHECK(CGColorGetNumberOfComponents(color) == 4);
  const CGFloat* components = CGColorGetComponents(color);
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * components[3]), // alpha
                        SkScalarRoundToInt(255.0 * components[0]), // red
                        SkScalarRoundToInt(255.0 * components[1]), // green
                        SkScalarRoundToInt(255.0 * components[2])); // blue
}

// Converts ARGB to CGColorRef.
CGColorRef CGColorCreateFromSkColor(SkColor color) {
  return CGColorCreateGenericRGB(SkColorGetR(color) / 255.0,
                                 SkColorGetG(color) / 255.0,
                                 SkColorGetB(color) / 255.0,
                                 SkColorGetA(color) / 255.0);
}

// Converts NSColor to ARGB
SkColor NSDeviceColorToSkColor(NSColor* color) {
  DCHECK([color colorSpace] == [NSColorSpace genericRGBColorSpace] ||
         [color colorSpace] == [NSColorSpace deviceRGBColorSpace]);
  CGFloat red, green, blue, alpha;
  color = [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * alpha),
                        SkScalarRoundToInt(255.0 * red),
                        SkScalarRoundToInt(255.0 * green),
                        SkScalarRoundToInt(255.0 * blue));
}

// Converts ARGB to NSColor.
NSColor* SkColorToCalibratedNSColor(SkColor color) {
  return [NSColor colorWithCalibratedRed:SkColorGetR(color) / 255.0
                                   green:SkColorGetG(color) / 255.0
                                    blue:SkColorGetB(color) / 255.0
                                   alpha:SkColorGetA(color) / 255.0];
}

NSColor* SkColorToDeviceNSColor(SkColor color) {
  return [NSColor colorWithDeviceRed:SkColorGetR(color) / 255.0
                               green:SkColorGetG(color) / 255.0
                                blue:SkColorGetB(color) / 255.0
                               alpha:SkColorGetA(color) / 255.0];
}

NSColor* SkColorToSRGBNSColor(SkColor color) {
  const CGFloat components[] = {
    SkColorGetR(color) / 255.0,
    SkColorGetG(color) / 255.0,
    SkColorGetB(color) / 255.0,
    SkColorGetA(color) / 255.0
  };
  return [NSColor colorWithColorSpace:[NSColorSpace sRGBColorSpace]
                           components:components
                                count:4];
}

SkBitmap CGImageToSkBitmap(CGImageRef image) {
  if (!image)
    return SkBitmap();

  int width = CGImageGetWidth(image);
  int height = CGImageGetHeight(image);

  sk_sp<SkCanvas> canvas(skia::CreatePlatformCanvas(
      nullptr, width, height, false, RETURN_NULL_ON_FAILURE));
  ScopedPlatformPaint p(canvas.get());
  CGContextRef context = p.GetPlatformSurface();

  // We need to invert the y-axis of the canvas so that Core Graphics drawing
  // happens right-side up. Skia has an upper-left origin and CG has a lower-
  // left one.
  CGContextScaleCTM(context, 1.0, -1.0);
  CGContextTranslateCTM(context, 0, -height);

  // We want to copy transparent pixels from |image|, instead of blending it
  // onto uninitialized pixels.
  CGContextSetBlendMode(context, kCGBlendModeCopy);

  CGRect rect = CGRectMake(0, 0, width, height);
  CGContextDrawImage(context, rect, image);

  return skia::ReadPixels(canvas.get());
}

SkBitmap NSImageToSkBitmapWithColorSpace(
    NSImage* image, bool is_opaque, CGColorSpaceRef color_space) {
  return NSImageOrNSImageRepToSkBitmapWithColorSpace(
      image, nil, [image size], is_opaque, color_space);
}

SkBitmap NSImageRepToSkBitmapWithColorSpace(NSImageRep* image_rep,
                                            NSSize size,
                                            bool is_opaque,
                                            CGColorSpaceRef color_space) {
  return NSImageOrNSImageRepToSkBitmapWithColorSpace(
      nil, image_rep, size, is_opaque, color_space);
}

NSBitmapImageRep* SkBitmapToNSBitmapImageRep(const SkBitmap& skiaBitmap) {
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  return SkBitmapToNSBitmapImageRepWithColorSpace(skiaBitmap, color_space);
}

NSBitmapImageRep* SkBitmapToNSBitmapImageRepWithColorSpace(
    const SkBitmap& skiaBitmap,
    CGColorSpaceRef colorSpace) {
  // First convert SkBitmap to CGImageRef.
  base::ScopedCFTypeRef<CGImageRef> cgimage(
      SkCreateCGImageRefWithColorspace(skiaBitmap, colorSpace));

  // Now convert to NSBitmapImageRep.
  base::scoped_nsobject<NSBitmapImageRep> bitmap(
      [[NSBitmapImageRep alloc] initWithCGImage:cgimage]);
  return [bitmap.release() autorelease];
}

NSImage* SkBitmapToNSImageWithColorSpace(const SkBitmap& skiaBitmap,
                                         CGColorSpaceRef colorSpace) {
  if (skiaBitmap.isNull())
    return nil;

  base::scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  [image addRepresentation:
      SkBitmapToNSBitmapImageRepWithColorSpace(skiaBitmap, colorSpace)];
  [image setSize:NSMakeSize(skiaBitmap.width(), skiaBitmap.height())];
  return [image.release() autorelease];
}

NSImage* SkBitmapToNSImage(const SkBitmap& skiaBitmap) {
  base::ScopedCFTypeRef<CGColorSpaceRef> colorSpace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  return SkBitmapToNSImageWithColorSpace(skiaBitmap, colorSpace.get());
}

SkiaBitLocker::SkiaBitLocker(SkCanvas* canvas,
                             const SkIRect& userClipRect,
                             SkScalar bitmapScaleFactor)
    : canvas_(canvas),
      cgContext_(0),
      bitmapScaleFactor_(bitmapScaleFactor),
      useDeviceBits_(false),
      bitmapIsDummy_(false) {
  canvas_->save();
  canvas_->clipRect(SkRect::MakeFromIRect(userClipRect));
}

SkiaBitLocker::~SkiaBitLocker() {
  releaseIfNeeded();
  canvas_->restore();
}

SkIRect SkiaBitLocker::computeDirtyRect() {
  // If the user specified a clip region, assume that it was tight and that the
  // dirty rect is approximately the whole bitmap.
  return SkIRect::MakeWH(offscreen_.width(), offscreen_.height());
}

// This must be called to balance calls to cgContext
void SkiaBitLocker::releaseIfNeeded() {
  if (!cgContext_)
    return;
  if (!useDeviceBits_ && !bitmapIsDummy_) {
    // Find the bits that were drawn to.
    SkIRect bounds = computeDirtyRect();
    SkBitmap subset;
    if (!offscreen_.extractSubset(&subset, bounds)) {
        return;
    }
    subset.setImmutable();  // Prevents a defensive copy inside Skia.
    canvas_->save();
    canvas_->setMatrix(SkMatrix::I());  // Reset back to device space.
    canvas_->translate(bounds.x() + bitmapOffset_.x(),
                       bounds.y() + bitmapOffset_.y());
    canvas_->scale(1.f / bitmapScaleFactor_, 1.f / bitmapScaleFactor_);
    canvas_->drawBitmap(subset, 0, 0);
    canvas_->restore();
  }
  CGContextRelease(cgContext_);
  cgContext_ = 0;
  useDeviceBits_ = false;
  bitmapIsDummy_ = false;
}

CGContextRef SkiaBitLocker::cgContext() {
  releaseIfNeeded(); // This flushes any prior bitmap use

  SkIRect clip_bounds;
  if (!canvas_->getClipDeviceBounds(&clip_bounds)) {
    // If the clip is empty, then there is nothing to draw. The caller may
    // attempt to draw (to-be-clipped) results, so ensure there is a dummy
    // non-NULL CGContext to use.
    bitmapIsDummy_ = true;
    clip_bounds = SkIRect::MakeXYWH(0, 0, 1, 1);
  }

  // remember the top/left, in case we need to compose this later
  bitmapOffset_.set(clip_bounds.x(), clip_bounds.y());

  // Now make clip_bounds be relative to the current layer/device
  if (!bitmapIsDummy_) {
    canvas_->temporary_internal_describeTopLayer(nullptr, &clip_bounds);
  }

  SkPixmap devicePixels;
  skia::GetWritablePixels(canvas_, &devicePixels);

  // Only draw directly if we have pixels, and we're only rect-clipped.
  // If not, we allocate an offscreen and draw into that, relying on the
  // compositing step to apply skia's clip.
  useDeviceBits_ = devicePixels.addr() &&
                   canvas_->isClipRect() &&
                   !bitmapIsDummy_;
  base::ScopedCFTypeRef<CGColorSpaceRef> colorSpace(
      CGColorSpaceCreateDeviceRGB());

  int displayHeight;
  if (useDeviceBits_) {
    SkPixmap subset;
    bool result = devicePixels.extractSubset(&subset, clip_bounds);
    DCHECK(result);
    if (!result)
      return 0;
    displayHeight = subset.height();
    cgContext_ = CGBitmapContextCreate(subset.writable_addr(), subset.width(),
      subset.height(), 8, subset.rowBytes(), colorSpace, 
      kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  } else {
    bool result = offscreen_.tryAllocN32Pixels(
        SkScalarCeilToInt(bitmapScaleFactor_ * clip_bounds.width()),
        SkScalarCeilToInt(bitmapScaleFactor_ * clip_bounds.height()));
    DCHECK(result);
    if (!result)
      return 0;
    offscreen_.eraseColor(0);
    displayHeight = offscreen_.height();
    cgContext_ = CGBitmapContextCreate(offscreen_.getPixels(),
      offscreen_.width(), offscreen_.height(), 8, offscreen_.rowBytes(),
      colorSpace, kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  }
  DCHECK(cgContext_);

  SkMatrix matrix = canvas_->getTotalMatrix();
  matrix.postTranslate(-SkIntToScalar(bitmapOffset_.x()),
                       -SkIntToScalar(bitmapOffset_.y()));
  matrix.postScale(bitmapScaleFactor_, -bitmapScaleFactor_);
  matrix.postTranslate(0, SkIntToScalar(displayHeight));

  CGContextConcatCTM(cgContext_, SkMatrixToCGAffineTransform(matrix));
  
  return cgContext_;
}

bool SkiaBitLocker::hasEmptyClipRegion() const {
  return canvas_->isClipEmpty();
}

}  // namespace skia
