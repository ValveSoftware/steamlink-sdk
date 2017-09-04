/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef InputType_h
#define InputType_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/html/TextControlElement.h"
#include "core/html/forms/ColorChooserClient.h"
#include "core/html/forms/StepRange.h"

namespace blink {

class ChromeClient;
class DragData;
class ExceptionState;
class FileList;
class FormData;
class InputTypeView;

// An InputType object represents the type-specific part of an HTMLInputElement.
// Do not expose instances of InputType and classes derived from it to classes
// other than HTMLInputElement.
// FIXME: InputType should not inherit InputTypeView. It's conceptually wrong.
class CORE_EXPORT InputType : public GarbageCollectedFinalized<InputType> {
  WTF_MAKE_NONCOPYABLE(InputType);

 public:
  static InputType* create(HTMLInputElement&, const AtomicString&);
  static InputType* createText(HTMLInputElement&);
  static const AtomicString& normalizeTypeName(const AtomicString&);
  virtual ~InputType();
  DECLARE_VIRTUAL_TRACE();

  virtual InputTypeView* createView() = 0;
  virtual const AtomicString& formControlType() const = 0;

  // Type query functions

  // Any time we are using one of these functions it's best to refactor
  // to add a virtual function to allow the input type object to do the
  // work instead, or at least make a query function that asks a higher
  // level question. These functions make the HTMLInputElement class
  // inflexible because it's harder to add new input types if there is
  // scattered code with special cases for various types.

  virtual bool isInteractiveContent() const;
  virtual bool isTextButton() const;
  virtual bool isTextField() const;

  // Form value functions

  virtual bool shouldSaveAndRestoreFormControlState() const;
  virtual bool isFormDataAppendable() const;
  virtual void appendToFormData(FormData&) const;
  virtual String resultForDialogSubmit() const;

  // DOM property functions

  // Returns a string value in ValueMode::kFilename.
  virtual String valueInFilenameValueMode() const;
  // Default string to be used for showing button and form submission if |value|
  // is missing.
  virtual String defaultLabel() const;

  // https://html.spec.whatwg.org/multipage/forms.html#dom-input-value
  enum class ValueMode { kValue, kDefault, kDefaultOn, kFilename };
  virtual ValueMode valueMode() const = 0;

  virtual double valueAsDate() const;
  virtual void setValueAsDate(double, ExceptionState&) const;
  virtual double valueAsDouble() const;
  virtual void setValueAsDouble(double,
                                TextFieldEventBehavior,
                                ExceptionState&) const;
  virtual void setValueAsDecimal(const Decimal&,
                                 TextFieldEventBehavior,
                                 ExceptionState&) const;
  virtual void readingChecked() const;

  // Validation functions

  // Returns a validation message as .first, and title attribute value as
  // .second if patternMismatch.
  std::pair<String, String> validationMessage(const InputTypeView&) const;
  virtual bool supportsValidation() const;
  virtual bool typeMismatchFor(const String&) const;
  // Type check for the current input value. We do nothing for some types
  // though typeMismatchFor() does something for them because of value
  // sanitization.
  virtual bool typeMismatch() const;
  virtual bool supportsRequired() const;
  virtual bool valueMissing(const String&) const;
  virtual bool patternMismatch(const String&) const;
  virtual bool tooLong(const String&,
                       TextControlElement::NeedsToCheckDirtyFlag) const;
  virtual bool tooShort(const String&,
                        TextControlElement::NeedsToCheckDirtyFlag) const;
  bool rangeUnderflow(const String&) const;
  bool rangeOverflow(const String&) const;
  bool isInRange(const String&) const;
  bool isOutOfRange(const String&) const;
  virtual Decimal defaultValueForStepUp() const;
  double minimum() const;
  double maximum() const;
  bool stepMismatch(const String&) const;
  virtual bool getAllowedValueStep(Decimal*) const;
  virtual StepRange createStepRange(AnyStepHandling) const;
  virtual void stepUp(double, ExceptionState&);
  virtual void stepUpFromLayoutObject(int);
  virtual String badInputText() const;
  virtual String rangeOverflowText(const Decimal& maximum) const;
  virtual String rangeUnderflowText(const Decimal& minimum) const;
  virtual String typeMismatchText() const;
  virtual String valueMissingText() const;
  virtual bool canSetStringValue() const;
  virtual String localizeValue(const String&) const;
  virtual String visibleValue() const;
  // Returing the null string means "use the default value."
  // This function must be called only by HTMLInputElement::sanitizeValue().
  virtual String sanitizeValue(const String&) const;
  virtual String sanitizeUserInputValue(const String&) const;
  virtual void warnIfValueIsInvalid(const String&) const;
  void warnIfValueIsInvalidAndElementIsVisible(const String&) const;

  virtual bool isKeyboardFocusable() const;
  virtual bool shouldShowFocusRingOnMouseFocus() const;
  virtual void enableSecureTextInput();
  virtual void disableSecureTextInput();
  virtual bool canBeSuccessfulSubmitButton();
  virtual bool matchesDefaultPseudoClass();

  // Miscellaneous functions

  virtual bool layoutObjectIsNeeded();
  virtual void countUsage();
  virtual void sanitizeValueInResponseToMinOrMaxAttributeChange();
  virtual bool shouldRespectAlignAttribute();
  virtual FileList* files();
  virtual void setFiles(FileList*);
  virtual void setFilesFromPaths(const Vector<String>&);
  // Should return true if the given DragData has more than one dropped files.
  virtual bool receiveDroppedFiles(const DragData*);
  virtual String droppedFileSystemId();
  // Should return true if the corresponding layoutObject for a type can display
  // a suggested value.
  virtual bool canSetSuggestedValue();
  virtual bool shouldSendChangeEventAfterCheckedChanged();
  virtual bool canSetValue(const String&);
  virtual void setValue(const String&,
                        bool valueChanged,
                        TextFieldEventBehavior);
  virtual bool shouldRespectListAttribute();
  virtual bool isEnumeratable();
  virtual bool isCheckable();
  virtual bool isSteppable() const;
  virtual bool shouldRespectHeightAndWidthAttributes();
  virtual int maxLength() const;
  virtual int minLength() const;
  virtual bool supportsPlaceholder() const;
  virtual bool supportsReadOnly() const;
  virtual String defaultToolTip(const InputTypeView&) const;
  virtual Decimal findClosestTickMarkValue(const Decimal&);
  virtual bool hasLegalLinkAttribute(const QualifiedName&) const;
  virtual const QualifiedName& subResourceAttributeName() const;
  virtual bool supportsAutocapitalize() const;
  virtual const AtomicString& defaultAutocapitalize() const;
  virtual void copyNonAttributeProperties(const HTMLInputElement&);
  virtual void onAttachWithLayoutObject();
  virtual void onDetachWithLayoutObject();

  // Parses the specified string for the type, and return
  // the Decimal value for the parsing result if the parsing
  // succeeds; Returns defaultValue otherwise. This function can
  // return NaN or Infinity only if defaultValue is NaN or Infinity.
  virtual Decimal parseToNumber(const String&,
                                const Decimal& defaultValue) const;

  // Create a string representation of the specified Decimal value for the
  // input type. If NaN or Infinity is specified, this returns an empty
  // string. This should not be called for types without valueAsNumber.
  virtual String serialize(const Decimal&) const;

  virtual bool shouldAppearIndeterminate() const;

  virtual bool supportsInputModeAttribute() const;

  virtual bool supportsSelectionAPI() const;

  // Gets width and height of the input element if the type of the
  // element is image. It returns 0 if the element is not image type.
  virtual unsigned height() const;
  virtual unsigned width() const;

  virtual bool shouldDispatchFormControlChangeEvent(String&, String&);
  virtual void dispatchSearchEvent();

  // For test purpose
  virtual ColorChooserClient* colorChooserClient();

 protected:
  InputType(HTMLInputElement& element) : m_element(element) {}
  HTMLInputElement& element() const { return *m_element; }
  ChromeClient* chromeClient() const;
  Locale& locale() const;
  Decimal parseToNumberOrNaN(const String&) const;
  void countUsageIfVisible(UseCounter::Feature) const;

  // Derive the step base, following the HTML algorithm steps.
  Decimal findStepBase(const Decimal&) const;

  StepRange createStepRange(AnyStepHandling,
                            const Decimal& stepBaseDefault,
                            const Decimal& minimumDefault,
                            const Decimal& maximumDefault,
                            const StepRange::StepDescription&) const;
  void addWarningToConsole(const char* messageFormat,
                           const String& value) const;

 private:
  // Helper for stepUp()/stepDown(). Adds step value * count to the current
  // value.
  void applyStep(const Decimal&,
                 double count,
                 AnyStepHandling,
                 TextFieldEventBehavior,
                 ExceptionState&);

  Member<HTMLInputElement> m_element;
};

}  // namespace blink
#endif
