/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "platform/graphics/filters/FEMerge.h"

#include "SkMergeImageFilter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

FEMerge::FEMerge(Filter* filter)
    : FilterEffect(filter)
{
}

FEMerge* FEMerge::create(Filter* filter)
{
    return new FEMerge(filter);
}

sk_sp<SkImageFilter> FEMerge::createImageFilter()
{
    unsigned size = numberOfEffectInputs();

    std::unique_ptr<sk_sp<SkImageFilter>[]> inputRefs = wrapArrayUnique(new sk_sp<SkImageFilter>[size]);
    for (unsigned i = 0; i < size; ++i)
        inputRefs[i] = SkiaImageFilterBuilder::build(inputEffect(i), operatingColorSpace());
    SkImageFilter::CropRect rect = getCropRect();
    return SkMergeImageFilter::Make(inputRefs.get(), size, 0, &rect);
}

TextStream& FEMerge::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feMerge";
    FilterEffect::externalRepresentation(ts);
    unsigned size = numberOfEffectInputs();
    ts << " mergeNodes=\"" << size << "\"]\n";
    for (unsigned i = 0; i < size; ++i)
        inputEffect(i)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace blink
