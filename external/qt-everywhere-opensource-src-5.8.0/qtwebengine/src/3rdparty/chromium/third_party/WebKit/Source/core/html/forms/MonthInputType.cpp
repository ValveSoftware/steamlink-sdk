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

#include "core/html/forms/MonthInputType.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/DateTimeFieldsState.h"
#include "platform/DateComponents.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

static const int monthDefaultStep = 1;
static const int monthDefaultStepBase = 0;
static const int monthStepScaleFactor = 1;

InputType* MonthInputType::create(HTMLInputElement& element)
{
    return new MonthInputType(element);
}

void MonthInputType::countUsage()
{
    countUsageIfVisible(UseCounter::InputTypeMonth);
}

const AtomicString& MonthInputType::formControlType() const
{
    return InputTypeNames::month;
}

double MonthInputType::valueAsDate() const
{
    DateComponents date;
    if (!parseToDateComponents(element().value(), &date))
        return DateComponents::invalidMilliseconds();
    double msec = date.millisecondsSinceEpoch();
    DCHECK(std::isfinite(msec));
    return msec;
}

String MonthInputType::serializeWithMilliseconds(double value) const
{
    DateComponents date;
    if (!date.setMillisecondsSinceEpochForMonth(value))
        return String();
    return serializeWithComponents(date);
}

Decimal MonthInputType::defaultValueForStepUp() const
{
    DateComponents date;
    date.setMillisecondsSinceEpochForMonth(convertToLocalTime(currentTimeMS()));
    double months = date.monthsSinceEpoch();
    DCHECK(std::isfinite(months));
    return Decimal::fromDouble(months);
}

StepRange MonthInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (monthDefaultStep, monthDefaultStepBase, monthStepScaleFactor, StepRange::ParsedStepValueShouldBeInteger));

    return InputType::createStepRange(anyStepHandling, Decimal::fromDouble(monthDefaultStepBase), Decimal::fromDouble(DateComponents::minimumMonth()), Decimal::fromDouble(DateComponents::maximumMonth()), stepDescription);
}

Decimal MonthInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    DateComponents date;
    if (!parseToDateComponents(src, &date))
        return defaultValue;
    double months = date.monthsSinceEpoch();
    DCHECK(std::isfinite(months));
    return Decimal::fromDouble(months);
}

bool MonthInputType::parseToDateComponentsInternal(const String& string, DateComponents* out) const
{
    DCHECK(out);
    unsigned end;
    return out->parseMonth(string, 0, end) && end == string.length();
}

bool MonthInputType::setMillisecondToDateComponents(double value, DateComponents* date) const
{
    DCHECK(date);
    return date->setMonthsSinceEpoch(value);
}

bool MonthInputType::canSetSuggestedValue()
{
    return true;
}

void MonthInputType::warnIfValueIsInvalid(const String& value) const
{
    if (value != element().sanitizeValue(value))
        addWarningToConsole("The specified value %s does not conform to the required format.  The format is \"yyyy-MM\" where yyyy is year in four or more digits, and MM is 01-12.", value);
}

String MonthInputType::formatDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState) const
{
    if (!dateTimeFieldsState.hasMonth() || !dateTimeFieldsState.hasYear())
        return emptyString();
    return String::format("%04u-%02u", dateTimeFieldsState.year(), dateTimeFieldsState.month());
}

void MonthInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    layoutParameters.dateTimeFormat = layoutParameters.locale.monthFormat();
    layoutParameters.fallbackDateTimeFormat = "yyyy-MM";
    if (!parseToDateComponents(element().fastGetAttribute(minAttr), &layoutParameters.minimum))
        layoutParameters.minimum = DateComponents();
    if (!parseToDateComponents(element().fastGetAttribute(maxAttr), &layoutParameters.maximum))
        layoutParameters.maximum = DateComponents();
    layoutParameters.placeholderForMonth = "--";
    layoutParameters.placeholderForYear = "----";
}

bool MonthInputType::isValidFormat(bool hasYear, bool hasMonth, bool hasWeek, bool hasDay, bool hasAMPM, bool hasHour, bool hasMinute, bool hasSecond) const
{
    return hasYear && hasMonth;
}

} // namespace blink
