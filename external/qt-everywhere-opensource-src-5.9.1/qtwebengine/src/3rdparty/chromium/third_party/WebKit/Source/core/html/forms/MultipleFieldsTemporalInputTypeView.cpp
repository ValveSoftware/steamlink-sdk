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

#include "core/html/forms/MultipleFieldsTemporalInputTypeView.h"

#include "core/CSSValueKeywords.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/BaseTemporalInputType.h"
#include "core/html/forms/DateTimeFieldsState.h"
#include "core/html/forms/FormController.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "platform/DateComponents.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/DateTimeFormat.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/DateMath.h"

namespace blink {

class DateTimeFormatValidator : public DateTimeFormat::TokenHandler {
 public:
  DateTimeFormatValidator()
      : m_hasYear(false),
        m_hasMonth(false),
        m_hasWeek(false),
        m_hasDay(false),
        m_hasAMPM(false),
        m_hasHour(false),
        m_hasMinute(false),
        m_hasSecond(false) {}

  void visitField(DateTimeFormat::FieldType, int) final;
  void visitLiteral(const String&) final {}

  bool validateFormat(const String& format, const BaseTemporalInputType&);

 private:
  bool m_hasYear;
  bool m_hasMonth;
  bool m_hasWeek;
  bool m_hasDay;
  bool m_hasAMPM;
  bool m_hasHour;
  bool m_hasMinute;
  bool m_hasSecond;
};

void DateTimeFormatValidator::visitField(DateTimeFormat::FieldType fieldType,
                                         int) {
  switch (fieldType) {
    case DateTimeFormat::FieldTypeYear:
      m_hasYear = true;
      break;
    case DateTimeFormat::FieldTypeMonth:  // Fallthrough.
    case DateTimeFormat::FieldTypeMonthStandAlone:
      m_hasMonth = true;
      break;
    case DateTimeFormat::FieldTypeWeekOfYear:
      m_hasWeek = true;
      break;
    case DateTimeFormat::FieldTypeDayOfMonth:
      m_hasDay = true;
      break;
    case DateTimeFormat::FieldTypePeriod:
      m_hasAMPM = true;
      break;
    case DateTimeFormat::FieldTypeHour11:  // Fallthrough.
    case DateTimeFormat::FieldTypeHour12:
      m_hasHour = true;
      break;
    case DateTimeFormat::FieldTypeHour23:  // Fallthrough.
    case DateTimeFormat::FieldTypeHour24:
      m_hasHour = true;
      m_hasAMPM = true;
      break;
    case DateTimeFormat::FieldTypeMinute:
      m_hasMinute = true;
      break;
    case DateTimeFormat::FieldTypeSecond:
      m_hasSecond = true;
      break;
    default:
      break;
  }
}

bool DateTimeFormatValidator::validateFormat(
    const String& format,
    const BaseTemporalInputType& inputType) {
  if (!DateTimeFormat::parse(format, *this))
    return false;
  return inputType.isValidFormat(m_hasYear, m_hasMonth, m_hasWeek, m_hasDay,
                                 m_hasAMPM, m_hasHour, m_hasMinute,
                                 m_hasSecond);
}

DateTimeEditElement* MultipleFieldsTemporalInputTypeView::dateTimeEditElement()
    const {
  return toDateTimeEditElementOrDie(
      element().userAgentShadowRoot()->getElementById(
          ShadowElementNames::dateTimeEdit()));
}

SpinButtonElement* MultipleFieldsTemporalInputTypeView::spinButtonElement()
    const {
  return toSpinButtonElementOrDie(
      element().userAgentShadowRoot()->getElementById(
          ShadowElementNames::spinButton()));
}

ClearButtonElement* MultipleFieldsTemporalInputTypeView::clearButtonElement()
    const {
  return toClearButtonElementOrDie(
      element().userAgentShadowRoot()->getElementById(
          ShadowElementNames::clearButton()));
}

PickerIndicatorElement*
MultipleFieldsTemporalInputTypeView::pickerIndicatorElement() const {
  return toPickerIndicatorElementOrDie(
      element().userAgentShadowRoot()->getElementById(
          ShadowElementNames::pickerIndicator()));
}

inline bool MultipleFieldsTemporalInputTypeView::containsFocusedShadowElement()
    const {
  return element().userAgentShadowRoot()->contains(
      element().document().focusedElement());
}

void MultipleFieldsTemporalInputTypeView::didBlurFromControl() {
  // We don't need to call blur(). This function is called when control
  // lost focus.

  if (containsFocusedShadowElement())
    return;
  EventQueueScope scope;
  // Remove focus ring by CSS "focus" pseudo class.
  element().setFocused(false);
  if (SpinButtonElement* spinButton = spinButtonElement())
    spinButton->releaseCapture();
}

void MultipleFieldsTemporalInputTypeView::didFocusOnControl() {
  // We don't need to call focus(). This function is called when control
  // got focus.

  if (!containsFocusedShadowElement())
    return;
  // Add focus ring by CSS "focus" pseudo class.
  // FIXME: Setting the focus flag to non-focused element is too tricky.
  element().setFocused(true);
}

void MultipleFieldsTemporalInputTypeView::editControlValueChanged() {
  String oldValue = element().value();
  String newValue = m_inputType->sanitizeValue(dateTimeEditElement()->value());
  // Even if oldValue is null and newValue is "", we should assume they are
  // same.
  if ((oldValue.isEmpty() && newValue.isEmpty()) || oldValue == newValue) {
    element().setNeedsValidityCheck();
  } else {
    element().setNonAttributeValue(newValue);
    element().setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::ControlValue));
    element().dispatchFormControlInputEvent();
  }
  element().notifyFormStateChanged();
  element().updateClearButtonVisibility();
}

String MultipleFieldsTemporalInputTypeView::formatDateTimeFieldsState(
    const DateTimeFieldsState& state) const {
  return m_inputType->formatDateTimeFieldsState(state);
}

bool MultipleFieldsTemporalInputTypeView::hasCustomFocusLogic() const {
  return false;
}

bool MultipleFieldsTemporalInputTypeView::isEditControlOwnerDisabled() const {
  return element().isDisabledFormControl();
}

bool MultipleFieldsTemporalInputTypeView::isEditControlOwnerReadOnly() const {
  return element().isReadOnly();
}

void MultipleFieldsTemporalInputTypeView::focusAndSelectSpinButtonOwner() {
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->focusIfNoFocus();
}

bool MultipleFieldsTemporalInputTypeView::
    shouldSpinButtonRespondToMouseEvents() {
  return !element().isDisabledOrReadOnly();
}

bool MultipleFieldsTemporalInputTypeView::
    shouldSpinButtonRespondToWheelEvents() {
  if (!shouldSpinButtonRespondToMouseEvents())
    return false;
  if (DateTimeEditElement* edit = dateTimeEditElement())
    return edit->hasFocusedField();
  return false;
}

void MultipleFieldsTemporalInputTypeView::spinButtonStepDown() {
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->stepDown();
}

void MultipleFieldsTemporalInputTypeView::spinButtonStepUp() {
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->stepUp();
}

void MultipleFieldsTemporalInputTypeView::spinButtonDidReleaseMouseCapture(
    SpinButtonElement::EventDispatch eventDispatch) {
  if (eventDispatch == SpinButtonElement::EventDispatchAllowed)
    element().dispatchFormControlChangeEvent();
}

bool MultipleFieldsTemporalInputTypeView::
    isPickerIndicatorOwnerDisabledOrReadOnly() const {
  return element().isDisabledOrReadOnly();
}

void MultipleFieldsTemporalInputTypeView::pickerIndicatorChooseValue(
    const String& value) {
  if (element().isValidValue(value)) {
    element().setValue(value, DispatchInputAndChangeEvent);
    return;
  }

  DateTimeEditElement* edit = this->dateTimeEditElement();
  if (!edit)
    return;
  EventQueueScope scope;
  DateComponents date;
  unsigned end;
  if (date.parseDate(value, 0, end) && end == value.length())
    edit->setOnlyYearMonthDay(date);
  element().dispatchFormControlChangeEvent();
}

void MultipleFieldsTemporalInputTypeView::pickerIndicatorChooseValue(
    double value) {
  DCHECK(std::isfinite(value) || std::isnan(value));
  if (std::isnan(value))
    element().setValue(emptyString(), DispatchInputAndChangeEvent);
  else
    element().setValueAsNumber(value, ASSERT_NO_EXCEPTION,
                               DispatchInputAndChangeEvent);
}

Element& MultipleFieldsTemporalInputTypeView::pickerOwnerElement() const {
  return element();
}

bool MultipleFieldsTemporalInputTypeView::setupDateTimeChooserParameters(
    DateTimeChooserParameters& parameters) {
  return element().setupDateTimeChooserParameters(parameters);
}

MultipleFieldsTemporalInputTypeView::MultipleFieldsTemporalInputTypeView(
    HTMLInputElement& element,
    BaseTemporalInputType& inputType)
    : InputTypeView(element),
      m_inputType(inputType),
      m_isDestroyingShadowSubtree(false),
      m_pickerIndicatorIsVisible(false),
      m_pickerIndicatorIsAlwaysVisible(false) {}

MultipleFieldsTemporalInputTypeView*
MultipleFieldsTemporalInputTypeView::create(HTMLInputElement& element,
                                            BaseTemporalInputType& inputType) {
  return new MultipleFieldsTemporalInputTypeView(element, inputType);
}

MultipleFieldsTemporalInputTypeView::~MultipleFieldsTemporalInputTypeView() {}

DEFINE_TRACE(MultipleFieldsTemporalInputTypeView) {
  visitor->trace(m_inputType);
  InputTypeView::trace(visitor);
}

void MultipleFieldsTemporalInputTypeView::blur() {
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->blurByOwner();
}

PassRefPtr<ComputedStyle>
MultipleFieldsTemporalInputTypeView::customStyleForLayoutObject(
    PassRefPtr<ComputedStyle> originalStyle) {
  EDisplay originalDisplay = originalStyle->display();
  EDisplay newDisplay = originalDisplay;
  if (originalDisplay == EDisplay::Inline ||
      originalDisplay == EDisplay::InlineBlock)
    newDisplay = EDisplay::InlineFlex;
  else if (originalDisplay == EDisplay::Block)
    newDisplay = EDisplay::Flex;
  TextDirection contentDirection = computedTextDirection();
  if (originalStyle->direction() == contentDirection &&
      originalDisplay == newDisplay)
    return originalStyle;

  RefPtr<ComputedStyle> style = ComputedStyle::clone(*originalStyle);
  style->setDirection(contentDirection);
  style->setDisplay(newDisplay);
  style->setUnique();
  return style.release();
}

void MultipleFieldsTemporalInputTypeView::createShadowSubtree() {
  DCHECK(element().shadow());

  // Element must not have a layoutObject here, because if it did
  // DateTimeEditElement::customStyleForLayoutObject() is called in
  // appendChild() before the field wrapper element is created.
  // FIXME: This code should not depend on such craziness.
  DCHECK(!element().layoutObject());

  Document& document = element().document();
  ContainerNode* container = element().userAgentShadowRoot();

  container->appendChild(DateTimeEditElement::create(document, *this));
  element().updateView();
  container->appendChild(ClearButtonElement::create(document, *this));
  container->appendChild(SpinButtonElement::create(document, *this));

  if (LayoutTheme::theme().supportsCalendarPicker(
          m_inputType->formControlType()))
    m_pickerIndicatorIsAlwaysVisible = true;
  container->appendChild(PickerIndicatorElement::create(document, *this));
  m_pickerIndicatorIsVisible = true;
  updatePickerIndicatorVisibility();
}

void MultipleFieldsTemporalInputTypeView::destroyShadowSubtree() {
  DCHECK(!m_isDestroyingShadowSubtree);
  m_isDestroyingShadowSubtree = true;
  if (SpinButtonElement* element = spinButtonElement())
    element->removeSpinButtonOwner();
  if (ClearButtonElement* element = clearButtonElement())
    element->removeClearButtonOwner();
  if (DateTimeEditElement* element = dateTimeEditElement())
    element->removeEditControlOwner();
  if (PickerIndicatorElement* element = pickerIndicatorElement())
    element->removePickerIndicatorOwner();

  // If a field element has focus, set focus back to the <input> itself before
  // deleting the field. This prevents unnecessary focusout/blur events.
  if (containsFocusedShadowElement())
    element().focus();

  InputTypeView::destroyShadowSubtree();
  m_isDestroyingShadowSubtree = false;
}

void MultipleFieldsTemporalInputTypeView::handleFocusInEvent(
    Element* oldFocusedElement,
    WebFocusType type) {
  DateTimeEditElement* edit = dateTimeEditElement();
  if (!edit || m_isDestroyingShadowSubtree)
    return;
  if (type == WebFocusTypeBackward) {
    if (element().document().page())
      element().document().page()->focusController().advanceFocus(type);
  } else if (type == WebFocusTypeNone || type == WebFocusTypeMouse ||
             type == WebFocusTypePage) {
    edit->focusByOwner(oldFocusedElement);
  } else {
    edit->focusByOwner();
  }
}

void MultipleFieldsTemporalInputTypeView::forwardEvent(Event* event) {
  if (SpinButtonElement* element = spinButtonElement()) {
    element->forwardEvent(event);
    if (event->defaultHandled())
      return;
  }

  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->defaultEventHandler(event);
}

void MultipleFieldsTemporalInputTypeView::disabledAttributeChanged() {
  EventQueueScope scope;
  spinButtonElement()->releaseCapture();
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->disabledStateChanged();
}

void MultipleFieldsTemporalInputTypeView::requiredAttributeChanged() {
  updateClearButtonVisibility();
}

void MultipleFieldsTemporalInputTypeView::handleKeydownEvent(
    KeyboardEvent* event) {
  if (!element().isFocused())
    return;
  if (m_pickerIndicatorIsVisible &&
      ((event->key() == "ArrowDown" && event->getModifierState("Alt")) ||
       (LayoutTheme::theme().shouldOpenPickerWithF4Key() &&
        event->key() == "F4"))) {
    if (PickerIndicatorElement* element = pickerIndicatorElement())
      element->openPopup();
    event->setDefaultHandled();
  } else {
    forwardEvent(event);
  }
}

bool MultipleFieldsTemporalInputTypeView::hasBadInput() const {
  DateTimeEditElement* edit = dateTimeEditElement();
  return element().value().isEmpty() && edit &&
         edit->anyEditableFieldsHaveValues();
}

AtomicString MultipleFieldsTemporalInputTypeView::localeIdentifier() const {
  return element().computeInheritedLanguage();
}

void MultipleFieldsTemporalInputTypeView::
    editControlDidChangeValueByKeyboard() {
  element().dispatchFormControlChangeEvent();
}

void MultipleFieldsTemporalInputTypeView::minOrMaxAttributeChanged() {
  updateView();
}

void MultipleFieldsTemporalInputTypeView::readonlyAttributeChanged() {
  EventQueueScope scope;
  spinButtonElement()->releaseCapture();
  if (DateTimeEditElement* edit = dateTimeEditElement())
    edit->readOnlyStateChanged();
}

void MultipleFieldsTemporalInputTypeView::restoreFormControlState(
    const FormControlState& state) {
  DateTimeEditElement* edit = dateTimeEditElement();
  if (!edit)
    return;
  DateTimeFieldsState dateTimeFieldsState =
      DateTimeFieldsState::restoreFormControlState(state);
  edit->setValueAsDateTimeFieldsState(dateTimeFieldsState);
  element().setNonAttributeValue(m_inputType->sanitizeValue(edit->value()));
  updateClearButtonVisibility();
}

FormControlState MultipleFieldsTemporalInputTypeView::saveFormControlState()
    const {
  if (DateTimeEditElement* edit = dateTimeEditElement())
    return edit->valueAsDateTimeFieldsState().saveFormControlState();
  return FormControlState();
}

void MultipleFieldsTemporalInputTypeView::didSetValue(
    const String& sanitizedValue,
    bool valueChanged) {
  DateTimeEditElement* edit = dateTimeEditElement();
  if (valueChanged || (sanitizedValue.isEmpty() && edit &&
                       edit->anyEditableFieldsHaveValues())) {
    element().updateView();
    element().setNeedsValidityCheck();
  }
}

void MultipleFieldsTemporalInputTypeView::stepAttributeChanged() {
  updateView();
}

void MultipleFieldsTemporalInputTypeView::updateView() {
  DateTimeEditElement* edit = dateTimeEditElement();
  if (!edit)
    return;

  DateTimeEditElement::LayoutParameters layoutParameters(
      element().locale(), m_inputType->createStepRange(AnyIsDefaultStep));

  DateComponents date;
  bool hasValue = false;
  if (!element().suggestedValue().isNull())
    hasValue =
        m_inputType->parseToDateComponents(element().suggestedValue(), &date);
  else
    hasValue = m_inputType->parseToDateComponents(element().value(), &date);
  if (!hasValue)
    m_inputType->setMillisecondToDateComponents(
        layoutParameters.stepRange.minimum().toDouble(), &date);

  m_inputType->setupLayoutParameters(layoutParameters, date);

  DEFINE_STATIC_LOCAL(AtomicString, datetimeformatAttr, ("datetimeformat"));
  edit->setAttribute(datetimeformatAttr,
                     AtomicString(layoutParameters.dateTimeFormat),
                     ASSERT_NO_EXCEPTION);
  const AtomicString pattern = edit->fastGetAttribute(HTMLNames::patternAttr);
  if (!pattern.isEmpty())
    layoutParameters.dateTimeFormat = pattern;

  if (!DateTimeFormatValidator().validateFormat(layoutParameters.dateTimeFormat,
                                                *m_inputType))
    layoutParameters.dateTimeFormat = layoutParameters.fallbackDateTimeFormat;

  if (hasValue)
    edit->setValueAsDate(layoutParameters, date);
  else
    edit->setEmptyValue(layoutParameters, date);
  updateClearButtonVisibility();
}

void MultipleFieldsTemporalInputTypeView::closePopupView() {
  if (PickerIndicatorElement* picker = pickerIndicatorElement())
    picker->closePopup();
}

void MultipleFieldsTemporalInputTypeView::valueAttributeChanged() {
  if (!element().hasDirtyValue())
    updateView();
}

void MultipleFieldsTemporalInputTypeView::listAttributeTargetChanged() {
  updatePickerIndicatorVisibility();
}

void MultipleFieldsTemporalInputTypeView::updatePickerIndicatorVisibility() {
  if (m_pickerIndicatorIsAlwaysVisible) {
    showPickerIndicator();
    return;
  }
  if (element().hasValidDataListOptions())
    showPickerIndicator();
  else
    hidePickerIndicator();
}

void MultipleFieldsTemporalInputTypeView::hidePickerIndicator() {
  if (!m_pickerIndicatorIsVisible)
    return;
  m_pickerIndicatorIsVisible = false;
  DCHECK(pickerIndicatorElement());
  pickerIndicatorElement()->setInlineStyleProperty(CSSPropertyDisplay,
                                                   CSSValueNone);
}

void MultipleFieldsTemporalInputTypeView::showPickerIndicator() {
  if (m_pickerIndicatorIsVisible)
    return;
  m_pickerIndicatorIsVisible = true;
  DCHECK(pickerIndicatorElement());
  pickerIndicatorElement()->removeInlineStyleProperty(CSSPropertyDisplay);
}

void MultipleFieldsTemporalInputTypeView::focusAndSelectClearButtonOwner() {
  element().focus();
}

bool MultipleFieldsTemporalInputTypeView::
    shouldClearButtonRespondToMouseEvents() {
  return !element().isDisabledOrReadOnly() && !element().isRequired();
}

void MultipleFieldsTemporalInputTypeView::clearValue() {
  element().setValue("", DispatchInputAndChangeEvent);
  element().updateClearButtonVisibility();
}

void MultipleFieldsTemporalInputTypeView::updateClearButtonVisibility() {
  ClearButtonElement* clearButton = clearButtonElement();
  if (!clearButton)
    return;

  if (element().isRequired() ||
      !dateTimeEditElement()->anyEditableFieldsHaveValues()) {
    clearButton->setInlineStyleProperty(CSSPropertyOpacity, 0.0,
                                        CSSPrimitiveValue::UnitType::Number);
    clearButton->setInlineStyleProperty(CSSPropertyPointerEvents, CSSValueNone);
  } else {
    clearButton->removeInlineStyleProperty(CSSPropertyOpacity);
    clearButton->removeInlineStyleProperty(CSSPropertyPointerEvents);
  }
}

TextDirection MultipleFieldsTemporalInputTypeView::computedTextDirection() {
  return element().locale().isRTL() ? RTL : LTR;
}

AXObject* MultipleFieldsTemporalInputTypeView::popupRootAXObject() {
  if (PickerIndicatorElement* picker = pickerIndicatorElement())
    return picker->popupRootAXObject();
  return nullptr;
}

}  // namespace blink
