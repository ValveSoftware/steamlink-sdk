/*
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
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

#ifndef SVGMaskElement_h
#define SVGMaskElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTests.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGMaskElement final : public SVGElement, public SVGTests {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGMaskElement);

 public:
  DECLARE_NODE_FACTORY(SVGMaskElement);

  SVGAnimatedLength* x() const { return m_x.get(); }
  SVGAnimatedLength* y() const { return m_y.get(); }
  SVGAnimatedLength* width() const { return m_width.get(); }
  SVGAnimatedLength* height() const { return m_height.get(); }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* maskUnits() {
    return m_maskUnits.get();
  }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* maskContentUnits() {
    return m_maskContentUnits.get();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGMaskElement(Document&);

  bool isValid() const override { return SVGTests::isValid(); }
  bool needsPendingResourceHandling() const override { return false; }

  void collectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  void svgAttributeChanged(const QualifiedName&) override;
  void childrenChanged(const ChildrenChange&) override;

  LayoutObject* createLayoutObject(const ComputedStyle&) override;

  bool selfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> m_x;
  Member<SVGAnimatedLength> m_y;
  Member<SVGAnimatedLength> m_width;
  Member<SVGAnimatedLength> m_height;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_maskUnits;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_maskContentUnits;
};

}  // namespace blink

#endif  // SVGMaskElement_h
