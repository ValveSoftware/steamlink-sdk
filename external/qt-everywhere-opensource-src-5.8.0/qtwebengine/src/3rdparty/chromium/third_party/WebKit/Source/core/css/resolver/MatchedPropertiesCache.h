/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef MatchedPropertiesCache_h
#define MatchedPropertiesCache_h

#include "core/css/StylePropertySet.h"
#include "core/css/resolver/MatchResult.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ComputedStyle;
class StyleResolverState;

class CachedMatchedProperties final : public GarbageCollectedFinalized<CachedMatchedProperties> {
public:
    HeapVector<MatchedProperties> matchedProperties;
    RefPtr<ComputedStyle> computedStyle;
    RefPtr<ComputedStyle> parentComputedStyle;

    void set(const ComputedStyle&, const ComputedStyle& parentStyle, const MatchedPropertiesVector&);
    void clear();
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(matchedProperties);
    }
};

// Specialize the HashTraits for CachedMatchedProperties to check for dead
// entries in the MatchedPropertiesCache.
struct CachedMatchedPropertiesHashTraits : HashTraits<Member<CachedMatchedProperties>> {
    static const WTF::WeakHandlingFlag weakHandlingFlag = WTF::WeakHandlingInCollections;

    template<typename VisitorDispatcher>
    static bool traceInCollection(VisitorDispatcher visitor, Member<CachedMatchedProperties>& cachedProperties, WTF::ShouldWeakPointersBeMarkedStrongly strongify)
    {
        // Only honor the cache's weakness semantics if the collection is traced
        // with WeakPointersActWeak. Otherwise just trace the cachedProperties
        // strongly, ie. call trace on it.
        if (cachedProperties && strongify == WTF::WeakPointersActWeak) {
            // A given cache entry is only kept alive if none of the MatchedProperties
            // in the CachedMatchedProperties value contain a dead "properties" field.
            // If there is a dead field the entire cache entry is removed.
            for (const auto& matchedProperties : cachedProperties->matchedProperties) {
                if (!ThreadHeap::isHeapObjectAlive(matchedProperties.properties)) {
                    // For now report the cache entry as dead. This might not
                    // be the final result if in a subsequent call for this entry,
                    // the "properties" field has been marked via another path.
                    return true;
                }
            }
        }
        // At this point none of the entries in the matchedProperties vector
        // had a dead "properties" field so trace CachedMatchedProperties strongly.
        // FIXME: traceInCollection is also called from WeakProcessing to check if the entry is dead.
        // Avoid calling trace in that case by only calling trace when cachedProperties is not yet marked.
        if (!ThreadHeap::isHeapObjectAlive(cachedProperties))
            visitor->trace(cachedProperties);
        return false;
    }
};

class MatchedPropertiesCache {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(MatchedPropertiesCache);
public:
    MatchedPropertiesCache();
    ~MatchedPropertiesCache()
    {
        ASSERT(m_cache.isEmpty());
    }

    const CachedMatchedProperties* find(unsigned hash, const StyleResolverState&, const MatchedPropertiesVector&);
    void add(const ComputedStyle&, const ComputedStyle& parentStyle, unsigned hash, const MatchedPropertiesVector&);

    void clear();
    void clearViewportDependent();

    static bool isCacheable(const ComputedStyle&, const ComputedStyle& parentStyle);

    DECLARE_TRACE();

private:
    using Cache = HeapHashMap<unsigned, Member<CachedMatchedProperties>, DefaultHash<unsigned>::Hash, HashTraits<unsigned>, CachedMatchedPropertiesHashTraits>;
    Cache m_cache;
};

} // namespace blink

#endif
