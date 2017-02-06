// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBuffer.h"

#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/text/Character.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextDirection.h"

namespace blink {

namespace {

inline void addGlyphToBuffer(GlyphBuffer* glyphBuffer, float advance, hb_direction_t direction,
    const SimpleFontData* fontData, const HarfBuzzRunGlyphData& glyphData)
{
    FloatPoint startOffset = HB_DIRECTION_IS_HORIZONTAL(direction)
        ? FloatPoint(advance, 0)
        : FloatPoint(0, advance);
    glyphBuffer->add(glyphData.glyph, fontData, startOffset + glyphData.offset);
}

inline void addEmphasisMark(GlyphBuffer* buffer,
    const GlyphData* emphasisData, FloatPoint glyphCenter,
    float midGlyphOffset)
{
    ASSERT(buffer);
    ASSERT(emphasisData);

    const SimpleFontData* emphasisFontData = emphasisData->fontData;
    ASSERT(emphasisFontData);

    bool isVertical = emphasisFontData->platformData().isVerticalAnyUpright()
        && emphasisFontData->verticalData();

    if (!isVertical) {
        buffer->add(emphasisData->glyph, emphasisFontData,
            midGlyphOffset - glyphCenter.x());
    } else {
        buffer->add(emphasisData->glyph, emphasisFontData,
            FloatPoint(-glyphCenter.x(), midGlyphOffset - glyphCenter.y()));
    }
}

inline unsigned countGraphemesInCluster(const UChar* str, unsigned strLength,
    uint16_t startIndex, uint16_t endIndex)
{
    if (startIndex > endIndex) {
        uint16_t tempIndex = startIndex;
        startIndex = endIndex;
        endIndex = tempIndex;
    }
    uint16_t length = endIndex - startIndex;
    ASSERT(static_cast<unsigned>(startIndex + length) <= strLength);
    TextBreakIterator* cursorPosIterator = cursorMovementIterator(&str[startIndex], length);

    int cursorPos = cursorPosIterator->current();
    int numGraphemes = -1;
    while (0 <= cursorPos) {
        cursorPos = cursorPosIterator->next();
        numGraphemes++;
    }
    return std::max(0, numGraphemes);
}

} // anonymous namespace

template<TextDirection direction>
float ShapeResultBuffer::fillGlyphBufferForRun(GlyphBuffer* glyphBuffer,
    const ShapeResult::RunInfo* run, float initialAdvance, unsigned from, unsigned to,
    unsigned runOffset)
{
    if (!run)
        return 0;
    float advanceSoFar = initialAdvance;
    const unsigned numGlyphs = run->m_glyphData.size();
    for (unsigned i = 0; i < numGlyphs; ++i) {
        const HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];
        uint16_t currentCharacterIndex = run->m_startIndex +
            glyphData.characterIndex + runOffset;
        if ((direction == RTL && currentCharacterIndex >= to)
            || (direction == LTR && currentCharacterIndex < from)) {
            advanceSoFar += glyphData.advance;
        } else if ((direction == RTL && currentCharacterIndex >= from)
            || (direction == LTR && currentCharacterIndex < to)) {
            addGlyphToBuffer(glyphBuffer, advanceSoFar, run->m_direction,
                run->m_fontData.get(), glyphData);
            advanceSoFar += glyphData.advance;
        }
    }
    return advanceSoFar - initialAdvance;
}

float ShapeResultBuffer::fillGlyphBufferForTextEmphasisRun(GlyphBuffer* glyphBuffer,
    const ShapeResult::RunInfo* run, const TextRun& textRun, const GlyphData* emphasisData,
    float initialAdvance, unsigned from, unsigned to, unsigned runOffset)
{
    if (!run)
        return 0;

    unsigned graphemesInCluster = 1;
    float clusterAdvance = 0;

    FloatPoint glyphCenter = emphasisData->fontData->
        boundsForGlyph(emphasisData->glyph).center();

    TextDirection direction = textRun.direction();

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be broken
    // down further from a text shaping point of view.
    // A cluster can contain multiple glyphs and grapheme clusters, with mutually
    // overlapping boundaries. Below we count grapheme clusters per HarfBuzz clusters,
    // then linearly split the sum of corresponding glyph advances by the number of
    // grapheme clusters in order to find positions for emphasis mark drawing.
    uint16_t clusterStart = static_cast<uint16_t>(direction == RTL
        ? run->m_startIndex + run->m_numCharacters + runOffset
        : run->glyphToCharacterIndex(0) + runOffset);

    float advanceSoFar = initialAdvance;
    const unsigned numGlyphs = run->m_glyphData.size();
    for (unsigned i = 0; i < numGlyphs; ++i) {
        const HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];
        uint16_t currentCharacterIndex = run->m_startIndex + glyphData.characterIndex + runOffset;
        bool isRunEnd = (i + 1 == numGlyphs);
        bool isClusterEnd =  isRunEnd || (run->glyphToCharacterIndex(i + 1) + runOffset != currentCharacterIndex);

        if ((direction == RTL && currentCharacterIndex >= to) || (direction != RTL && currentCharacterIndex < from)) {
            advanceSoFar += glyphData.advance;
            direction == RTL ? --clusterStart : ++clusterStart;
            continue;
        }

        clusterAdvance += glyphData.advance;

        if (textRun.is8Bit()) {
            float glyphAdvanceX = glyphData.advance;
            if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex])) {
                addEmphasisMark(glyphBuffer, emphasisData, glyphCenter, advanceSoFar + glyphAdvanceX / 2);
            }
            advanceSoFar += glyphAdvanceX;
        } else if (isClusterEnd) {
            uint16_t clusterEnd;
            if (direction == RTL)
                clusterEnd = currentCharacterIndex;
            else
                clusterEnd = static_cast<uint16_t>(isRunEnd ? run->m_startIndex + run->m_numCharacters + runOffset : run->glyphToCharacterIndex(i + 1) + runOffset);

            graphemesInCluster = countGraphemesInCluster(textRun.characters16(), textRun.charactersLength(), clusterStart, clusterEnd);
            if (!graphemesInCluster || !clusterAdvance)
                continue;

            float glyphAdvanceX = clusterAdvance / graphemesInCluster;
            for (unsigned j = 0; j < graphemesInCluster; ++j) {
                // Do not put emphasis marks on space, separator, and control characters.
                if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex]))
                    addEmphasisMark(glyphBuffer, emphasisData, glyphCenter, advanceSoFar + glyphAdvanceX / 2);
                advanceSoFar += glyphAdvanceX;
            }
            clusterStart = clusterEnd;
            clusterAdvance = 0;
        }
    }
    return advanceSoFar - initialAdvance;
}

float ShapeResultBuffer::fillFastHorizontalGlyphBuffer(GlyphBuffer* glyphBuffer,
    TextDirection dir) const
{
    ASSERT(!hasVerticalOffsets());

    float advance = 0;

    for (unsigned i = 0; i < m_results.size(); ++i) {
        const auto& wordResult =
            isLeftToRightDirection(dir) ? m_results[i] : m_results[m_results.size() - 1 - i];
        ASSERT(!wordResult->hasVerticalOffsets());

        for (const auto& run : wordResult->m_runs) {
            ASSERT(run);
            ASSERT(HB_DIRECTION_IS_HORIZONTAL(run->m_direction));

            for (const auto& glyphData : run->m_glyphData) {
                ASSERT(!glyphData.offset.height());

                glyphBuffer->add(glyphData.glyph, run->m_fontData.get(),
                    advance + glyphData.offset.width());
                advance += glyphData.advance;
            }
        }
    }

    ASSERT(!glyphBuffer->hasVerticalOffsets());

    return advance;
}

float ShapeResultBuffer::fillGlyphBuffer(GlyphBuffer* glyphBuffer, const TextRun& textRun,
    unsigned from, unsigned to) const
{
    // Fast path: full run with no vertical offsets
    if (!from && to == textRun.length() && !hasVerticalOffsets())
        return fillFastHorizontalGlyphBuffer(glyphBuffer, textRun.direction());

    float advance = 0;

    if (textRun.rtl()) {
        unsigned wordOffset = textRun.length();
        for (unsigned j = 0; j < m_results.size(); j++) {
            unsigned resolvedIndex = m_results.size() - 1 - j;
            const RefPtr<const ShapeResult>& wordResult = m_results[resolvedIndex];
            for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
                advance += fillGlyphBufferForRun<RTL>(glyphBuffer,
                    wordResult->m_runs[i].get(), advance, from, to,
                    wordOffset - wordResult->numCharacters());
            }
            wordOffset -= wordResult->numCharacters();
        }
    } else {
        unsigned wordOffset = 0;
        for (unsigned j = 0; j < m_results.size(); j++) {
            const RefPtr<const ShapeResult>& wordResult = m_results[j];
            for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
                advance += fillGlyphBufferForRun<LTR>(glyphBuffer,
                    wordResult->m_runs[i].get(), advance, from, to, wordOffset);
            }
            wordOffset += wordResult->numCharacters();
        }
    }

    return advance;
}

float ShapeResultBuffer::fillGlyphBufferForTextEmphasis(GlyphBuffer* glyphBuffer,
    const TextRun& textRun, const GlyphData* emphasisData, unsigned from, unsigned to) const
{
    float advance = 0;
    unsigned wordOffset = textRun.rtl() ? textRun.length() : 0;

    for (unsigned j = 0; j < m_results.size(); j++) {
        unsigned resolvedIndex = textRun.rtl() ? m_results.size() - 1 - j : j;
        const RefPtr<const ShapeResult>& wordResult = m_results[resolvedIndex];
        for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
            unsigned resolvedOffset = wordOffset -
                (textRun.rtl() ? wordResult->numCharacters() : 0);
            advance += fillGlyphBufferForTextEmphasisRun(
                glyphBuffer, wordResult->m_runs[i].get(), textRun, emphasisData,
                advance, from, to, resolvedOffset);
        }
        wordOffset += wordResult->numCharacters() * (textRun.rtl() ? -1 : 1);
    }

    return advance;
}

CharacterRange ShapeResultBuffer::getCharacterRange(TextDirection direction,
    float totalWidth, unsigned absoluteFrom, unsigned absoluteTo) const
{
    float currentX = 0;
    float fromX = 0;
    float toX = 0;
    bool foundFromX = false;
    bool foundToX = false;

    if (direction == RTL)
        currentX = totalWidth;

    // The absoluteFrom and absoluteTo arguments represent the start/end offset
    // for the entire run, from/to are continuously updated to be relative to
    // the current word (ShapeResult instance).
    int from = absoluteFrom;
    int to = absoluteTo;

    unsigned totalNumCharacters = 0;
    for (unsigned j = 0; j < m_results.size(); j++) {
        const RefPtr<const ShapeResult> result = m_results[j];
        if (direction == RTL) {
            // Convert logical offsets to visual offsets, because results are in
            // logical order while runs are in visual order.
            if (!foundFromX && from >= 0 && static_cast<unsigned>(from) < result->numCharacters())
                from = result->numCharacters() - from - 1;
            if (!foundToX && to >= 0 && static_cast<unsigned>(to) < result->numCharacters())
                to = result->numCharacters() - to - 1;
            currentX -= result->width();
        }
        for (unsigned i = 0; i < result->m_runs.size(); i++) {
            if (!result->m_runs[i])
                continue;
            ASSERT((direction == RTL) == result->m_runs[i]->rtl());
            int numCharacters = result->m_runs[i]->m_numCharacters;
            if (!foundFromX && from >= 0 && from < numCharacters) {
                fromX = result->m_runs[i]->xPositionForVisualOffset(from, AdjustToStart) + currentX;
                foundFromX = true;
            } else {
                from -= numCharacters;
            }

            if (!foundToX && to >= 0 && to < numCharacters) {
                toX = result->m_runs[i]->xPositionForVisualOffset(to, AdjustToEnd) + currentX;
                foundToX = true;
            } else {
                to -= numCharacters;
            }

            if (foundFromX && foundToX)
                break;
            currentX += result->m_runs[i]->m_width;
        }
        if (direction == RTL)
            currentX -= result->width();
        totalNumCharacters += result->numCharacters();
    }

    // The position in question might be just after the text.
    if (!foundFromX && absoluteFrom == totalNumCharacters) {
        fromX = direction == RTL ? 0 : totalWidth;
        foundFromX = true;
    }
    if (!foundToX && absoluteTo == totalNumCharacters) {
        toX = direction == RTL ? 0 : totalWidth;
        foundToX = true;
    }
    if (!foundFromX)
        fromX = 0;
    if (!foundToX)
        toX = direction == RTL ? 0 : totalWidth;

    // None of our runs is part of the selection, possibly invalid arguments.
    if (!foundToX && !foundFromX)
        fromX = toX = 0;
    if (fromX < toX)
        return CharacterRange(fromX, toX);
    return CharacterRange(toX, fromX);
}

void ShapeResultBuffer::addRunInfoRanges(const ShapeResult::RunInfo& runInfo,
    float offset, Vector<CharacterRange>& ranges)
{
    Vector<float> characterWidths(runInfo.m_numCharacters);
    for (const auto& glyph : runInfo.m_glyphData)
        characterWidths[glyph.characterIndex] += glyph.advance;

    for (unsigned characterIndex = 0; characterIndex < runInfo.m_numCharacters; characterIndex++) {
        float start = offset;
        offset += characterWidths[characterIndex];
        float end = offset;

        // To match getCharacterRange we flip ranges to ensure start <= end.
        if (end < start)
            ranges.append(CharacterRange(end, start));
        else
            ranges.append(CharacterRange(start, end));
    }
}

Vector<CharacterRange> ShapeResultBuffer::individualCharacterRanges(
    TextDirection direction, float totalWidth) const
{
    Vector<CharacterRange> ranges;
    float currentX = direction == RTL ? totalWidth : 0;
    for (const RefPtr<const ShapeResult> result : m_results) {
        if (direction == RTL)
            currentX -= result->width();
        unsigned runCount = result->m_runs.size();
        for (unsigned index = 0; index < runCount; index++) {
            unsigned runIndex = direction == RTL ? runCount - 1 - index : index;
            addRunInfoRanges(*result->m_runs[runIndex], currentX, ranges);
            currentX += result->m_runs[runIndex]->m_width;
        }
        if (direction == RTL)
            currentX -= result->width();
    }
    return ranges;
}

int ShapeResultBuffer::offsetForPosition(const TextRun& run, float targetX, bool includePartialGlyphs) const
{
    unsigned totalOffset;
    if (run.rtl()) {
        totalOffset = run.length();
        for (unsigned i = m_results.size(); i; --i) {
            const RefPtr<const ShapeResult>& wordResult = m_results[i - 1];
            if (!wordResult)
                continue;
            totalOffset -= wordResult->numCharacters();
            if (targetX >= 0 && targetX <= wordResult->width()) {
                int offsetForWord = wordResult->offsetForPosition(targetX, includePartialGlyphs);
                return totalOffset + offsetForWord;
            }
            targetX -= wordResult->width();
        }
    } else {
        totalOffset = 0;
        for (const auto& wordResult : m_results) {
            if (!wordResult)
                continue;
            int offsetForWord = wordResult->offsetForPosition(targetX, includePartialGlyphs);
            ASSERT(offsetForWord >= 0);
            totalOffset += offsetForWord;
            if (targetX >= 0 && targetX <= wordResult->width())
                return totalOffset;
            targetX -= wordResult->width();
        }
    }
    return totalOffset;
}

} // namespace blink
