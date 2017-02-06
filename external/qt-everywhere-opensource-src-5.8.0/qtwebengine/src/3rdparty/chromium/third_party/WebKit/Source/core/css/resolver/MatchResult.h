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

#ifndef MatchResult_h
#define MatchResult_h

#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class StylePropertySet;

struct CORE_EXPORT MatchedProperties {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    MatchedProperties();
    ~MatchedProperties();

    DECLARE_TRACE();

    Member<StylePropertySet> properties;

    union {
        struct {
            unsigned linkMatchType : 2;
            unsigned whitelistType : 2;
        } m_types;
        // Used to make sure all memory is zero-initialized since we compute the hash over the bytes of this object.
        void* possiblyPaddedMember;
    };
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MatchedProperties);

namespace blink {

using MatchedPropertiesVector = HeapVector<MatchedProperties, 64>;

// MatchedPropertiesRange is used to represent a subset of the matched properties from
// a given origin, for instance UA rules, author rules, or a shadow tree scope. This is
// needed because rules from different origins are applied in the opposite order for
// !important rules, yet in the same order as for normal rules within the same origin.

class MatchedPropertiesRange {
public:
    MatchedPropertiesRange(MatchedPropertiesVector::const_iterator begin, MatchedPropertiesVector::const_iterator end)
        : m_begin(begin)
        , m_end(end)
    {
    }

    MatchedPropertiesVector::const_iterator begin() const { return m_begin; }
    MatchedPropertiesVector::const_iterator end() const { return m_end; }

    bool isEmpty() const { return begin() == end(); }

private:
    MatchedPropertiesVector::const_iterator m_begin;
    MatchedPropertiesVector::const_iterator m_end;
};

class CORE_EXPORT MatchResult {
    WTF_MAKE_NONCOPYABLE(MatchResult);
    STACK_ALLOCATED();
public:
    MatchResult() {}

    void addMatchedProperties(const StylePropertySet* properties, unsigned linkMatchType = CSSSelector::MatchAll, PropertyWhitelistType = PropertyWhitelistNone);
    bool hasMatchedProperties() const { return m_matchedProperties.size(); }

    void finishAddingUARules();
    void finishAddingAuthorRulesForTreeScope();

    void setIsCacheable(bool cacheable) { m_isCacheable = cacheable; }
    bool isCacheable() const { return m_isCacheable; }

    MatchedPropertiesRange allRules() const { return MatchedPropertiesRange(m_matchedProperties.begin(), m_matchedProperties.end()); }
    MatchedPropertiesRange uaRules() const { return MatchedPropertiesRange(m_matchedProperties.begin(), m_matchedProperties.begin() + m_uaRangeEnd); }
    MatchedPropertiesRange authorRules() const { return MatchedPropertiesRange(m_matchedProperties.begin() + m_uaRangeEnd, m_matchedProperties.end()); }

    const MatchedPropertiesVector& matchedProperties() const { return m_matchedProperties; }

private:
    friend class ImportantAuthorRanges;
    friend class ImportantAuthorRangeIterator;

    MatchedPropertiesVector m_matchedProperties;
    Vector<unsigned, 16> m_authorRangeEnds;
    unsigned m_uaRangeEnd = 0;
    bool m_isCacheable = true;
};

class ImportantAuthorRangeIterator {
    STACK_ALLOCATED();
public:
    ImportantAuthorRangeIterator(const MatchResult& result, int endIndex)
        : m_result(result)
        , m_endIndex(endIndex) { }

    MatchedPropertiesRange operator*() const
    {
        unsigned rangeEnd = m_result.m_authorRangeEnds[m_endIndex];
        unsigned rangeBegin = m_endIndex ? m_result.m_authorRangeEnds[m_endIndex - 1] : m_result.m_uaRangeEnd;
        return MatchedPropertiesRange(m_result.matchedProperties().begin() + rangeBegin, m_result.matchedProperties().begin() + rangeEnd);
    }

    ImportantAuthorRangeIterator& operator++()
    {
        --m_endIndex;
        return *this;
    }

    bool operator==(const ImportantAuthorRangeIterator& other) const { return m_endIndex == other.m_endIndex && &m_result == &other.m_result; }
    bool operator!=(const ImportantAuthorRangeIterator& other) const { return !(*this == other); }

private:
    const MatchResult& m_result;
    unsigned m_endIndex;
};

class ImportantAuthorRanges {
    STACK_ALLOCATED();
public:
    explicit ImportantAuthorRanges(const MatchResult& result) : m_result(result) { }

    ImportantAuthorRangeIterator begin() const { return ImportantAuthorRangeIterator(m_result, m_result.m_authorRangeEnds.size() - 1); }
    ImportantAuthorRangeIterator end() const { return ImportantAuthorRangeIterator(m_result, -1); }

private:
    const MatchResult& m_result;
};

inline bool operator==(const MatchedProperties& a, const MatchedProperties& b)
{
    return a.properties == b.properties && a.m_types.linkMatchType == b.m_types.linkMatchType;
}

inline bool operator!=(const MatchedProperties& a, const MatchedProperties& b)
{
    return !(a == b);
}

} // namespace blink

#endif // MatchResult_h
