/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGFEConvolveMatrixElement.h"

#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<EdgeModeType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(EDGEMODE_DUPLICATE, "duplicate"));
        entries.append(std::make_pair(EDGEMODE_WRAP, "wrap"));
        entries.append(std::make_pair(EDGEMODE_NONE, "none"));
    }
    return entries;
}

class SVGAnimatedOrder : public SVGAnimatedIntegerOptionalInteger {
public:
    static SVGAnimatedOrder* create(SVGElement* contextElement)
    {
        return new SVGAnimatedOrder(contextElement);
    }

    SVGParsingError setBaseValueAsString(const String&) override;

protected:
    SVGAnimatedOrder(SVGElement* contextElement)
        : SVGAnimatedIntegerOptionalInteger(contextElement, SVGNames::orderAttr, 0, 0)
    {
    }

    static SVGParsingError checkValue(SVGParsingError parseStatus, int value)
    {
        if (parseStatus != SVGParseStatus::NoError)
            return parseStatus;
        if (value < 0)
            return SVGParseStatus::NegativeValue;
        if (value == 0)
            return SVGParseStatus::ZeroValue;
        return SVGParseStatus::NoError;
    }
};

SVGParsingError SVGAnimatedOrder::setBaseValueAsString(const String& value)
{
    SVGParsingError parseStatus = SVGAnimatedIntegerOptionalInteger::setBaseValueAsString(value);
    // Check for semantic errors.
    parseStatus = checkValue(parseStatus, firstInteger()->baseValue()->value());
    parseStatus = checkValue(parseStatus, secondInteger()->baseValue()->value());
    return parseStatus;
}

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feConvolveMatrixTag, document)
    , m_bias(SVGAnimatedNumber::create(this, SVGNames::biasAttr, SVGNumber::create()))
    , m_divisor(SVGAnimatedNumber::create(this, SVGNames::divisorAttr, SVGNumber::create()))
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
    , m_edgeMode(SVGAnimatedEnumeration<EdgeModeType>::create(this, SVGNames::edgeModeAttr, EDGEMODE_DUPLICATE))
    , m_kernelMatrix(SVGAnimatedNumberList::create(this, SVGNames::kernelMatrixAttr, SVGNumberList::create()))
    , m_kernelUnitLength(SVGAnimatedNumberOptionalNumber::create(this, SVGNames::kernelUnitLengthAttr))
    , m_order(SVGAnimatedOrder::create(this))
    , m_preserveAlpha(SVGAnimatedBoolean::create(this, SVGNames::preserveAlphaAttr, SVGBoolean::create()))
    , m_targetX(SVGAnimatedInteger::create(this, SVGNames::targetXAttr, SVGInteger::create()))
    , m_targetY(SVGAnimatedInteger::create(this, SVGNames::targetYAttr, SVGInteger::create()))
{
    addToPropertyMap(m_preserveAlpha);
    addToPropertyMap(m_divisor);
    addToPropertyMap(m_bias);
    addToPropertyMap(m_kernelUnitLength);
    addToPropertyMap(m_kernelMatrix);
    addToPropertyMap(m_in1);
    addToPropertyMap(m_edgeMode);
    addToPropertyMap(m_order);
    addToPropertyMap(m_targetX);
    addToPropertyMap(m_targetY);
}

DEFINE_TRACE(SVGFEConvolveMatrixElement)
{
    visitor->trace(m_bias);
    visitor->trace(m_divisor);
    visitor->trace(m_in1);
    visitor->trace(m_edgeMode);
    visitor->trace(m_kernelMatrix);
    visitor->trace(m_kernelUnitLength);
    visitor->trace(m_order);
    visitor->trace(m_preserveAlpha);
    visitor->trace(m_targetX);
    visitor->trace(m_targetY);
    SVGFilterPrimitiveStandardAttributes::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEConvolveMatrixElement)

bool SVGFEConvolveMatrixElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEConvolveMatrix* convolveMatrix = static_cast<FEConvolveMatrix*>(effect);
    if (attrName == SVGNames::edgeModeAttr)
        return convolveMatrix->setEdgeMode(m_edgeMode->currentValue()->enumValue());
    if (attrName == SVGNames::divisorAttr)
        return convolveMatrix->setDivisor(m_divisor->currentValue()->value());
    if (attrName == SVGNames::biasAttr)
        return convolveMatrix->setBias(m_bias->currentValue()->value());
    if (attrName == SVGNames::targetXAttr)
        return convolveMatrix->setTargetOffset(IntPoint(m_targetX->currentValue()->value(), m_targetY->currentValue()->value()));
    if (attrName == SVGNames::targetYAttr)
        return convolveMatrix->setTargetOffset(IntPoint(m_targetX->currentValue()->value(), m_targetY->currentValue()->value()));
    if (attrName == SVGNames::preserveAlphaAttr)
        return convolveMatrix->setPreserveAlpha(m_preserveAlpha->currentValue()->value());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEConvolveMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::edgeModeAttr
        || attrName == SVGNames::divisorAttr
        || attrName == SVGNames::biasAttr
        || attrName == SVGNames::targetXAttr
        || attrName == SVGNames::targetYAttr
        || attrName == SVGNames::preserveAlphaAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr
        || attrName == SVGNames::orderAttr
        || attrName == SVGNames::kernelMatrixAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

FilterEffect* SVGFEConvolveMatrixElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));
    ASSERT(input1);

    int orderXValue = orderX()->currentValue()->value();
    int orderYValue = orderY()->currentValue()->value();
    if (!m_order->isSpecified()) {
        orderXValue = 3;
        orderYValue = 3;
    }

    int targetXValue = m_targetX->currentValue()->value();
    // The spec says the default value is: targetX = floor ( orderX / 2 ))
    if (!m_targetX->isSpecified())
        targetXValue = static_cast<int>(floorf(orderXValue / 2));

    int targetYValue = m_targetY->currentValue()->value();
    // The spec says the default value is: targetY = floor ( orderY / 2 ))
    if (!m_targetY->isSpecified())
        targetYValue = static_cast<int>(floorf(orderYValue / 2));

    float divisorValue = m_divisor->currentValue()->value();
    if (!m_divisor->isSpecified()) {
        SVGNumberList* kernelMatrix = m_kernelMatrix->currentValue();
        size_t kernelMatrixSize = kernelMatrix->length();
        for (size_t i = 0; i < kernelMatrixSize; ++i)
            divisorValue += kernelMatrix->at(i)->value();
        if (!divisorValue)
            divisorValue = 1;
    }

    FilterEffect* effect = FEConvolveMatrix::create(filter,
        IntSize(orderXValue, orderYValue), divisorValue,
        m_bias->currentValue()->value(), IntPoint(targetXValue, targetYValue), m_edgeMode->currentValue()->enumValue(),
        m_preserveAlpha->currentValue()->value(), m_kernelMatrix->currentValue()->toFloatVector());
    effect->inputEffects().append(input1);
    return effect;
}

} // namespace blink
