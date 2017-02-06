/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/SVGEnumeration.h"

#include "core/svg/SVGAnimationElement.h"

namespace blink {

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGEnumerationBase);

SVGEnumerationBase::~SVGEnumerationBase()
{
}

SVGPropertyBase* SVGEnumerationBase::cloneForAnimation(const String& value) const
{
    SVGEnumerationBase* svgEnumeration = clone();
    svgEnumeration->setValueAsString(value);
    return svgEnumeration;
}

String SVGEnumerationBase::valueAsString() const
{
    for (const auto& entry : m_entries) {
        if (m_value == entry.first)
            return entry.second;
    }

    ASSERT(m_value < maxInternalEnumValue());
    return emptyString();
}

void SVGEnumerationBase::setValue(unsigned short value)
{
    m_value = value;
    notifyChange();
}

SVGParsingError SVGEnumerationBase::setValueAsString(const String& string)
{
    for (const auto& entry : m_entries) {
        if (string == entry.second) {
            // 0 corresponds to _UNKNOWN enumeration values, and should not be settable.
            ASSERT(entry.first);
            m_value = entry.first;
            notifyChange();
            return SVGParseStatus::NoError;
        }
    }

    notifyChange();
    return SVGParseStatus::ExpectedEnumeration;
}

void SVGEnumerationBase::add(SVGPropertyBase*, SVGElement*)
{
    ASSERT_NOT_REACHED();
}

void SVGEnumerationBase::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase*, SVGElement*)
{
    ASSERT(animationElement);
    unsigned short fromEnumeration = animationElement->getAnimationMode() == ToAnimation ? m_value : toSVGEnumerationBase(from)->value();
    unsigned short toEnumeration = toSVGEnumerationBase(to)->value();

    animationElement->animateDiscreteType<unsigned short>(percentage, fromEnumeration, toEnumeration, m_value);
}

float SVGEnumerationBase::calculateDistance(SVGPropertyBase*, SVGElement*)
{
    // No paced animations for boolean.
    return -1;
}

} // namespace blink
