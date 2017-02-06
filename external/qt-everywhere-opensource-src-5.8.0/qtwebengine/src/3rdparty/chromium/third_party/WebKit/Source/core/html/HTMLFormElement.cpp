/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "core/html/HTMLFormElement.h"

#include "bindings/core/v8/RadioNodeListOrElement.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/events/Event.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFormControlsCollection.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/RadioNodeList.h"
#include "core/html/forms/FormController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NavigationScheduler.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "wtf/text/AtomicString.h"
#include <limits>

namespace blink {

using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(Document& document)
    : HTMLElement(formTag, document)
    , m_associatedElementsAreDirty(false)
    , m_imageElementsAreDirty(false)
    , m_hasElementsAssociatedByParser(false)
    , m_hasElementsAssociatedByFormAttribute(false)
    , m_didFinishParsingChildren(false)
    , m_isSubmittingOrInUserJSSubmitEvent(false)
    , m_shouldSubmit(false)
    , m_isInResetFunction(false)
    , m_wasDemoted(false)
{
}

HTMLFormElement* HTMLFormElement::create(Document& document)
{
    UseCounter::count(document, UseCounter::FormElement);
    return new HTMLFormElement(document);
}

HTMLFormElement::~HTMLFormElement()
{
}

DEFINE_TRACE(HTMLFormElement)
{
    visitor->trace(m_pastNamesMap);
    visitor->trace(m_radioButtonGroupScope);
    visitor->trace(m_associatedElements);
    visitor->trace(m_imageElements);
    HTMLElement::trace(visitor);
}

bool HTMLFormElement::matchesValidityPseudoClasses() const
{
    return true;
}

bool HTMLFormElement::isValidElement()
{
    return !checkInvalidControlsAndCollectUnhandled(0, CheckValidityDispatchNoEvent);
}

bool HTMLFormElement::layoutObjectIsNeeded(const ComputedStyle& style)
{
    if (!m_wasDemoted)
        return HTMLElement::layoutObjectIsNeeded(style);

    ContainerNode* node = parentNode();
    if (!node || !node->layoutObject())
        return HTMLElement::layoutObjectIsNeeded(style);
    LayoutObject* parentLayoutObject = node->layoutObject();
    // FIXME: Shouldn't we also check for table caption (see |formIsTablePart| below).
    // FIXME: This check is not correct for Shadow DOM.
    bool parentIsTableElementPart = (parentLayoutObject->isTable() && isHTMLTableElement(*node))
        || (parentLayoutObject->isTableRow() && isHTMLTableRowElement(*node))
        || (parentLayoutObject->isTableSection() && node->hasTagName(tbodyTag))
        || (parentLayoutObject->isLayoutTableCol() && node->hasTagName(colTag))
        || (parentLayoutObject->isTableCell() && isHTMLTableRowElement(*node));

    if (!parentIsTableElementPart)
        return true;

    EDisplay display = style.display();
    bool formIsTablePart = display == TABLE || display == INLINE_TABLE || display == TABLE_ROW_GROUP
        || display == TABLE_HEADER_GROUP || display == TABLE_FOOTER_GROUP || display == TABLE_ROW
        || display == TABLE_COLUMN_GROUP || display == TABLE_COLUMN || display == TABLE_CELL
        || display == TABLE_CAPTION;

    return formIsTablePart;
}

Node::InsertionNotificationRequest HTMLFormElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    logAddElementIfIsolatedWorldAndInDocument("form", methodAttr, actionAttr);
    if (insertionPoint->inShadowIncludingDocument())
        this->document().didAssociateFormControl(this);
    return InsertionDone;
}

template<class T>
void notifyFormRemovedFromTree(const T& elements, Node& root)
{
    for (const auto& element : elements)
        element->formRemovedFromTree(root);
}

void HTMLFormElement::removedFrom(ContainerNode* insertionPoint)
{
    // We don't need to take care of form association by 'form' content
    // attribute becuse IdTargetObserver handles it.
    if (m_hasElementsAssociatedByParser) {
        Node& root = NodeTraversal::highestAncestorOrSelf(*this);
        if (!m_associatedElementsAreDirty) {
            FormAssociatedElement::List elements(associatedElements());
            notifyFormRemovedFromTree(elements, root);
        } else {
            FormAssociatedElement::List elements;
            collectAssociatedElements(NodeTraversal::highestAncestorOrSelf(*insertionPoint), elements);
            notifyFormRemovedFromTree(elements, root);
            collectAssociatedElements(root, elements);
            notifyFormRemovedFromTree(elements, root);
        }

        if (!m_imageElementsAreDirty) {
            HeapVector<Member<HTMLImageElement>> images(imageElements());
            notifyFormRemovedFromTree(images, root);
        } else {
            HeapVector<Member<HTMLImageElement>> images;
            collectImageElements(NodeTraversal::highestAncestorOrSelf(*insertionPoint), images);
            notifyFormRemovedFromTree(images, root);
            collectImageElements(root, images);
            notifyFormRemovedFromTree(images, root);
        }
    }
    document().formController().willDeleteForm(this);
    HTMLElement::removedFrom(insertionPoint);
}

void HTMLFormElement::handleLocalEvents(Event& event)
{
    Node* targetNode = event.target()->toNode();
    if (event.eventPhase() != Event::CAPTURING_PHASE && targetNode && targetNode != this && (event.type() == EventTypeNames::submit || event.type() == EventTypeNames::reset)) {
        event.stopPropagation();
        return;
    }
    HTMLElement::handleLocalEvents(event);
}

unsigned HTMLFormElement::length() const
{
    const FormAssociatedElement::List& elements = associatedElements();
    unsigned len = 0;
    for (unsigned i = 0; i < elements.size(); ++i) {
        if (elements[i]->isEnumeratable())
            ++len;
    }
    return len;
}

HTMLElement* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

void HTMLFormElement::submitImplicitly(Event* event, bool fromImplicitSubmissionTrigger)
{
    int submissionTriggerCount = 0;
    bool seenDefaultButton = false;
    const FormAssociatedElement::List& elements = associatedElements();
    for (unsigned i = 0; i < elements.size(); ++i) {
        FormAssociatedElement* formAssociatedElement = elements[i];
        if (!formAssociatedElement->isFormControlElement())
            continue;
        HTMLFormControlElement* control = toHTMLFormControlElement(formAssociatedElement);
        if (!seenDefaultButton && control->canBeSuccessfulSubmitButton()) {
            if (fromImplicitSubmissionTrigger)
                seenDefaultButton = true;
            if (control->isSuccessfulSubmitButton()) {
                control->dispatchSimulatedClick(event);
                return;
            }
            if (fromImplicitSubmissionTrigger) {
                // Default (submit) button is not activated; no implicit submission.
                return;
            }
        } else if (control->canTriggerImplicitSubmission()) {
            ++submissionTriggerCount;
        }
    }
    if (fromImplicitSubmissionTrigger && submissionTriggerCount == 1)
        prepareForSubmission(event);
}

// FIXME: Consolidate this and similar code in FormSubmission.cpp.
static inline HTMLFormControlElement* submitElementFromEvent(const Event* event)
{
    for (Node* node = event->target()->toNode(); node; node = node->parentOrShadowHostNode()) {
        if (node->isElementNode() && toElement(node)->isFormControlElement())
            return toHTMLFormControlElement(node);
    }
    return 0;
}

bool HTMLFormElement::validateInteractively()
{
    UseCounter::count(document(), UseCounter::FormValidationStarted);
    const FormAssociatedElement::List& elements = associatedElements();
    for (unsigned i = 0; i < elements.size(); ++i) {
        if (elements[i]->isFormControlElement())
            toHTMLFormControlElement(elements[i])->hideVisibleValidationMessage();
    }

    HeapVector<Member<HTMLFormControlElement>> unhandledInvalidControls;
    if (!checkInvalidControlsAndCollectUnhandled(&unhandledInvalidControls, CheckValidityDispatchInvalidEvent))
        return true;
    UseCounter::count(document(), UseCounter::FormValidationAbortedSubmission);
    // Because the form has invalid controls, we abort the form submission and
    // show a validation message on a focusable form control.

    // Needs to update layout now because we'd like to call isFocusable(), which
    // has !layoutObject()->needsLayout() assertion.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Focus on the first focusable control and show a validation message.
    for (unsigned i = 0; i < unhandledInvalidControls.size(); ++i) {
        HTMLFormControlElement* unhandled = unhandledInvalidControls[i].get();
        if (unhandled->isFocusable()) {
            unhandled->showValidationMessage();
            UseCounter::count(document(), UseCounter::FormValidationShowedMessage);
            break;
        }
    }
    // Warn about all of unfocusable controls.
    if (document().frame()) {
        for (unsigned i = 0; i < unhandledInvalidControls.size(); ++i) {
            HTMLFormControlElement* unhandled = unhandledInvalidControls[i].get();
            if (unhandled->isFocusable())
                continue;
            String message("An invalid form control with name='%name' is not focusable.");
            message.replace("%name", unhandled->name());
            document().addConsoleMessage(ConsoleMessage::create(RenderingMessageSource, ErrorMessageLevel, message));
        }
    }
    return false;
}

void HTMLFormElement::prepareForSubmission(Event* event)
{
    LocalFrame* frame = document().frame();
    if (!frame || m_isSubmittingOrInUserJSSubmitEvent)
        return;

    bool skipValidation = !document().page() || noValidate();
    ASSERT(event);
    HTMLFormControlElement* submitElement = submitElementFromEvent(event);
    if (submitElement && submitElement->formNoValidate())
        skipValidation = true;

    UseCounter::count(document(), UseCounter::FormSubmissionStarted);
    // Interactive validation must be done before dispatching the submit event.
    if (!skipValidation && !validateInteractively())
        return;

    m_isSubmittingOrInUserJSSubmitEvent = true;
    m_shouldSubmit = false;

    frame->loader().client()->dispatchWillSendSubmitEvent(this);

    if (dispatchEvent(Event::createCancelableBubble(EventTypeNames::submit)) == DispatchEventResult::NotCanceled)
        m_shouldSubmit = true;

    m_isSubmittingOrInUserJSSubmitEvent = false;

    if (m_shouldSubmit)
        submit(event, true);
}

void HTMLFormElement::submitFromJavaScript()
{
    submit(0, false);
}

void HTMLFormElement::submitDialog(FormSubmission* formSubmission)
{
    for (Node* node = this; node; node = node->parentOrShadowHostNode()) {
        if (isHTMLDialogElement(*node)) {
            toHTMLDialogElement(*node).closeDialog(formSubmission->result());
            return;
        }
    }
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton)
{
    FrameView* view = document().view();
    LocalFrame* frame = document().frame();
    if (!view || !frame || !frame->page())
        return;
    // See crbug.com/586749.
    if (!inShadowIncludingDocument())
        UseCounter::count(document(), UseCounter::FormSubmissionNotInDocumentTree);

    if (m_isSubmittingOrInUserJSSubmitEvent) {
        m_shouldSubmit = true;
        return;
    }

    m_isSubmittingOrInUserJSSubmitEvent = true;

    HTMLFormControlElement* firstSuccessfulSubmitButton = nullptr;
    bool needButtonActivation = activateSubmitButton; // do we need to activate a submit button?

    const FormAssociatedElement::List& elements = associatedElements();
    for (unsigned i = 0; i < elements.size(); ++i) {
        FormAssociatedElement* associatedElement = elements[i];
        if (!associatedElement->isFormControlElement())
            continue;
        if (needButtonActivation) {
            HTMLFormControlElement* control = toHTMLFormControlElement(associatedElement);
            if (control->isActivatedSubmit())
                needButtonActivation = false;
            else if (firstSuccessfulSubmitButton == 0 && control->isSuccessfulSubmitButton())
                firstSuccessfulSubmitButton = control;
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(true);

    FormSubmission* formSubmission = FormSubmission::create(this, m_attributes, event);
    EventQueueScope scopeForDialogClose; // Delay dispatching 'close' to dialog until done submitting.
    if (formSubmission->method() == FormSubmission::DialogMethod)
        submitDialog(formSubmission);
    else
        scheduleFormSubmission(formSubmission);

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(false);

    m_shouldSubmit = false;
    m_isSubmittingOrInUserJSSubmitEvent = false;
}

void HTMLFormElement::scheduleFormSubmission(FormSubmission* submission)
{
    ASSERT(submission->method() == FormSubmission::PostMethod || submission->method() == FormSubmission::GetMethod);
    ASSERT(submission->data());
    ASSERT(submission->form());
    if (submission->action().isEmpty())
        return;
    if (document().isSandboxed(SandboxForms)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        document().addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Blocked form submission to '" + submission->action().elidedString() + "' because the form's frame is sandboxed and the 'allow-forms' permission is not set."));
        return;
    }

    if (!document().contentSecurityPolicy()->allowFormAction(
            submission->action())) {
      return;
    }

    if (protocolIsJavaScript(submission->action())) {
        document().frame()->script().executeScriptIfJavaScriptURL(submission->action());
        return;
    }

    Frame* targetFrame = document().frame()->findFrameForNavigation(submission->target(), *document().frame());
    if (!targetFrame) {
        if (!LocalDOMWindow::allowPopUp(*document().frame()) && !UserGestureIndicator::utilizeUserGesture())
            return;
        targetFrame = document().frame();
    } else {
        submission->clearTarget();
    }
    if (!targetFrame->host())
        return;

    UseCounter::count(document(), UseCounter::FormsSubmitted);
    if (MixedContentChecker::isMixedFormAction(document().frame(), submission->action()))
        UseCounter::count(document().frame(), UseCounter::MixedContentFormsSubmitted);

    // TODO(lukasza): Investigate if the code below can uniformly handle remote
    // and local frames (i.e. by calling virtual Frame::navigate from a timer).
    // See also https://goo.gl/95d2KA.
    if (targetFrame->isLocalFrame()) {
        toLocalFrame(targetFrame)->navigationScheduler().scheduleFormSubmission(&document(), submission);
    } else {
        FrameLoadRequest frameLoadRequest = submission->createFrameLoadRequest(&document());
        toRemoteFrame(targetFrame)->navigate(frameLoadRequest);
    }
}

void HTMLFormElement::reset()
{
    LocalFrame* frame = document().frame();
    if (m_isInResetFunction || !frame)
        return;

    m_isInResetFunction = true;

    if (dispatchEvent(Event::createCancelableBubble(EventTypeNames::reset)) != DispatchEventResult::NotCanceled) {
        m_isInResetFunction = false;
        return;
    }

    const FormAssociatedElement::List& elements = associatedElements();
    for (unsigned i = 0; i < elements.size(); ++i) {
        if (elements[i]->isFormControlElement())
            toHTMLFormControlElement(elements[i])->reset();
    }

    m_isInResetFunction = false;
}

void HTMLFormElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == actionAttr) {
        m_attributes.parseAction(value);
        logUpdateAttributeIfIsolatedWorldAndInDocument("form", actionAttr, oldValue, value);

        // If we're not upgrading insecure requests, and the new action attribute is pointing to
        // an insecure "action" location from a secure page it is marked as "passive" mixed content.
        if (document().getInsecureRequestPolicy() & kUpgradeInsecureRequests)
            return;
        KURL actionURL = document().completeURL(m_attributes.action().isEmpty() ? document().url().getString() : m_attributes.action());
        if (MixedContentChecker::isMixedFormAction(document().frame(), actionURL))
            UseCounter::count(document().frame(), UseCounter::MixedContentFormPresent);
    } else if (name == targetAttr) {
        m_attributes.setTarget(value);
    } else if (name == methodAttr) {
        m_attributes.updateMethodType(value);
    } else if (name == enctypeAttr) {
        m_attributes.updateEncodingType(value);
    } else if (name == accept_charsetAttr) {
        m_attributes.setAcceptCharset(value);
    } else {
        HTMLElement::parseAttribute(name, oldValue, value);
    }
}

void HTMLFormElement::associate(FormAssociatedElement& e)
{
    m_associatedElementsAreDirty = true;
    m_associatedElements.clear();
    if (toHTMLElement(e).fastHasAttribute(formAttr))
        m_hasElementsAssociatedByFormAttribute = true;
}

void HTMLFormElement::disassociate(FormAssociatedElement& e)
{
    m_associatedElementsAreDirty = true;
    m_associatedElements.clear();
    removeFromPastNamesMap(toHTMLElement(e));
}

bool HTMLFormElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == actionAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLFormElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == actionAttr || HTMLElement::hasLegalLinkAttribute(name);
}

void HTMLFormElement::associate(HTMLImageElement& e)
{
    m_imageElementsAreDirty = true;
    m_imageElements.clear();
}

void HTMLFormElement::disassociate(HTMLImageElement& e)
{
    m_imageElementsAreDirty = true;
    m_imageElements.clear();
    removeFromPastNamesMap(e);
}

void HTMLFormElement::didAssociateByParser()
{
    if (!m_didFinishParsingChildren)
        return;
    m_hasElementsAssociatedByParser = true;
    UseCounter::count(document(), UseCounter::FormAssociationByParser);
}

HTMLFormControlsCollection* HTMLFormElement::elements()
{
    return ensureCachedCollection<HTMLFormControlsCollection>(FormControls);
}

void HTMLFormElement::collectAssociatedElements(Node& root, FormAssociatedElement::List& elements) const
{
    elements.clear();
    for (HTMLElement& element : Traversal<HTMLElement>::startsAfter(root)) {
        FormAssociatedElement* associatedElement = 0;
        if (element.isFormControlElement())
            associatedElement = toHTMLFormControlElement(&element);
        else if (isHTMLObjectElement(element))
            associatedElement = toHTMLObjectElement(&element);
        else
            continue;
        if (associatedElement->form()== this)
            elements.append(associatedElement);
    }
}

// This function should be const conceptually. However we update some fields
// because of lazy evaluation.
const FormAssociatedElement::List& HTMLFormElement::associatedElements() const
{
    if (!m_associatedElementsAreDirty)
        return m_associatedElements;
    HTMLFormElement* mutableThis = const_cast<HTMLFormElement*>(this);
    Node* scope = mutableThis;
    if (m_hasElementsAssociatedByParser)
        scope = &NodeTraversal::highestAncestorOrSelf(*mutableThis);
    if (inShadowIncludingDocument() && m_hasElementsAssociatedByFormAttribute)
        scope = &treeScope().rootNode();
    ASSERT(scope);
    collectAssociatedElements(*scope, mutableThis->m_associatedElements);
    mutableThis->m_associatedElementsAreDirty = false;
    return m_associatedElements;
}

void HTMLFormElement::collectImageElements(Node& root, HeapVector<Member<HTMLImageElement>>& elements)
{
    elements.clear();
    for (HTMLImageElement& image : Traversal<HTMLImageElement>::startsAfter(root)) {
        if (image.formOwner() == this)
            elements.append(&image);
    }
}

const HeapVector<Member<HTMLImageElement>>& HTMLFormElement::imageElements()
{
    if (!m_imageElementsAreDirty)
        return m_imageElements;
    collectImageElements(m_hasElementsAssociatedByParser ? NodeTraversal::highestAncestorOrSelf(*this) : *this, m_imageElements);
    m_imageElementsAreDirty = false;
    return m_imageElements;
}

String HTMLFormElement::name() const
{
    return getNameAttribute();
}

bool HTMLFormElement::noValidate() const
{
    return fastHasAttribute(novalidateAttr);
}

// FIXME: This function should be removed because it does not do the same thing as the
// JavaScript binding for action, which treats action as a URL attribute. Last time I
// (Darin Adler) removed this, someone added it back, so I am leaving it in for now.
const AtomicString& HTMLFormElement::action() const
{
    return getAttribute(actionAttr);
}

void HTMLFormElement::setEnctype(const AtomicString& value)
{
    setAttribute(enctypeAttr, value);
}

String HTMLFormElement::method() const
{
    return FormSubmission::Attributes::methodString(m_attributes.method());
}

void HTMLFormElement::setMethod(const AtomicString& value)
{
    setAttribute(methodAttr, value);
}

HTMLFormControlElement* HTMLFormElement::findDefaultButton() const
{
    for (const auto& element : associatedElements()) {
        if (!element->isFormControlElement())
            continue;
        HTMLFormControlElement* control = toHTMLFormControlElement(element);
        if (control->canBeSuccessfulSubmitButton())
            return control;
    }
    return nullptr;
}

bool HTMLFormElement::checkValidity()
{
    return !checkInvalidControlsAndCollectUnhandled(0, CheckValidityDispatchInvalidEvent);
}

bool HTMLFormElement::checkInvalidControlsAndCollectUnhandled(HeapVector<Member<HTMLFormControlElement>>* unhandledInvalidControls, CheckValidityEventBehavior eventBehavior)
{
    // Copy associatedElements because event handlers called from
    // HTMLFormControlElement::checkValidity() might change associatedElements.
    const FormAssociatedElement::List& associatedElements = this->associatedElements();
    HeapVector<Member<FormAssociatedElement>> elements;
    elements.reserveCapacity(associatedElements.size());
    for (unsigned i = 0; i < associatedElements.size(); ++i)
        elements.append(associatedElements[i]);
    int invalidControlsCount = 0;
    for (unsigned i = 0; i < elements.size(); ++i) {
        if (elements[i]->form() == this && elements[i]->isFormControlElement()) {
            HTMLFormControlElement* control = toHTMLFormControlElement(elements[i].get());
            if (control->isSubmittableElement() && !control->checkValidity(unhandledInvalidControls, eventBehavior) && control->formOwner() == this) {
                ++invalidControlsCount;
                if (!unhandledInvalidControls && eventBehavior == CheckValidityDispatchNoEvent)
                    return true;
            }
        }
    }
    return invalidControlsCount;
}

bool HTMLFormElement::reportValidity()
{
    return validateInteractively();
}

Element* HTMLFormElement::elementFromPastNamesMap(const AtomicString& pastName)
{
    if (pastName.isEmpty() || !m_pastNamesMap)
        return 0;
    Element* element = m_pastNamesMap->get(pastName);
#if ENABLE(ASSERT)
    if (!element)
        return 0;
    ASSERT_WITH_SECURITY_IMPLICATION(toHTMLElement(element)->formOwner() == this);
    if (isHTMLImageElement(*element)) {
        ASSERT_WITH_SECURITY_IMPLICATION(imageElements().find(element) != kNotFound);
    } else if (isHTMLObjectElement(*element)) {
        ASSERT_WITH_SECURITY_IMPLICATION(associatedElements().find(toHTMLObjectElement(element)) != kNotFound);
    } else {
        ASSERT_WITH_SECURITY_IMPLICATION(associatedElements().find(toHTMLFormControlElement(element)) != kNotFound);
    }
#endif
    return element;
}

void HTMLFormElement::addToPastNamesMap(Element* element, const AtomicString& pastName)
{
    if (pastName.isEmpty())
        return;
    if (!m_pastNamesMap)
        m_pastNamesMap = new PastNamesMap;
    m_pastNamesMap->set(pastName, element);
}

void HTMLFormElement::removeFromPastNamesMap(HTMLElement& element)
{
    if (!m_pastNamesMap)
        return;
    for (auto& it : *m_pastNamesMap) {
        if (it.value == &element) {
            it.value = nullptr;
            // Keep looping. Single element can have multiple names.
        }
    }
}

void HTMLFormElement::getNamedElements(const AtomicString& name, HeapVector<Member<Element>>& namedItems)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/forms.html#dom-form-nameditem
    elements()->namedItems(name, namedItems);

    Element* elementFromPast = elementFromPastNamesMap(name);
    if (namedItems.size() && namedItems.first() != elementFromPast) {
        addToPastNamesMap(namedItems.first().get(), name);
    } else if (elementFromPast && namedItems.isEmpty()) {
        namedItems.append(elementFromPast);
        UseCounter::count(document(), UseCounter::FormNameAccessForPastNamesMap);
    }
}

bool HTMLFormElement::shouldAutocomplete() const
{
    return !equalIgnoringCase(fastGetAttribute(autocompleteAttr), "off");
}

void HTMLFormElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    document().formController().restoreControlStateIn(*this);
    m_didFinishParsingChildren = true;
}

void HTMLFormElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    m_wasDemoted = static_cast<const HTMLFormElement&>(source).m_wasDemoted;
    HTMLElement::copyNonAttributePropertiesFromElement(source);
}

void HTMLFormElement::anonymousNamedGetter(const AtomicString& name, RadioNodeListOrElement& returnValue)
{
    // Call getNamedElements twice, first time check if it has a value
    // and let HTMLFormElement update its cache.
    // See issue: 867404
    {
        HeapVector<Member<Element>> elements;
        getNamedElements(name, elements);
        if (elements.isEmpty())
            return;
    }

    // Second call may return different results from the first call,
    // but if the first the size cannot be zero.
    HeapVector<Member<Element>> elements;
    getNamedElements(name, elements);
    ASSERT(!elements.isEmpty());

    bool onlyMatchImg = !elements.isEmpty() && isHTMLImageElement(*elements.first());
    if (onlyMatchImg) {
        UseCounter::count(document(), UseCounter::FormNameAccessForImageElement);
        // The following code has performance impact, but it should be small
        // because <img> access via <form> name getter is rarely used.
        for (auto& element : elements) {
            if (isHTMLImageElement(*element) && !element->isDescendantOf(this)) {
                UseCounter::count(document(), UseCounter::FormNameAccessForNonDescendantImageElement);
                break;
            }
        }
    }
    if (elements.size() == 1) {
        returnValue.setElement(elements.at(0));
        return;
    }

    returnValue.setRadioNodeList(radioNodeList(name, onlyMatchImg));
}

void HTMLFormElement::setDemoted(bool demoted)
{
    if (demoted)
        UseCounter::count(document(), UseCounter::DemotedFormElement);
    m_wasDemoted = demoted;
}

void HTMLFormElement::invalidateDefaultButtonStyle() const
{
    for (const auto& control : associatedElements()) {
        if (!control->isFormControlElement())
            continue;
        if (toHTMLFormControlElement(control)->canBeSuccessfulSubmitButton())
            toHTMLFormControlElement(control)->pseudoStateChanged(CSSSelector::PseudoDefault);
    }
}

} // namespace blink
