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

#ifndef BaseMultipleFieldsDateAndTimeInputType_h
#define BaseMultipleFieldsDateAndTimeInputType_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/forms/BaseDateAndTimeInputType.h"

#include "core/html/shadow/ClearButtonElement.h"
#include "core/html/shadow/DateTimeEditElement.h"
#include "core/html/shadow/PickerIndicatorElement.h"
#include "core/html/shadow/SpinButtonElement.h"

namespace WebCore {

struct DateTimeChooserParameters;

class BaseMultipleFieldsDateAndTimeInputType
    : public BaseDateAndTimeInputType
    , protected DateTimeEditElement::EditControlOwner
    , protected PickerIndicatorElement::PickerIndicatorOwner
    , protected SpinButtonElement::SpinButtonOwner
    , protected ClearButtonElement::ClearButtonOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BaseMultipleFieldsDateAndTimeInputType);

public:
    virtual bool isValidFormat(bool hasYear, bool hasMonth, bool hasWeek, bool hasDay, bool hasAMPM, bool hasHour, bool hasMinute, bool hasSecond) const = 0;

protected:
    BaseMultipleFieldsDateAndTimeInputType(HTMLInputElement&);
    virtual ~BaseMultipleFieldsDateAndTimeInputType();

    virtual void setupLayoutParameters(DateTimeEditElement::LayoutParameters&, const DateComponents&) const = 0;
    bool shouldHaveSecondField(const DateComponents&) const;

private:
    // DateTimeEditElement::EditControlOwner functions
    virtual void didBlurFromControl() OVERRIDE FINAL;
    virtual void didFocusOnControl() OVERRIDE FINAL;
    virtual void editControlValueChanged() OVERRIDE FINAL;
    virtual bool isEditControlOwnerDisabled() const OVERRIDE FINAL;
    virtual bool isEditControlOwnerReadOnly() const OVERRIDE FINAL;
    virtual AtomicString localeIdentifier() const OVERRIDE FINAL;
    virtual void editControlDidChangeValueByKeyboard() OVERRIDE FINAL;

    // SpinButtonElement::SpinButtonOwner functions.
    virtual void focusAndSelectSpinButtonOwner() OVERRIDE;
    virtual bool shouldSpinButtonRespondToMouseEvents() OVERRIDE;
    virtual bool shouldSpinButtonRespondToWheelEvents() OVERRIDE;
    virtual void spinButtonStepDown() OVERRIDE;
    virtual void spinButtonStepUp() OVERRIDE;
    virtual void spinButtonDidReleaseMouseCapture(SpinButtonElement::EventDispatch) OVERRIDE;

    // PickerIndicatorElement::PickerIndicatorOwner functions
    virtual bool isPickerIndicatorOwnerDisabledOrReadOnly() const OVERRIDE FINAL;
    virtual void pickerIndicatorChooseValue(const String&) OVERRIDE FINAL;
    virtual void pickerIndicatorChooseValue(double) OVERRIDE FINAL;
    virtual bool setupDateTimeChooserParameters(DateTimeChooserParameters&) OVERRIDE FINAL;

    // ClearButtonElement::ClearButtonOwner functions.
    virtual void focusAndSelectClearButtonOwner() OVERRIDE;
    virtual bool shouldClearButtonRespondToMouseEvents() OVERRIDE;
    virtual void clearValue() OVERRIDE;

    // InputType functions
    virtual String badInputText() const OVERRIDE;
    virtual void blur() OVERRIDE FINAL;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(PassRefPtr<RenderStyle>) OVERRIDE;
    virtual void createShadowSubtree() OVERRIDE FINAL;
    virtual void destroyShadowSubtree() OVERRIDE FINAL;
    virtual void disabledAttributeChanged() OVERRIDE FINAL;
    virtual void forwardEvent(Event*) OVERRIDE FINAL;
    virtual void handleFocusEvent(Element* oldFocusedElement, FocusType) OVERRIDE;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE FINAL;
    virtual bool hasBadInput() const OVERRIDE;
    virtual bool hasCustomFocusLogic() const OVERRIDE FINAL;
    virtual void minOrMaxAttributeChanged() OVERRIDE FINAL;
    virtual void readonlyAttributeChanged() OVERRIDE FINAL;
    virtual void requiredAttributeChanged() OVERRIDE FINAL;
    virtual void restoreFormControlState(const FormControlState&) OVERRIDE FINAL;
    virtual FormControlState saveFormControlState() const OVERRIDE FINAL;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) OVERRIDE FINAL;
    virtual bool shouldUseInputMethod() const OVERRIDE FINAL;
    virtual void stepAttributeChanged() OVERRIDE FINAL;
    virtual void updateView() OVERRIDE FINAL;
    virtual void valueAttributeChanged() OVERRIDE;
    virtual void listAttributeTargetChanged() OVERRIDE FINAL;
    virtual void updateClearButtonVisibility() OVERRIDE FINAL;

    DateTimeEditElement* dateTimeEditElement() const;
    SpinButtonElement* spinButtonElement() const;
    ClearButtonElement* clearButtonElement() const;
    PickerIndicatorElement* pickerIndicatorElement() const;
    bool containsFocusedShadowElement() const;
    void showPickerIndicator();
    void hidePickerIndicator();
    void updatePickerIndicatorVisibility();

    bool m_isDestroyingShadowSubtree;
    bool m_pickerIndicatorIsVisible;
    bool m_pickerIndicatorIsAlwaysVisible;
};

} // namespace WebCore

#endif
#endif // BaseMultipleFieldsDateAndTimeInputType_h
