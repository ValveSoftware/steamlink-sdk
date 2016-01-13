/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGTextElement.h"

#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGText.h"

namespace WebCore {

inline SVGTextElement::SVGTextElement(Document& doc)
    : SVGTextPositioningElement(SVGNames::textTag, doc)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGTextElement)

// We override SVGGraphics::animatedLocalTransform() so that the transform-origin
// is not taken into account.
AffineTransform SVGTextElement::animatedLocalTransform() const
{
    AffineTransform matrix;
    RenderStyle* style = renderer() ? renderer()->style() : 0;

    // if CSS property was set, use that, otherwise fallback to attribute (if set)
    if (style && style->hasTransform()) {
        TransformationMatrix t;
        // For now, the transform-origin is not taken into account
        // Also, any percentage values will not be taken into account
        style->applyTransform(t, IntSize(0, 0), RenderStyle::ExcludeTransformOrigin);
        // Flatten any 3D transform
        matrix = t.toAffineTransform();
    } else {
        transform()->currentValue()->concatenate(matrix);
    }

    const AffineTransform* transform = const_cast<SVGTextElement*>(this)->supplementalTransform();
    if (transform)
        return *transform * matrix;
    return matrix;
}

RenderObject* SVGTextElement::createRenderer(RenderStyle*)
{
    return new RenderSVGText(this);
}

}
