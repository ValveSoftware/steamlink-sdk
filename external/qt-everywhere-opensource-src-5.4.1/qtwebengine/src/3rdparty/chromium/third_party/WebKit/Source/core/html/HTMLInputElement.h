/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLInputElement_h
#define HTMLInputElement_h

#include "core/html/HTMLTextFormControlElement.h"
#include "core/html/forms/StepRange.h"
#include "platform/FileChooser.h"

namespace WebCore {

class DragData;
class ExceptionState;
class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class HTMLOptionElement;
class InputType;
class InputTypeView;
class KURL;
class ListAttributeTargetObserver;
class RadioButtonGroupScope;
struct DateTimeChooserParameters;

class HTMLInputElement : public HTMLTextFormControlElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLInputElement> create(Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLInputElement();
    virtual void trace(Visitor*) OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitspeechchange);

    virtual bool shouldAutocomplete() const OVERRIDE FINAL;

    // For ValidityState
    virtual bool hasBadInput() const OVERRIDE FINAL;
    virtual bool patternMismatch() const OVERRIDE FINAL;
    virtual bool rangeUnderflow() const OVERRIDE FINAL;
    virtual bool rangeOverflow() const OVERRIDE FINAL;
    virtual bool stepMismatch() const OVERRIDE FINAL;
    virtual bool tooLong() const OVERRIDE FINAL;
    virtual bool typeMismatch() const OVERRIDE FINAL;
    virtual bool valueMissing() const OVERRIDE FINAL;
    virtual String validationMessage() const OVERRIDE FINAL;

    // Returns the minimum value for type=date, number, or range.  Don't call this for other types.
    double minimum() const;
    // Returns the maximum value for type=date, number, or range.  Don't call this for other types.
    // This always returns a value which is >= minimum().
    double maximum() const;
    // Sets the "allowed value step" defined in the HTML spec to the specified double pointer.
    // Returns false if there is no "allowed value step."
    bool getAllowedValueStep(Decimal*) const;
    StepRange createStepRange(AnyStepHandling) const;

    Decimal findClosestTickMarkValue(const Decimal&);

    // Implementations of HTMLInputElement::stepUp() and stepDown().
    void stepUp(int, ExceptionState&);
    void stepDown(int, ExceptionState&);
    void stepUp(ExceptionState& exceptionState) { stepUp(1, exceptionState); }
    void stepDown(ExceptionState& exceptionState) { stepDown(1, exceptionState); }
    // stepUp()/stepDown() for user-interaction.
    bool isSteppable() const;

    bool isTextButton() const;

    bool isRadioButton() const;
    bool isTextField() const;
    bool isSearchField() const;
    bool isInputTypeHidden() const;
    bool isPasswordField() const;
    bool isCheckbox() const;
    bool isRangeControl() const;

    // FIXME: It's highly likely that any call site calling this function should instead
    // be using a different one. Many input elements behave like text fields, and in addition
    // any unknown input type is treated as text. Consider, for example, isTextField or
    // isTextField && !isPasswordField.
    bool isText() const;

    bool isEmailField() const;
    bool isFileUpload() const;
    bool isImageButton() const;
    bool isNumberField() const;
    bool isTelephoneField() const;
    bool isURLField() const;
    bool isDateField() const;
    bool isDateTimeLocalField() const;
    bool isMonthField() const;
    bool isTimeField() const;
    bool isWeekField() const;

    bool checked() const { return m_isChecked; }
    void setChecked(bool, TextFieldEventBehavior = DispatchNoEvent);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_isIndeterminate; }
    void setIndeterminate(bool);
    // shouldAppearChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    bool shouldAppearChecked() const;
    virtual bool shouldAppearIndeterminate() const OVERRIDE;

    int size() const;
    bool sizeShouldIncludeDecoration(int& preferredSize) const;

    void setType(const AtomicString&);

    virtual String value() const OVERRIDE;
    void setValue(const String&, ExceptionState&, TextFieldEventBehavior = DispatchNoEvent);
    void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
    void setValueForUser(const String&);
    // Checks if the specified string would be a valid value.
    // We should not call this for types with no string value such as CHECKBOX and RADIO.
    bool isValidValue(const String&) const;
    bool hasDirtyValue() const { return !m_valueIfDirty.isNull(); };

    String sanitizeValue(const String&) const;

    String localizeValue(const String&) const;

    const String& suggestedValue() const;
    void setSuggestedValue(const String&);

    void setEditingValue(const String&);

    double valueAsDate(bool& isNull) const;
    void setValueAsDate(double, ExceptionState&);

    double valueAsNumber() const;
    void setValueAsNumber(double, ExceptionState&, TextFieldEventBehavior = DispatchNoEvent);

    String valueWithDefault() const;

    void setValueFromRenderer(const String&);

    int selectionStartForBinding(ExceptionState&) const;
    int selectionEndForBinding(ExceptionState&) const;
    String selectionDirectionForBinding(ExceptionState&) const;
    void setSelectionStartForBinding(int, ExceptionState&);
    void setSelectionEndForBinding(int, ExceptionState&);
    void setSelectionDirectionForBinding(const String&, ExceptionState&);
    void setSelectionRangeForBinding(int start, int end, ExceptionState&);
    void setSelectionRangeForBinding(int start, int end, const String& direction, ExceptionState&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE FINAL;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE FINAL;
    virtual void updateFocusAppearance(bool restorePreviousSelection) OVERRIDE FINAL;

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    virtual bool isActivatedSubmit() const OVERRIDE FINAL;
    virtual void setActivatedSubmit(bool flag) OVERRIDE FINAL;

    String altText() const;

    int maxResults() const { return m_maxResults; }

    const AtomicString& defaultValue() const;
    void setDefaultValue(const AtomicString&);

    Vector<String> acceptMIMETypes();
    Vector<String> acceptFileExtensions();
    const AtomicString& alt() const;

    void setSize(unsigned);
    void setSize(unsigned, ExceptionState&);

    KURL src() const;

    int maxLength() const;
    void setMaxLength(int, ExceptionState&);

    bool multiple() const;

    FileList* files();
    void setFiles(PassRefPtrWillBeRawPtr<FileList>);

    // Returns true if the given DragData has more than one dropped files.
    bool receiveDroppedFiles(const DragData*);

    String droppedFileSystemId();

    // These functions are used for rendering the input active during a
    // drag-and-drop operation.
    bool canReceiveDroppedFiles() const;
    void setCanReceiveDroppedFiles(bool);

    void onSearch();

    void updateClearButtonVisibility();

    virtual bool willRespondToMouseClickEvents() OVERRIDE;

    HTMLElement* list() const;
    HTMLDataListElement* dataList() const;
    bool hasValidDataListOptions() const;
    void listAttributeTargetChanged();

    HTMLInputElement* checkedRadioButtonForGroup() const;
    bool isInRequiredRadioButtonGroup();

    // Functions for InputType classes.
    void setValueInternal(const String&, TextFieldEventBehavior);
    bool valueAttributeWasUpdatedAfterParsing() const { return m_valueAttributeWasUpdatedAfterParsing; }
    void updateView();
    bool needsToUpdateViewValue() const { return m_needsToUpdateViewValue; }
    virtual void setInnerEditorValue(const String&) OVERRIDE;

    void cacheSelectionInResponseToSetValue(int caretOffset) { cacheSelection(caretOffset, caretOffset, SelectionHasNoDirection); }

    // For test purposes.
    void selectColorInColorChooser(const Color&);

    String defaultToolTip() const;

    static const int maximumLength;

    unsigned height() const;
    unsigned width() const;
    void setHeight(unsigned);
    void setWidth(unsigned);

    virtual void blur() OVERRIDE FINAL;
    void defaultBlur();

    virtual const AtomicString& name() const OVERRIDE FINAL;

    void beginEditing();
    void endEditing();

    static Vector<FileChooserFileInfo> filesFromFileInputFormControlState(const FormControlState&);

    virtual bool matchesReadOnlyPseudoClass() const OVERRIDE FINAL;
    virtual bool matchesReadWritePseudoClass() const OVERRIDE FINAL;
    virtual void setRangeText(const String& replacement, ExceptionState&) OVERRIDE FINAL;
    virtual void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionState&) OVERRIDE FINAL;

    bool hasImageLoader() const { return m_imageLoader; }
    HTMLImageLoader* imageLoader();

    bool setupDateTimeChooserParameters(DateTimeChooserParameters&);

    bool supportsInputModeAttribute() const;

    void setShouldRevealPassword(bool value);
    bool shouldRevealPassword() const { return m_shouldRevealPassword; }

protected:
    HTMLInputElement(Document&, HTMLFormElement*, bool createdByParser);

    virtual void defaultEventHandler(Event*) OVERRIDE;

private:
    enum AutoCompleteSetting { Uninitialized, On, Off };

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE FINAL;
    virtual void willAddFirstAuthorShadowRoot() OVERRIDE FINAL;

    virtual void willChangeForm() OVERRIDE FINAL;
    virtual void didChangeForm() OVERRIDE FINAL;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE FINAL;
    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE FINAL;
    virtual void removeAllEventListeners() OVERRIDE FINAL;

    virtual bool hasCustomFocusLogic() const OVERRIDE FINAL;
    virtual bool isKeyboardFocusable() const OVERRIDE FINAL;
    virtual bool shouldShowFocusRingOnMouseFocus() const OVERRIDE FINAL;
    virtual bool isEnumeratable() const OVERRIDE FINAL;
    virtual bool isInteractiveContent() const OVERRIDE FINAL;
    virtual bool supportLabels() const OVERRIDE FINAL;
    virtual bool shouldUseInputMethod() OVERRIDE FINAL;

    virtual bool isTextFormControl() const OVERRIDE FINAL { return isTextField(); }

    virtual bool canTriggerImplicitSubmission() const OVERRIDE FINAL { return isTextField(); }

    virtual const AtomicString& formControlType() const OVERRIDE FINAL;

    virtual bool shouldSaveAndRestoreFormControlState() const OVERRIDE FINAL;
    virtual FormControlState saveFormControlState() const OVERRIDE FINAL;
    virtual void restoreFormControlState(const FormControlState&) OVERRIDE FINAL;

    virtual bool canStartSelection() const OVERRIDE FINAL;

    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE FINAL;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE FINAL;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE FINAL;
    virtual void finishParsingChildren() OVERRIDE FINAL;

    virtual void copyNonAttributePropertiesFromElement(const Element&) OVERRIDE FINAL;

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE FINAL;

    virtual bool appendFormData(FormDataList&, bool) OVERRIDE FINAL;
    virtual String resultForDialogSubmit() OVERRIDE FINAL;

    virtual bool canBeSuccessfulSubmitButton() const OVERRIDE FINAL;

    virtual void resetImpl() OVERRIDE FINAL;
    virtual bool supportsAutofocus() const OVERRIDE FINAL;

    virtual void* preDispatchEventHandler(Event*) OVERRIDE FINAL;
    virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch) OVERRIDE FINAL;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE FINAL;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE FINAL;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE FINAL;
    virtual bool isInRange() const OVERRIDE FINAL;
    virtual bool isOutOfRange() const OVERRIDE FINAL;

    bool isTextType() const;
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    virtual bool supportsPlaceholder() const OVERRIDE FINAL;
    virtual void updatePlaceholderText() OVERRIDE FINAL;
    virtual bool isEmptyValue() const OVERRIDE FINAL { return innerEditorValue().isEmpty(); }
    virtual bool isEmptySuggestedValue() const OVERRIDE FINAL { return suggestedValue().isEmpty(); }
    virtual void handleFocusEvent(Element* oldFocusedElement, FocusType) OVERRIDE FINAL;
    virtual void handleBlurEvent() OVERRIDE FINAL;

    virtual bool isOptionalFormControl() const OVERRIDE FINAL { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const OVERRIDE FINAL;
    virtual bool recalcWillValidate() const OVERRIDE FINAL;
    virtual void requiredAttributeChanged() OVERRIDE FINAL;

    void updateType();

    virtual void subtreeHasChanged() OVERRIDE FINAL;

    void setListAttributeTargetObserver(PassOwnPtrWillBeRawPtr<ListAttributeTargetObserver>);
    void resetListAttributeTargetObserver();
    void parseMaxLengthAttribute(const AtomicString&);
    void updateValueIfNeeded();

    // Returns null if this isn't associated with any radio button group.
    RadioButtonGroupScope* radioButtonGroupScope() const;
    void addToRadioButtonGroup();
    void removeFromRadioButtonGroup();
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
#endif

    virtual bool shouldDispatchFormControlChangeEvent(String&, String&) OVERRIDE;

    AtomicString m_name;
    String m_valueIfDirty;
    String m_suggestedValue;
    int m_size;
    int m_maxLength;
    short m_maxResults;
    bool m_isChecked : 1;
    bool m_reflectsCheckedAttribute : 1;
    bool m_isIndeterminate : 1;
    bool m_isActivatedSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_hasNonEmptyList : 1;
    bool m_stateRestored : 1;
    bool m_parsingInProgress : 1;
    bool m_valueAttributeWasUpdatedAfterParsing : 1;
    bool m_canReceiveDroppedFiles : 1;
    bool m_hasTouchEventHandler : 1;
    bool m_shouldRevealPassword : 1;
    bool m_needsToUpdateViewValue : 1;
    RefPtrWillBeMember<InputType> m_inputType;
    RefPtrWillBeMember<InputTypeView> m_inputTypeView;
    // The ImageLoader must be owned by this element because the loader code assumes
    // that it lives as long as its owning element lives. If we move the loader into
    // the ImageInput object we may delete the loader while this element lives on.
    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;
    OwnPtrWillBeMember<ListAttributeTargetObserver> m_listAttributeTargetObserver;
};

} //namespace
#endif
