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

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/fonts/shaping/Shaper.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/CharacterNames.h"
#include <hb.h>
#include <memory>
#include <unicode/uscript.h>

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;
class HarfBuzzShaper;
class UnicodeRangeSet;

// Shaping text runs is split into several stages: Run segmentation, shaping the
// initial segment, identify shaped and non-shaped sequences of the shaping
// result, and processing sub-runs by trying to shape them with a fallback font
// until the last resort font is reached.
//
// If caps formatting is requested, an additional lowercase/uppercase
// segmentation stage is required. In this stage, OpenType features in the font
// are matched against the requested formatting and formatting is synthesized as
// required by the CSS Level 3 Fonts Module.
//
// Going through one example - for simplicity without caps formatting - to
// illustrate the process: The following is a run of vertical text to be
// shaped. After run segmentation in RunSegmenter it is split into 4
// segments. The segments indicated by the segementation results showing the
// script, orientation information and small caps handling of the individual
// segment. The Japanese text at the beginning has script "Hiragana", does not
// need rotation when laid out vertically and does not need uppercasing when
// small caps is requested.
//
// 0 い
// 1 ろ
// 2 は USCRIPT_HIRAGANA,
//     OrientationIterator::OrientationKeep,
//     SmallCapsIterator::SmallCapsSameCase
//
// 3 a
// 4 ̄ (Combining Macron)
// 5 a
// 6 A USCRIPT_LATIN,
//     OrientationIterator::OrientationRotateSideways,
//     SmallCapsIterator::SmallCapsUppercaseNeeded
//
// 7 い
// 8 ろ
// 9 は USCRIPT_HIRAGANA,
//      OrientationIterator::OrientationKeep,
//      SmallCapsIterator::SmallCapsSameCase
//
//
// Let's assume the CSS for this text run is as follows:
//     font-family: "Heiti SC", Tinos, sans-serif;
// where Tinos is a web font, defined as a composite font, with two sub ranges,
// one for Latin U+00-U+FF and one unrestricted unicode-range.
//
// FontFallbackIterator provides the shaper with Heiti SC, then Tinos of the
// restricted unicode-range, then the unrestricted full unicode-range Tinos, then
// a system sans-serif.
//
// The initial segment 0-2 to the shaper, together with the segmentation
// properties and the initial Heiti SC font. Characters 0-2 are shaped
// successfully with Heiti SC. The next segment, 3-5 is passed to the shaper. The
// shaper attempts to shape it with Heiti SC, which fails for the Combining
// Macron. So the shaping result for this segment would look similar to this.
//
// Glyphpos: 0 1 2 3
// Cluster:  0 0 2 3
// Glyph:    a x a A (where x is .notdef)
//
// Now in the extractShapeResults() step we notice that there is more work to do,
// since Heiti SC does not have a glyph for the Combining Macron combined with an
// a. So, this cluster together with a Todo item for switching to the next font
// is put into HolesQueue.
//
// After shaping the initial segment, the remaining items in the HolesQueue are
// processed, picking them from the head of the queue. So, first, the next font
// is requested from the FontFallbackIterator. In this case, Tinos (for the range
// U+00-U+FF) comes back. Shaping using this font, assuming it is subsetted,
// fails again since there is no combining mark available. This triggers
// requesting yet another font. This time, the Tinos font for the full
// range. With this, shaping succeeds with the following HarfBuzz result:
//
//  Glyphpos 0 1 2 3
//  Cluster: 0 0 2 3
//  Glyph:   a ̄ a A (with glyph coordinates placing the ̄ above the first a)
//
// Now this sub run is successfully processed and can be appended to
// ShapeResult. A new ShapeResult::RunInfo is created. The logic in
// insertRunIntoShapeResult then takes care of merging the shape result into
// the right position the vector of RunInfos in ShapeResult.
//
// Shaping then continues analogously for the remaining Hiragana Japanese
// sub-run, and the result is inserted into ShapeResult as well.
class PLATFORM_EXPORT HarfBuzzShaper final : public Shaper {
public:
    HarfBuzzShaper(const Font*, const TextRun&);
    PassRefPtr<ShapeResult> shapeResult();
    ~HarfBuzzShaper() { }

    enum HolesQueueItemAction {
        HolesQueueNextFont,
        HolesQueueRange
    };

    struct HolesQueueItem {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        HolesQueueItemAction m_action;
        unsigned m_startIndex;
        unsigned m_numCharacters;
        HolesQueueItem(HolesQueueItemAction action, unsigned start, unsigned num)
            : m_action(action)
            , m_startIndex(start)
            , m_numCharacters(num) {};
    };

protected:
    using FeaturesVector = Vector<hb_feature_t, 6>;

    class CapsFeatureSettingsScopedOverlay final {
        STACK_ALLOCATED()

    public:
        CapsFeatureSettingsScopedOverlay(FeaturesVector&, FontDescription::FontVariantCaps);
        CapsFeatureSettingsScopedOverlay() = delete;
        ~CapsFeatureSettingsScopedOverlay();
    private:
        void overlayCapsFeatures(FontDescription::FontVariantCaps);
        void prependCounting(const hb_feature_t&);
        FeaturesVector& m_features;
        size_t m_countFeatures;
    };

private:
    void setFontFeatures();

    void appendToHolesQueue(HolesQueueItemAction,
        unsigned startIndex,
        unsigned numCharacters);
    void prependHolesQueue(HolesQueueItemAction,
        unsigned startIndex,
        unsigned numCharacters);
    void splitUntilNextCaseChange(HolesQueueItem& currentQueueItem, SmallCapsIterator::SmallCapsBehavior&);
    inline bool shapeRange(hb_buffer_t* harfBuzzBuffer,
        unsigned startIndex,
        unsigned numCharacters,
        const SimpleFontData* currentFont,
        PassRefPtr<UnicodeRangeSet> currentFontRangeSet,
        UScriptCode currentRunScript,
        hb_language_t);
    bool extractShapeResults(hb_buffer_t* harfBuzzBuffer,
        ShapeResult*,
        bool& fontCycleQueued,
        const HolesQueueItem& currentQueueItem,
        const SimpleFontData* currentFont,
        UScriptCode currentRunScript,
        bool isLastResort);
    bool collectFallbackHintChars(Vector<UChar32>& hint, bool needsList);

    void insertRunIntoShapeResult(ShapeResult*, std::unique_ptr<ShapeResult::RunInfo> runToInsert, unsigned startGlyph, unsigned numGlyphs, hb_buffer_t*);

    std::unique_ptr<UChar[]> m_normalizedBuffer;
    unsigned m_normalizedBufferLength;

    FeaturesVector m_features;
    Deque<HolesQueueItem> m_holesQueue;
};


} // namespace blink

#endif // HarfBuzzShaper_h
