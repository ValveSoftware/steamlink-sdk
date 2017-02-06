// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementTestHelpers_h
#define CustomElementTestHelpers_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "core/html/HTMLDocument.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

#include <utility>
#include <vector>

namespace blink {

class CreateElement {
    STACK_ALLOCATED()
public:
    CreateElement(const AtomicString& localName)
        : m_namespaceURI(HTMLNames::xhtmlNamespaceURI)
        , m_localName(localName)
    {
    }

    CreateElement& inDocument(Document* document)
    {
        m_document = document;
        return *this;
    }

    CreateElement& inNamespace(const AtomicString& uri)
    {
        m_namespaceURI = uri;
        return *this;
    }

    CreateElement& withId(const AtomicString& id)
    {
        m_attributes.push_back(std::make_pair(HTMLNames::idAttr, id));
        return *this;
    }

    CreateElement& withIsAttribute(const AtomicString& value)
    {
        m_attributes.push_back(std::make_pair(HTMLNames::isAttr, value));
        return *this;
    }

    operator Element*() const
    {
        Document* document = m_document.get();
        if (!document)
            document = HTMLDocument::create();
        NonThrowableExceptionState noExceptions;
        Element* element = document->createElementNS(
            m_namespaceURI,
            m_localName,
            noExceptions);
        for (const auto& attribute : m_attributes)
            element->setAttribute(attribute.first, attribute.second);
        return element;
    }

private:
    Member<Document> m_document;
    AtomicString m_namespaceURI;
    AtomicString m_localName;
    std::vector<std::pair<QualifiedName, AtomicString>> m_attributes;
};

} // namespace blink

#endif // CustomElementTestHelpers_h
