// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ImagePattern.h"

#include "platform/graphics/Image.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

PassRefPtr<ImagePattern> ImagePattern::create(PassRefPtr<Image> image, RepeatMode repeatMode)
{
    return adoptRef(new ImagePattern(image, repeatMode));
}

ImagePattern::ImagePattern(PassRefPtr<Image> image, RepeatMode repeatMode)
    : Pattern(repeatMode)
    , m_tileImage(toSkSp(image->imageForCurrentFrame()))
{
    if (m_tileImage) {
        // TODO(fmalita): mechanism to extract the actual SkImageInfo from an SkImage?
        const SkImageInfo info =
            SkImageInfo::MakeN32Premul(m_tileImage->width(), m_tileImage->height());
        adjustExternalMemoryAllocated(info.getSafeSize(info.minRowBytes()));
    }
}

sk_sp<SkShader> ImagePattern::createShader(const SkMatrix& localMatrix) const
{
    if (!m_tileImage)
        return SkShader::MakeColorShader(SK_ColorTRANSPARENT);

    if (isRepeatXY()) {
        // Fast path: for repeatXY we just return a shader from the original image.
        return m_tileImage->makeShader(SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix);
    }

    // Skia does not have a "draw the tile only once" option. Clamp_TileMode
    // repeats the last line of the image after drawing one tile. To avoid
    // filling the space with arbitrary pixels, this workaround forces the
    // image to have a line of transparent pixels on the "repeated" edge(s),
    // thus causing extra space to be transparent filled.
    SkShader::TileMode tileModeX = isRepeatX()
        ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
    SkShader::TileMode tileModeY = isRepeatY()
        ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
    int expandW = isRepeatX() ? 0 : 1;
    int expandH = isRepeatY() ? 0 : 1;

    // Create a transparent image 1 pixel wider and/or taller than the
    // original, then copy the orignal into it.
    // FIXME: Is there a better way to pad (not scale) an image in skia?
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        m_tileImage->width() + expandW, m_tileImage->height() + expandH);
    if (!surface)
        return SkShader::MakeColorShader(SK_ColorTRANSPARENT);

    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    surface->getCanvas()->drawImage(m_tileImage, 0, 0, &paint);

    return surface->makeImageSnapshot()->makeShader(tileModeX, tileModeY, &localMatrix);
}

bool ImagePattern::isTextureBacked() const
{
    return m_tileImage && m_tileImage->isTextureBacked();
}

} // namespace blink
