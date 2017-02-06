/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/dom/custom/V0CustomElement.h"

#include "core/HTMLNames.h"
#include "core/MathMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElementMicrotaskRunQueue.h"
#include "core/dom/custom/V0CustomElementObserver.h"
#include "core/dom/custom/V0CustomElementScheduler.h"

namespace blink {

V0CustomElementMicrotaskImportStep* V0CustomElement::didCreateImport(HTMLImportChild* import)
{
    return V0CustomElementScheduler::scheduleImport(import);
}

void V0CustomElement::didFinishLoadingImport(Document& master)
{
    master.customElementMicrotaskRunQueue()->requestDispatchIfNeeded();
}

Vector<AtomicString>& V0CustomElement::embedderCustomElementNames()
{
    DEFINE_STATIC_LOCAL(Vector<AtomicString>, names, ());
    return names;
}

void V0CustomElement::addEmbedderCustomElementName(const AtomicString& name)
{
    AtomicString lower = name.lower();
    if (isValidName(lower, EmbedderNames))
        return;
    embedderCustomElementNames().append(lower);
}

static inline bool isValidNCName(const AtomicString& name)
{
    if (kNotFound != name.find(':'))
        return false;

    if (!name.getString().is8Bit()) {
        const UChar32 c = name.characters16()[0];
        // These characters comes under CombiningChar in NCName and according to
        // NCName only BaseChar and Ideodgraphic can come as first chars.
        // Also these characters come under Letter_Other in UnicodeData, thats
        // why they pass as valid document name.
        if (c == 0x0B83 || c == 0x0F88 || c == 0x0F89 || c == 0x0F8A || c == 0x0F8B)
            return false;
    }

    return Document::isValidName(name.getString());
}

bool V0CustomElement::isValidName(const AtomicString& name, NameSet validNames)
{
    if ((validNames & EmbedderNames) && kNotFound != embedderCustomElementNames().find(name))
        return Document::isValidName(name);

    if ((validNames & StandardNames) && kNotFound != name.find('-')) {
        DEFINE_STATIC_LOCAL(Vector<AtomicString>, reservedNames, ());
        if (reservedNames.isEmpty()) {
            // FIXME(crbug.com/426605): We should be able to remove this.
            reservedNames.append(MathMLNames::annotation_xmlTag.localName());
        }

        if (kNotFound == reservedNames.find(name))
            return isValidNCName(name);
    }

    return false;
}

void V0CustomElement::define(Element* element, V0CustomElementDefinition* definition)
{
    switch (element->getV0CustomElementState()) {
    case Element::V0NotCustomElement:
    case Element::V0Upgraded:
        ASSERT_NOT_REACHED();
        break;

    case Element::V0WaitingForUpgrade:
        element->setCustomElementDefinition(definition);
        V0CustomElementScheduler::scheduleCallback(definition->callbacks(), element, V0CustomElementLifecycleCallbacks::CreatedCallback);
        break;
    }
}

void V0CustomElement::attributeDidChange(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    DCHECK_EQ(element->getV0CustomElementState(), Element::V0Upgraded);
    V0CustomElementScheduler::scheduleAttributeChangedCallback(element->customElementDefinition()->callbacks(), element, name, oldValue, newValue);
}

void V0CustomElement::didAttach(Element* element, const Document& document)
{
    DCHECK_EQ(element->getV0CustomElementState(), Element::V0Upgraded);
    if (!document.domWindow())
        return;
    V0CustomElementScheduler::scheduleCallback(element->customElementDefinition()->callbacks(), element, V0CustomElementLifecycleCallbacks::AttachedCallback);
}

void V0CustomElement::didDetach(Element* element, const Document& document)
{
    DCHECK_EQ(element->getV0CustomElementState(), Element::V0Upgraded);
    if (!document.domWindow())
        return;
    V0CustomElementScheduler::scheduleCallback(element->customElementDefinition()->callbacks(), element, V0CustomElementLifecycleCallbacks::DetachedCallback);
}

void V0CustomElement::wasDestroyed(Element* element)
{
    switch (element->getV0CustomElementState()) {
    case Element::V0NotCustomElement:
        ASSERT_NOT_REACHED();
        break;

    case Element::V0WaitingForUpgrade:
    case Element::V0Upgraded:
        V0CustomElementObserver::notifyElementWasDestroyed(element);
        break;
    }
}

} // namespace blink
