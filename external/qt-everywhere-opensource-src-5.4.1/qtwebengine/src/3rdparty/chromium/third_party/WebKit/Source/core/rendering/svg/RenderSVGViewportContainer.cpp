/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
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
#include "core/rendering/svg/RenderSVGViewportContainer.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGUseElement.h"
#include "platform/graphics/GraphicsContext.h"

namespace WebCore {

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGElement* node)
    : RenderSVGContainer(node)
    , m_didTransformToRootUpdate(false)
    , m_isLayoutSizeChanged(false)
    , m_needsTransformUpdate(true)
{
}

void RenderSVGViewportContainer::determineIfLayoutSizeChanged()
{
    ASSERT(element());
    if (!isSVGSVGElement(*element()))
        return;

    m_isLayoutSizeChanged = toSVGSVGElement(element())->hasRelativeLengths() && selfNeedsLayout();
}

void RenderSVGViewportContainer::applyViewportClip(PaintInfo& paintInfo)
{
    if (SVGRenderSupport::isOverflowHidden(this))
        paintInfo.context->clip(m_viewport);
}

void RenderSVGViewportContainer::calcViewport()
{
    SVGElement* element = this->element();
    ASSERT(element);
    if (!isSVGSVGElement(*element))
        return;
    SVGSVGElement* svg = toSVGSVGElement(element);
    FloatRect oldViewport = m_viewport;

    SVGLengthContext lengthContext(element);
    m_viewport = FloatRect(svg->x()->currentValue()->value(lengthContext), svg->y()->currentValue()->value(lengthContext), svg->width()->currentValue()->value(lengthContext), svg->height()->currentValue()->value(lengthContext));

    if (oldViewport != m_viewport) {
        setNeedsBoundariesUpdate();
        setNeedsTransformUpdate();
    }
}

bool RenderSVGViewportContainer::calculateLocalTransform()
{
    m_didTransformToRootUpdate = m_needsTransformUpdate || SVGRenderSupport::transformToRootChanged(parent());
    if (!m_needsTransformUpdate)
        return false;

    m_localToParentTransform = AffineTransform::translation(m_viewport.x(), m_viewport.y()) * viewportTransform();
    m_needsTransformUpdate = false;
    return true;
}

AffineTransform RenderSVGViewportContainer::viewportTransform() const
{
    ASSERT(element());
    if (isSVGSVGElement(*element())) {
        SVGSVGElement* svg = toSVGSVGElement(element());
        return svg->viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
    }

    return AffineTransform();
}

bool RenderSVGViewportContainer::pointIsInsideViewportClip(const FloatPoint& pointInParent)
{
    // Respect the viewport clip (which is in parent coords)
    if (!SVGRenderSupport::isOverflowHidden(this))
        return true;

    return m_viewport.contains(pointInParent);
}

void RenderSVGViewportContainer::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(element());
    // An empty viewBox disables rendering.
    if (isSVGSVGElement(*element()) && toSVGSVGElement(*element()).hasEmptyViewBox())
        return;

    RenderSVGContainer::paint(paintInfo, paintOffset);
}

}
