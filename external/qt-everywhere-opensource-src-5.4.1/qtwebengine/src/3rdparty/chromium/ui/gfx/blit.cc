// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/blit.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

#if defined(USE_CAIRO)
#if defined(OS_OPENBSD)
#include <cairo.h>
#elif defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include <cairo/cairo.h>
#endif
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#endif

namespace gfx {

namespace {

// Returns true if the given canvas has any part of itself clipped out or
// any non-identity tranform.
bool HasClipOrTransform(SkCanvas& canvas) {
  if (!canvas.getTotalMatrix().isIdentity())
    return true;

  if (!canvas.isClipRect())
    return true;

  // Now we know the clip is a regular rectangle, make sure it covers the
  // entire canvas.
  SkIRect clip_bounds;
  canvas.getClipDeviceBounds(&clip_bounds);

  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas.accessTopLayerPixels(&info, &row_bytes);
  DCHECK(pixels);
  if (!pixels)
    return true;

  if (clip_bounds.fLeft != 0 || clip_bounds.fTop != 0 ||
      clip_bounds.fRight != info.width() ||
      clip_bounds.fBottom != info.height())
    return true;

  return false;
}

}  // namespace

void BlitContextToContext(NativeDrawingContext dst_context,
                          const Rect& dst_rect,
                          NativeDrawingContext src_context,
                          const Point& src_origin) {
#if defined(OS_WIN)
  BitBlt(dst_context, dst_rect.x(), dst_rect.y(),
         dst_rect.width(), dst_rect.height(),
         src_context, src_origin.x(), src_origin.y(), SRCCOPY);
#elif defined(OS_MACOSX)
  // Only translations and/or vertical flips in the source context are
  // supported; more complex source context transforms will be ignored.

  // If there is a translation on the source context, we need to account for
  // it ourselves since CGBitmapContextCreateImage will bypass it.
  Rect src_rect(src_origin, dst_rect.size());
  CGAffineTransform transform = CGContextGetCTM(src_context);
  bool flipped = fabs(transform.d + 1) < 0.0001;
  CGFloat delta_y = flipped ? CGBitmapContextGetHeight(src_context) -
                              transform.ty
                            : transform.ty;
  src_rect.Offset(transform.tx, delta_y);

  base::ScopedCFTypeRef<CGImageRef> src_image(
      CGBitmapContextCreateImage(src_context));
  base::ScopedCFTypeRef<CGImageRef> src_sub_image(
      CGImageCreateWithImageInRect(src_image, src_rect.ToCGRect()));
  CGContextDrawImage(dst_context, dst_rect.ToCGRect(), src_sub_image);
#elif defined(USE_CAIRO)
  // Only translations in the source context are supported; more complex
  // source context transforms will be ignored.
  cairo_save(dst_context);
  double surface_x = src_origin.x();
  double surface_y = src_origin.y();
  cairo_user_to_device(src_context, &surface_x, &surface_y);
  cairo_set_source_surface(dst_context, cairo_get_target(src_context),
                           dst_rect.x()-surface_x, dst_rect.y()-surface_y);
  cairo_rectangle(dst_context, dst_rect.x(), dst_rect.y(),
                  dst_rect.width(), dst_rect.height());
  cairo_clip(dst_context);
  cairo_paint(dst_context);
  cairo_restore(dst_context);
#else
  NOTIMPLEMENTED();
#endif
}

void BlitContextToCanvas(SkCanvas *dst_canvas,
                         const Rect& dst_rect,
                         NativeDrawingContext src_context,
                         const Point& src_origin) {
  DCHECK(skia::SupportsPlatformPaint(dst_canvas));
  BlitContextToContext(skia::BeginPlatformPaint(dst_canvas), dst_rect,
                       src_context, src_origin);
  skia::EndPlatformPaint(dst_canvas);
}

void BlitCanvasToContext(NativeDrawingContext dst_context,
                         const Rect& dst_rect,
                         SkCanvas *src_canvas,
                         const Point& src_origin) {
  DCHECK(skia::SupportsPlatformPaint(src_canvas));
  BlitContextToContext(dst_context, dst_rect,
                       skia::BeginPlatformPaint(src_canvas), src_origin);
  skia::EndPlatformPaint(src_canvas);
}

void BlitCanvasToCanvas(SkCanvas *dst_canvas,
                        const Rect& dst_rect,
                        SkCanvas *src_canvas,
                        const Point& src_origin) {
  DCHECK(skia::SupportsPlatformPaint(dst_canvas));
  DCHECK(skia::SupportsPlatformPaint(src_canvas));
  BlitContextToContext(skia::BeginPlatformPaint(dst_canvas), dst_rect,
                       skia::BeginPlatformPaint(src_canvas), src_origin);
  skia::EndPlatformPaint(src_canvas);
  skia::EndPlatformPaint(dst_canvas);
}

void ScrollCanvas(SkCanvas* canvas,
                  const gfx::Rect& in_clip,
                  const gfx::Vector2d& offset) {
  DCHECK(!HasClipOrTransform(*canvas));  // Don't support special stuff.
#if defined(OS_WIN)
  // If we have a PlatformCanvas, we should use ScrollDC. Otherwise, fall
  // through to the software implementation.
  if (skia::SupportsPlatformPaint(canvas)) {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas);
    HDC hdc = scoped_platform_paint.GetPlatformSurface();

    RECT damaged_rect;
    RECT r = in_clip.ToRECT();
    ScrollDC(hdc, offset.x(), offset.y(), NULL, &r, NULL, &damaged_rect);
    return;
  }
#endif  // defined(OS_WIN)
  // For non-windows, always do scrolling in software.
  // Cairo has no nice scroll function so we do our own. On Mac it's possible to
  // use platform scroll code, but it's complex so we just use the same path
  // here. Either way it will be software-only, so it shouldn't matter much.
  SkBitmap& bitmap = const_cast<SkBitmap&>(
      skia::GetTopDevice(*canvas)->accessBitmap(true));
  SkAutoLockPixels lock(bitmap);

  // We expect all coords to be inside the canvas, so clip here.
  gfx::Rect clip = gfx::IntersectRects(
      in_clip, gfx::Rect(0, 0, bitmap.width(), bitmap.height()));

  // Compute the set of pixels we'll actually end up painting.
  gfx::Rect dest_rect = gfx::IntersectRects(clip + offset, clip);
  if (dest_rect.size().IsEmpty())
    return;  // Nothing to do.

  // Compute the source pixels that will map to the dest_rect
  gfx::Rect src_rect = dest_rect - offset;

  size_t row_bytes = dest_rect.width() * 4;
  if (offset.y() > 0) {
    // Data is moving down, copy from the bottom up.
    for (int y = dest_rect.height() - 1; y >= 0; y--) {
      memcpy(bitmap.getAddr32(dest_rect.x(), dest_rect.y() + y),
             bitmap.getAddr32(src_rect.x(), src_rect.y() + y),
             row_bytes);
    }
  } else if (offset.y() < 0) {
    // Data is moving up, copy from the top down.
    for (int y = 0; y < dest_rect.height(); y++) {
      memcpy(bitmap.getAddr32(dest_rect.x(), dest_rect.y() + y),
             bitmap.getAddr32(src_rect.x(), src_rect.y() + y),
             row_bytes);
    }
  } else if (offset.x() != 0) {
    // Horizontal-only scroll. We can do it in either top-to-bottom or bottom-
    // to-top, but have to be careful about the order for copying each row.
    // Fortunately, memmove already handles this for us.
    for (int y = 0; y < dest_rect.height(); y++) {
      memmove(bitmap.getAddr32(dest_rect.x(), dest_rect.y() + y),
              bitmap.getAddr32(src_rect.x(), src_rect.y() + y),
              row_bytes);
    }
  }
}

}  // namespace gfx
