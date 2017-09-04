/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/skia/SkiaUtils.h"

#include "platform/graphics/GraphicsContext.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"

namespace blink {

static const struct CompositOpToXfermodeMode {
  CompositeOperator mCompositOp;
  SkBlendMode m_xfermodeMode;
} gMapCompositOpsToXfermodeModes[] = {
    {CompositeClear, SkBlendMode::kClear},
    {CompositeCopy, SkBlendMode::kSrc},
    {CompositeSourceOver, SkBlendMode::kSrcOver},
    {CompositeSourceIn, SkBlendMode::kSrcIn},
    {CompositeSourceOut, SkBlendMode::kSrcOut},
    {CompositeSourceAtop, SkBlendMode::kSrcATop},
    {CompositeDestinationOver, SkBlendMode::kDstOver},
    {CompositeDestinationIn, SkBlendMode::kDstIn},
    {CompositeDestinationOut, SkBlendMode::kDstOut},
    {CompositeDestinationAtop, SkBlendMode::kDstATop},
    {CompositeXOR, SkBlendMode::kXor},
    {CompositePlusLighter, SkBlendMode::kPlus}};

// Keep this array in sync with the WebBlendMode enum in
// public/platform/WebBlendMode.h.
static const SkBlendMode gMapBlendOpsToXfermodeModes[] = {
    SkBlendMode::kClear,       // WebBlendModeNormal
    SkBlendMode::kMultiply,    // WebBlendModeMultiply
    SkBlendMode::kScreen,      // WebBlendModeScreen
    SkBlendMode::kOverlay,     // WebBlendModeOverlay
    SkBlendMode::kDarken,      // WebBlendModeDarken
    SkBlendMode::kLighten,     // WebBlendModeLighten
    SkBlendMode::kColorDodge,  // WebBlendModeColorDodge
    SkBlendMode::kColorBurn,   // WebBlendModeColorBurn
    SkBlendMode::kHardLight,   // WebBlendModeHardLight
    SkBlendMode::kSoftLight,   // WebBlendModeSoftLight
    SkBlendMode::kDifference,  // WebBlendModeDifference
    SkBlendMode::kExclusion,   // WebBlendModeExclusion
    SkBlendMode::kHue,         // WebBlendModeHue
    SkBlendMode::kSaturation,  // WebBlendModeSaturation
    SkBlendMode::kColor,       // WebBlendModeColor
    SkBlendMode::kLuminosity   // WebBlendModeLuminosity
};

SkBlendMode WebCoreCompositeToSkiaComposite(CompositeOperator op,
                                            WebBlendMode blendMode) {
  ASSERT(op == CompositeSourceOver || blendMode == WebBlendModeNormal);
  if (blendMode != WebBlendModeNormal) {
    if (static_cast<uint8_t>(blendMode) >=
        SK_ARRAY_COUNT(gMapBlendOpsToXfermodeModes)) {
      SkDEBUGF(
          ("GraphicsContext::setPlatformCompositeOperation unknown "
           "WebBlendMode %d\n",
           blendMode));
      return SkBlendMode::kSrcOver;
    }
    return gMapBlendOpsToXfermodeModes[static_cast<uint8_t>(blendMode)];
  }

  const CompositOpToXfermodeMode* table = gMapCompositOpsToXfermodeModes;
  if (static_cast<uint8_t>(op) >=
      SK_ARRAY_COUNT(gMapCompositOpsToXfermodeModes)) {
    SkDEBUGF(
        ("GraphicsContext::setPlatformCompositeOperation unknown "
         "CompositeOperator %d\n",
         op));
    return SkBlendMode::kSrcOver;
  }
  SkASSERT(table[static_cast<uint8_t>(op)].mCompositOp == op);
  return table[static_cast<uint8_t>(op)].m_xfermodeMode;
}

CompositeOperator compositeOperatorFromSkia(SkBlendMode xferMode) {
  switch (xferMode) {
    case SkBlendMode::kClear:
      return CompositeClear;
    case SkBlendMode::kSrc:
      return CompositeCopy;
    case SkBlendMode::kSrcOver:
      return CompositeSourceOver;
    case SkBlendMode::kSrcIn:
      return CompositeSourceIn;
    case SkBlendMode::kSrcOut:
      return CompositeSourceOut;
    case SkBlendMode::kSrcATop:
      return CompositeSourceAtop;
    case SkBlendMode::kDstOver:
      return CompositeDestinationOver;
    case SkBlendMode::kDstIn:
      return CompositeDestinationIn;
    case SkBlendMode::kDstOut:
      return CompositeDestinationOut;
    case SkBlendMode::kDstATop:
      return CompositeDestinationAtop;
    case SkBlendMode::kXor:
      return CompositeXOR;
    case SkBlendMode::kPlus:
      return CompositePlusLighter;
    default:
      break;
  }
  return CompositeSourceOver;
}

WebBlendMode blendModeFromSkia(SkBlendMode xferMode) {
  switch (xferMode) {
    case SkBlendMode::kSrcOver:
      return WebBlendModeNormal;
    case SkBlendMode::kMultiply:
      return WebBlendModeMultiply;
    case SkBlendMode::kScreen:
      return WebBlendModeScreen;
    case SkBlendMode::kOverlay:
      return WebBlendModeOverlay;
    case SkBlendMode::kDarken:
      return WebBlendModeDarken;
    case SkBlendMode::kLighten:
      return WebBlendModeLighten;
    case SkBlendMode::kColorDodge:
      return WebBlendModeColorDodge;
    case SkBlendMode::kColorBurn:
      return WebBlendModeColorBurn;
    case SkBlendMode::kHardLight:
      return WebBlendModeHardLight;
    case SkBlendMode::kSoftLight:
      return WebBlendModeSoftLight;
    case SkBlendMode::kDifference:
      return WebBlendModeDifference;
    case SkBlendMode::kExclusion:
      return WebBlendModeExclusion;
    case SkBlendMode::kHue:
      return WebBlendModeHue;
    case SkBlendMode::kSaturation:
      return WebBlendModeSaturation;
    case SkBlendMode::kColor:
      return WebBlendModeColor;
    case SkBlendMode::kLuminosity:
      return WebBlendModeLuminosity;
    default:
      break;
  }
  return WebBlendModeNormal;
}

SkMatrix affineTransformToSkMatrix(const AffineTransform& source) {
  SkMatrix result;

  result.setScaleX(WebCoreDoubleToSkScalar(source.a()));
  result.setSkewX(WebCoreDoubleToSkScalar(source.c()));
  result.setTranslateX(WebCoreDoubleToSkScalar(source.e()));

  result.setScaleY(WebCoreDoubleToSkScalar(source.d()));
  result.setSkewY(WebCoreDoubleToSkScalar(source.b()));
  result.setTranslateY(WebCoreDoubleToSkScalar(source.f()));

  // FIXME: Set perspective properly.
  result.setPerspX(0);
  result.setPerspY(0);
  result.set(SkMatrix::kMPersp2, SK_Scalar1);

  return result;
}

bool nearlyIntegral(float value) {
  return fabs(value - floorf(value)) < std::numeric_limits<float>::epsilon();
}

InterpolationQuality limitInterpolationQuality(
    const GraphicsContext& context,
    InterpolationQuality resampling) {
  return std::min(resampling, context.imageInterpolationQuality());
}

InterpolationQuality computeInterpolationQuality(float srcWidth,
                                                 float srcHeight,
                                                 float destWidth,
                                                 float destHeight,
                                                 bool isDataComplete) {
  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  const float kFractionalChangeThreshold = 0.025f;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  float diffWidth = fabs(destWidth - srcWidth);
  float diffHeight = fabs(destHeight - srcHeight);
  bool widthNearlyEqual = diffWidth < std::numeric_limits<float>::epsilon();
  bool heightNearlyEqual = diffHeight < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (widthNearlyEqual && heightNearlyEqual)
    return InterpolationNone;

  if (srcWidth <= kSmallImageSizeThreshold ||
      srcHeight <= kSmallImageSizeThreshold ||
      destWidth <= kSmallImageSizeThreshold ||
      destHeight <= kSmallImageSizeThreshold) {
    // Small image detected.

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!nearlyIntegral(destWidth) &&
         srcWidth > 1 + std::numeric_limits<float>::epsilon()) ||
        (!nearlyIntegral(destHeight) &&
         srcHeight > 1 + std::numeric_limits<float>::epsilon()))
      return InterpolationLow;

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return InterpolationNone;
  }

  if (srcHeight * kLargeStretch <= destHeight ||
      srcWidth * kLargeStretch <= destWidth) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (widthNearlyEqual || heightNearlyEqual)
      return InterpolationNone;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return InterpolationLow;
  }

  if ((diffWidth / srcWidth < kFractionalChangeThreshold) &&
      (diffHeight / srcHeight < kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return InterpolationNone;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!isDataComplete)
    return InterpolationLow;

  // Everything else gets resampled at high quality.
  return InterpolationHigh;
}

int clampedAlphaForBlending(float alpha) {
  if (alpha < 0)
    return 0;
  int roundedAlpha = roundf(alpha * 256);
  if (roundedAlpha > 256)
    roundedAlpha = 256;
  return roundedAlpha;
}

SkColor scaleAlpha(SkColor color, float alpha) {
  return scaleAlpha(color, clampedAlphaForBlending(alpha));
}

SkColor scaleAlpha(SkColor color, int alpha) {
  int a = (SkColorGetA(color) * alpha) >> 8;
  return (color & 0x00FFFFFF) | (a << 24);
}

template <typename PrimitiveType>
void drawFocusRingPrimitive(const PrimitiveType&,
                            SkCanvas*,
                            const SkPaint&,
                            float cornerRadius) {
  ASSERT_NOT_REACHED();  // Missing an explicit specialization?
}

template <>
void drawFocusRingPrimitive<SkRect>(const SkRect& rect,
                                    SkCanvas* canvas,
                                    const SkPaint& paint,
                                    float cornerRadius) {
  SkRRect rrect;
  rrect.setRectXY(rect, SkFloatToScalar(cornerRadius),
                  SkFloatToScalar(cornerRadius));
  canvas->drawRRect(rrect, paint);
}

template <>
void drawFocusRingPrimitive<SkPath>(const SkPath& path,
                                    SkCanvas* canvas,
                                    const SkPaint& paint,
                                    float cornerRadius) {
  SkPaint pathPaint = paint;
  pathPaint.setPathEffect(
      SkCornerPathEffect::Make(SkFloatToScalar(cornerRadius)));
  canvas->drawPath(path, pathPaint);
}

template <typename PrimitiveType>
void drawPlatformFocusRing(const PrimitiveType& primitive,
                           SkCanvas* canvas,
                           SkColor color,
                           float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(color);
  paint.setStrokeWidth(width);

#if OS(MACOSX)
  paint.setAlpha(64);
  const float cornerRadius = (width - 1) * 0.5f;
#else
  const float cornerRadius = width;
#endif

  drawFocusRingPrimitive(primitive, canvas, paint, cornerRadius);

#if OS(MACOSX)
  // Inner part
  paint.setAlpha(128);
  paint.setStrokeWidth(paint.getStrokeWidth() * 0.5f);
  drawFocusRingPrimitive(primitive, canvas, paint, cornerRadius);
#endif
}

template void PLATFORM_EXPORT drawPlatformFocusRing<SkRect>(const SkRect&,
                                                            SkCanvas*,
                                                            SkColor,
                                                            float width);
template void PLATFORM_EXPORT drawPlatformFocusRing<SkPath>(const SkPath&,
                                                            SkCanvas*,
                                                            SkColor,
                                                            float width);

}  // namespace blink
