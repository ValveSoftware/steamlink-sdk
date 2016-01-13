/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "core/dom/Attr.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/UseCounter.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

Attr::Attr(Element& element, const QualifiedName& name)
    : ContainerNode(&element.document())
    , m_element(&element)
    , m_name(name)
    , m_ignoreChildrenChanged(0)
{
    ScriptWrappable::init(this);
}

Attr::Attr(Document& document, const QualifiedName& name, const AtomicString& standaloneValue)
    : ContainerNode(&document)
    , m_element(nullptr)
    , m_name(name)
    , m_standaloneValueOrAttachedLocalName(standaloneValue)
    , m_ignoreChildrenChanged(0)
{
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<Attr> Attr::create(Element& element, const QualifiedName& name)
{
    RefPtrWillBeRawPtr<Attr> attr = adoptRefWillBeNoop(new Attr(element, name));
    attr->createTextChild();
    return attr.release();
}

PassRefPtrWillBeRawPtr<Attr> Attr::create(Document& document, const QualifiedName& name, const AtomicString& value)
{
    RefPtrWillBeRawPtr<Attr> attr = adoptRefWillBeNoop(new Attr(document, name, value));
    attr->createTextChild();
    return attr.release();
}

Attr::~Attr()
{
}

const QualifiedName Attr::qualifiedName() const
{
    if (m_element && !m_standaloneValueOrAttachedLocalName.isNull()) {
        // In the unlikely case the Element attribute has a local name
        // that differs by case, construct the qualified name based on
        // it. This is the qualified name that must be used when
        // looking up the attribute on the element.
        return QualifiedName(m_name.prefix(), m_standaloneValueOrAttachedLocalName, m_name.namespaceURI());
    }

    return m_name;
}

void Attr::createTextChild()
{
#if !ENABLE(OILPAN)
    ASSERT(refCount());
#endif
    if (!value().isEmpty()) {
        RefPtrWillBeRawPtr<Text> textNode = document().createTextNode(value().string());

        // This does everything appendChild() would do in this situation (assuming m_ignoreChildrenChanged was set),
        // but much more efficiently.
        textNode->setParentOrShadowHostNode(this);
        treeScope().adoptIfNeeded(*textNode);
        setFirstChild(textNode.get());
        setLastChild(textNode.get());
    }
}

void Attr::setValue(const AtomicString& value)
{
    EventQueueScope scope;
    m_ignoreChildrenChanged++;
    removeChildren();
    if (m_element)
        elementAttribute().setValue(value);
    else
        m_standaloneValueOrAttachedLocalName = value;
    createTextChild();
    m_ignoreChildrenChanged--;

    QualifiedName name = qualifiedName();
    invalidateNodeListCachesInAncestors(&name, m_element);
}

void Attr::setValueInternal(const AtomicString& value)
{
    if (m_element)
        m_element->willModifyAttribute(qualifiedName(), this->value(), value);

    setValue(value);

    if (m_element)
        m_element->didModifyAttribute(qualifiedName(), value);
}

const AtomicString& Attr::valueForBindings() const
{
    UseCounter::count(document(), UseCounter::AttrGetValue);
    return value();
}

void Attr::setValueForBindings(const AtomicString& value)
{
    UseCounter::count(document(), UseCounter::AttrSetValue);
    if (m_element)
        UseCounter::count(document(), UseCounter::AttrSetValueWithElement);
    setValueInternal(value);
}

void Attr::setNodeValue(const String& v)
{
    // Attr uses AtomicString type for its value to save memory as there
    // is duplication among Elements' attributes values.
    setValueInternal(AtomicString(v));
}

PassRefPtrWillBeRawPtr<Node> Attr::cloneNode(bool /*deep*/)
{
    RefPtrWillBeRawPtr<Attr> clone = adoptRefWillBeNoop(new Attr(document(), m_name, value()));
    cloneChildNodes(clone.get());
    return clone.release();
}

// DOM Section 1.1.1
bool Attr::childTypeAllowed(NodeType type) const
{
    return TEXT_NODE == type;
}

void Attr::childrenChanged(bool, Node*, Node*, int)
{
    if (m_ignoreChildrenChanged > 0)
        return;

    QualifiedName name = qualifiedName();
    invalidateNodeListCachesInAncestors(&name, m_element);

    StringBuilder valueBuilder;
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            valueBuilder.append(toText(n)->data());
    }

    AtomicString newValue = valueBuilder.toAtomicString();
    if (m_element)
        m_element->willModifyAttribute(qualifiedName(), value(), newValue);

    if (m_element)
        elementAttribute().setValue(newValue);
    else
        m_standaloneValueOrAttachedLocalName = newValue;

    if (m_element)
        m_element->attributeChanged(qualifiedName(), newValue);
}

const AtomicString& Attr::value() const
{
    if (m_element)
        return m_element->getAttribute(qualifiedName());
    return m_standaloneValueOrAttachedLocalName;
}

Attribute& Attr::elementAttribute()
{
    ASSERT(m_element);
    ASSERT(m_element->elementData());
    return *m_element->ensureUniqueElementData().findAttributeByName(qualifiedName());
}

void Attr::detachFromElementWithValue(const AtomicString& value)
{
    ASSERT(m_element);
    m_standaloneValueOrAttachedLocalName = value;
    m_element = nullptr;
}

void Attr::attachToElement(Element* element, const AtomicString& attachedLocalName)
{
    ASSERT(!m_element);
    m_element = element;
    m_standaloneValueOrAttachedLocalName = attachedLocalName;
}

void Attr::trace(Visitor* visitor)
{
    visitor->trace(m_element);
    ContainerNode::trace(visitor);
}

}
