/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef FontDataForRangeSet_h
#define FontDataForRangeSet_h

#include "platform/fonts/FontData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/UnicodeRangeSet.h"
#include "wtf/Allocator.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

class SimpleFontData;

class PLATFORM_EXPORT FontDataForRangeSet : public RefCounted<FontDataForRangeSet> {
public:
    explicit FontDataForRangeSet(PassRefPtr<SimpleFontData> fontData = nullptr, PassRefPtr<UnicodeRangeSet> rangeSet = nullptr)
        : m_fontData(fontData)
        , m_rangeSet(rangeSet)
    {
    }

    // Shorthand for GlyphPageTreeNode tests.
    explicit FontDataForRangeSet(PassRefPtr<SimpleFontData> fontData, UChar32 from, UChar32 to)
        : m_fontData(fontData)
    {
        UnicodeRange range(from, to);
        Vector<UnicodeRange> rangeVector;
        rangeVector.append(range);
        m_rangeSet = adoptRef(new UnicodeRangeSet(rangeVector));
    }

    FontDataForRangeSet(const FontDataForRangeSet& other);

    virtual ~FontDataForRangeSet() { };

    bool contains(UChar32 testChar) const { return m_rangeSet->contains(testChar); }
    bool isEntireRange() const { return m_rangeSet->isEntireRange(); }
    UnicodeRangeSet* ranges() const { return m_rangeSet.get(); }
    bool hasFontData() const { return m_fontData.get(); }
    const SimpleFontData* fontData() const { return m_fontData.get(); }

protected:
    RefPtr<SimpleFontData> m_fontData;
    RefPtr<UnicodeRangeSet> m_rangeSet;
};

class PLATFORM_EXPORT FontDataForRangeSetFromCache : public FontDataForRangeSet {
public:
    explicit FontDataForRangeSetFromCache(PassRefPtr<SimpleFontData> fontData,
        PassRefPtr<UnicodeRangeSet> rangeSet = nullptr)
        : FontDataForRangeSet(fontData, rangeSet)
    {
    }
    virtual ~FontDataForRangeSetFromCache();
};

} // namespace blink

#endif
