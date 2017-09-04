/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/filters/SkiaImageFilterBuilder.h"

#include "SkBlurImageFilter.h"
#include "SkColorFilterImageFilter.h"
#include "SkColorMatrixFilter.h"
#include "SkTableColorFilter.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/effects/SkXfermodeImageFilter.h"

namespace blink {
namespace SkiaImageFilterBuilder {

void populateSourceGraphicImageFilters(FilterEffect* sourceGraphic,
                                       sk_sp<SkImageFilter> input,
                                       ColorSpace inputColorSpace) {
  // Prepopulate SourceGraphic with two image filters: one with a null image
  // filter, and the other with a colorspace conversion filter.
  // We don't know what color space the interior nodes will request, so we
  // have to initialize SourceGraphic with both options.
  // Since we know SourceGraphic is always PM-valid, we also use these for
  // the PM-validated options.
  sk_sp<SkImageFilter> deviceFilter =
      transformColorSpace(input, inputColorSpace, ColorSpaceDeviceRGB);
  sk_sp<SkImageFilter> linearFilter =
      transformColorSpace(input, inputColorSpace, ColorSpaceLinearRGB);
  sourceGraphic->setImageFilter(ColorSpaceDeviceRGB, false, deviceFilter);
  sourceGraphic->setImageFilter(ColorSpaceLinearRGB, false, linearFilter);
  sourceGraphic->setImageFilter(ColorSpaceDeviceRGB, true, deviceFilter);
  sourceGraphic->setImageFilter(ColorSpaceLinearRGB, true, linearFilter);
}

sk_sp<SkImageFilter> build(FilterEffect* effect,
                           ColorSpace colorSpace,
                           bool destinationRequiresValidPreMultipliedPixels) {
  if (!effect)
    return nullptr;

  bool requiresPMColorValidation =
      effect->mayProduceInvalidPreMultipliedPixels() &&
      destinationRequiresValidPreMultipliedPixels;

  if (SkImageFilter* filter =
          effect->getImageFilter(colorSpace, requiresPMColorValidation))
    return sk_ref_sp(filter);

  // Note that we may still need the color transform even if the filter is null
  sk_sp<SkImageFilter> origFilter =
      requiresPMColorValidation ? effect->createImageFilter()
                                : effect->createImageFilterWithoutValidation();
  sk_sp<SkImageFilter> filter = transformColorSpace(
      origFilter, effect->operatingColorSpace(), colorSpace);
  effect->setImageFilter(colorSpace, requiresPMColorValidation, filter);
  if (filter.get() != origFilter.get())
    effect->setImageFilter(effect->operatingColorSpace(),
                           requiresPMColorValidation, std::move(origFilter));
  return filter;
}

sk_sp<SkImageFilter> transformColorSpace(sk_sp<SkImageFilter> input,
                                         ColorSpace srcColorSpace,
                                         ColorSpace dstColorSpace) {
  sk_sp<SkColorFilter> colorFilter =
      ColorSpaceUtilities::createColorSpaceFilter(srcColorSpace, dstColorSpace);
  if (!colorFilter)
    return input;

  return SkColorFilterImageFilter::Make(std::move(colorFilter),
                                        std::move(input));
}

void buildSourceGraphic(FilterEffect* sourceGraphic, sk_sp<SkPicture> picture) {
  ASSERT(picture);
  SkRect cullRect = picture->cullRect();
  sk_sp<SkImageFilter> filter =
      SkPictureImageFilter::Make(std::move(picture), cullRect);
  populateSourceGraphicImageFilters(sourceGraphic, std::move(filter),
                                    sourceGraphic->operatingColorSpace());
}

static float kMaxMaskBufferSize =
    50.f * 1024.f * 1024.f / 4.f;  // 50MB / 4 bytes per pixel

sk_sp<SkImageFilter> buildBoxReflectFilter(const BoxReflection& reflection,
                                           sk_sp<SkImageFilter> input) {
  sk_sp<SkImageFilter> maskedInput;
  if (SkPicture* maskPicture = reflection.mask()) {
    // Since SkPictures can't be serialized to the browser process, first raster
    // the mask to a bitmap, then encode it in an SkImageSource, which can be
    // serialized.
    SkBitmap bitmap;
    const SkRect cullRect = maskPicture->cullRect();
    if (static_cast<float>(cullRect.width()) *
            static_cast<float>(cullRect.height()) <
        kMaxMaskBufferSize) {
      bitmap.allocPixels(
          SkImageInfo::MakeN32Premul(cullRect.width(), cullRect.height()));
      SkCanvas canvas(bitmap);
      canvas.clear(SK_ColorTRANSPARENT);
      canvas.translate(-cullRect.x(), -cullRect.y());
      canvas.drawPicture(maskPicture);
      sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);

      // SkXfermodeImageFilter can choose an excessively large size if the
      // mask is smaller than the filtered contents (due to overflow).
      // http://skbug.com/5210
      SkImageFilter::CropRect cropRect(maskPicture->cullRect());
      maskedInput = SkXfermodeImageFilter::Make(
          SkBlendMode::kSrcIn,
          SkOffsetImageFilter::Make(cullRect.x(), cullRect.y(),
                                    SkImageSource::Make(image)),
          input, &cropRect);
    } else {
      // If the buffer is excessively big, give up and make an
      // SkPictureImageFilter
      // anyway, even if it might not render.
      SkImageFilter::CropRect cropRect(maskPicture->cullRect());
      maskedInput = SkXfermodeImageFilter::Make(
          SkBlendMode::kSrcOver,
          SkPictureImageFilter::Make(sk_ref_sp(maskPicture)), input, &cropRect);
    }
  } else {
    maskedInput = input;
  }
  sk_sp<SkImageFilter> flipImageFilter = SkImageFilter::MakeMatrixFilter(
      reflection.reflectionMatrix(), kLow_SkFilterQuality,
      std::move(maskedInput));
  return SkXfermodeImageFilter::Make(SkBlendMode::kSrcOver,
                                     std::move(flipImageFilter),
                                     std::move(input), nullptr);
}

}  // namespace SkiaImageFilterBuilder
}  // namespace blink
