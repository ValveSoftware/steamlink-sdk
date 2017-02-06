/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "platform/DragImage.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontMetrics.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/text/BidiTextRun.h"
#include "platform/text/StringTruncator.h"
#include "platform/text/TextRun.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/weborigin/KURL.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <algorithm>
#include <memory>

namespace blink {

namespace {

const float kDragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float kDragLabelBorderY = 2;
const float kLabelBorderYOffset = 2;

const float kMaxDragLabelWidth = 300;
const float kMaxDragLabelStringWidth = (kMaxDragLabelWidth - 2 * kDragLabelBorderX);

const float kDragLinkLabelFontSize = 11;
const float kDragLinkUrlFontSize = 10;

} // anonymous namespace

PassRefPtr<SkImage> DragImage::resizeAndOrientImage(PassRefPtr<SkImage> image, ImageOrientation orientation,
    FloatSize imageScale, float opacity, InterpolationQuality interpolationQuality)
{
    IntSize size(image->width(), image->height());
    size.scale(imageScale.width(), imageScale.height());
    AffineTransform transform;
    if (orientation != DefaultImageOrientation) {
        if (orientation.usesWidthAsHeight())
            size = size.transposedSize();
        transform *= orientation.transformFromDefault(FloatSize(size));
    }
    transform.scaleNonUniform(imageScale.width(), imageScale.height());

    if (size.isEmpty())
        return nullptr;

    if (transform.isIdentity() && opacity == 1) {
        // Nothing to adjust, just use the original.
        ASSERT(image->width() == size.width());
        ASSERT(image->height() == size.height());
        return image;
    }

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(size.width(), size.height());
    if (!surface)
        return nullptr;

    SkPaint paint;
    ASSERT(opacity >= 0 && opacity <= 1);
    paint.setAlpha(opacity * 255);
    paint.setFilterQuality(interpolationQuality == InterpolationNone
        ? kNone_SkFilterQuality : kHigh_SkFilterQuality);

    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->concat(affineTransformToSkMatrix(transform));
    canvas->drawImage(image.get(), 0, 0, &paint);

    return fromSkSp(surface->makeImageSnapshot());
}

FloatSize DragImage::clampedImageScale(const IntSize& imageSize, const IntSize& size,
    const IntSize& maxSize)
{
    // Non-uniform scaling for size mapping.
    FloatSize imageScale(
        static_cast<float>(size.width()) / imageSize.width(),
        static_cast<float>(size.height()) / imageSize.height());

    // Uniform scaling for clamping.
    const float clampScaleX = size.width() > maxSize.width()
        ? static_cast<float>(maxSize.width()) / size.width() : 1;
    const float clampScaleY = size.height() > maxSize.height()
        ? static_cast<float>(maxSize.height()) / size.height() : 1;
    imageScale.scale(std::min(clampScaleX, clampScaleY));

    return imageScale;
}

std::unique_ptr<DragImage> DragImage::create(Image* image,
    RespectImageOrientationEnum shouldRespectImageOrientation, float deviceScaleFactor,
    InterpolationQuality interpolationQuality, float opacity, FloatSize imageScale)
{
    if (!image)
        return nullptr;

    RefPtr<SkImage> skImage = image->imageForCurrentFrame();
    if (!skImage)
        return nullptr;

    ImageOrientation orientation;
    if (shouldRespectImageOrientation == RespectImageOrientation && image->isBitmapImage())
        orientation = toBitmapImage(image)->currentFrameOrientation();

    SkBitmap bm;
    RefPtr<SkImage> resizedImage =
        resizeAndOrientImage(skImage.release(), orientation, imageScale, opacity, interpolationQuality);
    if (!resizedImage || !resizedImage->asLegacyBitmap(&bm, SkImage::kRO_LegacyBitmapMode))
        return nullptr;

    return wrapUnique(new DragImage(bm, deviceScaleFactor, interpolationQuality));
}

static Font deriveDragLabelFont(int size, FontWeight fontWeight, const FontDescription& systemFont)
{
    FontDescription description = systemFont;
    description.setWeight(fontWeight);
    description.setSpecifiedSize(size);
    description.setComputedSize(size);
    Font result(description);
    result.update(nullptr);
    return result;
}

std::unique_ptr<DragImage> DragImage::create(const KURL& url, const String& inLabel, const FontDescription& systemFont, float deviceScaleFactor)
{
    const Font labelFont = deriveDragLabelFont(kDragLinkLabelFontSize, FontWeightBold, systemFont);
    const Font urlFont = deriveDragLabelFont(kDragLinkUrlFontSize, FontWeightNormal, systemFont);
    FontCachePurgePreventer fontCachePurgePreventer;

    bool drawURLString = true;
    bool clipURLString = false;
    bool clipLabelString = false;
    float maxDragLabelStringWidthDIP = kMaxDragLabelStringWidth / deviceScaleFactor;

    String urlString = url.getString();
    String label = inLabel.stripWhiteSpace();
    if (label.isEmpty()) {
        drawURLString = false;
        label = urlString;
    }

    // First step is drawing the link drag image width.
    TextRun labelRun(label.impl());
    TextRun urlRun(urlString.impl());
    IntSize labelSize(labelFont.width(labelRun), labelFont.getFontMetrics().ascent() + labelFont.getFontMetrics().descent());

    if (labelSize.width() > maxDragLabelStringWidthDIP) {
        labelSize.setWidth(maxDragLabelStringWidthDIP);
        clipLabelString = true;
    }

    IntSize urlStringSize;
    IntSize imageSize(labelSize.width() + kDragLabelBorderX * 2, labelSize.height() + kDragLabelBorderY * 2);

    if (drawURLString) {
        urlStringSize.setWidth(urlFont.width(urlRun));
        urlStringSize.setHeight(urlFont.getFontMetrics().ascent() + urlFont.getFontMetrics().descent());
        imageSize.setHeight(imageSize.height() + urlStringSize.height());
        if (urlStringSize.width() > maxDragLabelStringWidthDIP) {
            imageSize.setWidth(maxDragLabelStringWidthDIP);
            clipURLString = true;
        } else
            imageSize.setWidth(std::max(labelSize.width(), urlStringSize.width()) + kDragLabelBorderX * 2);
    }

    // We now know how big the image needs to be, so we create and
    // fill the background
    IntSize scaledImageSize = imageSize;
    scaledImageSize.scale(deviceScaleFactor);
    std::unique_ptr<ImageBuffer> buffer(ImageBuffer::create(scaledImageSize));
    if (!buffer)
        return nullptr;

    buffer->canvas()->scale(deviceScaleFactor, deviceScaleFactor);

    const float DragLabelRadius = 5;

    IntRect rect(IntPoint(), imageSize);
    SkPaint backgroundPaint;
    backgroundPaint.setColor(SkColorSetRGB(140, 140, 140));
    SkRRect rrect;
    rrect.setRectXY(SkRect::MakeWH(imageSize.width(), imageSize.height()), DragLabelRadius, DragLabelRadius);
    buffer->canvas()->drawRRect(rrect, backgroundPaint);

    // Draw the text
    SkPaint textPaint;
    if (drawURLString) {
        if (clipURLString)
            urlString = StringTruncator::centerTruncate(urlString, imageSize.width() - (kDragLabelBorderX * 2.0f), urlFont);
        IntPoint textPos(kDragLabelBorderX, imageSize.height() - (kLabelBorderYOffset + urlFont.getFontMetrics().descent()));
        TextRun textRun(urlString);
        urlFont.drawText(buffer->canvas(), TextRunPaintInfo(textRun), textPos, deviceScaleFactor, textPaint);
    }

    if (clipLabelString)
        label = StringTruncator::rightTruncate(label, imageSize.width() - (kDragLabelBorderX * 2.0f), labelFont);

    bool hasStrongDirectionality;
    TextRun textRun = textRunWithDirectionality(label, &hasStrongDirectionality);
    IntPoint textPos(kDragLabelBorderX, kDragLabelBorderY + labelFont.getFontDescription().computedPixelSize());
    if (hasStrongDirectionality && textRun.direction() == RTL) {
        float textWidth = labelFont.width(textRun);
        int availableWidth = imageSize.width() - kDragLabelBorderX * 2;
        textPos.setX(availableWidth - ceilf(textWidth));
    }
    labelFont.drawBidiText(buffer->canvas(), TextRunPaintInfo(textRun), FloatPoint(textPos), Font::DoNotPaintIfFontNotReady, deviceScaleFactor, textPaint);

    RefPtr<Image> image = buffer->newImageSnapshot();
    return DragImage::create(image.get(), DoNotRespectImageOrientation, deviceScaleFactor);
}

DragImage::DragImage(const SkBitmap& bitmap, float resolutionScale, InterpolationQuality interpolationQuality)
    : m_bitmap(bitmap)
    , m_resolutionScale(resolutionScale)
    , m_interpolationQuality(interpolationQuality)
{
}

DragImage::~DragImage()
{
}

void DragImage::scale(float scaleX, float scaleY)
{
    skia::ImageOperations::ResizeMethod resizeMethod = m_interpolationQuality == InterpolationNone ? skia::ImageOperations::RESIZE_BOX : skia::ImageOperations::RESIZE_LANCZOS3;
    int imageWidth = scaleX * m_bitmap.width();
    int imageHeight = scaleY * m_bitmap.height();
    m_bitmap = skia::ImageOperations::Resize(m_bitmap, resizeMethod, imageWidth, imageHeight);
}

} // namespace blink
