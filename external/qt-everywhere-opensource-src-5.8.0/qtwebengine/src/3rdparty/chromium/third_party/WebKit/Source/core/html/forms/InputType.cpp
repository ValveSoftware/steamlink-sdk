/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "core/html/forms/InputType.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/InputTypeNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/fileapi/FileList.h"
#include "core/frame/FrameHost.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/forms/ButtonInputType.h"
#include "core/html/forms/CheckboxInputType.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/ColorInputType.h"
#include "core/html/forms/DateInputType.h"
#include "core/html/forms/DateTimeLocalInputType.h"
#include "core/html/forms/EmailInputType.h"
#include "core/html/forms/FileInputType.h"
#include "core/html/forms/HiddenInputType.h"
#include "core/html/forms/ImageInputType.h"
#include "core/html/forms/MonthInputType.h"
#include "core/html/forms/NumberInputType.h"
#include "core/html/forms/PasswordInputType.h"
#include "core/html/forms/RadioInputType.h"
#include "core/html/forms/RangeInputType.h"
#include "core/html/forms/ResetInputType.h"
#include "core/html/forms/SearchInputType.h"
#include "core/html/forms/SubmitInputType.h"
#include "core/html/forms/TelephoneInputType.h"
#include "core/html/forms/TextInputType.h"
#include "core/html/forms/TimeInputType.h"
#include "core/html/forms/URLInputType.h"
#include "core/html/forms/WeekInputType.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutTheme.h"
#include "platform/JSONValues.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

using InputTypeFactoryFunction = InputType* (*)(HTMLInputElement&);
using InputTypeFactoryMap = HashMap<AtomicString, InputTypeFactoryFunction, CaseFoldingHash>;

static std::unique_ptr<InputTypeFactoryMap> createInputTypeFactoryMap()
{
    std::unique_ptr<InputTypeFactoryMap> map = wrapUnique(new InputTypeFactoryMap);
    map->add(InputTypeNames::button, ButtonInputType::create);
    map->add(InputTypeNames::checkbox, CheckboxInputType::create);
    map->add(InputTypeNames::color, ColorInputType::create);
    map->add(InputTypeNames::date, DateInputType::create);
    map->add(InputTypeNames::datetime_local, DateTimeLocalInputType::create);
    map->add(InputTypeNames::email, EmailInputType::create);
    map->add(InputTypeNames::file, FileInputType::create);
    map->add(InputTypeNames::hidden, HiddenInputType::create);
    map->add(InputTypeNames::image, ImageInputType::create);
    map->add(InputTypeNames::month, MonthInputType::create);
    map->add(InputTypeNames::number, NumberInputType::create);
    map->add(InputTypeNames::password, PasswordInputType::create);
    map->add(InputTypeNames::radio, RadioInputType::create);
    map->add(InputTypeNames::range, RangeInputType::create);
    map->add(InputTypeNames::reset, ResetInputType::create);
    map->add(InputTypeNames::search, SearchInputType::create);
    map->add(InputTypeNames::submit, SubmitInputType::create);
    map->add(InputTypeNames::tel, TelephoneInputType::create);
    map->add(InputTypeNames::time, TimeInputType::create);
    map->add(InputTypeNames::url, URLInputType::create);
    map->add(InputTypeNames::week, WeekInputType::create);
    // No need to register "text" because it is the default type.
    return map;
}

static const InputTypeFactoryMap* factoryMap()
{
    static const InputTypeFactoryMap* factoryMap = createInputTypeFactoryMap().release();
    return factoryMap;
}

InputType* InputType::create(HTMLInputElement& element, const AtomicString& typeName)
{
    InputTypeFactoryFunction factory = typeName.isEmpty() ? 0 : factoryMap()->get(typeName);
    if (!factory)
        factory = TextInputType::create;
    return factory(element);
}

InputType* InputType::createText(HTMLInputElement& element)
{
    return TextInputType::create(element);
}

const AtomicString& InputType::normalizeTypeName(const AtomicString& typeName)
{
    if (typeName.isEmpty())
        return InputTypeNames::text;
    InputTypeFactoryMap::const_iterator it = factoryMap()->find(typeName);
    return it == factoryMap()->end() ? InputTypeNames::text : it->key;
}

InputType::~InputType()
{
}

DEFINE_TRACE(InputType)
{
    visitor->trace(m_element);
}

bool InputType::isTextField() const
{
    return false;
}

bool InputType::shouldSaveAndRestoreFormControlState() const
{
    return true;
}

bool InputType::isFormDataAppendable() const
{
    // There is no form data unless there's a name for non-image types.
    return !element().name().isEmpty();
}

void InputType::appendToFormData(FormData& formData) const
{
    formData.append(element().name(), element().value());
}

String InputType::resultForDialogSubmit() const
{
    return element().fastGetAttribute(valueAttr);
}

double InputType::valueAsDate() const
{
    return DateComponents::invalidMilliseconds();
}

void InputType::setValueAsDate(double, ExceptionState& exceptionState) const
{
    exceptionState.throwDOMException(InvalidStateError, "This input element does not support Date values.");
}

double InputType::valueAsDouble() const
{
    return std::numeric_limits<double>::quiet_NaN();
}

void InputType::setValueAsDouble(double doubleValue, TextFieldEventBehavior eventBehavior, ExceptionState& exceptionState) const
{
    exceptionState.throwDOMException(InvalidStateError, "This input element does not support Number values.");
}

void InputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior, ExceptionState&) const
{
    element().setValue(serialize(newValue), eventBehavior);
}

void InputType::readingChecked() const
{
}

bool InputType::supportsValidation() const
{
    return true;
}

bool InputType::typeMismatchFor(const String&) const
{
    return false;
}

bool InputType::typeMismatch() const
{
    return false;
}

bool InputType::supportsRequired() const
{
    // Almost all validatable types support @required.
    return supportsValidation();
}

bool InputType::valueMissing(const String&) const
{
    return false;
}

bool InputType::tooLong(const String&, HTMLTextFormControlElement::NeedsToCheckDirtyFlag) const
{
    return false;
}

bool InputType::tooShort(const String&, HTMLTextFormControlElement::NeedsToCheckDirtyFlag) const
{
    return false;
}

bool InputType::patternMismatch(const String&) const
{
    return false;
}

bool InputType::rangeUnderflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue < createStepRange(RejectAny).minimum();
}

bool InputType::rangeOverflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue > createStepRange(RejectAny).maximum();
}

Decimal InputType::defaultValueForStepUp() const
{
    return 0;
}

double InputType::minimum() const
{
    return createStepRange(RejectAny).minimum().toDouble();
}

double InputType::maximum() const
{
    return createStepRange(RejectAny).maximum().toDouble();
}

bool InputType::isInRange(const String& value) const
{
    if (!isSteppable())
        return false;

    // This function should return true if both of validity.rangeUnderflow and
    // validity.rangeOverflow are false.
    // If the INPUT has no value, they are false.
    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return true;

    StepRange stepRange(createStepRange(RejectAny));
    return stepRange.hasRangeLimitations() && numericValue >= stepRange.minimum() && numericValue <= stepRange.maximum();
}

bool InputType::isOutOfRange(const String& value) const
{
    if (!isSteppable())
        return false;

    // This function should return true if either validity.rangeUnderflow or
    // validity.rangeOverflow are true.
    // If the INPUT has no value, they are false.
    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    StepRange stepRange(createStepRange(RejectAny));
    return stepRange.hasRangeLimitations() && (numericValue < stepRange.minimum() || numericValue > stepRange.maximum());
}

bool InputType::stepMismatch(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return createStepRange(RejectAny).stepMismatch(numericValue);
}

String InputType::badInputText() const
{
    NOTREACHED();
    return locale().queryString(WebLocalizedString::ValidationTypeMismatch);
}

String InputType::rangeOverflowText(const Decimal&) const
{
    NOTREACHED();
    return String();
}

String InputType::rangeUnderflowText(const Decimal&) const
{
    NOTREACHED();
    return String();
}

String InputType::typeMismatchText() const
{
    return locale().queryString(WebLocalizedString::ValidationTypeMismatch);
}

String InputType::valueMissingText() const
{
    return locale().queryString(WebLocalizedString::ValidationValueMissing);
}

std::pair<String, String> InputType::validationMessage(const InputTypeView& inputTypeView) const
{
    const String value = element().value();

    // The order of the following checks is meaningful. e.g. We'd like to show the
    // badInput message even if the control has other validation errors.
    if (inputTypeView.hasBadInput())
        return std::make_pair(badInputText(), emptyString());

    if (valueMissing(value))
        return std::make_pair(valueMissingText(), emptyString());

    if (typeMismatch())
        return std::make_pair(typeMismatchText(), emptyString());

    if (patternMismatch(value)) {
        // https://html.spec.whatwg.org/multipage/forms.html#attr-input-pattern
        //   When an input element has a pattern attribute specified, authors
        //   should include a title attribute to give a description of the
        //   pattern. User agents may use the contents of this attribute, if it
        //   is present, when informing the user that the pattern is not matched
        return std::make_pair(locale().queryString(WebLocalizedString::ValidationPatternMismatch), element().fastGetAttribute(titleAttr).getString());
    }

    if (element().tooLong())
        return std::make_pair(locale().validationMessageTooLongText(value.length(), element().maxLength()), emptyString());

    if (element().tooShort())
        return std::make_pair(locale().validationMessageTooShortText(value.length(), element().minLength()), emptyString());

    if (!isSteppable())
        return std::make_pair(emptyString(), emptyString());

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return std::make_pair(emptyString(), emptyString());

    StepRange stepRange(createStepRange(RejectAny));

    if (numericValue < stepRange.minimum())
        return std::make_pair(rangeUnderflowText(stepRange.minimum()), emptyString());

    if (numericValue > stepRange.maximum())
        return std::make_pair(rangeOverflowText(stepRange.maximum()), emptyString());

    if (stepRange.stepMismatch(numericValue)) {
        DCHECK(stepRange.hasStep());
        Decimal candidate1 = stepRange.clampValue(numericValue);
        String localizedCandidate1 = localizeValue(serialize(candidate1));
        Decimal candidate2 = candidate1 < numericValue ? candidate1 + stepRange.step() : candidate1 - stepRange.step();
        if (!candidate2.isFinite() || candidate2 < stepRange.minimum() || candidate2 > stepRange.maximum())
            return std::make_pair(locale().queryString(WebLocalizedString::ValidationStepMismatchCloseToLimit, localizedCandidate1), emptyString());
        String localizedCandidate2 = localizeValue(serialize(candidate2));
        if (candidate1 < candidate2)
            return std::make_pair(locale().queryString(WebLocalizedString::ValidationStepMismatch, localizedCandidate1, localizedCandidate2), emptyString());
        return std::make_pair(locale().queryString(WebLocalizedString::ValidationStepMismatch, localizedCandidate2, localizedCandidate1), emptyString());
    }

    return std::make_pair(emptyString(), emptyString());
}

Decimal InputType::parseToNumber(const String&, const Decimal& defaultValue) const
{
    NOTREACHED();
    return defaultValue;
}

Decimal InputType::parseToNumberOrNaN(const String& string) const
{
    return parseToNumber(string, Decimal::nan());
}

String InputType::serialize(const Decimal&) const
{
    NOTREACHED();
    return String();
}

ChromeClient* InputType::chromeClient() const
{
    if (FrameHost* host = element().document().frameHost())
        return &host->chromeClient();
    return nullptr;
}

Locale& InputType::locale() const
{
    return element().locale();
}

bool InputType::canSetStringValue() const
{
    return true;
}

bool InputType::isKeyboardFocusable() const
{
    return element().isFocusable();
}

bool InputType::shouldShowFocusRingOnMouseFocus() const
{
    return false;
}

void InputType::enableSecureTextInput()
{
}

void InputType::disableSecureTextInput()
{
}

void InputType::countUsage()
{
}

bool InputType::shouldRespectAlignAttribute()
{
    return false;
}

void InputType::sanitizeValueInResponseToMinOrMaxAttributeChange()
{
}

bool InputType::canBeSuccessfulSubmitButton()
{
    return false;
}

bool InputType::matchesDefaultPseudoClass()
{
    return false;
}

bool InputType::layoutObjectIsNeeded()
{
    return true;
}

FileList* InputType::files()
{
    return nullptr;
}

void InputType::setFiles(FileList*)
{
}

bool InputType::getTypeSpecificValue(String&)
{
    return false;
}

String InputType::fallbackValue() const
{
    return String();
}

String InputType::defaultValue() const
{
    return String();
}

bool InputType::canSetSuggestedValue()
{
    return false;
}

bool InputType::shouldSendChangeEventAfterCheckedChanged()
{
    return true;
}

bool InputType::storesValueSeparateFromAttribute()
{
    return true;
}

bool InputType::shouldDispatchFormControlChangeEvent(String& oldValue, String& newValue)
{
    return !equalIgnoringNullity(oldValue, newValue);
}

void InputType::dispatchSearchEvent()
{
}

void InputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    element().setValueInternal(sanitizedValue, eventBehavior);
    if (!valueChanged)
        return;
    switch (eventBehavior) {
    case DispatchChangeEvent:
        element().dispatchFormControlChangeEvent();
        break;
    case DispatchInputAndChangeEvent:
        element().dispatchFormControlInputEvent();
        element().dispatchFormControlChangeEvent();
        break;
    case DispatchNoEvent:
        break;
    }
}

bool InputType::canSetValue(const String&)
{
    return true;
}

String InputType::localizeValue(const String& proposedValue) const
{
    return proposedValue;
}

String InputType::visibleValue() const
{
    return element().value();
}

String InputType::sanitizeValue(const String& proposedValue) const
{
    return proposedValue;
}

String InputType::sanitizeUserInputValue(const String& proposedValue) const
{
    return sanitizeValue(proposedValue);
}

void InputType::warnIfValueIsInvalidAndElementIsVisible(const String& value) const
{
    // Don't warn if the value is set in Modernizr.
    const ComputedStyle* style = element().computedStyle();
    if (style && style->visibility() != HIDDEN)
        warnIfValueIsInvalid(value);
}

void InputType::warnIfValueIsInvalid(const String&) const
{
}

bool InputType::receiveDroppedFiles(const DragData*)
{
    NOTREACHED();
    return false;
}

String InputType::droppedFileSystemId()
{
    NOTREACHED();
    return String();
}

bool InputType::shouldRespectListAttribute()
{
    return false;
}

bool InputType::isTextButton() const
{
    return false;
}

bool InputType::isInteractiveContent() const
{
    return true;
}

bool InputType::isEnumeratable()
{
    return true;
}

bool InputType::isCheckable()
{
    return false;
}

bool InputType::isSteppable() const
{
    return false;
}

bool InputType::shouldRespectHeightAndWidthAttributes()
{
    return false;
}

int InputType::maxLength() const
{
    return HTMLInputElement::maximumLength;
}

int InputType::minLength() const
{
    return 0;
}

bool InputType::supportsPlaceholder() const
{
    return false;
}

bool InputType::supportsReadOnly() const
{
    return false;
}

String InputType::defaultToolTip(const InputTypeView& inputTypeView) const
{
    if (element().form() && element().form()->noValidate())
        return String();
    return validationMessage(inputTypeView).first;
}

Decimal InputType::findClosestTickMarkValue(const Decimal&)
{
    NOTREACHED();
    return Decimal::nan();
}

bool InputType::hasLegalLinkAttribute(const QualifiedName&) const
{
    return false;
}

const QualifiedName& InputType::subResourceAttributeName() const
{
    return QualifiedName::null();
}

bool InputType::supportsAutocapitalize() const
{
    return false;
}

const AtomicString& InputType::defaultAutocapitalize() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
    return none;
}

bool InputType::shouldAppearIndeterminate() const
{
    return false;
}

bool InputType::supportsInputModeAttribute() const
{
    return false;
}

bool InputType::supportsSelectionAPI() const
{
    return false;
}

unsigned InputType::height() const
{
    return 0;
}

unsigned InputType::width() const
{
    return 0;
}

ColorChooserClient* InputType::colorChooserClient()
{
    return nullptr;
}

void InputType::applyStep(const Decimal& current, int count, AnyStepHandling anyStepHandling, TextFieldEventBehavior eventBehavior, ExceptionState& exceptionState)
{
    // https://html.spec.whatwg.org/multipage/forms.html#dom-input-stepup

    StepRange stepRange(createStepRange(anyStepHandling));
    // 2. If the element has no allowed value step, then throw an
    // InvalidStateError exception, and abort these steps.
    if (!stepRange.hasStep()) {
        exceptionState.throwDOMException(InvalidStateError, "This form element does not have an allowed value step.");
        return;
    }

    // 3. If the element has a minimum and a maximum and the minimum is greater
    // than the maximum, then abort these steps.
    if (stepRange.minimum() > stepRange.maximum())
        return;

    // 4. If the element has a minimum and a maximum and there is no value
    // greater than or equal to the element's minimum and less than or equal to
    // the element's maximum that, when subtracted from the step base, is an
    // integral multiple of the allowed value step, then abort these steps.
    Decimal alignedMaximum = stepRange.stepSnappedMaximum();
    if (!alignedMaximum.isFinite())
        return;

    Decimal base = stepRange.stepBase();
    Decimal step = stepRange.step();
    EventQueueScope scope;
    Decimal newValue = current;
    const AtomicString& stepString = element().fastGetAttribute(stepAttr);
    if (!equalIgnoringCase(stepString, "any") && stepRange.stepMismatch(current)) {
        // Snap-to-step / clamping steps
        // If the current value is not matched to step value:
        // - The value should be the larger matched value nearest to 0 if count > 0
        //   e.g. <input type=number value=3 min=-100 step=3> -> 5
        // - The value should be the smaller matched value nearest to 0 if count < 0
        //   e.g. <input type=number value=3 min=-100 step=3> -> 2
        //

        DCHECK(!step.isZero());
        if (count < 0) {
            newValue = base + ((newValue - base) / step).floor() * step;
            ++count;
        } else if (count > 0) {
            newValue = base + ((newValue - base) / step).ceil() * step;
            --count;
        }
    }
    newValue = newValue + stepRange.step() * count;

    if (!equalIgnoringCase(stepString, "any"))
        newValue = stepRange.alignValueForStep(current, newValue);

    // 7. If the element has a minimum, and value is less than that minimum,
    // then set value to the smallest value that, when subtracted from the step
    // base, is an integral multiple of the allowed value step, and that is more
    // than or equal to minimum.
    // 8. If the element has a maximum, and value is greater than that maximum,
    // then set value to the largest value that, when subtracted from the step
    // base, is an integral multiple of the allowed value step, and that is less
    // than or equal to maximum.
    if (newValue > stepRange.maximum()) {
        newValue = alignedMaximum;
    } else if (newValue < stepRange.minimum()) {
        const Decimal alignedMinimum = base + ((stepRange.minimum() - base) / step).ceil() * step;
        DCHECK_GE(alignedMinimum, stepRange.minimum());
        newValue = alignedMinimum;
    }

    // 9. Let value as string be the result of running the algorithm to convert
    // a number to a string, as defined for the input element's type attribute's
    // current state, on value.
    // 10. Set the value of the element to value as string.
    setValueAsDecimal(newValue, eventBehavior, exceptionState);

    if (AXObjectCache* cache = element().document().existingAXObjectCache())
        cache->handleValueChanged(&element());
}

bool InputType::getAllowedValueStep(Decimal* step) const
{
    StepRange stepRange(createStepRange(RejectAny));
    *step = stepRange.step();
    return stepRange.hasStep();
}

StepRange InputType::createStepRange(AnyStepHandling) const
{
    NOTREACHED();
    return StepRange();
}

void InputType::stepUp(int n, ExceptionState& exceptionState)
{
    if (!isSteppable()) {
        exceptionState.throwDOMException(InvalidStateError, "This form element is not steppable.");
        return;
    }
    const Decimal current = parseToNumber(element().value(), 0);
    applyStep(current, n, RejectAny, DispatchNoEvent, exceptionState);
}

void InputType::stepUpFromLayoutObject(int n)
{
    // The only difference from stepUp()/stepDown() is the extra treatment
    // of the current value before applying the step:
    //
    // If the current value is not a number, including empty, the current value is assumed as 0.
    //   * If 0 is in-range, and matches to step value
    //     - The value should be the +step if n > 0
    //     - The value should be the -step if n < 0
    //     If -step or +step is out of range, new value should be 0.
    //   * If 0 is smaller than the minimum value
    //     - The value should be the minimum value for any n
    //   * If 0 is larger than the maximum value
    //     - The value should be the maximum value for any n
    //   * If 0 is in-range, but not matched to step value
    //     - The value should be the larger matched value nearest to 0 if n > 0
    //       e.g. <input type=number min=-100 step=3> -> 2
    //     - The value should be the smaler matched value nearest to 0 if n < 0
    //       e.g. <input type=number min=-100 step=3> -> -1
    //   As for date/datetime-local/month/time/week types, the current value is assumed as "the current local date/time".
    //   As for datetime type, the current value is assumed as "the current date/time in UTC".
    // If the current value is smaller than the minimum value:
    //  - The value should be the minimum value if n > 0
    //  - Nothing should happen if n < 0
    // If the current value is larger than the maximum value:
    //  - The value should be the maximum value if n < 0
    //  - Nothing should happen if n > 0
    //
    // n is assumed as -n if step < 0.

    DCHECK(isSteppable());
    if (!isSteppable())
        return;
    DCHECK(n);
    if (!n)
        return;

    StepRange stepRange(createStepRange(AnyIsDefaultStep));

    // FIXME: Not any changes after stepping, even if it is an invalid value, may be better.
    // (e.g. Stepping-up for <input type="number" value="foo" step="any" /> => "foo")
    if (!stepRange.hasStep())
        return;

    EventQueueScope scope;
    const Decimal step = stepRange.step();

    int sign;
    if (step > 0)
        sign = n;
    else if (step < 0)
        sign = -n;
    else
        sign = 0;

    Decimal current = parseToNumberOrNaN(element().value());
    if (!current.isFinite()) {
        current = defaultValueForStepUp();
        const Decimal nextDiff = step * n;
        if (current < stepRange.minimum() - nextDiff)
            current = stepRange.minimum() - nextDiff;
        if (current > stepRange.maximum() - nextDiff)
            current = stepRange.maximum() - nextDiff;
        setValueAsDecimal(current, DispatchNoEvent, IGNORE_EXCEPTION);
    }
    if ((sign > 0 && current < stepRange.minimum()) || (sign < 0 && current > stepRange.maximum())) {
        setValueAsDecimal(sign > 0 ? stepRange.minimum() : stepRange.maximum(), DispatchChangeEvent, IGNORE_EXCEPTION);
        return;
    }
    if ((sign > 0 && current >= stepRange.maximum()) || (sign < 0 && current <= stepRange.minimum()))
        return;
    applyStep(current, n, AnyIsDefaultStep, DispatchChangeEvent, IGNORE_EXCEPTION);
}

void InputType::countUsageIfVisible(UseCounter::Feature feature) const
{
    if (const ComputedStyle* style = element().computedStyle()) {
        if (style->visibility() != HIDDEN)
            UseCounter::count(element().document(), feature);
    }
}

Decimal InputType::findStepBase(const Decimal& defaultValue) const
{
    Decimal stepBase = parseToNumber(element().fastGetAttribute(minAttr), Decimal::nan());
    if (!stepBase.isFinite())
        stepBase = parseToNumber(element().fastGetAttribute(valueAttr), defaultValue);
    return stepBase;
}

StepRange InputType::createStepRange(AnyStepHandling anyStepHandling, const Decimal& stepBaseDefault, const Decimal& minimumDefault, const Decimal& maximumDefault, const StepRange::StepDescription& stepDescription) const
{
    bool hasRangeLimitations = false;
    const Decimal stepBase = findStepBase(stepBaseDefault);
    Decimal minimum = parseToNumberOrNaN(element().fastGetAttribute(minAttr));
    if (minimum.isFinite())
        hasRangeLimitations = true;
    else
        minimum = minimumDefault;
    Decimal maximum = parseToNumberOrNaN(element().fastGetAttribute(maxAttr));
    if (maximum.isFinite())
        hasRangeLimitations = true;
    else
        maximum = maximumDefault;
    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element().fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, hasRangeLimitations, step, stepDescription);
}

void InputType::addWarningToConsole(const char* messageFormat, const String& value) const
{
    element().document().addConsoleMessage(ConsoleMessage::create(RenderingMessageSource, WarningMessageLevel,
        String::format(messageFormat, JSONValue::quoteString(value).utf8().data())));
}

} // namespace blink
