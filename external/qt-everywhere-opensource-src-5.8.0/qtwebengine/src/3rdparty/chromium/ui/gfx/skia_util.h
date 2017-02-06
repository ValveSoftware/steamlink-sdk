// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

class SkBitmap;
class SkDrawLooper;

namespace gfx {

class ImageSkiaRep;
class Point;
class PointF;
class Rect;
class RectF;
class ShadowValue;
class Transform;

// Convert between Skia and gfx types.
GFX_EXPORT SkPoint PointToSkPoint(const Point& point);
GFX_EXPORT SkIPoint PointToSkIPoint(const Point& point);
GFX_EXPORT SkPoint PointFToSkPoint(const PointF& point);
GFX_EXPORT SkRect RectToSkRect(const Rect& rect);
GFX_EXPORT SkIRect RectToSkIRect(const Rect& rect);
GFX_EXPORT Rect SkIRectToRect(const SkIRect& rect);
GFX_EXPORT SkRect RectFToSkRect(const RectF& rect);
GFX_EXPORT RectF SkRectToRectF(const SkRect& rect);
GFX_EXPORT SkSize SizeFToSkSize(const SizeF& size);
GFX_EXPORT SizeF SkSizeToSizeF(const SkSize& size);
GFX_EXPORT Size SkISizeToSize(const SkISize& size);

GFX_EXPORT void QuadFToSkPoints(const gfx::QuadF& quad, SkPoint points[4]);

GFX_EXPORT void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                             SkMatrix* flattened);

// Creates a bitmap shader for the image rep with the image rep's scale factor.
// Sets the created shader's local matrix such that it displays the image rep at
// the correct scale factor.
// The shader's local matrix should not be changed after the shader is created.
// TODO(pkotwicz): Allow shader's local matrix to be changed after the shader
// is created.
//
GFX_EXPORT sk_sp<SkShader> CreateImageRepShader(
    const gfx::ImageSkiaRep& image_rep,
    SkShader::TileMode tile_mode,
    const SkMatrix& local_matrix);

// Creates a bitmap shader for the image rep with the passed in scale factor.
GFX_EXPORT sk_sp<SkShader> CreateImageRepShaderForScale(
    const gfx::ImageSkiaRep& image_rep,
    SkShader::TileMode tile_mode,
    const SkMatrix& local_matrix,
    SkScalar scale);

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
GFX_EXPORT sk_sp<SkShader> CreateGradientShader(int start_point,
                                                int end_point,
                                                SkColor start_color,
                                                SkColor end_color);

// Creates a draw looper to generate |shadows|. The caller owns the draw looper.
// NULL is returned if |shadows| is empty since no draw looper is needed in
// this case.
// DEPRECATED: See below. TODO(estade): remove this: crbug.com/624175
GFX_EXPORT sk_sp<SkDrawLooper> CreateShadowDrawLooper(
    const std::vector<ShadowValue>& shadows);

// Creates a draw looper to generate |shadows|. This creates a looper with the
// correct amount of blur. Callers of the existing CreateShadowDrawLooper may
// rely on the wrong amount of blur being applied but new code should use this
// function.
GFX_EXPORT sk_sp<SkDrawLooper> CreateShadowDrawLooperCorrectBlur(
    const std::vector<ShadowValue>& shadows);

// Returns true if the two bitmaps contain the same pixels.
GFX_EXPORT bool BitmapsAreEqual(const SkBitmap& bitmap1,
                                const SkBitmap& bitmap2);

// Converts Skia ARGB format pixels in |skia| to RGBA.
GFX_EXPORT void ConvertSkiaToRGBA(const unsigned char* skia,
                                  int pixel_width,
                                  unsigned char* rgba);

// Converts a Skia floating-point value to an int appropriate for hb_position_t.
GFX_EXPORT int SkiaScalarToHarfBuzzUnits(SkScalar value);

// Converts an hb_position_t to a Skia floating-point value.
GFX_EXPORT SkScalar HarfBuzzUnitsToSkiaScalar(int value);

// Converts an hb_position_t to a float.
GFX_EXPORT float HarfBuzzUnitsToFloat(int value);

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTIL_H_
