/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/html/forms/BaseTemporalInputType.h"

#include "core/html/HTMLInputElement.h"
#include "core/html/forms/ChooserOnlyTemporalInputTypeView.h"
#include "core/html/forms/MultipleFieldsTemporalInputTypeView.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"
#include <limits>

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

static const int msecPerMinute = 60 * 1000;
static const int msecPerSecond = 1000;

String BaseTemporalInputType::badInputText() const
{
    return locale().queryString(WebLocalizedString::ValidationBadInputForDateTime);
}

InputTypeView* BaseTemporalInputType::createView()
{
    if (RuntimeEnabledFeatures::inputMultipleFieldsUIEnabled())
        return MultipleFieldsTemporalInputTypeView::create(element(), *this);
    return ChooserOnlyTemporalInputTypeView::create(element(), *this);
}

double BaseTemporalInputType::valueAsDate() const
{
    return valueAsDouble();
}

void BaseTemporalInputType::setValueAsDate(double value, ExceptionState&) const
{
    element().setValue(serializeWithMilliseconds(value));
}

double BaseTemporalInputType::valueAsDouble() const
{
    const Decimal value = parseToNumber(element().value(), Decimal::nan());
    return value.isFinite() ? value.toDouble() : DateComponents::invalidMilliseconds();
}

void BaseTemporalInputType::setValueAsDouble(double newValue, TextFieldEventBehavior eventBehavior, ExceptionState& exceptionState) const
{
    setValueAsDecimal(Decimal::fromDouble(newValue), eventBehavior, exceptionState);
}

bool BaseTemporalInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !parseToDateComponents(value, 0);
}

bool BaseTemporalInputType::typeMismatch() const
{
    return typeMismatchFor(element().value());
}

String BaseTemporalInputType::rangeOverflowText(const Decimal& maximum) const
{
    return locale().queryString(WebLocalizedString::ValidationRangeOverflowDateTime, localizeValue(serialize(maximum)));
}

String BaseTemporalInputType::rangeUnderflowText(const Decimal& minimum) const
{
    return locale().queryString(WebLocalizedString::ValidationRangeUnderflowDateTime, localizeValue(serialize(minimum)));
}

Decimal BaseTemporalInputType::defaultValueForStepUp() const
{
    return Decimal::fromDouble(convertToLocalTime(currentTimeMS()));
}

bool BaseTemporalInputType::isSteppable() const
{
    return true;
}

Decimal BaseTemporalInputType::parseToNumber(const String& source, const Decimal& defaultValue) const
{
    DateComponents date;
    if (!parseToDateComponents(source, &date))
        return defaultValue;
    double msec = date.millisecondsSinceEpoch();
    DCHECK(std::isfinite(msec));
    return Decimal::fromDouble(msec);
}

bool BaseTemporalInputType::parseToDateComponents(const String& source, DateComponents* out) const
{
    if (source.isEmpty())
        return false;
    DateComponents ignoredResult;
    if (!out)
        out = &ignoredResult;
    return parseToDateComponentsInternal(source, out);
}

String BaseTemporalInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return String();
    DateComponents date;
    if (!setMillisecondToDateComponents(value.toDouble(), &date))
        return String();
    return serializeWithComponents(date);
}

String BaseTemporalInputType::serializeWithComponents(const DateComponents& date) const
{
    Decimal step;
    if (!element().getAllowedValueStep(&step))
        return date.toString();
    if (step.remainder(msecPerMinute).isZero())
        return date.toString(DateComponents::None);
    if (step.remainder(msecPerSecond).isZero())
        return date.toString(DateComponents::Second);
    return date.toString(DateComponents::Millisecond);
}

String BaseTemporalInputType::serializeWithMilliseconds(double value) const
{
    return serialize(Decimal::fromDouble(value));
}

String BaseTemporalInputType::localizeValue(const String& proposedValue) const
{
    DateComponents date;
    if (!parseToDateComponents(proposedValue, &date))
        return proposedValue;

    String localized = element().locale().formatDateTime(date);
    return localized.isEmpty() ? proposedValue : localized;
}

String BaseTemporalInputType::visibleValue() const
{
    return localizeValue(element().value());
}

String BaseTemporalInputType::sanitizeValue(const String& proposedValue) const
{
    return typeMismatchFor(proposedValue) ? emptyString() : proposedValue;
}

bool BaseTemporalInputType::supportsReadOnly() const
{
    return true;
}

bool BaseTemporalInputType::shouldRespectListAttribute()
{
    return true;
}

bool BaseTemporalInputType::valueMissing(const String& value) const
{
    return element().isRequired() && value.isEmpty();
}

bool BaseTemporalInputType::shouldShowFocusRingOnMouseFocus() const
{
    return true;
}

bool BaseTemporalInputType::shouldHaveSecondField(const DateComponents& date) const
{
    StepRange stepRange = createStepRange(AnyIsDefaultStep);
    return date.second() || date.millisecond()
        || !stepRange.minimum().remainder(static_cast<int>(msPerMinute)).isZero()
        || !stepRange.step().remainder(static_cast<int>(msPerMinute)).isZero();
}

} // namespace blink
