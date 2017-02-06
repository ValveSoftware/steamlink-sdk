/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "core/html/HTMLLabelElement.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionController.h"
#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormAssociatedElement.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"

namespace blink {

using namespace HTMLNames;

inline HTMLLabelElement::HTMLLabelElement(Document& document)
    : HTMLElement(labelTag, document)
    , m_processingClick(false)
{
}

HTMLLabelElement* HTMLLabelElement::create(Document& document)
{
    return new HTMLLabelElement(document);
}

LabelableElement* HTMLLabelElement::control() const
{
    const AtomicString& controlId = getAttribute(forAttr);
    if (controlId.isNull()) {
        // Search the children and descendants of the label element for a form element.
        // per http://dev.w3.org/html5/spec/Overview.html#the-label-element
        // the form element must be "labelable form-associated element".
        for (LabelableElement& element : Traversal<LabelableElement>::descendantsOf(*this)) {
            if (element.supportLabels()) {
                if (!element.isFormControlElement())
                    UseCounter::count(document(), UseCounter::HTMLLabelElementControlForNonFormAssociatedElement);
                return &element;
            }
        }
        return nullptr;
    }

    if (Element* element = treeScope().getElementById(controlId)) {
        if (isLabelableElement(*element) && toLabelableElement(*element).supportLabels()) {
            if (!element->isFormControlElement())
                UseCounter::count(document(), UseCounter::HTMLLabelElementControlForNonFormAssociatedElement);
            return toLabelableElement(element);
        }
    }

    return nullptr;
}

HTMLFormElement* HTMLLabelElement::form() const
{
    if (LabelableElement* control = this->control())
        return control->isFormControlElement() ? toHTMLFormControlElement(control)->form() : nullptr;
    return nullptr;
}

void HTMLLabelElement::setActive(bool down)
{
    if (down != active()) {
        // Update our status first.
        HTMLElement::setActive(down);
    }

    // Also update our corresponding control.
    HTMLElement* controlElement = control();
    if (controlElement && controlElement->active() != active())
        controlElement->setActive(active());
}

void HTMLLabelElement::setHovered(bool over)
{
    if (over != hovered()) {
        // Update our status first.
        HTMLElement::setHovered(over);
    }

    // Also update our corresponding control.
    HTMLElement* element = control();
    if (element && element->hovered() != hovered())
        element->setHovered(hovered());
}

bool HTMLLabelElement::isInteractiveContent() const
{
    return true;
}

bool HTMLLabelElement::isInInteractiveContent(Node* node) const
{
    if (!isShadowIncludingInclusiveAncestorOf(node))
        return false;
    while (node && this != node) {
        if (node->isHTMLElement() && toHTMLElement(node)->isInteractiveContent())
            return true;
        node = node->parentOrShadowHostNode();
    }
    return false;
}

void HTMLLabelElement::defaultEventHandler(Event* evt)
{
    if (evt->type() == EventTypeNames::click && !m_processingClick) {
        HTMLElement* element = control();

        // If we can't find a control or if the control received the click
        // event, then there's no need for us to do anything.
        if (!element || (evt->target() && element->isShadowIncludingInclusiveAncestorOf(evt->target()->toNode())))
            return;

        if (evt->target() && isInInteractiveContent(evt->target()->toNode()))
            return;

        //   Behaviour of label element is as follows:
        //     - If there is double click, two clicks will be passed to control
        //       element. Control element will *not* be focused.
        //     - If there is selection of label element by dragging, no click
        //       event is passed. Also, no focus on control element.
        //     - If there is already a selection on label element and then label
        //       is clicked, then click event is passed to control element and
        //       control element is focused.

        bool isLabelTextSelected = false;

        // If the click is not simulated and the text of the label element
        // is selected by dragging over it, then return without passing the
        // click event to control element.
        // Note: check if it is a MouseEvent because a click event may
        // not be an instance of a MouseEvent if created by document.createEvent().
        if (evt->isMouseEvent() && toMouseEvent(evt)->hasPosition()) {
            if (LocalFrame* frame = document().frame()) {
                // Check if there is a selection and click is not on the
                // selection.
                if (layoutObject() && layoutObject()->isSelectable() && frame->selection().isRange() && !frame->eventHandler().selectionController().mouseDownWasSingleClickInSelection())
                    isLabelTextSelected = true;
                // If selection is there and is single click i.e. text is
                // selected by dragging over label text, then return.
                // Click count >=2, meaning double click or triple click,
                // should pass click event to control element.
                // Only in case of drag, *neither* we pass the click event,
                // *nor* we focus the control element.
                if (isLabelTextSelected && frame->eventHandler().clickCount() == 1)
                    return;
            }
        }

        m_processingClick = true;

        document().updateStyleAndLayoutIgnorePendingStylesheets();
        if (element->isMouseFocusable()) {
            // If the label is *not* selected, or if the click happened on
            // selection of label, only then focus the control element.
            // In case of double click or triple click, selection will be there,
            // so do not focus the control element.
            if (!isLabelTextSelected)
                element->focus(FocusParams(SelectionBehaviorOnFocus::Restore, WebFocusTypeMouse, nullptr));
        }

        // Click the corresponding control.
        element->dispatchSimulatedClick(evt);

        m_processingClick = false;

        evt->setDefaultHandled();
    }

    HTMLElement::defaultEventHandler(evt);
}

bool HTMLLabelElement::willRespondToMouseClickEvents()
{
    if (control() && control()->willRespondToMouseClickEvents())
        return true;

    return HTMLElement::willRespondToMouseClickEvents();
}

void HTMLLabelElement::focus(const FocusParams& params)
{
    document().updateStyleAndLayoutTreeForNode(this);
    if (isFocusable()) {
        HTMLElement::focus(params);
        return;
    }
    // To match other browsers, always restore previous selection.
    if (HTMLElement* element = control())
        element->focus(FocusParams(SelectionBehaviorOnFocus::Restore, params.type, params.sourceCapabilities));
}

void HTMLLabelElement::accessKeyAction(bool sendMouseEvents)
{
    if (HTMLElement* element = control())
        element->accessKeyAction(sendMouseEvents);
    else
        HTMLElement::accessKeyAction(sendMouseEvents);
}

Node::InsertionNotificationRequest HTMLLabelElement::insertedInto(ContainerNode* insertionPoint)
{
    InsertionNotificationRequest result = HTMLElement::insertedInto(insertionPoint);

    // Trigger for elements outside of forms.
    if (!formOwner() && insertionPoint->inShadowIncludingDocument())
        document().didAssociateFormControl(this);

    return result;
}

void HTMLLabelElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    document().removeFormAssociation(this);
}

} // namespace blink
