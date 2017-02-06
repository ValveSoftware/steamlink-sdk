/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#ifndef ShapeResultInlineHeaders_h
#define ShapeResultInlineHeaders_h

#include "platform/fonts/shaping/ShapeResult.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

#include <hb.h>

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;
class HarfBuzzShaper;

struct HarfBuzzRunGlyphData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    uint16_t glyph;
    uint16_t characterIndex;
    float advance;
    FloatSize offset;
};

enum AdjustMidCluster { AdjustToStart, AdjustToEnd };

struct ShapeResult::RunInfo {
    USING_FAST_MALLOC(RunInfo);
public:
    RunInfo(const SimpleFontData* font, hb_direction_t dir, hb_script_t script,
        unsigned startIndex, unsigned numGlyphs, unsigned numCharacters)
        : m_fontData(const_cast<SimpleFontData*>(font)), m_direction(dir)
        , m_script(script), m_glyphData(numGlyphs), m_startIndex(startIndex)
        , m_numCharacters(numCharacters), m_width(0.0f)
    {
    }

    RunInfo(const RunInfo& other)
        : m_fontData(other.m_fontData)
        , m_direction(other.m_direction)
        , m_script(other.m_script)
        , m_glyphData(other.m_glyphData)
        , m_startIndex(other.m_startIndex)
        , m_numCharacters(other.m_numCharacters)
        , m_width(other.m_width)
    {
    }

    bool rtl() const { return HB_DIRECTION_IS_BACKWARD(m_direction); }
    float xPositionForVisualOffset(unsigned, AdjustMidCluster) const;
    float xPositionForOffset(unsigned, AdjustMidCluster) const;
    int characterIndexForXPosition(float, bool includePartialGlyphs) const;
    void setGlyphAndPositions(unsigned index, uint16_t glyphId, float advance,
        float offsetX, float offsetY);

    size_t glyphToCharacterIndex(size_t i) const
    {
        return m_startIndex + m_glyphData[i].characterIndex;
    }

    // For memory reporting.
    size_t byteSize() const
    {
        return sizeof(this) + m_glyphData.size() * sizeof(HarfBuzzRunGlyphData);
    }

    RefPtr<SimpleFontData> m_fontData;
    hb_direction_t m_direction;
    hb_script_t m_script;
    Vector<HarfBuzzRunGlyphData> m_glyphData;
    unsigned m_startIndex;
    unsigned m_numCharacters;
    float m_width;
};

} // namespace blink

#endif // ShapeResultInlineHeaders_h
