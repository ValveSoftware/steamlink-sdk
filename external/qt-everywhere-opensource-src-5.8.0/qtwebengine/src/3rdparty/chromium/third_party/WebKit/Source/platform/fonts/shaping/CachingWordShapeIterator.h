/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CachingWordShapeIterator_h
#define CachingWordShapeIterator_h

#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/CachingWordShapeIterator.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeCache.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "wtf/Allocator.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

class CachingWordShapeIterator final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(CachingWordShapeIterator);
public:
    CachingWordShapeIterator(ShapeCache* cache, const TextRun& run,
        const Font* font)
        : m_shapeCache(cache), m_textRun(run), m_font(font)
        , m_spacing(run, font->getFontDescription())
        , m_widthSoFar(0), m_startIndex(0)
    {
        ASSERT(font);

        // Shaping word by word is faster as each word is cached. If we cannot
        // use the cache or if the font doesn't support word by word shaping
        // fall back on shaping the entire run.
        m_shapeByWord = m_font->canShapeWordByWord();
    }

    bool next(RefPtr<const ShapeResult>* wordResult)
    {
        if (UNLIKELY(m_textRun.allowTabs()))
            return nextForAllowTabs(wordResult);

        if (!m_shapeByWord) {
            if (m_startIndex)
                return false;
            *wordResult = shapeWord(m_textRun, m_font);
            m_startIndex = 1;
            return wordResult->get();
        }

        return nextWord(wordResult);
    }

private:
    PassRefPtr<const ShapeResult> shapeWordWithoutSpacing(
        const TextRun& wordRun, const Font* font)
    {
        ShapeCacheEntry* cacheEntry = m_shapeCache->add(wordRun,
            ShapeCacheEntry());
        if (cacheEntry && cacheEntry->m_shapeResult)
            return cacheEntry->m_shapeResult;

        HarfBuzzShaper shaper(font, wordRun);
        RefPtr<const ShapeResult> shapeResult = shaper.shapeResult();
        if (!shapeResult)
            return nullptr;

        if (cacheEntry)
            cacheEntry->m_shapeResult = shapeResult;

        return shapeResult.release();
    }

    PassRefPtr<const ShapeResult> shapeWord(const TextRun& wordRun,
        const Font* font)
    {
        if (LIKELY(!m_spacing.hasSpacing()))
            return shapeWordWithoutSpacing(wordRun, font);

        RefPtr<const ShapeResult> result = shapeWordWithoutSpacing(wordRun, font);
        return result->applySpacingToCopy(m_spacing, wordRun);
    }

    bool nextWord(RefPtr<const ShapeResult>* wordResult)
    {
        return shapeToEndIndex(wordResult, nextWordEndIndex());
    }

    static bool isWordDelimiter(UChar ch)
    {
        return ch == spaceCharacter || ch == tabulationCharacter;
    }

    unsigned nextWordEndIndex()
    {
        const unsigned length = m_textRun.length();
        if (m_startIndex >= length)
            return 0;

        if (m_startIndex + 1u == length || isWordDelimiter(m_textRun[m_startIndex]))
            return m_startIndex + 1;

        // Delimit every CJK character because these scripts do not delimit
        // words by spaces, and not delimiting hits the performance.
        if (!m_textRun.is8Bit()) {
            UChar32 ch;
            unsigned end = m_startIndex;
            U16_NEXT(m_textRun.characters16(), end, length, ch);
            if (Character::isCJKIdeographOrSymbol(ch)) {
                bool hasAnyScript = !Character::isCommonOrInheritedScript(ch);
                for (unsigned i = end; i < length; end = i) {
                    U16_NEXT(m_textRun.characters16(), i, length, ch);
                    // ZWJ and modifier check in order not to split those Emoji sequences.
                    if (U_GET_GC_MASK(ch) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK)
                        || ch == zeroWidthJoinerCharacter || Character::isModifier(ch))
                        continue;
                    // Avoid delimiting COMMON/INHERITED alone, which makes harder to
                    // identify the script.
                    if (Character::isCJKIdeographOrSymbol(ch)) {
                        if (Character::isCommonOrInheritedScript(ch))
                            continue;
                        if (!hasAnyScript) {
                            hasAnyScript = true;
                            continue;
                        }
                    }
                    return end;
                }
                return length;
            }
        }

        for (unsigned i = m_startIndex + 1; ; i++) {
            if (i == length || isWordDelimiter(m_textRun[i])) {
                return i;
            }
            if (!m_textRun.is8Bit()) {
                UChar32 nextChar;
                U16_GET(m_textRun.characters16(), 0, i, length, nextChar);
                if (Character::isCJKIdeographOrSymbolBase(nextChar))
                    return i;
            }
        }
    }

    bool shapeToEndIndex(RefPtr<const ShapeResult>* result, unsigned endIndex)
    {
        if (!endIndex || endIndex <= m_startIndex)
            return false;

        const unsigned length = m_textRun.length();
        if (!m_startIndex && endIndex == length) {
            *result = shapeWord(m_textRun, m_font);
        } else {
            ASSERT(endIndex <= length);
            TextRun subRun = m_textRun.subRun(m_startIndex, endIndex - m_startIndex);
            *result = shapeWord(subRun, m_font);
        }
        m_startIndex = endIndex;
        return result->get();
    }

    unsigned endIndexUntil(UChar ch)
    {
        unsigned length = m_textRun.length();
        ASSERT(m_startIndex < length);
        for (unsigned i = m_startIndex + 1; ; i++) {
            if (i == length || m_textRun[i] == ch)
                return i;
        }
    }

    bool nextForAllowTabs(RefPtr<const ShapeResult>* wordResult)
    {
        unsigned length = m_textRun.length();
        if (m_startIndex >= length)
            return false;

        if (UNLIKELY(m_textRun[m_startIndex] == tabulationCharacter)) {
            for (unsigned i = m_startIndex + 1; ; i++) {
                if (i == length || m_textRun[i] != tabulationCharacter) {
                    *wordResult = ShapeResult::createForTabulationCharacters(
                        m_font, m_textRun, m_widthSoFar, i - m_startIndex);
                    m_startIndex = i;
                    break;
                }
            }
        } else if (!m_shapeByWord) {
            if (!shapeToEndIndex(wordResult, endIndexUntil(tabulationCharacter)))
                return false;
        } else {
            if (!nextWord(wordResult))
                return false;
        }
        ASSERT(*wordResult);
        m_widthSoFar += (*wordResult)->width();
        return true;
    }

    ShapeCache* m_shapeCache;
    const TextRun& m_textRun;
    const Font* m_font;
    ShapeResultSpacing m_spacing;
    float m_widthSoFar; // Used only when allowTabs()
    unsigned m_startIndex : 31;
    unsigned m_shapeByWord : 1;
};

} // namespace blink

#endif // CachingWordShapeIterator_h
