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

#ifndef BaseTemporalInputType_h
#define BaseTemporalInputType_h

#include "core/html/forms/InputType.h"
#include "core/html/shadow/DateTimeEditElement.h"
#include "platform/DateComponents.h"

namespace blink {

class ExceptionState;

// A super class of date, datetime, datetime-local, month, time, and week types.
// TODO(tkent): A single temporal input type creates two InputTypeView instances
// unnecessarily.  One is ChooserOnlyTemporalInputTypeView or
// MultipleFieldsTemporalInputType, and another is BaseTemporalInputType, which
// inherits from InputTypeView through InputType.  The latter is not used.
class BaseTemporalInputType : public InputType {
public:
    String visibleValue() const override;
    String sanitizeValue(const String&) const override;
    // Parses the specified string for this InputType, and returns true if it
    // is successfully parsed. An instance pointed by the DateComponents*
    // parameter will have parsed values and be modified even if the parsing
    // fails. The DateComponents* parameter may be 0.
    bool parseToDateComponents(const String&, DateComponents*) const;
    virtual bool setMillisecondToDateComponents(double, DateComponents*) const = 0;

    // Provide some helpers for MultipleFieldsTemporalInputTypeView.
    virtual String formatDateTimeFieldsState(const DateTimeFieldsState&) const = 0;
    virtual void setupLayoutParameters(DateTimeEditElement::LayoutParameters&, const DateComponents&) const = 0;
    virtual bool isValidFormat(bool hasYear, bool hasMonth, bool hasWeek, bool hasDay, bool hasAMPM, bool hasHour, bool hasMinute, bool hasSecond) const = 0;

protected:
    BaseTemporalInputType(HTMLInputElement& element) : InputType(element) { }
    Decimal parseToNumber(const String&, const Decimal&) const override;
    String serialize(const Decimal&) const override;
    String serializeWithComponents(const DateComponents&) const;
    bool shouldHaveSecondField(const DateComponents&) const;

private:
    virtual bool parseToDateComponentsInternal(const String&, DateComponents*) const = 0;

    String badInputText() const override;
    InputTypeView* createView() override;
    double valueAsDate() const override;
    void setValueAsDate(double, ExceptionState&) const override;
    double valueAsDouble() const override;
    void setValueAsDouble(double, TextFieldEventBehavior, ExceptionState&) const override;
    bool typeMismatchFor(const String&) const override;
    bool typeMismatch() const override;
    bool valueMissing(const String&) const override;
    String rangeOverflowText(const Decimal& maximum) const override;
    String rangeUnderflowText(const Decimal& minimum) const override;
    Decimal defaultValueForStepUp() const override;
    bool isSteppable() const override;
    virtual String serializeWithMilliseconds(double) const;
    String localizeValue(const String&) const override;
    bool supportsReadOnly() const override;
    bool shouldRespectListAttribute() override;
    bool shouldShowFocusRingOnMouseFocus() const override;
};

} // namespace blink
#endif // BaseTemporalInputType_h
