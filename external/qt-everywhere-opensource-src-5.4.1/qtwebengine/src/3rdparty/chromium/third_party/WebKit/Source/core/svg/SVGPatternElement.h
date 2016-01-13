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

#ifndef SVGPatternElement_h
#define SVGPatternElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGTests.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"

namespace WebCore {

struct PatternAttributes;

class SVGPatternElement FINAL : public SVGElement,
                                public SVGURIReference,
                                public SVGTests,
                                public SVGFitToViewBox {
public:
    DECLARE_NODE_FACTORY(SVGPatternElement);

    void collectPatternAttributes(PatternAttributes&) const;

    virtual AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const OVERRIDE;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }
    SVGAnimatedTransformList* patternTransform() { return m_patternTransform.get(); }
    const SVGAnimatedTransformList* patternTransform() const { return m_patternTransform.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* patternUnits() { return m_patternUnits.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* patternContentUnits() { return m_patternContentUnits.get(); }
    const SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* patternUnits() const { return m_patternUnits.get(); }
    const SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* patternContentUnits() const { return m_patternContentUnits.get(); }

private:
    explicit SVGPatternElement(Document&);

    virtual bool isValid() const OVERRIDE { return SVGTests::isValid(); }
    virtual bool needsPendingResourceHandling() const OVERRIDE { return false; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    RefPtr<SVGAnimatedLength> m_x;
    RefPtr<SVGAnimatedLength> m_y;
    RefPtr<SVGAnimatedLength> m_width;
    RefPtr<SVGAnimatedLength> m_height;
    RefPtr<SVGAnimatedTransformList> m_patternTransform;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_patternUnits;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_patternContentUnits;
};

} // namespace WebCore

#endif
