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

#include "platform/fonts/shaping/ShapeResult.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "wtf/PtrUtil.h"
#include <hb.h>
#include <memory>

namespace blink {

float ShapeResult::RunInfo::xPositionForVisualOffset(unsigned offset, AdjustMidCluster adjustMidCluster) const
{
    ASSERT(offset < m_numCharacters);
    if (rtl())
        offset = m_numCharacters - offset - 1;
    return xPositionForOffset(offset, adjustMidCluster);
}

float ShapeResult::RunInfo::xPositionForOffset(unsigned offset, AdjustMidCluster adjustMidCluster) const
{
    ASSERT(offset <= m_numCharacters);
    const unsigned numGlyphs = m_glyphData.size();
    unsigned glyphIndex = 0;
    float position = 0;
    if (rtl()) {
        while (glyphIndex < numGlyphs && m_glyphData[glyphIndex].characterIndex > offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        // Adjust offset if it's not on the cluster boundary. In RTL, this means
        // that the adjusted position is the left side of the character.
        if (adjustMidCluster == AdjustToEnd && (glyphIndex < numGlyphs
            ? m_glyphData[glyphIndex].characterIndex : m_numCharacters) < offset) {
            return position;
        }
        // For RTL, we need to return the right side boundary of the character.
        // Add advance of glyphs which are part of the character.
        while (glyphIndex < numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        position += m_glyphData[glyphIndex].advance;
    } else {
        while (glyphIndex < numGlyphs && m_glyphData[glyphIndex].characterIndex < offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        // Adjust offset if it's not on the cluster boundary.
        if (adjustMidCluster == AdjustToStart && glyphIndex && (glyphIndex < numGlyphs
            ? m_glyphData[glyphIndex].characterIndex : m_numCharacters) > offset) {
            offset = m_glyphData[--glyphIndex].characterIndex;
            for (; m_glyphData[glyphIndex].characterIndex == offset; --glyphIndex) {
                position -= m_glyphData[glyphIndex].advance;
                if (!glyphIndex)
                    break;
            }
        }
    }
    return position;
}

int ShapeResult::RunInfo::characterIndexForXPosition(float targetX, bool includePartialGlyphs) const
{
    DCHECK(targetX >= 0 && targetX <= m_width);
    const unsigned numGlyphs = m_glyphData.size();
    float currentX = 0;
    float currentAdvance = 0;
    unsigned glyphIndex = 0;
    unsigned prevCharacterIndex = m_numCharacters; // used only when rtl()

    while (glyphIndex < numGlyphs) {
        float prevAdvance = currentAdvance;
        unsigned currentCharacterIndex = m_glyphData[glyphIndex].characterIndex;
        currentAdvance = m_glyphData[glyphIndex].advance;
        while (glyphIndex < numGlyphs - 1 && currentCharacterIndex == m_glyphData[glyphIndex + 1].characterIndex)
            currentAdvance += m_glyphData[++glyphIndex].advance;
        float nextX;
        if (includePartialGlyphs) {
            // For hit testing, find the closest caret point by incuding
            // end-half of the previous character and start-half of the current
            // character.
            currentAdvance = currentAdvance / 2.0;
            nextX = currentX + prevAdvance + currentAdvance;
        } else {
            nextX = currentX + currentAdvance;
        }
        if (currentX <= targetX && targetX <= nextX)
            return includePartialGlyphs && rtl() ? prevCharacterIndex : currentCharacterIndex;
        currentX = nextX;
        prevCharacterIndex = currentCharacterIndex;
        ++glyphIndex;
    }

    return rtl() ? 0 : m_numCharacters;
}

void ShapeResult::RunInfo::setGlyphAndPositions(unsigned index,
    uint16_t glyphId, float advance, float offsetX, float offsetY)
{
    HarfBuzzRunGlyphData& data = m_glyphData[index];
    data.glyph = glyphId;
    data.advance = advance;
    data.offset = FloatSize(offsetX, offsetY);
}

ShapeResult::ShapeResult(const Font* font, unsigned numCharacters, TextDirection direction)
    : m_width(0)
    , m_primaryFont(const_cast<SimpleFontData*>(font->primaryFont()))
    , m_numCharacters(numCharacters)
    , m_numGlyphs(0)
    , m_direction(direction)
    , m_hasVerticalOffsets(0)
{
}

ShapeResult::ShapeResult(const ShapeResult& other)
    : m_width(other.m_width)
    , m_glyphBoundingBox(other.m_glyphBoundingBox)
    , m_primaryFont(other.m_primaryFont)
    , m_numCharacters(other.m_numCharacters)
    , m_numGlyphs(other.m_numGlyphs)
    , m_direction(other.m_direction)
    , m_hasVerticalOffsets(other.m_hasVerticalOffsets)
{
    m_runs.reserveCapacity(other.m_runs.size());
    for (const auto& run : other.m_runs)
        m_runs.append(wrapUnique(new ShapeResult::RunInfo(*run)));
}

ShapeResult::~ShapeResult()
{
}

size_t ShapeResult::byteSize() const
{
    size_t selfByteSize = sizeof(this);
    for (unsigned i = 0; i < m_runs.size(); ++i) {
        selfByteSize += m_runs[i]->byteSize();
    }
    return selfByteSize;
}

int ShapeResult::offsetForPosition(float targetX, bool includePartialGlyphs) const
{
    int charactersSoFar = 0;
    float currentX = 0;

    if (m_direction == RTL) {
        charactersSoFar = m_numCharacters;
        for (unsigned i = 0; i < m_runs.size(); ++i) {
            if (!m_runs[i])
                continue;
            charactersSoFar -= m_runs[i]->m_numCharacters;
            float nextX = currentX + m_runs[i]->m_width;
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_runs[i]->m_width) {
                // The x value in question is within this script run.
                const unsigned index = m_runs[i]->characterIndexForXPosition(offsetForRun, includePartialGlyphs);
                return charactersSoFar + index;
            }
            currentX = nextX;
        }
    } else {
        for (unsigned i = 0; i < m_runs.size(); ++i) {
            if (!m_runs[i])
                continue;
            float nextX = currentX + m_runs[i]->m_width;
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_runs[i]->m_width) {
                const unsigned index = m_runs[i]->characterIndexForXPosition(offsetForRun, includePartialGlyphs);
                return charactersSoFar + index;
            }
            charactersSoFar += m_runs[i]->m_numCharacters;
            currentX = nextX;
        }
    }

    return charactersSoFar;
}

void ShapeResult::fallbackFonts(HashSet<const SimpleFontData*>* fallback) const
{
    ASSERT(fallback);
    ASSERT(m_primaryFont);
    for (unsigned i = 0; i < m_runs.size(); ++i) {
        if (m_runs[i] && m_runs[i]->m_fontData != m_primaryFont
            && !m_runs[i]->m_fontData->isTextOrientationFallbackOf(m_primaryFont.get())) {
            fallback->add(m_runs[i]->m_fontData.get());
        }
    }
}

void ShapeResult::applySpacing(ShapeResultSpacing& spacing, const TextRun& textRun)
{
    float offsetX, offsetY;
    float& offset = spacing.isVerticalOffset() ? offsetY : offsetX;
    float totalSpace = 0;
    for (auto& run : m_runs) {
        if (!run)
            continue;
        float totalSpaceForRun = 0;
        for (size_t i = 0; i < run->m_glyphData.size(); i++) {
            HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];

            // Skip if it's not a grapheme cluster boundary.
            if (i + 1 < run->m_glyphData.size()
                && glyphData.characterIndex == run->m_glyphData[i + 1].characterIndex) {
                // In RTL, marks need the same letter-spacing offset as the base.
                if (textRun.rtl() && spacing.letterSpacing()) {
                    offsetX = offsetY = 0;
                    offset = spacing.letterSpacing();
                    glyphData.offset.expand(offsetX, offsetY);
                }
            } else {
                offsetX = offsetY = 0;
                float space = spacing.computeSpacing(textRun,
                    run->m_startIndex + glyphData.characterIndex, offset);
                glyphData.advance += space;
                totalSpaceForRun += space;
                if (textRun.rtl()) {
                    // In RTL, spacing should be added to left side of glyphs.
                    offset += space;
                }
                glyphData.offset.expand(offsetX, offsetY);
            }
            m_hasVerticalOffsets |= (glyphData.offset.height() != 0);
        }
        run->m_width += totalSpaceForRun;
        totalSpace += totalSpaceForRun;
    }
    m_width += totalSpace;
    if (spacing.isVerticalOffset())
        m_glyphBoundingBox.setHeight(m_glyphBoundingBox.height() + totalSpace);
    else
        m_glyphBoundingBox.setWidth(m_glyphBoundingBox.width() + totalSpace);
}

PassRefPtr<ShapeResult> ShapeResult::applySpacingToCopy(
    ShapeResultSpacing& spacing, const TextRun& run) const
{
    RefPtr<ShapeResult> result = ShapeResult::create(*this);
    result->applySpacing(spacing, run);
    return result.release();
}

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

void ShapeResult::insertRun(std::unique_ptr<ShapeResult::RunInfo> runToInsert,
    unsigned startGlyph, unsigned numGlyphs, hb_buffer_t* harfBuzzBuffer)
{
    ASSERT(numGlyphs > 0);
    std::unique_ptr<ShapeResult::RunInfo> run(std::move(runToInsert));
    ASSERT(numGlyphs == run->m_glyphData.size());

    const SimpleFontData* currentFontData = run->m_fontData.get();
    const hb_glyph_info_t* glyphInfos =
        hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);
    const hb_glyph_position_t* glyphPositions =
        hb_buffer_get_glyph_positions(harfBuzzBuffer, 0);
    const unsigned startCluster =
        HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))
        ? glyphInfos[startGlyph].cluster
        : glyphInfos[startGlyph + numGlyphs - 1].cluster;

    float totalAdvance = 0.0f;
    FloatPoint glyphOrigin;
    bool hasVerticalOffsets = !HB_DIRECTION_IS_HORIZONTAL(run->m_direction);

    // HarfBuzz returns result in visual order, no need to flip for RTL.
    for (unsigned i = 0; i < numGlyphs; ++i) {
        uint16_t glyph = glyphInfos[startGlyph + i].codepoint;
        hb_glyph_position_t pos = glyphPositions[startGlyph + i];

        float offsetX = harfBuzzPositionToFloat(pos.x_offset);
        float offsetY = -harfBuzzPositionToFloat(pos.y_offset);

        // One out of x_advance and y_advance is zero, depending on
        // whether the buffer direction is horizontal or vertical.
        float advance = harfBuzzPositionToFloat(pos.x_advance - pos.y_advance);

        run->m_glyphData[i].characterIndex =
            glyphInfos[startGlyph + i].cluster - startCluster;

        run->setGlyphAndPositions(i, glyph, advance, offsetX, offsetY);
        totalAdvance += advance;
        hasVerticalOffsets |= (offsetY != 0);

        FloatRect glyphBounds = currentFontData->boundsForGlyph(glyph);
        glyphBounds.move(glyphOrigin.x(), glyphOrigin.y());
        m_glyphBoundingBox.unite(glyphBounds);
        glyphOrigin += FloatSize(advance + offsetX, offsetY);
    }

    run->m_width = std::max(0.0f, totalAdvance);
    m_width += run->m_width;
    m_numGlyphs += numGlyphs;
    ASSERT(m_numGlyphs >= numGlyphs);
    m_hasVerticalOffsets |= hasVerticalOffsets;

    // The runs are stored in result->m_runs in visual order. For LTR, we place
    // the run to be inserted before the next run with a bigger character
    // start index. For RTL, we place the run before the next run with a lower
    // character index. Otherwise, for both directions, at the end.
    if (HB_DIRECTION_IS_FORWARD(run->m_direction)) {
        for (size_t pos = 0; pos < m_runs.size(); ++pos) {
            if (m_runs.at(pos)->m_startIndex > run->m_startIndex) {
                m_runs.insert(pos, std::move(run));
                break;
            }
        }
    } else {
        for (size_t pos = 0; pos < m_runs.size(); ++pos) {
            if (m_runs.at(pos)->m_startIndex < run->m_startIndex) {
                m_runs.insert(pos, std::move(run));
                break;
            }
        }
    }
    // If we didn't find an existing slot to place it, append.
    if (run)
        m_runs.append(std::move(run));
}

} // namespace blink
