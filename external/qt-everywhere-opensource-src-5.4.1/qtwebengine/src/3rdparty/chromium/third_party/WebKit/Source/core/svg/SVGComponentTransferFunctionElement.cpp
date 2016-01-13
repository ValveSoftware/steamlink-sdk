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

#include "core/svg/SVGComponentTransferFunctionElement.h"

#include "core/SVGNames.h"
#include "core/dom/Attribute.h"
#include "core/svg/SVGFEComponentTransferElement.h"
#include "core/svg/SVGNumberList.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<ComponentTransferType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(FECOMPONENTTRANSFER_TYPE_IDENTITY, "identity"));
        entries.append(std::make_pair(FECOMPONENTTRANSFER_TYPE_TABLE, "table"));
        entries.append(std::make_pair(FECOMPONENTTRANSFER_TYPE_DISCRETE, "discrete"));
        entries.append(std::make_pair(FECOMPONENTTRANSFER_TYPE_LINEAR, "linear"));
        entries.append(std::make_pair(FECOMPONENTTRANSFER_TYPE_GAMMA, "gamma"));
    }
    return entries;
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_tableValues(SVGAnimatedNumberList::create(this, SVGNames::tableValuesAttr, SVGNumberList::create()))
    , m_slope(SVGAnimatedNumber::create(this, SVGNames::slopeAttr, SVGNumber::create(1)))
    , m_intercept(SVGAnimatedNumber::create(this, SVGNames::interceptAttr, SVGNumber::create()))
    , m_amplitude(SVGAnimatedNumber::create(this, SVGNames::amplitudeAttr, SVGNumber::create(1)))
    , m_exponent(SVGAnimatedNumber::create(this, SVGNames::exponentAttr, SVGNumber::create(1)))
    , m_offset(SVGAnimatedNumber::create(this, SVGNames::offsetAttr, SVGNumber::create()))
    , m_type(SVGAnimatedEnumeration<ComponentTransferType>::create(this, SVGNames::typeAttr, FECOMPONENTTRANSFER_TYPE_IDENTITY))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_tableValues);
    addToPropertyMap(m_slope);
    addToPropertyMap(m_intercept);
    addToPropertyMap(m_amplitude);
    addToPropertyMap(m_exponent);
    addToPropertyMap(m_offset);
    addToPropertyMap(m_type);
}

bool SVGComponentTransferFunctionElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::typeAttr);
        supportedAttributes.add(SVGNames::tableValuesAttr);
        supportedAttributes.add(SVGNames::slopeAttr);
        supportedAttributes.add(SVGNames::interceptAttr);
        supportedAttributes.add(SVGNames::amplitudeAttr);
        supportedAttributes.add(SVGNames::exponentAttr);
        supportedAttributes.add(SVGNames::offsetAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGComponentTransferFunctionElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::tableValuesAttr)
        m_tableValues->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::typeAttr)
        m_type->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::slopeAttr)
        m_slope->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::interceptAttr)
        m_intercept->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::amplitudeAttr)
        m_amplitude->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::exponentAttr)
        m_exponent->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::offsetAttr)
        m_offset->setBaseValueAsString(value, parseError);

    reportAttributeParsingError(parseError, name, value);
}

void SVGComponentTransferFunctionElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    invalidateFilterPrimitiveParent(this);
}

ComponentTransferFunction SVGComponentTransferFunctionElement::transferFunction() const
{
    ComponentTransferFunction func;
    func.type = m_type->currentValue()->enumValue();
    func.slope = m_slope->currentValue()->value();
    func.intercept = m_intercept->currentValue()->value();
    func.amplitude = m_amplitude->currentValue()->value();
    func.exponent = m_exponent->currentValue()->value();
    func.offset = m_offset->currentValue()->value();
    func.tableValues = m_tableValues->currentValue()->toFloatVector();
    return func;
}

}
