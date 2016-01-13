/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "config.h"
#include "platform/graphics/skia/NativeImageSkia.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkShader.h"

#include <algorithm>
#include <math.h>
#include <limits>

namespace WebCore {

static bool nearlyIntegral(float value)
{
    return fabs(value - floorf(value)) < std::numeric_limits<float>::epsilon();
}

InterpolationQuality NativeImageSkia::computeInterpolationQuality(const SkMatrix& matrix, float srcWidth, float srcHeight, float destWidth, float destHeight) const
{
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

    if (srcWidth <= kSmallImageSizeThreshold
        || srcHeight <= kSmallImageSizeThreshold
        || destWidth <= kSmallImageSizeThreshold
        || destHeight <= kSmallImageSizeThreshold) {
        // Small image detected.

        // Resample in the case where the new size would be non-integral.
        // This can cause noticeable breaks in repeating patterns, except
        // when the source image is only one pixel wide in that dimension.
        if ((!nearlyIntegral(destWidth) && srcWidth > 1 + std::numeric_limits<float>::epsilon())
            || (!nearlyIntegral(destHeight) && srcHeight > 1 + std::numeric_limits<float>::epsilon()))
            return InterpolationLow;

        // Otherwise, don't resample small images. These are often used for
        // borders and rules (think 1x1 images used to make lines).
        return InterpolationNone;
    }

    if (srcHeight * kLargeStretch <= destHeight || srcWidth * kLargeStretch <= destWidth) {
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

    if ((diffWidth / srcWidth < kFractionalChangeThreshold)
        && (diffHeight / srcHeight < kFractionalChangeThreshold)) {
        // It is disappointingly common on the web for image sizes to be off by
        // one or two pixels. We don't bother resampling if the size difference
        // is a small fraction of the original size.
        return InterpolationNone;
    }

    // When the image is not yet done loading, use linear. We don't cache the
    // partially resampled images, and as they come in incrementally, it causes
    // us to have to resample the whole thing every time.
    if (!isDataComplete())
        return InterpolationLow;

    // Everything else gets resampled.
    // High quality interpolation only enabled for scaling and translation.
    if (!(matrix.getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
        return InterpolationHigh;

    return InterpolationLow;
}

static InterpolationQuality limitInterpolationQuality(GraphicsContext* context, InterpolationQuality resampling)
{
    return std::min(resampling, context->imageInterpolationQuality());
}

static SkPaint::FilterLevel convertToSkiaFilterLevel(bool useBicubicFilter, InterpolationQuality resampling)
{
    // FIXME: If we get rid of this special case, this function can go away entirely.
    if (useBicubicFilter)
        return SkPaint::kHigh_FilterLevel;

    // InterpolationHigh if useBicubicFilter is false means that we do
    // a manual high quality resampling before drawing to Skia.
    if (resampling == InterpolationHigh)
        return SkPaint::kNone_FilterLevel;

    return static_cast<SkPaint::FilterLevel>(resampling);
}

// This function is used to scale an image and extract a scaled fragment.
//
// ALGORITHM
//
// Because the scaled image size has to be integers, we approximate the real
// scale with the following formula (only X direction is shown):
//
// scaledImageWidth = round(scaleX * imageRect.width())
// approximateScaleX = scaledImageWidth / imageRect.width()
//
// With this method we maintain a constant scale factor among fragments in
// the scaled image. This allows fragments to stitch together to form the
// full scaled image. The downside is there will be a small difference
// between |scaleX| and |approximateScaleX|.
//
// A scaled image fragment is identified by:
//
// - Scaled image size
// - Scaled image fragment rectangle (IntRect)
//
// Scaled image size has been determined and the next step is to compute the
// rectangle for the scaled image fragment which needs to be an IntRect.
//
// scaledSrcRect = srcRect * (approximateScaleX, approximateScaleY)
// enclosingScaledSrcRect = enclosingIntRect(scaledSrcRect)
//
// Finally we extract the scaled image fragment using
// (scaledImageSize, enclosingScaledSrcRect).
//
SkBitmap NativeImageSkia::extractScaledImageFragment(const SkRect& srcRect, float scaleX, float scaleY, SkRect* scaledSrcRect) const
{
    SkISize imageSize = SkISize::Make(bitmap().width(), bitmap().height());
    SkISize scaledImageSize = SkISize::Make(clampToInteger(roundf(imageSize.width() * scaleX)),
        clampToInteger(roundf(imageSize.height() * scaleY)));

    SkRect imageRect = SkRect::MakeWH(imageSize.width(), imageSize.height());
    SkRect scaledImageRect = SkRect::MakeWH(scaledImageSize.width(), scaledImageSize.height());

    SkMatrix scaleTransform;
    scaleTransform.setRectToRect(imageRect, scaledImageRect, SkMatrix::kFill_ScaleToFit);
    scaleTransform.mapRect(scaledSrcRect, srcRect);

    scaledSrcRect->intersect(scaledImageRect);
    SkIRect enclosingScaledSrcRect = enclosingIntRect(*scaledSrcRect);

    // |enclosingScaledSrcRect| can be larger than |scaledImageSize| because
    // of float inaccuracy so clip to get inside.
    enclosingScaledSrcRect.intersect(SkIRect::MakeSize(scaledImageSize));

    // scaledSrcRect is relative to the pixel snapped fragment we're extracting.
    scaledSrcRect->offset(-enclosingScaledSrcRect.x(), -enclosingScaledSrcRect.y());

    return resizedBitmap(scaledImageSize, enclosingScaledSrcRect);
}

// This does a lot of computation to resample only the portion of the bitmap
// that will only be drawn. This is critical for performance since when we are
// scrolling, for example, we are only drawing a small strip of the image.
// Resampling the whole image every time is very slow, so this speeds up things
// dramatically.
//
// Note: this code is only used when the canvas transformation is limited to
// scaling or translation.
void NativeImageSkia::drawResampledBitmap(GraphicsContext* context, SkPaint& paint, const SkRect& srcRect, const SkRect& destRect) const
{
    TRACE_EVENT0("skia", "drawResampledBitmap");
    if (context->paintingDisabled())
        return;
    // We want to scale |destRect| with transformation in the canvas to obtain
    // the final scale. The final scale is a combination of scale transform
    // in canvas and explicit scaling (srcRect and destRect).
    SkRect screenRect;
    context->getTotalMatrix().mapRect(&screenRect, destRect);
    float realScaleX = screenRect.width() / srcRect.width();
    float realScaleY = screenRect.height() / srcRect.height();

    // This part of code limits scaling only to visible portion in the
    SkRect destRectVisibleSubset;
    if (!context->canvas()->getClipBounds(&destRectVisibleSubset))
        return;

    // ClipRectToCanvas often overshoots, resulting in a larger region than our
    // original destRect. Intersecting gets us back inside.
    if (!destRectVisibleSubset.intersect(destRect))
        return; // Nothing visible in destRect.

    // Find the corresponding rect in the source image.
    SkMatrix destToSrcTransform;
    SkRect srcRectVisibleSubset;
    destToSrcTransform.setRectToRect(destRect, srcRect, SkMatrix::kFill_ScaleToFit);
    destToSrcTransform.mapRect(&srcRectVisibleSubset, destRectVisibleSubset);

    SkRect scaledSrcRect;
    SkBitmap scaledImageFragment = extractScaledImageFragment(srcRectVisibleSubset, realScaleX, realScaleY, &scaledSrcRect);

    context->drawBitmapRect(scaledImageFragment, &scaledSrcRect, destRectVisibleSubset, &paint);
}

NativeImageSkia::NativeImageSkia()
    : m_resizeRequests(0)
{
}

NativeImageSkia::NativeImageSkia(const SkBitmap& other)
    : m_image(other)
    , m_resizeRequests(0)
{
}

NativeImageSkia::NativeImageSkia(const SkBitmap& image, const SkBitmap& resizedImage, const ImageResourceInfo& cachedImageInfo, int resizeRequests)
    : m_image(image)
    , m_resizedImage(resizedImage)
    , m_cachedImageInfo(cachedImageInfo)
    , m_resizeRequests(resizeRequests)
{
}

NativeImageSkia::~NativeImageSkia()
{
}

int NativeImageSkia::decodedSize() const
{
    return m_image.getSize() + m_resizedImage.getSize();
}

bool NativeImageSkia::hasResizedBitmap(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
    bool imageScaleEqual = m_cachedImageInfo.scaledImageSize == scaledImageSize;
    bool scaledImageSubsetAvailable = m_cachedImageInfo.scaledImageSubset.contains(scaledImageSubset);
    return imageScaleEqual && scaledImageSubsetAvailable && !m_resizedImage.empty();
}

SkBitmap NativeImageSkia::resizedBitmap(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
    ASSERT(!DeferredImageDecoder::isLazyDecoded(m_image));

    if (!hasResizedBitmap(scaledImageSize, scaledImageSubset)) {
        bool shouldCache = isDataComplete()
            && shouldCacheResampling(scaledImageSize, scaledImageSubset);

        TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResizeImage", "cached", shouldCache);
        // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
        PlatformInstrumentation::willResizeImage(shouldCache);
        SkBitmap resizedImage = skia::ImageOperations::Resize(m_image, skia::ImageOperations::RESIZE_LANCZOS3, scaledImageSize.width(), scaledImageSize.height(), scaledImageSubset);
        resizedImage.setImmutable();
        PlatformInstrumentation::didResizeImage();

        if (!shouldCache)
            return resizedImage;

        m_resizedImage = resizedImage;
    }

    SkBitmap resizedSubset;
    SkIRect resizedSubsetRect = m_cachedImageInfo.rectInSubset(scaledImageSubset);
    m_resizedImage.extractSubset(&resizedSubset, resizedSubsetRect);
    return resizedSubset;
}

static bool shouldDrawAntiAliased(GraphicsContext* context, const SkRect& destRect)
{
    if (!context->shouldAntialias())
        return false;
    const SkMatrix totalMatrix = context->getTotalMatrix();
    // Don't disable anti-aliasing if we're rotated or skewed.
    if (!totalMatrix.rectStaysRect())
        return true;
    // Disable anti-aliasing for scales or n*90 degree rotations.
    // Allow to opt out of the optimization though for "hairline" geometry
    // images - using the shouldAntialiasHairlineImages() GraphicsContext flag.
    if (!context->shouldAntialiasHairlineImages())
        return false;
    // Check if the dimensions of the destination are "small" (less than one
    // device pixel). To prevent sudden drop-outs. Since we know that
    // kRectStaysRect_Mask is set, the matrix either has scale and no skew or
    // vice versa. We can query the kAffine_Mask flag to determine which case
    // it is.
    // FIXME: This queries the CTM while drawing, which is generally
    // discouraged. Always drawing with AA can negatively impact performance
    // though - that's why it's not always on.
    SkScalar widthExpansion, heightExpansion;
    if (totalMatrix.getType() & SkMatrix::kAffine_Mask)
        widthExpansion = totalMatrix[SkMatrix::kMSkewY], heightExpansion = totalMatrix[SkMatrix::kMSkewX];
    else
        widthExpansion = totalMatrix[SkMatrix::kMScaleX], heightExpansion = totalMatrix[SkMatrix::kMScaleY];
    return destRect.width() * fabs(widthExpansion) < 1 || destRect.height() * fabs(heightExpansion) < 1;
}

void NativeImageSkia::draw(GraphicsContext* context, const SkRect& srcRect, const SkRect& destRect, PassRefPtr<SkXfermode> compOp) const
{
    TRACE_EVENT0("skia", "NativeImageSkia::draw");
    SkPaint paint;
    paint.setXfermode(compOp.get());
    paint.setColorFilter(context->colorFilter());
    paint.setAlpha(context->getNormalizedAlpha());
    paint.setLooper(context->drawLooper());
    paint.setAntiAlias(shouldDrawAntiAliased(context, destRect));

    bool isLazyDecoded = DeferredImageDecoder::isLazyDecoded(bitmap());

    InterpolationQuality resampling;
    if (context->isAccelerated()) {
        resampling = InterpolationLow;
    } else if (context->printing()) {
        resampling = InterpolationNone;
    } else if (isLazyDecoded) {
        resampling = InterpolationHigh;
    } else {
        // Take into account scale applied to the canvas when computing sampling mode (e.g. CSS scale or page scale).
        SkRect destRectTarget = destRect;
        SkMatrix totalMatrix = context->getTotalMatrix();
        if (!(totalMatrix.getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
            totalMatrix.mapRect(&destRectTarget, destRect);

        resampling = computeInterpolationQuality(totalMatrix,
            SkScalarToFloat(srcRect.width()), SkScalarToFloat(srcRect.height()),
            SkScalarToFloat(destRectTarget.width()), SkScalarToFloat(destRectTarget.height()));
    }

    if (resampling == InterpolationNone) {
        // FIXME: This is to not break tests (it results in the filter bitmap flag
        // being set to true). We need to decide if we respect InterpolationNone
        // being returned from computeInterpolationQuality.
        resampling = InterpolationLow;
    }
    resampling = limitInterpolationQuality(context, resampling);

    // FIXME: Bicubic filtering in Skia is only applied to defer-decoded images
    // as an experiment. Once this filtering code path becomes stable we should
    // turn this on for all cases, including non-defer-decoded images.
    bool useBicubicFilter = resampling == InterpolationHigh && isLazyDecoded;

    paint.setFilterLevel(convertToSkiaFilterLevel(useBicubicFilter, resampling));

    if (resampling == InterpolationHigh && !useBicubicFilter) {
        // Resample the image and then draw the result to canvas with bilinear
        // filtering.
        drawResampledBitmap(context, paint, srcRect, destRect);
    } else {
        // We want to filter it if we decided to do interpolation above, or if
        // there is something interesting going on with the matrix (like a rotation).
        // Note: for serialization, we will want to subset the bitmap first so we
        // don't send extra pixels.
        context->drawBitmapRect(bitmap(), &srcRect, destRect, &paint);
    }
    if (isLazyDecoded)
        PlatformInstrumentation::didDrawLazyPixelRef(bitmap().getGenerationID());
    context->didDrawRect(destRect, paint, &bitmap());
}

static SkBitmap createBitmapWithSpace(const SkBitmap& bitmap, int spaceWidth, int spaceHeight)
{
    SkImageInfo info = bitmap.info();
    info.fWidth += spaceWidth;
    info.fHeight += spaceHeight;
    info.fAlphaType = kPremul_SkAlphaType;

    SkBitmap result;
    result.allocPixels(info);
    result.eraseColor(SK_ColorTRANSPARENT);
    bitmap.copyPixelsTo(reinterpret_cast<uint8_t*>(result.getPixels()), result.rowBytes() * result.height(), result.rowBytes());

    return result;
}

void NativeImageSkia::drawPattern(
    GraphicsContext* context,
    const FloatRect& floatSrcRect,
    const FloatSize& scale,
    const FloatPoint& phase,
    CompositeOperator compositeOp,
    const FloatRect& destRect,
    blink::WebBlendMode blendMode,
    const IntSize& repeatSpacing) const
{
    FloatRect normSrcRect = floatSrcRect;
    normSrcRect.intersect(FloatRect(0, 0, bitmap().width(), bitmap().height()));
    if (destRect.isEmpty() || normSrcRect.isEmpty())
        return; // nothing to draw

    SkMatrix totalMatrix = context->getTotalMatrix();
    AffineTransform ctm = context->getCTM();
    SkScalar ctmScaleX = ctm.xScale();
    SkScalar ctmScaleY = ctm.yScale();
    totalMatrix.preScale(scale.width(), scale.height());

    // Figure out what size the bitmap will be in the destination. The
    // destination rect is the bounds of the pattern, we need to use the
    // matrix to see how big it will be.
    SkRect destRectTarget;
    totalMatrix.mapRect(&destRectTarget, normSrcRect);

    float destBitmapWidth = SkScalarToFloat(destRectTarget.width());
    float destBitmapHeight = SkScalarToFloat(destRectTarget.height());

    bool isLazyDecoded = DeferredImageDecoder::isLazyDecoded(bitmap());

    // Compute the resampling mode.
    InterpolationQuality resampling;
    if (context->isAccelerated() || context->printing())
        resampling = InterpolationLow;
    else if (isLazyDecoded)
        resampling = InterpolationHigh;
    else
        resampling = computeInterpolationQuality(totalMatrix, normSrcRect.width(), normSrcRect.height(), destBitmapWidth, destBitmapHeight);
    resampling = limitInterpolationQuality(context, resampling);

    SkMatrix localMatrix;
    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the pattern. If WebKit wants
    // a shifted image, it will shift it from there using the localMatrix.
    const float adjustedX = phase.x() + normSrcRect.x() * scale.width();
    const float adjustedY = phase.y() + normSrcRect.y() * scale.height();
    localMatrix.setTranslate(SkFloatToScalar(adjustedX), SkFloatToScalar(adjustedY));

    RefPtr<SkShader> shader;

    // Bicubic filter is only applied to defer-decoded images, see
    // NativeImageSkia::draw for details.
    bool useBicubicFilter = resampling == InterpolationHigh && isLazyDecoded;

    if (resampling == InterpolationHigh && !useBicubicFilter) {
        // Do nice resampling.
        float scaleX = destBitmapWidth / normSrcRect.width();
        float scaleY = destBitmapHeight / normSrcRect.height();
        SkRect scaledSrcRect;

        // Since we are resizing the bitmap, we need to remove the scale
        // applied to the pixels in the bitmap shader. This means we need
        // CTM * localMatrix to have identity scale. Since we
        // can't modify CTM (or the rectangle will be drawn in the wrong
        // place), we must set localMatrix's scale to the inverse of
        // CTM scale.
        localMatrix.preScale(ctmScaleX ? 1 / ctmScaleX : 1, ctmScaleY ? 1 / ctmScaleY : 1);

        // The image fragment generated here is not exactly what is
        // requested. The scale factor used is approximated and image
        // fragment is slightly larger to align to integer
        // boundaries.
        SkBitmap resampled = extractScaledImageFragment(normSrcRect, scaleX, scaleY, &scaledSrcRect);
        if (repeatSpacing.isZero()) {
            shader = adoptRef(SkShader::CreateBitmapShader(resampled, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));
        } else {
            shader = adoptRef(SkShader::CreateBitmapShader(
                createBitmapWithSpace(resampled, repeatSpacing.width() * ctmScaleX, repeatSpacing.height() * ctmScaleY),
                SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));
        }
    } else {
        // Because no resizing occurred, the shader transform should be
        // set to the pattern's transform, which just includes scale.
        localMatrix.preScale(scale.width(), scale.height());

        // No need to resample before drawing.
        SkBitmap srcSubset;
        bitmap().extractSubset(&srcSubset, enclosingIntRect(normSrcRect));
        if (repeatSpacing.isZero()) {
            shader = adoptRef(SkShader::CreateBitmapShader(srcSubset, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));
        } else {
            shader = adoptRef(SkShader::CreateBitmapShader(
                createBitmapWithSpace(srcSubset, repeatSpacing.width() * ctmScaleX, repeatSpacing.height() * ctmScaleY),
                SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));
        }
    }

    SkPaint paint;
    paint.setShader(shader.get());
    paint.setXfermode(WebCoreCompositeToSkiaComposite(compositeOp, blendMode).get());
    paint.setColorFilter(context->colorFilter());
    paint.setFilterLevel(convertToSkiaFilterLevel(useBicubicFilter, resampling));

    if (isLazyDecoded)
        PlatformInstrumentation::didDrawLazyPixelRef(bitmap().getGenerationID());

    context->drawRect(destRect, paint);
}

bool NativeImageSkia::shouldCacheResampling(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
    // Check whether the requested dimensions match previous request.
    bool matchesPreviousRequest = m_cachedImageInfo.isEqual(scaledImageSize, scaledImageSubset);
    if (matchesPreviousRequest)
        ++m_resizeRequests;
    else {
        m_cachedImageInfo.set(scaledImageSize, scaledImageSubset);
        m_resizeRequests = 0;
        // Reset m_resizedImage now, because we don't distinguish
        // between the last requested resize info and m_resizedImage's
        // resize info.
        m_resizedImage.reset();
    }

    // We can not cache incomplete frames. This might be a good optimization in
    // the future, were we know how much of the frame has been decoded, so when
    // we incrementally draw more of the image, we only have to resample the
    // parts that are changed.
    if (!isDataComplete())
        return false;

    // If the destination bitmap is excessively large, we'll never allow caching.
    static const unsigned long long kLargeBitmapSize = 4096ULL * 4096ULL;
    unsigned long long fullSize = static_cast<unsigned long long>(scaledImageSize.width()) * static_cast<unsigned long long>(scaledImageSize.height());
    unsigned long long fragmentSize = static_cast<unsigned long long>(scaledImageSubset.width()) * static_cast<unsigned long long>(scaledImageSubset.height());

    if (fragmentSize > kLargeBitmapSize)
        return false;

    // If the destination bitmap is small, we'll always allow caching, since
    // there is not very much penalty for computing it and it may come in handy.
    static const unsigned kSmallBitmapSize = 4096;
    if (fragmentSize <= kSmallBitmapSize)
        return true;

    // If "too many" requests have been made for this bitmap, we assume that
    // many more will be made as well, and we'll go ahead and cache it.
    static const int kManyRequestThreshold = 4;
    if (m_resizeRequests >= kManyRequestThreshold)
        return true;

    // If more than 1/4 of the resized image is requested, it's worth caching.
    return fragmentSize > fullSize / 4;
}

NativeImageSkia::ImageResourceInfo::ImageResourceInfo()
{
    scaledImageSize.setEmpty();
    scaledImageSubset.setEmpty();
}

bool NativeImageSkia::ImageResourceInfo::isEqual(const SkISize& otherScaledImageSize, const SkIRect& otherScaledImageSubset) const
{
    return scaledImageSize == otherScaledImageSize && scaledImageSubset == otherScaledImageSubset;
}

void NativeImageSkia::ImageResourceInfo::set(const SkISize& otherScaledImageSize, const SkIRect& otherScaledImageSubset)
{
    scaledImageSize = otherScaledImageSize;
    scaledImageSubset = otherScaledImageSubset;
}

SkIRect NativeImageSkia::ImageResourceInfo::rectInSubset(const SkIRect& otherScaledImageSubset)
{
    if (!scaledImageSubset.contains(otherScaledImageSubset))
        return SkIRect::MakeEmpty();
    SkIRect subsetRect = otherScaledImageSubset;
    subsetRect.offset(-scaledImageSubset.x(), -scaledImageSubset.y());
    return subsetRect;
}

} // namespace WebCore
