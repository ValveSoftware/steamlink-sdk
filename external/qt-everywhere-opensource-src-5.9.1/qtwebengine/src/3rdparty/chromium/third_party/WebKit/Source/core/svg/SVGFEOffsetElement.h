/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGFEOffsetElement_h
#define SVGFEOffsetElement_h

#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFEOffsetElement final : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEOffsetElement);

  SVGAnimatedNumber* dx() { return m_dx.get(); }
  SVGAnimatedNumber* dy() { return m_dy.get(); }
  SVGAnimatedString* in1() { return m_in1.get(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGFEOffsetElement(Document&);

  void svgAttributeChanged(const QualifiedName&) override;
  FilterEffect* build(SVGFilterBuilder*, Filter*) override;

  Member<SVGAnimatedNumber> m_dx;
  Member<SVGAnimatedNumber> m_dy;
  Member<SVGAnimatedString> m_in1;
};

}  // namespace blink

#endif  // SVGFEOffsetElement_h
