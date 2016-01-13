/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#ifndef SVGFELightElement_h
#define SVGFELightElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/filters/LightSource.h"

namespace WebCore {

class Filter;

class SVGFELightElement : public SVGElement {
public:
    virtual PassRefPtr<LightSource> lightSource(Filter*) const = 0;
    static SVGFELightElement* findLightElement(const SVGElement&);

    SVGAnimatedNumber* azimuth() { return m_azimuth.get(); }
    const SVGAnimatedNumber* azimuth() const { return m_azimuth.get(); }
    SVGAnimatedNumber* elevation() { return m_elevation.get(); }
    const SVGAnimatedNumber* elevation() const { return m_elevation.get(); }
    SVGAnimatedNumber* x() { return m_x.get(); }
    const SVGAnimatedNumber* x() const { return m_x.get(); }
    SVGAnimatedNumber* y() { return m_y.get(); }
    const SVGAnimatedNumber* y() const { return m_y.get(); }
    SVGAnimatedNumber* z() { return m_z.get(); }
    const SVGAnimatedNumber* z() const { return m_z.get(); }
    SVGAnimatedNumber* pointsAtX() { return m_pointsAtX.get(); }
    const SVGAnimatedNumber* pointsAtX() const { return m_pointsAtX.get(); }
    SVGAnimatedNumber* pointsAtY() { return m_pointsAtY.get(); }
    const SVGAnimatedNumber* pointsAtY() const { return m_pointsAtY.get(); }
    SVGAnimatedNumber* pointsAtZ() { return m_pointsAtZ.get(); }
    const SVGAnimatedNumber* pointsAtZ() const { return m_pointsAtZ.get(); }
    SVGAnimatedNumber* specularExponent() { return m_specularExponent.get(); }
    const SVGAnimatedNumber* specularExponent() const { return m_specularExponent.get(); }
    SVGAnimatedNumber* limitingConeAngle() { return m_limitingConeAngle.get(); }
    const SVGAnimatedNumber* limitingConeAngle() const { return m_limitingConeAngle.get(); }

protected:
    SVGFELightElement(const QualifiedName&, Document&);

private:
    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE FINAL;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE FINAL;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE FINAL;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    RefPtr<SVGAnimatedNumber> m_azimuth;
    RefPtr<SVGAnimatedNumber> m_elevation;
    RefPtr<SVGAnimatedNumber> m_x;
    RefPtr<SVGAnimatedNumber> m_y;
    RefPtr<SVGAnimatedNumber> m_z;
    RefPtr<SVGAnimatedNumber> m_pointsAtX;
    RefPtr<SVGAnimatedNumber> m_pointsAtY;
    RefPtr<SVGAnimatedNumber> m_pointsAtZ;
    RefPtr<SVGAnimatedNumber> m_specularExponent;
    RefPtr<SVGAnimatedNumber> m_limitingConeAngle;
};

inline bool isSVGFELightElement(const Node& node)
{
    return node.hasTagName(SVGNames::feDistantLightTag) || node.hasTagName(SVGNames::fePointLightTag) || node.hasTagName(SVGNames::feSpotLightTag);
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGFELightElement);

} // namespace WebCore

#endif
