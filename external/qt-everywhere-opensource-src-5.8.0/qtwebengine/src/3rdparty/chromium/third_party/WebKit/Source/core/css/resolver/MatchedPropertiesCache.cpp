/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/resolver/MatchedPropertiesCache.h"

#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"

namespace blink {

void CachedMatchedProperties::set(const ComputedStyle& style, const ComputedStyle& parentStyle, const MatchedPropertiesVector& properties)
{
    matchedProperties.appendVector(properties);

    // Note that we don't cache the original ComputedStyle instance. It may be further modified.
    // The ComputedStyle in the cache is really just a holder for the substructures and never used as-is.
    this->computedStyle = ComputedStyle::clone(style);
    this->parentComputedStyle = ComputedStyle::clone(parentStyle);
}

void CachedMatchedProperties::clear()
{
    matchedProperties.clear();
    computedStyle = nullptr;
    parentComputedStyle = nullptr;
}

MatchedPropertiesCache::MatchedPropertiesCache()
{
}

const CachedMatchedProperties* MatchedPropertiesCache::find(unsigned hash, const StyleResolverState& styleResolverState, const MatchedPropertiesVector& properties)
{
    ASSERT(hash);

    Cache::iterator it = m_cache.find(hash);
    if (it == m_cache.end())
        return nullptr;
    CachedMatchedProperties* cacheItem = it->value.get();
    ASSERT(cacheItem);

    size_t size = properties.size();
    if (size != cacheItem->matchedProperties.size())
        return nullptr;
    if (cacheItem->computedStyle->insideLink() != styleResolverState.style()->insideLink())
        return nullptr;
    for (size_t i = 0; i < size; ++i) {
        if (properties[i] != cacheItem->matchedProperties[i])
            return nullptr;
    }
    return cacheItem;
}

void MatchedPropertiesCache::add(const ComputedStyle& style, const ComputedStyle& parentStyle, unsigned hash, const MatchedPropertiesVector& properties)
{
    ASSERT(hash);
    Cache::AddResult addResult = m_cache.add(hash, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = new CachedMatchedProperties;

    CachedMatchedProperties* cacheItem = addResult.storedValue->value.get();
    if (!addResult.isNewEntry)
        cacheItem->clear();

    cacheItem->set(style, parentStyle, properties);
}

void MatchedPropertiesCache::clear()
{
    // MatchedPropertiesCache must be cleared promptly because some
    // destructors in the properties (e.g., ~FontFallbackList) expect that
    // the destructors are called promptly without relying on a GC timing.
    for (auto& cacheEntry : m_cache) {
        cacheEntry.value->clear();
    }
    m_cache.clear();
}

void MatchedPropertiesCache::clearViewportDependent()
{
    Vector<unsigned, 16> toRemove;
    for (const auto& cacheEntry : m_cache) {
        CachedMatchedProperties* cacheItem = cacheEntry.value.get();
        if (cacheItem->computedStyle->hasViewportUnits())
            toRemove.append(cacheEntry.key);
    }
    m_cache.removeAll(toRemove);
}

bool MatchedPropertiesCache::isCacheable(const ComputedStyle& style, const ComputedStyle& parentStyle)
{
    if (style.unique() || (style.styleType() != PseudoIdNone && parentStyle.unique()))
        return false;
    if (style.zoom() != ComputedStyle::initialZoom())
        return false;
    if (style.getWritingMode() != ComputedStyle::initialWritingMode() || style.direction() != ComputedStyle::initialDirection())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle.hasExplicitlyInheritedProperties())
        return false;
    if (style.hasVariableReferenceFromNonInheritedProperty())
        return false;
    return true;
}

DEFINE_TRACE(MatchedPropertiesCache)
{
    visitor->trace(m_cache);
}

} // namespace blink
