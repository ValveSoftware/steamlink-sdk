/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "core/rendering/svg/RenderSVGGradientStop.h"

#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGGradientElement.h"
#include "core/svg/SVGStopElement.h"

namespace WebCore {

RenderSVGGradientStop::RenderSVGGradientStop(SVGStopElement* element)
    : RenderObject(element)
{
}

RenderSVGGradientStop::~RenderSVGGradientStop()
{
}

void RenderSVGGradientStop::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderObject::styleDidChange(diff, oldStyle);
    if (diff.hasNoChange())
        return;

    // <stop> elements should only be allowed to make renderers under gradient elements
    // but I can imagine a few cases we might not be catching, so let's not crash if our parent isn't a gradient.
    SVGGradientElement* gradient = gradientElement();
    if (!gradient)
        return;

    RenderObject* renderer = gradient->renderer();
    if (!renderer)
        return;

    RenderSVGResourceContainer* container = toRenderSVGResourceContainer(renderer);
    container->removeAllClientsFromCache();
}

void RenderSVGGradientStop::layout()
{
    clearNeedsLayout();
}

SVGGradientElement* RenderSVGGradientStop::gradientElement() const
{
    ContainerNode* parentNode = node()->parentNode();
    ASSERT(parentNode);
    return isSVGGradientElement(*parentNode) ? toSVGGradientElement(parentNode) : 0;
}

}
