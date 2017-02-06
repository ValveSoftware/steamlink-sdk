/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "UnicodeRangeSet.h"

#include "wtf/text/WTFString.h"

namespace blink {

UnicodeRangeSet::UnicodeRangeSet(const Vector<UnicodeRange>& ranges)
    : m_ranges(ranges)
{
    if (m_ranges.isEmpty())
        return;

    std::sort(m_ranges.begin(), m_ranges.end());

    // Unify overlapping ranges.
    UChar32 from = m_ranges[0].from();
    UChar32 to = m_ranges[0].to();
    size_t targetIndex = 0;
    for (size_t i = 1; i < m_ranges.size(); i++) {
        if (to + 1 >= m_ranges[i].from()) {
            to = std::max(to, m_ranges[i].to());
        } else {
            m_ranges[targetIndex++] = UnicodeRange(from, to);
            from = m_ranges[i].from();
            to = m_ranges[i].to();
        }
    }
    m_ranges[targetIndex++] = UnicodeRange(from, to);
    m_ranges.shrink(targetIndex);
}

bool UnicodeRangeSet::contains(UChar32 c) const
{
    if (isEntireRange())
        return true;
    Vector<UnicodeRange>::const_iterator it = std::lower_bound(m_ranges.begin(), m_ranges.end(), c);
    return it != m_ranges.end() && it->contains(c);
}

bool UnicodeRangeSet::intersectsWith(const String& text) const
{
    if (text.isEmpty())
        return false;
    if (isEntireRange())
        return true;
    if (text.is8Bit() && m_ranges[0].from() >= 0x100)
        return false;

    unsigned index = 0;
    while (index < text.length()) {
        UChar32 c = text.characterStartingAt(index);
        index += U16_LENGTH(c);
        if (contains(c))
            return true;
    }
    return false;
}

bool UnicodeRangeSet::operator==(const UnicodeRangeSet& other) const
{
    if (m_ranges.size() == 0 && other.size() == 0)
        return true;
    if (m_ranges.size() != other.size()) {
        return false;
    }
    bool equal = true;
    for (size_t i = 0; i < m_ranges.size(); ++i) {
        equal = equal && m_ranges[i] == other.m_ranges[i];
    }
    return equal;
}
}
