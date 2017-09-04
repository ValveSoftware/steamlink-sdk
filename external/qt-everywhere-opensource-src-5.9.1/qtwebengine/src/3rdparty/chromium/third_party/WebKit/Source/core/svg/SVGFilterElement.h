/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#include "core/CoreExport.h"
#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT SVGFilterElement final : public SVGElement,
                                           public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGFilterElement);

 public:
  DECLARE_NODE_FACTORY(SVGFilterElement);
  DECLARE_VIRTUAL_TRACE();

  ~SVGFilterElement() override;

  SVGAnimatedLength* x() const { return m_x.get(); }
  SVGAnimatedLength* y() const { return m_y.get(); }
  SVGAnimatedLength* width() const { return m_width.get(); }
  SVGAnimatedLength* height() const { return m_height.get(); }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* filterUnits() {
    return m_filterUnits.get();
  }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* primitiveUnits() {
    return m_primitiveUnits.get();
  }

 private:
  explicit SVGFilterElement(Document&);

  bool needsPendingResourceHandling() const override { return false; }

  void svgAttributeChanged(const QualifiedName&) override;
  void childrenChanged(const ChildrenChange&) override;

  LayoutObject* createLayoutObject(const ComputedStyle&) override;

  bool selfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> m_x;
  Member<SVGAnimatedLength> m_y;
  Member<SVGAnimatedLength> m_width;
  Member<SVGAnimatedLength> m_height;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_filterUnits;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> m_primitiveUnits;
};

}  // namespace blink

#endif  // SVGFilterElement_h
