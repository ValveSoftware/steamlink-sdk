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
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGTests.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class PatternAttributes;

class SVGPatternElement final : public SVGElement,
                                public SVGURIReference,
                                public SVGTests,
                                public SVGFitToViewBox {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(SVGPatternElement);
public:
    DECLARE_NODE_FACTORY(SVGPatternElement);

    void collectPatternAttributes(PatternAttributes&) const;

    AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const override;

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

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGPatternElement(Document&);

    bool isValid() const override { return SVGTests::isValid(); }
    bool needsPendingResourceHandling() const override { return false; }

    void svgAttributeChanged(const QualifiedName&) override;
    void childrenChanged(const ChildrenChange&) override;

    LayoutObject* createLayoutObject(const ComputedStyle&) override;

    bool selfHasRelativeLengths() const override;

    Member<SVGAnimatedLength> m_x;
    Member<SVGAnimatedLength> m_y;
    Member<SVGAnimatedLength> m_width;
    Member<SVGAnimatedLength> m_height;
    Member<SVGAnimatedTransformList> m_patternTransform;
    Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_patternUnits;
    Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_patternContentUnits;
};

} // namespace blink

#endif // SVGPatternElement_h
