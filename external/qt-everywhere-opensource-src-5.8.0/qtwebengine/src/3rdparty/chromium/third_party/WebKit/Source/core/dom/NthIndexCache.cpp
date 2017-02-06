// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/NthIndexCache.h"

#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"

namespace blink {

NthIndexCache::NthIndexCache(Document& document)
    : m_document(&document)
#if DCHECK_IS_ON()
    , m_domTreeVersion(document.domTreeVersion())
#endif
{
    document.setNthIndexCache(this);
}

NthIndexCache::~NthIndexCache()
{
#if DCHECK_IS_ON()
    DCHECK_EQ(m_domTreeVersion, m_document->domTreeVersion());
#endif
    m_document->setNthIndexCache(nullptr);
}

namespace {

// Generating the cached nth-index counts when the number of children
// exceeds this count. This number is picked based on testing
// querySelectorAll for :nth-child(3n+2) and :nth-of-type(3n+2) on an
// increasing number of children.

const unsigned kCachedSiblingCountLimit = 32;

unsigned uncachedNthChildIndex(Element& element)
{
    int index = 1;
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling))
        index++;

    return index;
}

unsigned uncachedNthLastChildIndex(Element& element)
{
    int index = 1;
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling))
        ++index;
    return index;
}

unsigned uncachedNthOfTypeIndex(Element& element, unsigned& siblingCount)
{
    int index = 1;
    const QualifiedName& tag = element.tagQName();
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
        if (sibling->tagQName() == tag)
            ++index;
        ++siblingCount;
    }
    return index;
}

unsigned uncachedNthLastOfTypeIndex(Element& element, unsigned& siblingCount)
{
    int index = 1;
    const QualifiedName& tag = element.tagQName();
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
        if (sibling->tagQName() == tag)
            ++index;
        ++siblingCount;
    }
    return index;
}

} // namespace

unsigned NthIndexCache::nthChildIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
    DCHECK(element.parentNode());
    NthIndexCache* nthIndexCache = element.document().nthIndexCache();
    NthIndexData* nthIndexData = nullptr;
    if (nthIndexCache && nthIndexCache->m_parentMap)
        nthIndexData = nthIndexCache->m_parentMap->get(element.parentNode());
    if (nthIndexData)
        return nthIndexData->nthIndex(element);
    unsigned index = uncachedNthChildIndex(element);
    if (nthIndexCache && index > kCachedSiblingCountLimit)
        nthIndexCache->cacheNthIndexDataForParent(element);
    return index;
}

unsigned NthIndexCache::nthLastChildIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
    DCHECK(element.parentNode());
    NthIndexCache* nthIndexCache = element.document().nthIndexCache();
    NthIndexData* nthIndexData = nullptr;
    if (nthIndexCache && nthIndexCache->m_parentMap)
        nthIndexData = nthIndexCache->m_parentMap->get(element.parentNode());
    if (nthIndexData)
        return nthIndexData->nthLastIndex(element);
    unsigned index = uncachedNthLastChildIndex(element);
    if (nthIndexCache && index > kCachedSiblingCountLimit)
        nthIndexCache->cacheNthIndexDataForParent(element);
    return index;
}

NthIndexData* NthIndexCache::nthTypeIndexDataForParent(Element& element) const
{
    DCHECK(element.parentNode());
    if (!m_parentMapForType)
        return nullptr;
    if (const IndexByType* map = m_parentMapForType->get(element.parentNode()))
        return map->get(element.tagName());
    return nullptr;
}

unsigned NthIndexCache::nthOfTypeIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
    NthIndexCache* nthIndexCache = element.document().nthIndexCache();
    if (nthIndexCache) {
        if (NthIndexData* nthIndexData = nthIndexCache->nthTypeIndexDataForParent(element))
            return nthIndexData->nthOfTypeIndex(element);
    }
    unsigned siblingCount = 0;
    unsigned index = uncachedNthOfTypeIndex(element, siblingCount);
    if (nthIndexCache && siblingCount > kCachedSiblingCountLimit)
        nthIndexCache->cacheNthOfTypeIndexDataForParent(element);
    return index;
}

unsigned NthIndexCache::nthLastOfTypeIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
    NthIndexCache* nthIndexCache = element.document().nthIndexCache();
    if (nthIndexCache) {
        if (NthIndexData* nthIndexData = nthIndexCache->nthTypeIndexDataForParent(element))
            return nthIndexData->nthLastOfTypeIndex(element);
    }
    unsigned siblingCount = 0;
    unsigned index = uncachedNthLastOfTypeIndex(element, siblingCount);
    if (nthIndexCache && siblingCount > kCachedSiblingCountLimit)
        nthIndexCache->cacheNthOfTypeIndexDataForParent(element);
    return index;
}

void NthIndexCache::cacheNthIndexDataForParent(Element& element)
{
    DCHECK(element.parentNode());
    if (!m_parentMap)
        m_parentMap = new ParentMap();

    ParentMap::AddResult addResult = m_parentMap->add(element.parentNode(), nullptr);
    DCHECK(addResult.isNewEntry);
    addResult.storedValue->value = new NthIndexData(*element.parentNode());
}

NthIndexCache::IndexByType& NthIndexCache::ensureTypeIndexMap(ContainerNode& parent)
{
    if (!m_parentMapForType)
        m_parentMapForType = new ParentMapForType();

    ParentMapForType::AddResult addResult = m_parentMapForType->add(&parent, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = new IndexByType();

    DCHECK(addResult.storedValue->value);
    return *addResult.storedValue->value;
}

void NthIndexCache::cacheNthOfTypeIndexDataForParent(Element& element)
{
    DCHECK(element.parentNode());
    IndexByType::AddResult addResult = ensureTypeIndexMap(*element.parentNode()).add(element.tagName(), nullptr);
    DCHECK(addResult.isNewEntry);
    addResult.storedValue->value = new NthIndexData(*element.parentNode(), element.tagQName());
}

unsigned NthIndexData::nthIndex(Element& element) const
{
    DCHECK(!element.isPseudoElement());

    unsigned index = 0;
    for (Element* sibling = &element; sibling; sibling = ElementTraversal::previousSibling(*sibling), index++) {
        auto it = m_elementIndexMap.find(sibling);
        if (it != m_elementIndexMap.end())
            return it->value + index;
    }
    return index;
}

unsigned NthIndexData::nthOfTypeIndex(Element& element) const
{
    DCHECK(!element.isPseudoElement());

    unsigned index = 0;
    for (Element* sibling = &element; sibling; sibling = ElementTraversal::previousSibling(*sibling, HasTagName(element.tagQName())), index++) {
        auto it = m_elementIndexMap.find(sibling);
        if (it != m_elementIndexMap.end())
            return it->value + index;
    }
    return index;
}

unsigned NthIndexData::nthLastIndex(Element& element) const
{
    return m_count - nthIndex(element) + 1;
}

unsigned NthIndexData::nthLastOfTypeIndex(Element& element) const
{
    return m_count - nthOfTypeIndex(element) + 1;
}

NthIndexData::NthIndexData(ContainerNode& parent)
{
    // The frequency at which we cache the nth-index for a set of siblings.
    // A spread value of 3 means every third Element will have its nth-index cached.
    // Using a spread value > 1 is done to save memory. Looking up the nth-index will
    // still be done in constant time in terms of sibling count, at most 'spread'
    // elements will be traversed.
    const unsigned spread = 3;
    unsigned count = 0;
    for (Element* sibling = ElementTraversal::firstChild(parent); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
        if (!(++count % spread))
            m_elementIndexMap.add(sibling, count);
    }
    DCHECK(count);
    m_count = count;
}

NthIndexData::NthIndexData(ContainerNode& parent, const QualifiedName& type)
{
    // The frequency at which we cache the nth-index of type for a set of siblings.
    // A spread value of 3 means every third Element of its type will have its nth-index cached.
    // Using a spread value > 1 is done to save memory. Looking up the nth-index of its type will
    // still be done in less time, as most number of elements traversed
    // will be equal to find 'spread' elements in the sibling set.
    const unsigned spread = 3;
    unsigned count = 0;
    for (Element* sibling = ElementTraversal::firstChild(parent, HasTagName(type)); sibling; sibling = ElementTraversal::nextSibling(*sibling, HasTagName(type))) {
        if (!(++count % spread))
            m_elementIndexMap.add(sibling, count);
    }
    DCHECK(count);
    m_count = count;
}

DEFINE_TRACE(NthIndexData)
{
    visitor->trace(m_elementIndexMap);
}

} // namespace blink
