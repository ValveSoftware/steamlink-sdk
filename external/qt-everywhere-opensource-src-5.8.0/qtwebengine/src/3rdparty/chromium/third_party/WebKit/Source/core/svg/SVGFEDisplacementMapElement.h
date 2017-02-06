/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#ifndef SVGFEDisplacementMapElement_h
#define SVGFEDisplacementMapElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEDisplacementMap.h"
#include "platform/heap/Handle.h"

namespace blink {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<ChannelSelectorType>();

class SVGFEDisplacementMapElement final : public SVGFilterPrimitiveStandardAttributes {
    DEFINE_WRAPPERTYPEINFO();
public:
    DECLARE_NODE_FACTORY(SVGFEDisplacementMapElement);

    static ChannelSelectorType stringToChannel(const String&);

    SVGAnimatedNumber* scale() { return m_scale.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
    SVGAnimatedString* in2() { return m_in2.get(); }
    SVGAnimatedEnumeration<ChannelSelectorType>* xChannelSelector() { return m_xChannelSelector.get(); }
    SVGAnimatedEnumeration<ChannelSelectorType>* yChannelSelector() { return m_yChannelSelector.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGFEDisplacementMapElement(Document&);

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    void svgAttributeChanged(const QualifiedName&) override;
    FilterEffect* build(SVGFilterBuilder*, Filter*) override;

    Member<SVGAnimatedNumber> m_scale;
    Member<SVGAnimatedString> m_in1;
    Member<SVGAnimatedString> m_in2;
    Member<SVGAnimatedEnumeration<ChannelSelectorType>> m_xChannelSelector;
    Member<SVGAnimatedEnumeration<ChannelSelectorType>> m_yChannelSelector;
};

} // namespace blink

#endif // SVGFEDisplacementMapElement_h
