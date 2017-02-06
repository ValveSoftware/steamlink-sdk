/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Image.h"

#include "platform/Length.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/PlatformInstrumentation.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/StdLibExtras.h"

#include <math.h>

namespace blink {

Image::Image(ImageObserver* observer)
    : m_imageObserver(observer)
    , m_imageObserverDisabled(false)
{
}

Image::~Image()
{
}

Image* Image::nullImage()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_REF(Image, nullImage, (BitmapImage::create()));
    return nullImage;
}

PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    const WebData& resource = Platform::current()->loadResource(name);
    if (resource.isEmpty())
        return Image::nullImage();

    RefPtr<Image> image = BitmapImage::create();
    image->setData(resource, true);
    return image.release();
}

bool Image::supportsType(const String& type)
{
    return MIMETypeRegistry::isSupportedImageResourceMIMEType(type);
}

bool Image::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_encodedImageData = data;
    if (!m_encodedImageData.get())
        return true;

    int length = m_encodedImageData->size();
    if (!length)
        return true;

    return dataChanged(allDataReceived);
}

void Image::drawTiled(GraphicsContext& ctxt, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& scaledTileSize, SkXfermode::Mode op, const FloatSize& repeatSpacing)
{
    FloatSize intrinsicTileSize = FloatSize(size());
    if (hasRelativeSize()) {
        intrinsicTileSize.setWidth(scaledTileSize.width());
        intrinsicTileSize.setHeight(scaledTileSize.height());
    }

    FloatSize scale(scaledTileSize.width() / intrinsicTileSize.width(),
                    scaledTileSize.height() / intrinsicTileSize.height());

    const FloatRect oneTileRect =
        computeTileContaining(destRect.location(), scaledTileSize, srcPoint, repeatSpacing);

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    if (oneTileRect.contains(destRect)) {
        const FloatRect visibleSrcRect = computeSubsetForTile(oneTileRect, destRect, intrinsicTileSize);
        ctxt.drawImage(this, destRect, &visibleSrcRect, op, DoNotRespectImageOrientation);
        return;
    }

    FloatRect tileRect(FloatPoint(), intrinsicTileSize);
    drawPattern(ctxt, tileRect, scale, oneTileRect.location(), op, destRect, repeatSpacing);

    startAnimation();
}

// FIXME: Merge with the other drawTiled eventually, since we need a combination of both for some things.
void Image::drawTiled(GraphicsContext& ctxt, const FloatRect& dstRect, const FloatRect& srcRect,
    const FloatSize& providedTileScaleFactor, TileRule hRule, TileRule vRule, SkXfermode::Mode op)
{
    // FIXME: We do not support 'space' yet. For now just map it to 'repeat'.
    if (hRule == SpaceTile)
        hRule = RepeatTile;
    if (vRule == SpaceTile)
        vRule = RepeatTile;

    // FIXME: if this code is used for background-repeat: round (in addition to
    // border-image-repeat), then add logic to deal with the background-size: auto
    // special case. The aspect ratio should be maintained in this case.
    FloatSize tileScaleFactor = providedTileScaleFactor;
    bool useLowInterpolationQuality = false;
    if (hRule == RoundTile) {
        float hRepetitions = std::max(1.0f, roundf(dstRect.width() / (tileScaleFactor.width() * srcRect.width())));
        tileScaleFactor.setWidth(dstRect.width() / (srcRect.width() * hRepetitions));
    }
    if (vRule == RoundTile) {
        float vRepetitions = std::max(1.0f, roundf(dstRect.height() / (tileScaleFactor.height() * srcRect.height())));
        tileScaleFactor.setHeight(dstRect.height() / (srcRect.height() * vRepetitions));
    }
    if (hRule == RoundTile || vRule == RoundTile) {
        // High interpolation quality rounds the scaled tile to an integer size (see Image::drawPattern).
        // To avoid causing a visual problem, linear interpolation must be used instead.
        // FIXME: Allow using high-quality interpolation in this case, too.
        useLowInterpolationQuality = true;
    }

    // We want to construct the phase such that the pattern is centered (when stretch is not
    // set for a particular rule).
    float hPhase = tileScaleFactor.width() * srcRect.x();
    float vPhase = tileScaleFactor.height() * srcRect.y();
    float scaledTileWidth = tileScaleFactor.width() * srcRect.width();
    float scaledTileHeight = tileScaleFactor.height() * srcRect.height();
    if (hRule == Image::RepeatTile)
        hPhase -= (dstRect.width() - scaledTileWidth) / 2;
    if (vRule == Image::RepeatTile)
        vPhase -= (dstRect.height() - scaledTileHeight) / 2;
    FloatPoint patternPhase(dstRect.x() - hPhase, dstRect.y() - vPhase);

    if (useLowInterpolationQuality) {
        InterpolationQuality previousInterpolationQuality = ctxt.imageInterpolationQuality();
        ctxt.setImageInterpolationQuality(InterpolationLow);
        drawPattern(ctxt, srcRect, tileScaleFactor, patternPhase, op, dstRect);
        ctxt.setImageInterpolationQuality(previousInterpolationQuality);
    } else {
        drawPattern(ctxt, srcRect, tileScaleFactor, patternPhase, op, dstRect);
    }

    startAnimation();
}

namespace {

sk_sp<SkShader> createPatternShader(const SkImage* image, const SkMatrix& shaderMatrix,
    const SkPaint& paint, const FloatSize& spacing)
{
    if (spacing.isZero())
        return image->makeShader(SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &shaderMatrix);

    // Arbitrary tiling is currently only supported for SkPictureShader - so we use it instead
    // of a plain bitmap shader to implement spacing.
    const SkRect tileRect = SkRect::MakeWH(
        image->width() + spacing.width(),
        image->height() + spacing.height());

    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(tileRect);
    canvas->drawImage(image, 0, 0, &paint);

    return SkShader::MakePictureShader(recorder.finishRecordingAsPicture(),
        SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &shaderMatrix, nullptr);
}

} // anonymous namespace

void Image::drawPattern(GraphicsContext& context, const FloatRect& floatSrcRect, const FloatSize& scale,
    const FloatPoint& phase, SkXfermode::Mode compositeOp, const FloatRect& destRect, const FloatSize& repeatSpacing)
{
    TRACE_EVENT0("skia", "Image::drawPattern");

    RefPtr<SkImage> image = imageForCurrentFrame();
    if (!image)
        return;

    FloatRect normSrcRect = floatSrcRect;

    normSrcRect.intersect(FloatRect(0, 0, image->width(), image->height()));
    if (destRect.isEmpty() || normSrcRect.isEmpty())
        return; // nothing to draw

    SkMatrix localMatrix;
    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the pattern. If WebKit wants
    // a shifted image, it will shift it from there using the localMatrix.
    const float adjustedX = phase.x() + normSrcRect.x() * scale.width();
    const float adjustedY = phase.y() + normSrcRect.y() * scale.height();
    localMatrix.setTranslate(SkFloatToScalar(adjustedX), SkFloatToScalar(adjustedY));

    // Because no resizing occurred, the shader transform should be
    // set to the pattern's transform, which just includes scale.
    localMatrix.preScale(scale.width(), scale.height());

    // Fetch this now as subsetting may swap the image.
    auto imageID = image->uniqueID();

    image = fromSkSp(image->makeSubset(enclosingIntRect(normSrcRect)));
    if (!image)
        return;

    {
        SkPaint paint = context.fillPaint();
        paint.setColor(SK_ColorBLACK);
        paint.setXfermodeMode(compositeOp);
        paint.setFilterQuality(context.computeFilterQuality(this, destRect, normSrcRect));
        paint.setAntiAlias(context.shouldAntialias());
        paint.setShader(createPatternShader(image.get(), localMatrix, paint,
            FloatSize(repeatSpacing.width() / scale.width(), repeatSpacing.height() / scale.height())));
        context.drawRect(destRect, paint);
    }

    if (currentFrameIsLazyDecoded())
        PlatformInstrumentation::didDrawLazyPixelRef(imageID);
}

PassRefPtr<Image> Image::imageForDefaultFrame()
{
    RefPtr<Image> image(this);

    return image.release();
}

bool Image::isTextureBacked()
{
    RefPtr<SkImage> image = imageForCurrentFrame();
    return image ? image->isTextureBacked() : false;
}

bool Image::applyShader(SkPaint& paint, const SkMatrix& localMatrix)
{
    // Default shader impl: attempt to build a shader based on the current frame SkImage.
    RefPtr<SkImage> image = imageForCurrentFrame();
    if (!image)
        return false;

    paint.setShader(
        image->makeShader(SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));

    return true;
}

FloatRect Image::computeTileContaining(const FloatPoint& point,
    const FloatSize& tileSize, const FloatPoint& tilePhase, const FloatSize& tileSpacing)
{
    const FloatSize actualTileSize(tileSize + tileSpacing);
    return FloatRect(
        FloatPoint(
            point.x() + fmodf(fmodf(-tilePhase.x(), actualTileSize.width()) - actualTileSize.width(), actualTileSize.width()),
            point.y() + fmodf(fmodf(-tilePhase.y(), actualTileSize.height()) - actualTileSize.height(), actualTileSize.height())
        ),
        tileSize);
}

FloatRect Image::computeSubsetForTile(const FloatRect& tile, const FloatRect& dest,
    const FloatSize& imageSize)
{
    DCHECK(tile.contains(dest));

    const FloatSize scale(tile.width() / imageSize.width(), tile.height() / imageSize.height());

    FloatRect subset = dest;
    subset.setX((dest.x() - tile.x()) / scale.width());
    subset.setY((dest.y() - tile.y()) / scale.height());
    subset.setWidth(dest.width() / scale.width());
    subset.setHeight(dest.height() / scale.height());

    return subset;
}

} // namespace blink
