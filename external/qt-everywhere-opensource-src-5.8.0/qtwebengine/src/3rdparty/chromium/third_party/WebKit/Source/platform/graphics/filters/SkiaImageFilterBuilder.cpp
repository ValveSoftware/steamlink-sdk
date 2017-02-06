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
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/effects/SkXfermodeImageFilter.h"

namespace blink {
namespace SkiaImageFilterBuilder {

namespace {

void populateSourceGraphicImageFilters(FilterEffect* sourceGraphic, sk_sp<SkImageFilter> input, ColorSpace inputColorSpace)
{
    // Prepopulate SourceGraphic with two image filters: one with a null image
    // filter, and the other with a colorspace conversion filter.
    // We don't know what color space the interior nodes will request, so we
    // have to initialize SourceGraphic with both options.
    // Since we know SourceGraphic is always PM-valid, we also use these for
    // the PM-validated options.
    sk_sp<SkImageFilter> deviceFilter = transformColorSpace(input, inputColorSpace, ColorSpaceDeviceRGB);
    sk_sp<SkImageFilter> linearFilter = transformColorSpace(input, inputColorSpace, ColorSpaceLinearRGB);
    sourceGraphic->setImageFilter(ColorSpaceDeviceRGB, false, deviceFilter);
    sourceGraphic->setImageFilter(ColorSpaceLinearRGB, false, linearFilter);
    sourceGraphic->setImageFilter(ColorSpaceDeviceRGB, true, deviceFilter);
    sourceGraphic->setImageFilter(ColorSpaceLinearRGB, true, linearFilter);
}

} // namespace

sk_sp<SkImageFilter> build(FilterEffect* effect, ColorSpace colorSpace, bool destinationRequiresValidPreMultipliedPixels)
{
    if (!effect)
        return nullptr;

    bool requiresPMColorValidation = effect->mayProduceInvalidPreMultipliedPixels() && destinationRequiresValidPreMultipliedPixels;

    if (SkImageFilter* filter = effect->getImageFilter(colorSpace, requiresPMColorValidation))
        return sk_ref_sp(filter);

    // Note that we may still need the color transform even if the filter is null
    sk_sp<SkImageFilter> origFilter = requiresPMColorValidation ? effect->createImageFilter() : effect->createImageFilterWithoutValidation();
    sk_sp<SkImageFilter> filter = transformColorSpace(origFilter, effect->operatingColorSpace(), colorSpace);
    effect->setImageFilter(colorSpace, requiresPMColorValidation, filter);
    if (filter.get() != origFilter.get())
        effect->setImageFilter(effect->operatingColorSpace(), requiresPMColorValidation, std::move(origFilter));
    return filter;
}

sk_sp<SkImageFilter> transformColorSpace(sk_sp<SkImageFilter> input, ColorSpace srcColorSpace, ColorSpace dstColorSpace)
{
    sk_sp<SkColorFilter> colorFilter = toSkSp(ColorSpaceUtilities::createColorSpaceFilter(srcColorSpace, dstColorSpace));
    if (!colorFilter)
        return input;

    return SkColorFilterImageFilter::Make(std::move(colorFilter), std::move(input));
}

void buildSourceGraphic(FilterEffect* sourceGraphic, sk_sp<SkPicture> picture)
{
    ASSERT(picture);
    SkRect cullRect = picture->cullRect();
    sk_sp<SkImageFilter> filter = SkPictureImageFilter::Make(std::move(picture), cullRect);
    populateSourceGraphicImageFilters(sourceGraphic, std::move(filter), sourceGraphic->operatingColorSpace());
}

void buildFilterOperations(const FilterOperations& operations, CompositorFilterOperations* filters)
{
    ColorSpace currentColorSpace = ColorSpaceDeviceRGB;

    for (size_t i = 0; i < operations.size(); ++i) {
        const FilterOperation& op = *operations.at(i);
        switch (op.type()) {
        case FilterOperation::REFERENCE: {
            Filter* referenceFilter = toReferenceFilterOperation(op).getFilter();
            if (referenceFilter && referenceFilter->lastEffect()) {
                populateSourceGraphicImageFilters(referenceFilter->getSourceGraphic(), nullptr, currentColorSpace);

                FilterEffect* filterEffect = referenceFilter->lastEffect();
                currentColorSpace = filterEffect->operatingColorSpace();
                filterEffect->determineFilterPrimitiveSubregion(MapRectForward);
                filters->appendReferenceFilter(SkiaImageFilterBuilder::build(filterEffect, currentColorSpace));
            }
            break;
        }
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE: {
            float amount = toBasicColorMatrixFilterOperation(op).amount();
            switch (op.type()) {
            case FilterOperation::GRAYSCALE:
                filters->appendGrayscaleFilter(amount);
                break;
            case FilterOperation::SEPIA:
                filters->appendSepiaFilter(amount);
                break;
            case FilterOperation::SATURATE:
                filters->appendSaturateFilter(amount);
                break;
            case FilterOperation::HUE_ROTATE:
                filters->appendHueRotateFilter(amount);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::OPACITY:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST: {
            float amount = toBasicComponentTransferFilterOperation(op).amount();
            switch (op.type()) {
            case FilterOperation::INVERT:
                filters->appendInvertFilter(amount);
                break;
            case FilterOperation::OPACITY:
                filters->appendOpacityFilter(amount);
                break;
            case FilterOperation::BRIGHTNESS:
                filters->appendBrightnessFilter(amount);
                break;
            case FilterOperation::CONTRAST:
                filters->appendContrastFilter(amount);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = toBlurFilterOperation(op).stdDeviation().getFloatValue();
            filters->appendBlurFilter(pixelRadius);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation& drop = toDropShadowFilterOperation(op);
            filters->appendDropShadowFilter(drop.location(), drop.stdDeviation(), drop.getColor());
            break;
        }
        case FilterOperation::BOX_REFLECT: {
            // TODO(jbroman): Consider explaining box reflect to the compositor,
            // instead of calling this a "reference filter".
            const auto& reflection = toBoxReflectFilterOperation(op).reflection();
            filters->appendReferenceFilter(buildBoxReflectFilter(reflection, nullptr));
            break;
        }
        case FilterOperation::NONE:
            break;
        }
    }
    if (currentColorSpace != ColorSpaceDeviceRGB) {
        // Transform to device color space at the end of processing, if required
        sk_sp<SkImageFilter> filter = transformColorSpace(nullptr, currentColorSpace, ColorSpaceDeviceRGB);
        filters->appendReferenceFilter(std::move(filter));
    }
}

sk_sp<SkImageFilter> buildBoxReflectFilter(const BoxReflection& reflection, sk_sp<SkImageFilter> input)
{
    sk_sp<SkImageFilter> maskedInput;
    if (SkPicture* maskPicture = reflection.mask()) {
        // SkXfermodeImageFilter can choose an excessively large size if the
        // mask is smaller than the filtered contents (due to overflow).
        // http://skbug.com/5210
        SkImageFilter::CropRect cropRect(maskPicture->cullRect());
        maskedInput = SkXfermodeImageFilter::Make(
            SkXfermode::Make(SkXfermode::kSrcIn_Mode),
            SkPictureImageFilter::Make(sk_ref_sp(maskPicture)),
            input, &cropRect);
    } else {
        maskedInput = input;
    }
    sk_sp<SkImageFilter> flipImageFilter = SkImageFilter::MakeMatrixFilter(
        reflection.reflectionMatrix(), kLow_SkFilterQuality, std::move(maskedInput));
    return SkXfermodeImageFilter::Make(nullptr, std::move(flipImageFilter), std::move(input), nullptr);
}

} // namespace SkiaImageFilterBuilder
} // namespace blink
