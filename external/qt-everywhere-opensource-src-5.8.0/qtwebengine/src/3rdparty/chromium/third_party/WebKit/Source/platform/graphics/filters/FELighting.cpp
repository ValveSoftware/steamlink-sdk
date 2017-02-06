/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/filters/FELighting.h"

#include "SkLightingImageFilter.h"
#include "SkPoint3.h"
#include "platform/graphics/filters/DistantLightSource.h"
#include "platform/graphics/filters/PointLightSource.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SpotLightSource.h"

namespace blink {

FELighting::FELighting(Filter* filter, LightingType lightingType, const Color& lightingColor, float surfaceScale,
    float diffuseConstant, float specularConstant, float specularExponent,
    PassRefPtr<LightSource> lightSource)
    : FilterEffect(filter)
    , m_lightingType(lightingType)
    , m_lightSource(lightSource)
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_diffuseConstant(std::max(diffuseConstant, 0.0f))
    , m_specularConstant(std::max(specularConstant, 0.0f))
    , m_specularExponent(clampTo(specularExponent, 1.0f, 128.0f))
{
}

FloatRect FELighting::mapPaintRect(const FloatRect& rect, bool) const
{
    FloatRect result = rect;
    // The areas affected need to be a pixel bigger to accommodate the Sobel kernel.
    result.inflate(1);
    return result;
}

sk_sp<SkImageFilter> FELighting::createImageFilter()
{
    if (!m_lightSource)
        return createTransparentBlack();

    SkImageFilter::CropRect rect = getCropRect();
    Color lightColor = adaptColorToOperatingColorSpace(m_lightingColor);
    sk_sp<SkImageFilter> input(SkiaImageFilterBuilder::build(inputEffect(0), operatingColorSpace()));
    switch (m_lightSource->type()) {
    case LS_DISTANT: {
        DistantLightSource* distantLightSource = static_cast<DistantLightSource*>(m_lightSource.get());
        float azimuthRad = deg2rad(distantLightSource->azimuth());
        float elevationRad = deg2rad(distantLightSource->elevation());
        const SkPoint3 direction = SkPoint3::Make(cosf(azimuthRad) * cosf(elevationRad), sinf(azimuthRad) * cosf(elevationRad), sinf(elevationRad));
        if (m_specularConstant > 0)
            return SkLightingImageFilter::MakeDistantLitSpecular(direction, lightColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, std::move(input), &rect);
        return SkLightingImageFilter::MakeDistantLitDiffuse(direction, lightColor.rgb(), m_surfaceScale, m_diffuseConstant, std::move(input), &rect);
    }
    case LS_POINT: {
        PointLightSource* pointLightSource = static_cast<PointLightSource*>(m_lightSource.get());
        const FloatPoint3D position = pointLightSource->position();
        const SkPoint3 skPosition = SkPoint3::Make(position.x(), position.y(), position.z());
        if (m_specularConstant > 0)
            return SkLightingImageFilter::MakePointLitSpecular(skPosition, lightColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, std::move(input), &rect);
        return SkLightingImageFilter::MakePointLitDiffuse(skPosition, lightColor.rgb(), m_surfaceScale, m_diffuseConstant, std::move(input), &rect);
    }
    case LS_SPOT: {
        SpotLightSource* spotLightSource = static_cast<SpotLightSource*>(m_lightSource.get());
        const SkPoint3 location = SkPoint3::Make(spotLightSource->position().x(), spotLightSource->position().y(), spotLightSource->position().z());
        const SkPoint3 target = SkPoint3::Make(spotLightSource->direction().x(), spotLightSource->direction().y(), spotLightSource->direction().z());
        float specularExponent = spotLightSource->specularExponent();
        float limitingConeAngle = spotLightSource->limitingConeAngle();
        if (!limitingConeAngle || limitingConeAngle > 90 || limitingConeAngle < -90)
            limitingConeAngle = 90;
        if (m_specularConstant > 0)
            return SkLightingImageFilter::MakeSpotLitSpecular(location, target, specularExponent, limitingConeAngle, lightColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, std::move(input), &rect);
        return SkLightingImageFilter::MakeSpotLitDiffuse(location, target, specularExponent, limitingConeAngle, lightColor.rgb(), m_surfaceScale, m_diffuseConstant, std::move(input), &rect);
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace blink
