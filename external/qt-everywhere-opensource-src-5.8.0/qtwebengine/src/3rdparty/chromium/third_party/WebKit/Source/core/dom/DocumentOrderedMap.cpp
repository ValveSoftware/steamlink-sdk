/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "core/dom/DocumentOrderedMap.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/TreeScope.h"
#include "core/html/HTMLMapElement.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

using namespace HTMLNames;


DocumentOrderedMap* DocumentOrderedMap::create()
{
    return new DocumentOrderedMap;
}

DocumentOrderedMap::DocumentOrderedMap()
{
}

#if DCHECK_IS_ON()
static int s_removeScopeLevel = 0;

DocumentOrderedMap::RemoveScope::RemoveScope()
{
    s_removeScopeLevel++;
}

DocumentOrderedMap::RemoveScope::~RemoveScope()
{
    DCHECK(s_removeScopeLevel);
    s_removeScopeLevel--;
}
#endif

inline bool keyMatchesId(const AtomicString& key, const Element& element)
{
    return element.getIdAttribute() == key;
}

inline bool keyMatchesMapName(const AtomicString& key, const Element& element)
{
    return isHTMLMapElement(element) && toHTMLMapElement(element).getName() == key;
}

inline bool keyMatchesSlotName(const AtomicString& key, const Element& element)
{
    return isHTMLSlotElement(element) && toHTMLSlotElement(element).name() == key;
}

inline bool keyMatchesLowercasedMapName(const AtomicString& key, const Element& element)
{
    return isHTMLMapElement(element) && toHTMLMapElement(element).getName().lower() == key;
}

void DocumentOrderedMap::add(const AtomicString& key, Element* element)
{
    DCHECK(key);
    DCHECK(element);

    Map::AddResult addResult = m_map.add(key, new MapEntry(element));
    if (addResult.isNewEntry)
        return;

    Member<MapEntry>& entry = addResult.storedValue->value;
    DCHECK(entry->count);
    entry->element = nullptr;
    entry->count++;
    entry->orderedList.clear();
}

void DocumentOrderedMap::remove(const AtomicString& key, Element* element)
{
    DCHECK(key);
    DCHECK(element);

    Map::iterator it = m_map.find(key);
    if (it == m_map.end())
        return;

    Member<MapEntry>& entry = it->value;
    DCHECK(entry->count);
    if (entry->count == 1) {
        DCHECK(!entry->element || entry->element == element);
        m_map.remove(it);
    } else {
        if (entry->element == element) {
            DCHECK(entry->orderedList.isEmpty() || entry->orderedList.first() == element);
            entry->element = entry->orderedList.size() > 1 ? entry->orderedList[1] : nullptr;
        }
        entry->count--;
        entry->orderedList.clear();
    }
}

template<bool keyMatches(const AtomicString&, const Element&)>
inline Element* DocumentOrderedMap::get(const AtomicString& key, const TreeScope* scope) const
{
    DCHECK(key);
    DCHECK(scope);

    MapEntry* entry = m_map.get(key);
    if (!entry)
        return 0;

    DCHECK(entry->count);
    if (entry->element)
        return entry->element;

    // Iterate to find the node that matches. Nothing will match iff an element
    // with children having duplicate IDs is being removed -- the tree traversal
    // will be over an updated tree not having that subtree. In all other cases,
    // a match is expected.
    for (Element& element : ElementTraversal::startsAfter(scope->rootNode())) {
        if (!keyMatches(key, element))
            continue;
        entry->element = &element;
        return &element;
    }
    // As get()/getElementById() can legitimately be called while handling element
    // removals, allow failure iff we're in the scope of node removals.
#if DCHECK_IS_ON()
    DCHECK(s_removeScopeLevel);
#endif
    return 0;
}

Element* DocumentOrderedMap::getElementById(const AtomicString& key, const TreeScope* scope) const
{
    return get<keyMatchesId>(key, scope);
}

const HeapVector<Member<Element>>& DocumentOrderedMap::getAllElementsById(const AtomicString& key, const TreeScope* scope) const
{
    DCHECK(key);
    DCHECK(scope);
    DEFINE_STATIC_LOCAL(HeapVector<Member<Element>>, emptyVector, (new HeapVector<Member<Element>>));

    Map::iterator it = m_map.find(key);
    if (it == m_map.end())
        return emptyVector;

    Member<MapEntry>& entry = it->value;
    DCHECK(entry->count);

    if (entry->orderedList.isEmpty()) {
        entry->orderedList.reserveCapacity(entry->count);
        for (Element* element = entry->element ? entry->element.get() : ElementTraversal::firstWithin(scope->rootNode()); entry->orderedList.size() < entry->count; element = ElementTraversal::next(*element)) {
            DCHECK(element);
            if (!keyMatchesId(key, *element))
                continue;
            entry->orderedList.uncheckedAppend(element);
        }
        if (!entry->element)
            entry->element = entry->orderedList.first();
    }

    return entry->orderedList;
}

Element* DocumentOrderedMap::getElementByMapName(const AtomicString& key, const TreeScope* scope) const
{
    return get<keyMatchesMapName>(key, scope);
}

// TODO(hayato): Template get<> by return type.
HTMLSlotElement* DocumentOrderedMap::getSlotByName(const AtomicString& key, const TreeScope* scope) const
{
    if (Element* slot = get<keyMatchesSlotName>(key, scope)) {
        DCHECK(isHTMLSlotElement(slot));
        return toHTMLSlotElement(slot);
    }
    return nullptr;
}

Element* DocumentOrderedMap::getElementByLowercasedMapName(const AtomicString& key, const TreeScope* scope) const
{
    return get<keyMatchesLowercasedMapName>(key, scope);
}

DEFINE_TRACE(DocumentOrderedMap)
{
    visitor->trace(m_map);
}

DEFINE_TRACE(DocumentOrderedMap::MapEntry)
{
    visitor->trace(element);
    visitor->trace(orderedList);
}

} // namespace blink
