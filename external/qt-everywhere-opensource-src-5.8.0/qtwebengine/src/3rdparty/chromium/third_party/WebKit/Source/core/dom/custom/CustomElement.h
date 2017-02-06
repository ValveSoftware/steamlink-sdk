// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Document;
class Element;
class HTMLElement;
class QualifiedName;
class CustomElementDefinition;
class CustomElementReaction;
class CustomElementRegistry;

class CORE_EXPORT CustomElement {
    STATIC_ONLY(CustomElement);
public:
    // Retrieves the CustomElementsRegistry for Element, if any. This
    // may be a different object for a given element over its lifetime
    // as it moves between documents.
    static CustomElementsRegistry* registry(const Element&);
    static CustomElementsRegistry* registry(const Document&);

    static CustomElementDefinition* definitionForElement(const Element&);

    static bool isValidName(const AtomicString& name);

    static bool shouldCreateCustomElement(Document&, const AtomicString& localName);
    static bool shouldCreateCustomElement(Document&, const QualifiedName&);

    static HTMLElement* createCustomElementSync(Document&, const AtomicString& localName, ExceptionState&);
    static HTMLElement* createCustomElementSync(Document&, const QualifiedName&, ExceptionState&);
    static HTMLElement* createCustomElementSync(Document&, const QualifiedName&);
    static HTMLElement* createCustomElementAsync(Document&, const QualifiedName&);

    static void enqueue(Element*, CustomElementReaction*);
    static void enqueueConnectedCallback(Element*);
    static void enqueueDisconnectedCallback(Element*);
    static void enqueueAttributeChangedCallback(Element*, const QualifiedName&,
        const AtomicString& oldValue, const AtomicString& newValue);

    static void tryToUpgrade(Element*);

private:
    static HTMLElement* createUndefinedElement(Document&, const QualifiedName&);
};

} // namespace blink

#endif // CustomElement_h
