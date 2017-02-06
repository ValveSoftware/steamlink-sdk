/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/TextControlInnerElements.h"

#include "core/HTMLNames.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/events/MouseEvent.h"
#include "core/events/TextEvent.h"
#include "core/events/TextEventInputType.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/layout/api/LayoutTextControlItem.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

using namespace HTMLNames;

TextControlInnerContainer::TextControlInnerContainer(Document& document)
    : HTMLDivElement(document)
{
}

TextControlInnerContainer* TextControlInnerContainer::create(Document& document)
{
    TextControlInnerContainer* element = new TextControlInnerContainer(document);
    element->setAttribute(idAttr, ShadowElementNames::textFieldContainer());
    return element;
}

LayoutObject* TextControlInnerContainer::createLayoutObject(const ComputedStyle&)
{
    return new LayoutTextControlInnerContainer(this);
}

// ---------------------------

EditingViewPortElement::EditingViewPortElement(Document& document)
    : HTMLDivElement(document)
{
    setHasCustomStyleCallbacks();
}

EditingViewPortElement* EditingViewPortElement::create(Document& document)
{
    EditingViewPortElement* element = new EditingViewPortElement(document);
    element->setAttribute(idAttr, ShadowElementNames::editingViewPort());
    return element;
}

PassRefPtr<ComputedStyle> EditingViewPortElement::customStyleForLayoutObject()
{
    // FXIME: Move these styles to html.css.

    RefPtr<ComputedStyle> style = ComputedStyle::create();
    style->inheritFrom(shadowHost()->computedStyleRef());

    style->setFlexGrow(1);
    style->setMinWidth(Length(0, Fixed));
    style->setDisplay(BLOCK);
    style->setDirection(LTR);

    // We don't want the shadow dom to be editable, so we set this block to
    // read-only in case the input itself is editable.
    style->setUserModify(READ_ONLY);
    style->setUnique();

    return style.release();
}

// ---------------------------

inline TextControlInnerEditorElement::TextControlInnerEditorElement(Document& document)
    : HTMLDivElement(document)
{
    setHasCustomStyleCallbacks();
}

TextControlInnerEditorElement* TextControlInnerEditorElement::create(Document& document)
{
    TextControlInnerEditorElement* element = new TextControlInnerEditorElement(document);
    element->setAttribute(idAttr, ShadowElementNames::innerEditor());
    return element;
}

void TextControlInnerEditorElement::defaultEventHandler(Event* event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event->isBeforeTextInsertedEvent() || event->type() == EventTypeNames::webkitEditableContentChanged) {
        Element* shadowAncestor = shadowHost();
        // A TextControlInnerTextElement can have no host if its been detached,
        // but kept alive by an EditCommand. In this case, an undo/redo can
        // cause events to be sent to the TextControlInnerTextElement. To
        // prevent an infinite loop, we must check for this case before sending
        // the event up the chain.
        if (shadowAncestor)
            shadowAncestor->defaultEventHandler(event);
    }
    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

LayoutObject* TextControlInnerEditorElement::createLayoutObject(const ComputedStyle&)
{
    return new LayoutTextControlInnerBlock(this);
}

PassRefPtr<ComputedStyle> TextControlInnerEditorElement::customStyleForLayoutObject()
{
    LayoutObject* parentLayoutObject = shadowHost()->layoutObject();
    if (!parentLayoutObject || !parentLayoutObject->isTextControl())
        return originalStyleForLayoutObject();
    LayoutTextControlItem textControlLayoutItem = LayoutTextControlItem(toLayoutTextControl(parentLayoutObject));
    RefPtr<ComputedStyle> innerEditorStyle = textControlLayoutItem.createInnerEditorStyle(textControlLayoutItem.styleRef());
    // Using StyleAdjuster::adjustComputedStyle updates unwanted style. We'd like
    // to apply only editing-related.
    StyleAdjuster::adjustStyleForEditing(*innerEditorStyle);
    return innerEditorStyle.release();
}

// ----------------------------

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document& document)
    : HTMLDivElement(document)
    , m_capturing(false)
{
}

SearchFieldCancelButtonElement* SearchFieldCancelButtonElement::create(Document& document)
{
    SearchFieldCancelButtonElement* element = new SearchFieldCancelButtonElement(document);
    element->setShadowPseudoId(AtomicString("-webkit-search-cancel-button"));
    element->setAttribute(idAttr, ShadowElementNames::clearButton());
    return element;
}

void SearchFieldCancelButtonElement::detach(const AttachContext& context)
{
    if (m_capturing) {
        if (LocalFrame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsNode(nullptr);
    }
    HTMLDivElement::detach(context);
}


void SearchFieldCancelButtonElement::defaultEventHandler(Event* event)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    HTMLInputElement* input(toHTMLInputElement(shadowHost()));
    if (!input || input->isDisabledOrReadOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }


    if (event->type() == EventTypeNames::click && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        input->setValueForUser("");
        input->setAutofilled(false);
        input->onSearch();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = toHTMLInputElement(shadowHost());
    if (input && !input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

} // namespace blink
