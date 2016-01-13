/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#ifndef SVGGraphicsElement_h
#define SVGGraphicsElement_h

#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGRectTearOff.h"
#include "core/svg/SVGTests.h"

namespace WebCore {

class AffineTransform;
class Path;
class SVGMatrixTearOff;

class SVGGraphicsElement : public SVGElement, public SVGTests {
public:
    virtual ~SVGGraphicsElement();

    enum StyleUpdateStrategy { AllowStyleUpdate, DisallowStyleUpdate };

    AffineTransform getCTM(StyleUpdateStrategy = AllowStyleUpdate);
    AffineTransform getScreenCTM(StyleUpdateStrategy = AllowStyleUpdate);
    PassRefPtr<SVGMatrixTearOff> getCTMFromJavascript();
    PassRefPtr<SVGMatrixTearOff> getScreenCTMFromJavascript();

    PassRefPtr<SVGMatrixTearOff> getTransformToElement(SVGElement*, ExceptionState&);

    SVGElement* nearestViewportElement() const;
    SVGElement* farthestViewportElement() const;

    virtual AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const OVERRIDE { return animatedLocalTransform(); }
    virtual AffineTransform animatedLocalTransform() const;
    virtual AffineTransform* supplementalTransform() OVERRIDE;

    virtual FloatRect getBBox();
    PassRefPtr<SVGRectTearOff> getBBoxFromJavascript();

    // "base class" methods for all the elements which render as paths
    virtual void toClipPath(Path&);
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    virtual bool isValid() const OVERRIDE FINAL { return SVGTests::isValid(); }

    SVGAnimatedTransformList* transform() { return m_transform.get(); }
    const SVGAnimatedTransformList* transform() const { return m_transform.get(); }

    AffineTransform computeCTM(SVGElement::CTMScope mode, SVGGraphicsElement::StyleUpdateStrategy,
        const SVGGraphicsElement* ancestor = 0) const;

protected:
    SVGGraphicsElement(const QualifiedName&, Document&, ConstructionType = CreateSVGElement);

    virtual bool supportsFocus() const OVERRIDE { return Element::supportsFocus() || hasFocusEventListeners(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    RefPtr<SVGAnimatedTransformList> m_transform;

private:
    virtual bool isSVGGraphicsElement() const OVERRIDE FINAL { return true; }

    // Used by <animateMotion>
    OwnPtr<AffineTransform> m_supplementalTransform;
};

inline bool isSVGGraphicsElement(const Node& node)
{
    return node.isSVGElement() && toSVGElement(node).isSVGGraphicsElement();
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGGraphicsElement);

} // namespace WebCore

#endif // SVGGraphicsElement_h
