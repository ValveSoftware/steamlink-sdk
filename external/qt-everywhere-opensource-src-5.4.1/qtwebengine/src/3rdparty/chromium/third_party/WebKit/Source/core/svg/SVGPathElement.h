/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGPathElement_h
#define SVGPathElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedPath.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGPathByteStream.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class SVGPathSegArcAbs;
class SVGPathSegArcRel;
class SVGPathSegClosePath;
class SVGPathSegLinetoAbs;
class SVGPathSegLinetoRel;
class SVGPathSegMovetoAbs;
class SVGPathSegMovetoRel;
class SVGPathSegCurvetoCubicAbs;
class SVGPathSegCurvetoCubicRel;
class SVGPathSegLinetoVerticalAbs;
class SVGPathSegLinetoVerticalRel;
class SVGPathSegLinetoHorizontalAbs;
class SVGPathSegLinetoHorizontalRel;
class SVGPathSegCurvetoQuadraticAbs;
class SVGPathSegCurvetoQuadraticRel;
class SVGPathSegCurvetoCubicSmoothAbs;
class SVGPathSegCurvetoCubicSmoothRel;
class SVGPathSegCurvetoQuadraticSmoothAbs;
class SVGPathSegCurvetoQuadraticSmoothRel;

class SVGPathElement FINAL : public SVGGeometryElement {
public:
    DECLARE_NODE_FACTORY(SVGPathElement);

    float getTotalLength();
    PassRefPtr<SVGPointTearOff> getPointAtLength(float distance);
    unsigned getPathSegAtLength(float distance);

    SVGAnimatedNumber* pathLength() { return m_pathLength.get(); }

    PassRefPtr<SVGPathSegClosePath> createSVGPathSegClosePath();
    PassRefPtr<SVGPathSegMovetoAbs> createSVGPathSegMovetoAbs(float x, float y);
    PassRefPtr<SVGPathSegMovetoRel> createSVGPathSegMovetoRel(float x, float y);
    PassRefPtr<SVGPathSegLinetoAbs> createSVGPathSegLinetoAbs(float x, float y);
    PassRefPtr<SVGPathSegLinetoRel> createSVGPathSegLinetoRel(float x, float y);
    PassRefPtr<SVGPathSegCurvetoCubicAbs> createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
    PassRefPtr<SVGPathSegCurvetoCubicRel> createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
    PassRefPtr<SVGPathSegCurvetoQuadraticAbs> createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
    PassRefPtr<SVGPathSegCurvetoQuadraticRel> createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
    PassRefPtr<SVGPathSegArcAbs> createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
    PassRefPtr<SVGPathSegArcRel> createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
    PassRefPtr<SVGPathSegLinetoHorizontalAbs> createSVGPathSegLinetoHorizontalAbs(float x);
    PassRefPtr<SVGPathSegLinetoHorizontalRel> createSVGPathSegLinetoHorizontalRel(float x);
    PassRefPtr<SVGPathSegLinetoVerticalAbs> createSVGPathSegLinetoVerticalAbs(float y);
    PassRefPtr<SVGPathSegLinetoVerticalRel> createSVGPathSegLinetoVerticalRel(float y);
    PassRefPtr<SVGPathSegCurvetoCubicSmoothAbs> createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
    PassRefPtr<SVGPathSegCurvetoCubicSmoothRel> createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
    PassRefPtr<SVGPathSegCurvetoQuadraticSmoothAbs> createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
    PassRefPtr<SVGPathSegCurvetoQuadraticSmoothRel> createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);

    // Used in the bindings only.
    SVGPathSegListTearOff* pathSegList() { return m_pathSegList->baseVal(); }
    SVGPathSegListTearOff* animatedPathSegList() { return m_pathSegList->animVal(); }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    SVGPathSegListTearOff* normalizedPathSegList() { return 0; }
    SVGPathSegListTearOff* animatedNormalizedPathSegList() { return 0; }

    const SVGPathByteStream* pathByteStream() const { return m_pathSegList->currentValue()->byteStream(); }

    void pathSegListChanged(ListModification = ListModificationUnknown);

    virtual FloatRect getBBox() OVERRIDE;

private:
    explicit SVGPathElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual Node::InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    void invalidateMPathDependencies();

    RefPtr<SVGAnimatedNumber> m_pathLength;
    RefPtr<SVGAnimatedPath> m_pathSegList;
};

} // namespace WebCore

#endif
