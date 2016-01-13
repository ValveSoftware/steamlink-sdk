/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGTSpanElement.h"

#include "core/SVGNames.h"
#include "core/rendering/svg/RenderSVGTSpan.h"

namespace WebCore {

inline SVGTSpanElement::SVGTSpanElement(Document& document)
    : SVGTextPositioningElement(SVGNames::tspanTag, document)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGTSpanElement)

RenderObject* SVGTSpanElement::createRenderer(RenderStyle*)
{
    return new RenderSVGTSpan(this);
}

bool SVGTSpanElement::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode()
        && (isSVGAElement(*parentNode())
#if ENABLE(SVG_FONTS)
            || isSVGAltGlyphElement(*parentNode())
#endif
            || isSVGTextElement(*parentNode())
            || isSVGTextPathElement(*parentNode())
            || isSVGTSpanElement(*parentNode())))
        return Element::rendererIsNeeded(style);

    return false;
}

}
