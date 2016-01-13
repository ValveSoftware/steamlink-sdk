/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGTextPositioningElement_h
#define SVGTextPositioningElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedLengthList.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGTextContentElement.h"

namespace WebCore {

class SVGTextPositioningElement : public SVGTextContentElement {
public:
    static SVGTextPositioningElement* elementFromRenderer(RenderObject*);

    SVGAnimatedLengthList* x() { return m_x.get(); }
    SVGAnimatedLengthList* y() { return m_y.get(); }
    SVGAnimatedLengthList* dx() { return m_dx.get(); }
    SVGAnimatedLengthList* dy() { return m_dy.get(); }
    SVGAnimatedNumberList* rotate() { return m_rotate.get(); }

protected:
    SVGTextPositioningElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE FINAL;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE FINAL;
    virtual bool isTextPositioning() const OVERRIDE FINAL { return true; }

    RefPtr<SVGAnimatedLengthList> m_x;
    RefPtr<SVGAnimatedLengthList> m_y;
    RefPtr<SVGAnimatedLengthList> m_dx;
    RefPtr<SVGAnimatedLengthList> m_dy;
    RefPtr<SVGAnimatedNumberList> m_rotate;
};

inline bool isSVGTextPositioningElement(const Node& node)
{
    return node.isSVGElement() && toSVGElement(node).isTextPositioning();
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGTextPositioningElement);

} // namespace WebCore

#endif
