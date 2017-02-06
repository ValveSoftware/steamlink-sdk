/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "platform/graphics/filters/SourceAlpha.h"

#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "wtf/text/WTFString.h"

namespace blink {

SourceAlpha* SourceAlpha::create(FilterEffect* sourceEffect)
{
    return new SourceAlpha(sourceEffect);
}

SourceAlpha::SourceAlpha(FilterEffect* sourceEffect)
    : FilterEffect(sourceEffect->getFilter())
{
    setOperatingColorSpace(sourceEffect->operatingColorSpace());
    inputEffects().append(sourceEffect);
}

FloatRect SourceAlpha::determineAbsolutePaintRect(const FloatRect& requestedRect)
{
    return inputEffect(0)->determineAbsolutePaintRect(requestedRect);
}

sk_sp<SkImageFilter> SourceAlpha::createImageFilter()
{
    sk_sp<SkImageFilter> sourceGraphic(SkiaImageFilterBuilder::build(inputEffect(0), operatingColorSpace()));
    SkScalar matrix[20] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, SK_Scalar1, 0
    };
    sk_sp<SkColorFilter> colorFilter = SkColorFilter::MakeMatrixFilterRowMajor255(matrix);
    return SkColorFilterImageFilter::Make(std::move(colorFilter), std::move(sourceGraphic));
}

TextStream& SourceAlpha::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[SourceAlpha]\n";
    return ts;
}

} // namespace blink
