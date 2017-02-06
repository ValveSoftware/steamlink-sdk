/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ShapeResult_h
#define ShapeResult_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextDirection.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

struct hb_buffer_t;

namespace blink {

class Font;
class ShapeResultSpacing;
class SimpleFontData;
class TextRun;
struct GlyphData;

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
public:
    static PassRefPtr<ShapeResult> create(const Font* font,
        unsigned numCharacters, TextDirection direction)
    {
        return adoptRef(new ShapeResult(font, numCharacters, direction));
    }
    static PassRefPtr<ShapeResult> createForTabulationCharacters(const Font*,
        const TextRun&, float positionOffset, unsigned count);
    ~ShapeResult();

    float width() const { return m_width; }
    const FloatRect& bounds() const { return m_glyphBoundingBox; }
    unsigned numCharacters() const { return m_numCharacters; }
    void fallbackFonts(HashSet<const SimpleFontData*>*) const;
    bool rtl() const { return m_direction == RTL; }
    bool hasVerticalOffsets() const { return m_hasVerticalOffsets; }

    // For memory reporting.
    size_t byteSize() const;

    int offsetForPosition(float targetX, bool includePartialGlyphs) const;

    PassRefPtr<ShapeResult> applySpacingToCopy(ShapeResultSpacing&,
        const TextRun&) const;

protected:
    struct RunInfo;

    ShapeResult(const Font*, unsigned numCharacters, TextDirection);
    ShapeResult(const ShapeResult&);

    static PassRefPtr<ShapeResult> create(const ShapeResult& other)
    {
        return adoptRef(new ShapeResult(other));
    }

    void applySpacing(ShapeResultSpacing&, const TextRun&);
    void insertRun(std::unique_ptr<ShapeResult::RunInfo>, unsigned startGlyph,
        unsigned numGlyphs, hb_buffer_t*);

    float m_width;
    FloatRect m_glyphBoundingBox;
    Vector<std::unique_ptr<RunInfo>> m_runs;
    RefPtr<SimpleFontData> m_primaryFont;

    unsigned m_numCharacters;
    unsigned m_numGlyphs : 30;

    // Overall direction for the TextRun, dictates which order each individual
    // sub run (represented by RunInfo structs in the m_runs vector) can have a
    // different text direction.
    unsigned m_direction : 1;

    // Tracks whether any runs contain glyphs with a y-offset != 0.
    unsigned m_hasVerticalOffsets : 1;

    friend class HarfBuzzShaper;
    friend class ShapeResultBuffer;
};

} // namespace blink

#endif // ShapeResult_h
