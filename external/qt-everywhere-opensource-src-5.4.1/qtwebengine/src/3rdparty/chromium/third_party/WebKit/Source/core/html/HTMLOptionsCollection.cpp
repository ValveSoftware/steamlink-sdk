/*
 * Copyright (C) 2006, 2011, 2012 Apple Computer, Inc.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
#include "core/html/HTMLOptionsCollection.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NamedNodesCollection.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"

namespace WebCore {

HTMLOptionsCollection::HTMLOptionsCollection(ContainerNode& select)
    : HTMLCollection(select, SelectOptions, DoesNotOverrideItemAfter)
{
    ASSERT(isHTMLSelectElement(select));
    ScriptWrappable::init(this);
}

void HTMLOptionsCollection::supportedPropertyNames(Vector<String>& names)
{
    // As per http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmloptionscollection:
    // The supported property names consist of the non-empty values of all the id and name attributes of all the elements
    // represented by the collection, in tree order, ignoring later duplicates, with the id of an element preceding its
    // name if it contributes both, they differ from each other, and neither is the duplicate of an earlier entry.
    HashSet<AtomicString> existingNames;
    unsigned length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        Element* element = item(i);
        ASSERT(element);
        const AtomicString& idAttribute = element->getIdAttribute();
        if (!idAttribute.isEmpty()) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(idAttribute);
            if (addResult.isNewEntry)
                names.append(idAttribute);
        }
        const AtomicString& nameAttribute = element->getNameAttribute();
        if (!nameAttribute.isEmpty()) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(nameAttribute);
            if (addResult.isNewEntry)
                names.append(nameAttribute);
        }
    }
}

PassRefPtrWillBeRawPtr<HTMLOptionsCollection> HTMLOptionsCollection::create(ContainerNode& select, CollectionType)
{
    return adoptRefWillBeNoop(new HTMLOptionsCollection(select));
}

void HTMLOptionsCollection::add(PassRefPtrWillBeRawPtr<HTMLOptionElement> element, ExceptionState& exceptionState)
{
    add(element, length(), exceptionState);
}

void HTMLOptionsCollection::add(PassRefPtrWillBeRawPtr<HTMLOptionElement> element, int index, ExceptionState& exceptionState)
{
    HTMLOptionElement* newOption = element.get();

    if (!newOption) {
        exceptionState.throwTypeError("The element provided was not an HTMLOptionElement.");
        return;
    }

    if (index < -1) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is less than -1.");
        return;
    }

    HTMLSelectElement& select = toHTMLSelectElement(ownerNode());

    if (index == -1 || unsigned(index) >= length())
        select.add(newOption, 0, exceptionState);
    else
        select.addBeforeOptionAtIndex(newOption, index, exceptionState);

    ASSERT(!exceptionState.hadException());
}

void HTMLOptionsCollection::remove(int index)
{
    toHTMLSelectElement(ownerNode()).remove(index);
}

void HTMLOptionsCollection::remove(HTMLOptionElement* option)
{
    return remove(option->index());
}

int HTMLOptionsCollection::selectedIndex() const
{
    return toHTMLSelectElement(ownerNode()).selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index)
{
    toHTMLSelectElement(ownerNode()).setSelectedIndex(index);
}

void HTMLOptionsCollection::setLength(unsigned length, ExceptionState& exceptionState)
{
    toHTMLSelectElement(ownerNode()).setLength(length, exceptionState);
}

void HTMLOptionsCollection::namedGetter(const AtomicString& name, bool& returnValue0Enabled, RefPtrWillBeRawPtr<NodeList>& returnValue0, bool& returnValue1Enabled, RefPtrWillBeRawPtr<Element>& returnValue1)
{
    WillBeHeapVector<RefPtrWillBeMember<Element> > namedItems;
    this->namedItems(name, namedItems);

    if (!namedItems.size())
        return;

    if (namedItems.size() == 1) {
        returnValue1Enabled = true;
        returnValue1 = namedItems.at(0);
        return;
    }

    // FIXME: The spec and Firefox do not return a NodeList. They always return the first matching Element.
    returnValue0Enabled = true;
    returnValue0 = NamedNodesCollection::create(namedItems);
}

bool HTMLOptionsCollection::anonymousIndexedSetter(unsigned index, PassRefPtrWillBeRawPtr<HTMLOptionElement> value, ExceptionState& exceptionState)
{
    HTMLSelectElement& base = toHTMLSelectElement(ownerNode());
    if (!value) { // undefined or null
        base.remove(index);
        return true;
    }
    base.setOption(index, value.get(), exceptionState);
    return true;
}

} //namespace

