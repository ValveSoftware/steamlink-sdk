/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFace_h
#define CSSFontFace_h

#include "core/CoreExport.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFace.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/UnicodeRangeSet.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class FontDescription;
class RemoteFontFaceSource;
class SimpleFontData;

class CORE_EXPORT CSSFontFace final : public GarbageCollectedFinalized<CSSFontFace> {
    WTF_MAKE_NONCOPYABLE(CSSFontFace);
public:
    CSSFontFace(FontFace* fontFace, Vector<UnicodeRange>& ranges)
        : m_ranges(adoptRef(new UnicodeRangeSet(ranges)))
        , m_segmentedFontFace(nullptr)
        , m_fontFace(fontFace)
    {
        ASSERT(m_fontFace);
    }

    FontFace* fontFace() const { return m_fontFace; }

    PassRefPtr<UnicodeRangeSet> ranges() { return m_ranges; }

    void setSegmentedFontFace(CSSSegmentedFontFace*);
    void clearSegmentedFontFace() { m_segmentedFontFace = nullptr; }

    bool isValid() const { return !m_sources.isEmpty(); }
    size_t approximateBlankCharacterCount() const;

    void addSource(CSSFontFaceSource*);

    void didBeginLoad();
    void fontLoaded(RemoteFontFaceSource*);
    void didBecomeVisibleFallback(RemoteFontFaceSource*);

    PassRefPtr<SimpleFontData> getFontData(const FontDescription&);

    FontFace::LoadStatusType loadStatus() const { return m_fontFace->loadStatus(); }
    bool maybeLoadFont(const FontDescription&, const String&);
    bool maybeLoadFont(const FontDescription&, const FontDataForRangeSet&);
    void load();
    void load(const FontDescription&);

    bool hadBlankText() { return isValid() && m_sources.first()->hadBlankText(); }

    DECLARE_TRACE();

private:
    void setLoadStatus(FontFace::LoadStatusType);

    RefPtr<UnicodeRangeSet> m_ranges;
    Member<CSSSegmentedFontFace> m_segmentedFontFace;
    HeapDeque<Member<CSSFontFaceSource>> m_sources;
    Member<FontFace> m_fontFace;
};

} // namespace blink

#endif
