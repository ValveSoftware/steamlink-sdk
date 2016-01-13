/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "core/html/HTMLFormControlElement.h"

#include "core/events/Event.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLegendElement.h"
#include "core/html/ValidityState.h"
#include "core/html/forms/ValidationMessage.h"
#include "core/frame/UseCounter.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/Vector.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : LabelableElement(tagName, document)
    , m_disabled(false)
    , m_isAutofilled(false)
    , m_isReadOnly(false)
    , m_isRequired(false)
    , m_ancestorDisabledState(AncestorDisabledStateUnknown)
    , m_dataListAncestorState(Unknown)
    , m_willValidateInitialized(false)
    , m_willValidate(true)
    , m_isValid(true)
    , m_wasChangedSinceLastFormControlChangeEvent(false)
    , m_wasFocusedByMouse(false)
{
    setHasCustomStyleCallbacks();
    associateByParser(form);
}

HTMLFormControlElement::~HTMLFormControlElement()
{
#if !ENABLE(OILPAN)
    setForm(0);
#endif
}

void HTMLFormControlElement::trace(Visitor* visitor)
{
    FormAssociatedElement::trace(visitor);
    LabelableElement::trace(visitor);
}

String HTMLFormControlElement::formEnctype() const
{
    const AtomicString& formEnctypeAttr = fastGetAttribute(formenctypeAttr);
    if (formEnctypeAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::parseEncodingType(formEnctypeAttr);
}

void HTMLFormControlElement::setFormEnctype(const AtomicString& value)
{
    setAttribute(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const
{
    const AtomicString& formMethodAttr = fastGetAttribute(formmethodAttr);
    if (formMethodAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::methodString(FormSubmission::Attributes::parseMethodType(formMethodAttr));
}

void HTMLFormControlElement::setFormMethod(const AtomicString& value)
{
    setAttribute(formmethodAttr, value);
}

bool HTMLFormControlElement::formNoValidate() const
{
    return fastHasAttribute(formnovalidateAttr);
}

void HTMLFormControlElement::updateAncestorDisabledState() const
{
    HTMLFieldSetElement* fieldSetAncestor = 0;
    ContainerNode* legendAncestor = 0;
    for (HTMLElement* ancestor = Traversal<HTMLElement>::firstAncestor(*this); ancestor; ancestor = Traversal<HTMLElement>::firstAncestor(*ancestor)) {
        if (!legendAncestor && isHTMLLegendElement(*ancestor))
            legendAncestor = ancestor;
        if (isHTMLFieldSetElement(*ancestor)) {
            fieldSetAncestor = toHTMLFieldSetElement(ancestor);
            break;
        }
    }
    m_ancestorDisabledState = (fieldSetAncestor && fieldSetAncestor->isDisabledFormControl() && !(legendAncestor && legendAncestor == fieldSetAncestor->legend())) ? AncestorDisabledStateDisabled : AncestorDisabledStateEnabled;
}

void HTMLFormControlElement::ancestorDisabledStateWasChanged()
{
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    disabledAttributeChanged();
}

void HTMLFormControlElement::reset()
{
    setAutofilled(false);
    resetImpl();
}

void HTMLFormControlElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == formAttr) {
        formAttributeChanged();
        UseCounter::count(document(), UseCounter::FormAttribute);
    } else if (name == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !value.isNull();
        if (oldDisabled != m_disabled)
            disabledAttributeChanged();
    } else if (name == readonlyAttr) {
        bool wasReadOnly = m_isReadOnly;
        m_isReadOnly = !value.isNull();
        if (wasReadOnly != m_isReadOnly) {
            setNeedsWillValidateCheck();
            setNeedsStyleRecalc(SubtreeStyleChange);
            if (renderer() && renderer()->style()->hasAppearance())
                RenderTheme::theme().stateChanged(renderer(), ReadOnlyControlState);
        }
    } else if (name == requiredAttr) {
        bool wasRequired = m_isRequired;
        m_isRequired = !value.isNull();
        if (wasRequired != m_isRequired)
            requiredAttributeChanged();
        UseCounter::count(document(), UseCounter::RequiredAttribute);
    } else if (name == autofocusAttr) {
        HTMLElement::parseAttribute(name, value);
        UseCounter::count(document(), UseCounter::AutoFocusAttribute);
    } else
        HTMLElement::parseAttribute(name, value);
}

void HTMLFormControlElement::disabledAttributeChanged()
{
    setNeedsWillValidateCheck();
    didAffectSelector(AffectedSelectorDisabled | AffectedSelectorEnabled);
    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), EnabledControlState);
    if (isDisabledFormControl() && treeScope().adjustedFocusedElement() == this) {
        // We might want to call blur(), but it's dangerous to dispatch events
        // here.
        document().setNeedsFocusedElementCheck();
    }
}

void HTMLFormControlElement::requiredAttributeChanged()
{
    setNeedsValidityCheck();
    // Style recalculation is needed because style selectors may include
    // :required and :optional pseudo-classes.
    setNeedsStyleRecalc(SubtreeStyleChange);
}

bool HTMLFormControlElement::supportsAutofocus() const
{
    return false;
}

bool HTMLFormControlElement::isAutofocusable() const
{
    return fastHasAttribute(autofocusAttr) && supportsAutofocus();
}

void HTMLFormControlElement::setAutofilled(bool autofilled)
{
    if (autofilled == m_isAutofilled)
        return;

    m_isAutofilled = autofilled;
    setNeedsStyleRecalc(SubtreeStyleChange);
}

static bool shouldAutofocusOnAttach(const HTMLFormControlElement* element)
{
    if (!element->isAutofocusable())
        return false;
    if (element->document().isSandboxed(SandboxAutomaticFeatures)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        element->document().addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked autofocusing on a form control because the form's frame is sandboxed and the 'allow-scripts' permission is not set.");
        return false;
    }

    return true;
}

void HTMLFormControlElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    if (!renderer())
        return;

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    renderer()->updateFromElement();

    // FIXME: Autofocus handling should be moved to insertedInto according to
    // the standard.
    if (shouldAutofocusOnAttach(this))
        document().setAutofocusElement(this);
}

void HTMLFormControlElement::didMoveToNewDocument(Document& oldDocument)
{
    FormAssociatedElement::didMoveToNewDocument(oldDocument);
    HTMLElement::didMoveToNewDocument(oldDocument);
}

Node::InsertionNotificationRequest HTMLFormControlElement::insertedInto(ContainerNode* insertionPoint)
{
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    m_dataListAncestorState = Unknown;
    setNeedsWillValidateCheck();
    HTMLElement::insertedInto(insertionPoint);
    FormAssociatedElement::insertedInto(insertionPoint);
    return InsertionDone;
}

void HTMLFormControlElement::removedFrom(ContainerNode* insertionPoint)
{
    hideVisibleValidationMessage();
    m_validationMessage = nullptr;
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    m_dataListAncestorState = Unknown;
    HTMLElement::removedFrom(insertionPoint);
    FormAssociatedElement::removedFrom(insertionPoint);
}

void HTMLFormControlElement::setChangedSinceLastFormControlChangeEvent(bool changed)
{
    m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void HTMLFormControlElement::dispatchChangeEvent()
{
    dispatchScopedEvent(Event::createBubble(EventTypeNames::change));
}

void HTMLFormControlElement::dispatchFormControlChangeEvent()
{
    dispatchChangeEvent();
    setChangedSinceLastFormControlChangeEvent(false);
}

void HTMLFormControlElement::dispatchFormControlInputEvent()
{
    setChangedSinceLastFormControlChangeEvent(true);
    HTMLElement::dispatchInputEvent();
}

HTMLFormElement* HTMLFormControlElement::formOwner() const
{
    return FormAssociatedElement::form();
}

bool HTMLFormControlElement::isDisabledFormControl() const
{
    if (m_disabled)
        return true;

    if (m_ancestorDisabledState == AncestorDisabledStateUnknown)
        updateAncestorDisabledState();
    return m_ancestorDisabledState == AncestorDisabledStateDisabled;
}

bool HTMLFormControlElement::isRequired() const
{
    return m_isRequired;
}

String HTMLFormControlElement::resultForDialogSubmit()
{
    return fastGetAttribute(valueAttr);
}

void HTMLFormControlElement::didRecalcStyle(StyleRecalcChange)
{
    if (RenderObject* renderer = this->renderer())
        renderer->updateFromElement();
}

bool HTMLFormControlElement::supportsFocus() const
{
    return !isDisabledFormControl();
}

bool HTMLFormControlElement::isKeyboardFocusable() const
{
    // Skip tabIndex check in a parent class.
    return isFocusable();
}

bool HTMLFormControlElement::shouldShowFocusRingOnMouseFocus() const
{
    return false;
}

void HTMLFormControlElement::dispatchFocusEvent(Element* oldFocusedElement, FocusType type)
{
    if (type != FocusTypePage)
        m_wasFocusedByMouse = type == FocusTypeMouse;
    HTMLElement::dispatchFocusEvent(oldFocusedElement, type);
}

bool HTMLFormControlElement::shouldHaveFocusAppearance() const
{
    ASSERT(focused());
    return shouldShowFocusRingOnMouseFocus() || !m_wasFocusedByMouse;
}

void HTMLFormControlElement::willCallDefaultEventHandler(const Event& event)
{
    if (!event.isKeyboardEvent() || event.type() != EventTypeNames::keydown)
        return;
    if (!m_wasFocusedByMouse)
        return;
    m_wasFocusedByMouse = false;
    if (renderer())
        renderer()->paintInvalidationForWholeRenderer();
}


short HTMLFormControlElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

bool HTMLFormControlElement::recalcWillValidate() const
{
    if (m_dataListAncestorState == Unknown) {
        if (Traversal<HTMLDataListElement>::firstAncestor(*this))
            m_dataListAncestorState = InsideDataList;
        else
            m_dataListAncestorState = NotInsideDataList;
    }
    return m_dataListAncestorState == NotInsideDataList && !isDisabledOrReadOnly();
}

bool HTMLFormControlElement::willValidate() const
{
    if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
        m_willValidateInitialized = true;
        bool newWillValidate = recalcWillValidate();
        if (m_willValidate != newWillValidate) {
            m_willValidate = newWillValidate;
            const_cast<HTMLFormControlElement*>(this)->setNeedsValidityCheck();
        }
    } else {
        // If the following assertion fails, setNeedsWillValidateCheck() is not
        // called correctly when something which changes recalcWillValidate() result
        // is updated.
        ASSERT(m_willValidate == recalcWillValidate());
    }
    return m_willValidate;
}

void HTMLFormControlElement::setNeedsWillValidateCheck()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = recalcWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;
    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;
    setNeedsValidityCheck();
    setNeedsStyleRecalc(SubtreeStyleChange);
    if (!m_willValidate)
        hideVisibleValidationMessage();
}

void HTMLFormControlElement::updateVisibleValidationMessage()
{
    Page* page = document().page();
    if (!page)
        return;
    String message;
    if (renderer() && willValidate())
        message = validationMessage().stripWhiteSpace();
    if (!m_validationMessage)
        m_validationMessage = ValidationMessage::create(this);
    m_validationMessage->updateValidationMessage(message);
}

void HTMLFormControlElement::hideVisibleValidationMessage()
{
    if (m_validationMessage)
        m_validationMessage->requestToHideMessage();
}

bool HTMLFormControlElement::checkValidity(WillBeHeapVector<RefPtrWillBeMember<FormAssociatedElement> >* unhandledInvalidControls)
{
    if (!willValidate() || isValidFormControlElement())
        return true;
    // An event handler can deref this object.
    RefPtrWillBeRawPtr<HTMLFormControlElement> protector(this);
    RefPtrWillBeRawPtr<Document> originalDocument(document());
    bool needsDefaultAction = dispatchEvent(Event::createCancelable(EventTypeNames::invalid));
    if (needsDefaultAction && unhandledInvalidControls && inDocument() && originalDocument == document())
        unhandledInvalidControls->append(this);
    return false;
}

bool HTMLFormControlElement::isValidFormControlElement()
{
    // If the following assertion fails, setNeedsValidityCheck() is not called
    // correctly when something which changes validity is updated.
    ASSERT(m_isValid == valid());
    return m_isValid;
}

void HTMLFormControlElement::setNeedsValidityCheck()
{
    bool newIsValid = valid();
    if (willValidate() && newIsValid != m_isValid) {
        // Update style for pseudo classes such as :valid :invalid.
        setNeedsStyleRecalc(SubtreeStyleChange);
    }
    m_isValid = newIsValid;

    // Updates only if this control already has a validation message.
    if (m_validationMessage && m_validationMessage->isVisible()) {
        // Calls updateVisibleValidationMessage() even if m_isValid is not
        // changed because a validation message can be chagned.
        updateVisibleValidationMessage();
    }
}

void HTMLFormControlElement::setCustomValidity(const String& error)
{
    FormAssociatedElement::setCustomValidity(error);
    setNeedsValidityCheck();
}

void HTMLFormControlElement::dispatchBlurEvent(Element* newFocusedElement)
{
    HTMLElement::dispatchBlurEvent(newFocusedElement);
    hideVisibleValidationMessage();
}

bool HTMLFormControlElement::isSuccessfulSubmitButton() const
{
    return canBeSuccessfulSubmitButton() && !isDisabledFormControl();
}

bool HTMLFormControlElement::isDefaultButtonForForm() const
{
    return isSuccessfulSubmitButton() && form() && form()->defaultButton() == this;
}

HTMLFormControlElement* HTMLFormControlElement::enclosingFormControlElement(Node* node)
{
    if (!node)
        return 0;
    return Traversal<HTMLFormControlElement>::firstAncestorOrSelf(*node);
}

String HTMLFormControlElement::nameForAutofill() const
{
    String fullName = name();
    String trimmedName = fullName.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return trimmedName;
    fullName = getIdAttribute();
    trimmedName = fullName.stripWhiteSpace();
    return trimmedName;
}

void HTMLFormControlElement::setFocus(bool flag)
{
    LabelableElement::setFocus(flag);

    if (!flag && wasChangedSinceLastFormControlChangeEvent())
        dispatchFormControlChangeEvent();
}

} // namespace Webcore
