// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultBuffer_h
#define ShapeResultBuffer_h

#include "platform/fonts/shaping/ShapeResult.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

struct CharacterRange;
class GlyphBuffer;
class TextRun;

class ShapeResultBuffer {
    WTF_MAKE_NONCOPYABLE(ShapeResultBuffer);
    STACK_ALLOCATED();
public:
    ShapeResultBuffer()
        : m_hasVerticalOffsets(false) { }

    void appendResult(PassRefPtr<const ShapeResult> result)
    {
        m_hasVerticalOffsets |= result->hasVerticalOffsets();
        m_results.append(result);
    }

    bool hasVerticalOffsets() const { return m_hasVerticalOffsets; }

    float fillGlyphBuffer(GlyphBuffer*, const TextRun&, unsigned from, unsigned to) const;
    float fillGlyphBufferForTextEmphasis(GlyphBuffer*, const TextRun&,
        const GlyphData* emphasisData, unsigned from, unsigned to) const;
    int offsetForPosition(const TextRun&, float targetX, bool includePartialGlyphs) const;
    CharacterRange getCharacterRange(TextDirection, float totalWidth,
        unsigned from, unsigned to) const;
    Vector<CharacterRange> individualCharacterRanges(TextDirection,
        float totalWidth) const;

private:
    float fillFastHorizontalGlyphBuffer(GlyphBuffer*, TextDirection) const;

    template<TextDirection>
    static float fillGlyphBufferForRun(GlyphBuffer*, const ShapeResult::RunInfo*,
        float initialAdvance, unsigned from, unsigned to, unsigned runOffset);
    static float fillGlyphBufferForTextEmphasisRun(GlyphBuffer*, const ShapeResult::RunInfo*,
        const TextRun&, const GlyphData*, float initialAdvance, unsigned from, unsigned to,
        unsigned runOffset);

    static void addRunInfoRanges(const ShapeResult::RunInfo&, float offset,
        Vector<CharacterRange>&);

    // Empirically, cases where we get more than 50 ShapeResults are extremely rare.
    Vector<RefPtr<const ShapeResult>, 64>m_results;
    bool m_hasVerticalOffsets;
};

} // namespace blink

#endif // ShapeResultBuffer_h
