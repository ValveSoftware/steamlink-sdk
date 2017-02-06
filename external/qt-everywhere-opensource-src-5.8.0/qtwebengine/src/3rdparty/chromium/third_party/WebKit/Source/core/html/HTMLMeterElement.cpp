/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "core/html/HTMLMeterElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(Document& document)
    : LabelableElement(meterTag, document)
{
    UseCounter::count(document, UseCounter::MeterElement);
}

HTMLMeterElement::~HTMLMeterElement()
{
}

HTMLMeterElement* HTMLMeterElement::create(Document& document)
{
    HTMLMeterElement* meter = new HTMLMeterElement(document);
    meter->ensureUserAgentShadowRoot();
    return meter;
}

LayoutObject* HTMLMeterElement::createLayoutObject(const ComputedStyle& style)
{
    switch (style.appearance()) {
    case MeterPart:
        UseCounter::count(document(), UseCounter::MeterElementWithMeterAppearance);
        break;
    case NoControlPart:
        UseCounter::count(document(), UseCounter::MeterElementWithNoneAppearance);
        break;
    default:
        break;
    }
    return LayoutObject::createObject(this, style);
}

void HTMLMeterElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == valueAttr || name == minAttr || name == maxAttr || name == lowAttr || name == highAttr || name == optimumAttr)
        didElementStateChange();
    else
        LabelableElement::parseAttribute(name, oldValue, value);
}

double HTMLMeterElement::value() const
{
    double value = getFloatingPointAttribute(valueAttr, 0);
    return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value)
{
    setFloatingPointAttribute(valueAttr, value);
}

double HTMLMeterElement::min() const
{
    return getFloatingPointAttribute(minAttr, 0);
}

void HTMLMeterElement::setMin(double min)
{
    setFloatingPointAttribute(minAttr, min);
}

double HTMLMeterElement::max() const
{
    return std::max(getFloatingPointAttribute(maxAttr, std::max(1.0, min())), min());
}

void HTMLMeterElement::setMax(double max)
{
    setFloatingPointAttribute(maxAttr, max);
}

double HTMLMeterElement::low() const
{
    double low = getFloatingPointAttribute(lowAttr, min());
    return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low)
{
    setFloatingPointAttribute(lowAttr, low);
}

double HTMLMeterElement::high() const
{
    double high = getFloatingPointAttribute(highAttr, max());
    return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high)
{
    setFloatingPointAttribute(highAttr, high);
}

double HTMLMeterElement::optimum() const
{
    double optimum = getFloatingPointAttribute(optimumAttr, (max() + min()) / 2);
    return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum)
{
    setFloatingPointAttribute(optimumAttr, optimum);
}

HTMLMeterElement::GaugeRegion HTMLMeterElement::getGaugeRegion() const
{
    double lowValue = low();
    double highValue = high();
    double theValue = value();
    double optimumValue = optimum();

    if (optimumValue < lowValue) {
        // The optimum range stays under low
        if (theValue <= lowValue)
            return GaugeRegionOptimum;
        if (theValue <= highValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }

    if (highValue < optimumValue) {
        // The optimum range stays over high
        if (highValue <= theValue)
            return GaugeRegionOptimum;
        if (lowValue <= theValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }

    // The optimum range stays between high and low.
    // According to the standard, <meter> never show GaugeRegionEvenLessGood in this case
    // because the value is never less or greater than min or max.
    if (lowValue <= theValue && theValue <= highValue)
        return GaugeRegionOptimum;
    return GaugeRegionSuboptimal;
}

double HTMLMeterElement::valueRatio() const
{
    double min = this->min();
    double max = this->max();
    double value = this->value();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

void HTMLMeterElement::didElementStateChange()
{
    updateValueAppearance(valueRatio() * 100);
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_value);

    HTMLDivElement* inner = HTMLDivElement::create(document());
    inner->setShadowPseudoId(AtomicString("-webkit-meter-inner-element"));
    root.appendChild(inner);

    HTMLDivElement* bar = HTMLDivElement::create(document());
    bar->setShadowPseudoId(AtomicString("-webkit-meter-bar"));

    m_value = HTMLDivElement::create(document());
    updateValueAppearance(0);
    bar->appendChild(m_value);

    inner->appendChild(bar);

    HTMLDivElement* fallback = HTMLDivElement::create(document());
    fallback->appendChild(HTMLContentElement::create(document()));
    fallback->setShadowPseudoId(AtomicString("-internal-fallback"));
    root.appendChild(fallback);
}

void HTMLMeterElement::updateValueAppearance(double percentage)
{
    DEFINE_STATIC_LOCAL(AtomicString, optimumPseudoId, ("-webkit-meter-optimum-value"));
    DEFINE_STATIC_LOCAL(AtomicString, suboptimumPseudoId, ("-webkit-meter-suboptimum-value"));
    DEFINE_STATIC_LOCAL(AtomicString, evenLessGoodPseudoId, ("-webkit-meter-even-less-good-value"));

    m_value->setInlineStyleProperty(CSSPropertyWidth, percentage, CSSPrimitiveValue::UnitType::Percentage);
    switch (getGaugeRegion()) {
    case GaugeRegionOptimum:
        m_value->setShadowPseudoId(optimumPseudoId);
        break;
    case GaugeRegionSuboptimal:
        m_value->setShadowPseudoId(suboptimumPseudoId);
        break;
    case GaugeRegionEvenLessGood:
        m_value->setShadowPseudoId(evenLessGoodPseudoId);
        break;
    }
}

bool HTMLMeterElement::canContainRangeEndPoint() const
{
    document().updateStyleAndLayoutTreeForNode(this);
    return computedStyle() && !computedStyle()->hasAppearance();
}

DEFINE_TRACE(HTMLMeterElement)
{
    visitor->trace(m_value);
    LabelableElement::trace(visitor);
}

} // namespace blink
