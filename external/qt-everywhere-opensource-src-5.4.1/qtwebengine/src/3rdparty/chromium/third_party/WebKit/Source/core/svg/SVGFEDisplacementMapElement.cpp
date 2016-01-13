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

#include "config.h"

#include "core/svg/SVGFEDisplacementMapElement.h"

#include "core/SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<ChannelSelectorType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(CHANNEL_R, "R"));
        entries.append(std::make_pair(CHANNEL_G, "G"));
        entries.append(std::make_pair(CHANNEL_B, "B"));
        entries.append(std::make_pair(CHANNEL_A, "A"));
    }
    return entries;
}

inline SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feDisplacementMapTag, document)
    , m_scale(SVGAnimatedNumber::create(this, SVGNames::scaleAttr, SVGNumber::create(0)))
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
    , m_in2(SVGAnimatedString::create(this, SVGNames::in2Attr, SVGString::create()))
    , m_xChannelSelector(SVGAnimatedEnumeration<ChannelSelectorType>::create(this, SVGNames::xChannelSelectorAttr, CHANNEL_A))
    , m_yChannelSelector(SVGAnimatedEnumeration<ChannelSelectorType>::create(this, SVGNames::yChannelSelectorAttr, CHANNEL_A))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_scale);
    addToPropertyMap(m_in1);
    addToPropertyMap(m_in2);
    addToPropertyMap(m_xChannelSelector);
    addToPropertyMap(m_yChannelSelector);
}

DEFINE_NODE_FACTORY(SVGFEDisplacementMapElement)

bool SVGFEDisplacementMapElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::in2Attr);
        supportedAttributes.add(SVGNames::xChannelSelectorAttr);
        supportedAttributes.add(SVGNames::yChannelSelectorAttr);
        supportedAttributes.add(SVGNames::scaleAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEDisplacementMapElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::inAttr)
        m_in1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::in2Attr)
        m_in2->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::scaleAttr)
        m_scale->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::xChannelSelectorAttr)
        m_xChannelSelector->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::yChannelSelectorAttr)
        m_yChannelSelector->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFEDisplacementMapElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEDisplacementMap* displacementMap = static_cast<FEDisplacementMap*>(effect);
    if (attrName == SVGNames::xChannelSelectorAttr)
        return displacementMap->setXChannelSelector(m_xChannelSelector->currentValue()->enumValue());
    if (attrName == SVGNames::yChannelSelectorAttr)
        return displacementMap->setYChannelSelector(m_yChannelSelector->currentValue()->enumValue());
    if (attrName == SVGNames::scaleAttr)
        return displacementMap->setScale(m_scale->currentValue()->value());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDisplacementMapElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::xChannelSelectorAttr || attrName == SVGNames::yChannelSelectorAttr || attrName == SVGNames::scaleAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEDisplacementMapElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));
    FilterEffect* input2 = filterBuilder->getEffectById(AtomicString(m_in2->currentValue()->value()));

    if (!input1 || !input2)
        return nullptr;

    RefPtr<FilterEffect> effect = FEDisplacementMap::create(filter, m_xChannelSelector->currentValue()->enumValue(), m_yChannelSelector->currentValue()->enumValue(), m_scale->currentValue()->value());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);
    return effect.release();
}

}
