/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
 */

#include "config.h"
#include "core/dom/NamedNodeMap.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Attr.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"

namespace WebCore {

using namespace HTMLNames;

#if !ENABLE(OILPAN)
void NamedNodeMap::ref()
{
    m_element->ref();
}

void NamedNodeMap::deref()
{
    m_element->deref();
}
#endif

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::getNamedItem(const AtomicString& name) const
{
    return m_element->getAttributeNode(name);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::getNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return m_element->getAttributeNodeNS(namespaceURI, localName);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::removeNamedItem(const AtomicString& name, ExceptionState& exceptionState)
{
    size_t index = m_element->hasAttributes() ? m_element->findAttributeIndexByName(name, m_element->shouldIgnoreAttributeCase()) : kNotFound;
    if (index == kNotFound) {
        exceptionState.throwDOMException(NotFoundError, "No item with name '" + name + "' was found.");
        return nullptr;
    }
    return m_element->detachAttribute(index);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::removeNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName, ExceptionState& exceptionState)
{
    size_t index = m_element->hasAttributes() ? m_element->findAttributeIndexByName(QualifiedName(nullAtom, localName, namespaceURI)) : kNotFound;
    if (index == kNotFound) {
        exceptionState.throwDOMException(NotFoundError, "No item with name '" + namespaceURI + "::" + localName + "' was found.");
        return nullptr;
    }
    return m_element->detachAttribute(index);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::setNamedItem(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(NotFoundError, "The node provided was null.");
        return nullptr;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!node->isAttributeNode()) {
        exceptionState.throwDOMException(HierarchyRequestError, "The node provided is not an attribute node.");
        return nullptr;
    }

    return m_element->setAttributeNode(toAttr(node), exceptionState);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::setNamedItemNS(Node* node, ExceptionState& exceptionState)
{
    return setNamedItem(node, exceptionState);
}

PassRefPtrWillBeRawPtr<Node> NamedNodeMap::item(unsigned index) const
{
    if (index >= length())
        return nullptr;
    return m_element->ensureAttr(m_element->attributeAt(index).name());
}

size_t NamedNodeMap::length() const
{
    if (!m_element->hasAttributes())
        return 0;
    return m_element->attributeCount();
}

void NamedNodeMap::trace(Visitor* visitor)
{
    visitor->trace(m_element);
}

} // namespace WebCore
