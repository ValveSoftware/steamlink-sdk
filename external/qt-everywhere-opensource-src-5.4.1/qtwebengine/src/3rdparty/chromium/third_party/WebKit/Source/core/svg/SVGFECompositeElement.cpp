/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFECompositeElement.h"

#include "core/SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<CompositeOperationType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_OVER, "over"));
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_IN, "in"));
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_OUT, "out"));
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_ATOP, "atop"));
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_XOR, "xor"));
        entries.append(std::make_pair(FECOMPOSITE_OPERATOR_ARITHMETIC, "arithmetic"));
    }
    return entries;
}

inline SVGFECompositeElement::SVGFECompositeElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feCompositeTag, document)
    , m_k1(SVGAnimatedNumber::create(this, SVGNames::k1Attr, SVGNumber::create()))
    , m_k2(SVGAnimatedNumber::create(this, SVGNames::k2Attr, SVGNumber::create()))
    , m_k3(SVGAnimatedNumber::create(this, SVGNames::k3Attr, SVGNumber::create()))
    , m_k4(SVGAnimatedNumber::create(this, SVGNames::k4Attr, SVGNumber::create()))
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
    , m_in2(SVGAnimatedString::create(this, SVGNames::in2Attr, SVGString::create()))
    , m_svgOperator(SVGAnimatedEnumeration<CompositeOperationType>::create(this, SVGNames::operatorAttr, FECOMPOSITE_OPERATOR_OVER))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_k1);
    addToPropertyMap(m_k2);
    addToPropertyMap(m_k3);
    addToPropertyMap(m_k4);
    addToPropertyMap(m_in1);
    addToPropertyMap(m_in2);
    addToPropertyMap(m_svgOperator);
}

DEFINE_NODE_FACTORY(SVGFECompositeElement)

bool SVGFECompositeElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::in2Attr);
        supportedAttributes.add(SVGNames::operatorAttr);
        supportedAttributes.add(SVGNames::k1Attr);
        supportedAttributes.add(SVGNames::k2Attr);
        supportedAttributes.add(SVGNames::k3Attr);
        supportedAttributes.add(SVGNames::k4Attr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFECompositeElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
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
    else if (name == SVGNames::k1Attr)
        m_k1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::k2Attr)
        m_k2->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::k3Attr)
        m_k3->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::k4Attr)
        m_k4->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::operatorAttr)
        m_svgOperator->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFECompositeElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEComposite* composite = static_cast<FEComposite*>(effect);
    if (attrName == SVGNames::operatorAttr)
        return composite->setOperation(m_svgOperator->currentValue()->enumValue());
    if (attrName == SVGNames::k1Attr)
        return composite->setK1(m_k1->currentValue()->value());
    if (attrName == SVGNames::k2Attr)
        return composite->setK2(m_k2->currentValue()->value());
    if (attrName == SVGNames::k3Attr)
        return composite->setK3(m_k3->currentValue()->value());
    if (attrName == SVGNames::k4Attr)
        return composite->setK4(m_k4->currentValue()->value());

    ASSERT_NOT_REACHED();
    return false;
}


void SVGFECompositeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::operatorAttr
        || attrName == SVGNames::k1Attr
        || attrName == SVGNames::k2Attr
        || attrName == SVGNames::k3Attr
        || attrName == SVGNames::k4Attr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFECompositeElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));
    FilterEffect* input2 = filterBuilder->getEffectById(AtomicString(m_in2->currentValue()->value()));

    if (!input1 || !input2)
        return nullptr;

    RefPtr<FilterEffect> effect = FEComposite::create(filter, m_svgOperator->currentValue()->enumValue(), m_k1->currentValue()->value(), m_k2->currentValue()->value(), m_k3->currentValue()->value(), m_k4->currentValue()->value());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);
    return effect.release();
}

}
