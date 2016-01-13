/*
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
#include "core/svg/graphics/SVGImageForContainer.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

IntSize SVGImageForContainer::size() const
{
    FloatSize scaledContainerSize(m_containerSize);
    scaledContainerSize.scale(m_zoom);
    return roundedIntSize(scaledContainerSize);
}

void SVGImageForContainer::draw(GraphicsContext* context, const FloatRect& dstRect,
    const FloatRect& srcRect, CompositeOperator compositeOp, blink::WebBlendMode blendMode)
{
    m_image->drawForContainer(context, m_containerSize, m_zoom, dstRect, srcRect, compositeOp, blendMode);
}

void SVGImageForContainer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator compositeOp, const FloatRect& dstRect, blink::WebBlendMode blendMode, const IntSize& repeatSpacing)
{
    m_image->drawPatternForContainer(context, m_containerSize, m_zoom, srcRect, scale, phase, compositeOp, dstRect, blendMode, repeatSpacing);
}

PassRefPtr<NativeImageSkia> SVGImageForContainer::nativeImageForCurrentFrame()
{
    return m_image->nativeImageForCurrentFrame();
}

} // namespace WebCore
