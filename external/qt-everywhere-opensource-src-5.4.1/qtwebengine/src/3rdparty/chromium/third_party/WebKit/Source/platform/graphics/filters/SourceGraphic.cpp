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

#include "config.h"

#include "platform/graphics/filters/SourceGraphic.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/text/TextStream.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtr<SourceGraphic> SourceGraphic::create(Filter* filter)
{
    return adoptRef(new SourceGraphic(filter));
}

const AtomicString& SourceGraphic::effectName()
{
    DEFINE_STATIC_LOCAL(const AtomicString, s_effectName, ("SourceGraphic", AtomicString::ConstructFromLiteral));
    return s_effectName;
}

FloatRect SourceGraphic::determineAbsolutePaintRect(const FloatRect& requestedRect)
{
    FloatRect srcRect = filter()->sourceImageRect();
    srcRect.intersect(requestedRect);
    addAbsolutePaintRect(srcRect);
    return srcRect;
}

void SourceGraphic::applySoftware()
{
    ImageBuffer* resultImage = createImageBufferResult();
    Filter* filter = this->filter();
    if (!resultImage || !filter->sourceImage())
        return;

    IntRect srcRect = filter->sourceImageRect();
    if (ImageBuffer* sourceImageBuffer = filter->sourceImage()) {
        resultImage->context()->drawImageBuffer(sourceImageBuffer,
            FloatRect(IntPoint(srcRect.location() - absolutePaintRect().location()), sourceImageBuffer->size()));
    }
}

TextStream& SourceGraphic::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[SourceGraphic]\n";
    return ts;
}

} // namespace WebCore
