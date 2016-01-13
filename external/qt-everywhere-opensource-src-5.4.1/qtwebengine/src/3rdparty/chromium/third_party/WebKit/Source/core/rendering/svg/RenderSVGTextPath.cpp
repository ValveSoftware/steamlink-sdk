/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/rendering/svg/RenderSVGTextPath.h"

#include "core/rendering/svg/SVGPathData.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGTextPathElement.h"

namespace WebCore {

RenderSVGTextPath::RenderSVGTextPath(Element* element)
    : RenderSVGInline(element)
{
}

bool RenderSVGTextPath::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    if (child->isText())
        return SVGRenderSupport::isRenderableTextNode(child);

#if ENABLE(SVG_FONTS)
    // 'altGlyph' is supported by the content model for 'textPath', but...
    ASSERT(child->node());
    if (isSVGAltGlyphElement(*child->node()))
        return false;
#endif

    return child->isSVGInline() && !child->isSVGTextPath();
}

Path RenderSVGTextPath::layoutPath() const
{
    SVGTextPathElement* textPathElement = toSVGTextPathElement(node());
    Element* targetElement = SVGURIReference::targetElementFromIRIString(textPathElement->href()->currentValue()->value(), textPathElement->treeScope());
    if (!isSVGPathElement(targetElement))
        return Path();

    SVGPathElement& pathElement = toSVGPathElement(*targetElement);

    Path pathData;
    updatePathFromGraphicsElement(&pathElement, pathData);

    // Spec:  The transform attribute on the referenced 'path' element represents a
    // supplemental transformation relative to the current user coordinate system for
    // the current 'text' element, including any adjustments to the current user coordinate
    // system due to a possible transform attribute on the current 'text' element.
    // http://www.w3.org/TR/SVG/text.html#TextPathElement
    pathData.transform(pathElement.animatedLocalTransform());
    return pathData;
}

float RenderSVGTextPath::startOffset() const
{
    return toSVGTextPathElement(node())->startOffset()->currentValue()->valueAsPercentage();
}

}
