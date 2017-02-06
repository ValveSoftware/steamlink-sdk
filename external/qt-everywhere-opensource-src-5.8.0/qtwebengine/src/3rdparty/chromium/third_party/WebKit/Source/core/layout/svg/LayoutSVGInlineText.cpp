/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/LayoutSVGInlineText.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/FontSize.h"
#include "core/dom/StyleEngine.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/text/BidiCharacterRun.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"

namespace blink {

// Turn tabs, newlines and carriage returns into spaces. In the future this
// should be removed in favor of letting the generic white-space code handle
// this.
static PassRefPtr<StringImpl> normalizeWhitespace(PassRefPtr<StringImpl> string)
{
    RefPtr<StringImpl> newString = string->replace('\t', ' ');
    newString = newString->replace('\n', ' ');
    newString = newString->replace('\r', ' ');
    return newString.release();
}

LayoutSVGInlineText::LayoutSVGInlineText(Node* n, PassRefPtr<StringImpl> string)
    : LayoutText(n, normalizeWhitespace(string))
    , m_scalingFactor(1)
{
}

void LayoutSVGInlineText::setTextInternal(PassRefPtr<StringImpl> text)
{
    LayoutText::setTextInternal(text);
    if (LayoutSVGText* textLayoutObject = LayoutSVGText::locateLayoutSVGTextAncestor(this))
        textLayoutObject->subtreeTextDidChange();
}

void LayoutSVGInlineText::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutText::styleDidChange(diff, oldStyle);
    updateScaledFont();

    bool newPreserves = style() ? style()->whiteSpace() == PRE : false;
    bool oldPreserves = oldStyle ? oldStyle->whiteSpace() == PRE : false;
    if (oldPreserves != newPreserves) {
        setText(originalText(), true);
        return;
    }

    if (!diff.needsFullLayout())
        return;

    // The text metrics may be influenced by style changes.
    if (LayoutSVGText* textLayoutObject = LayoutSVGText::locateLayoutSVGTextAncestor(this)) {
        textLayoutObject->setNeedsTextMetricsUpdate();
        textLayoutObject->setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::StyleChange);
    }
}

InlineTextBox* LayoutSVGInlineText::createTextBox(int start, unsigned short length)
{
    InlineTextBox* box = new SVGInlineTextBox(LineLayoutItem(this), start, length);
    box->setHasVirtualLogicalHeight();
    return box;
}

LayoutRect LayoutSVGInlineText::localCaretRect(InlineBox* box, int caretOffset, LayoutUnit*)
{
    if (!box || !box->isInlineTextBox())
        return LayoutRect();

    InlineTextBox* textBox = toInlineTextBox(box);
    if (static_cast<unsigned>(caretOffset) < textBox->start() || static_cast<unsigned>(caretOffset) > textBox->start() + textBox->len())
        return LayoutRect();

    // Use the edge of the selection rect to determine the caret rect.
    if (static_cast<unsigned>(caretOffset) < textBox->start() + textBox->len()) {
        LayoutRect rect = textBox->localSelectionRect(caretOffset, caretOffset + 1);
        LayoutUnit x = box->isLeftToRightDirection() ? rect.x() : rect.maxX();
        return LayoutRect(x, rect.y(), caretWidth(), rect.height());
    }

    LayoutRect rect = textBox->localSelectionRect(caretOffset - 1, caretOffset);
    LayoutUnit x = box->isLeftToRightDirection() ? rect.maxX() : rect.x();
    return LayoutRect(x, rect.y(), caretWidth(), rect.height());
}

FloatRect LayoutSVGInlineText::floatLinesBoundingBox() const
{
    FloatRect boundingBox;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        boundingBox.unite(FloatRect(box->calculateBoundaries()));
    return boundingBox;
}

LayoutRect LayoutSVGInlineText::linesBoundingBox() const
{
    return enclosingLayoutRect(floatLinesBoundingBox());
}

bool LayoutSVGInlineText::characterStartsNewTextChunk(int position) const
{
    ASSERT(position >= 0);
    ASSERT(position < static_cast<int>(textLength()));

    // Each <textPath> element starts a new text chunk, regardless of any x/y values.
    if (!position && parent()->isSVGTextPath() && !previousSibling())
        return true;

    const SVGCharacterDataMap::const_iterator it = m_characterDataMap.find(static_cast<unsigned>(position + 1));
    if (it == m_characterDataMap.end())
        return false;

    return it->value.hasX() || it->value.hasY();
}

PositionWithAffinity LayoutSVGInlineText::positionForPoint(const LayoutPoint& point)
{
    if (!hasTextBoxes() || !textLength())
        return createPositionWithAffinity(0);

    ASSERT(m_scalingFactor);
    float baseline = m_scaledFont.getFontMetrics().floatAscent() / m_scalingFactor;

    LayoutBlock* containingBlock = this->containingBlock();
    ASSERT(containingBlock);

    // Map local point to absolute point, as the character origins stored in the text fragments use absolute coordinates.
    FloatPoint absolutePoint(point);
    absolutePoint.moveBy(containingBlock->location());

    float closestDistance = std::numeric_limits<float>::max();
    float closestDistancePosition = 0;
    const SVGTextFragment* closestDistanceFragment = nullptr;
    SVGInlineTextBox* closestDistanceBox = nullptr;

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        if (!box->isSVGInlineTextBox())
            continue;

        SVGInlineTextBox* textBox = toSVGInlineTextBox(box);
        for (const SVGTextFragment& fragment : textBox->textFragments()) {
            FloatRect fragmentRect = fragment.boundingBox(baseline);

            float distance = 0;
            if (!fragmentRect.contains(absolutePoint))
                distance = fragmentRect.squaredDistanceTo(absolutePoint);

            if (distance <= closestDistance) {
                closestDistance = distance;
                closestDistanceBox = textBox;
                closestDistanceFragment = &fragment;
                closestDistancePosition = fragmentRect.x();
            }
        }
    }

    if (!closestDistanceFragment)
        return createPositionWithAffinity(0);

    int offset = closestDistanceBox->offsetForPositionInFragment(*closestDistanceFragment, LayoutUnit(absolutePoint.x() - closestDistancePosition), true);
    return createPositionWithAffinity(offset + closestDistanceBox->start(), offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : TextAffinity::Downstream);
}

namespace {

inline bool isValidSurrogatePair(const TextRun& run, unsigned index)
{
    if (!U16_IS_LEAD(run[index]))
        return false;
    if (index + 1 >= run.length())
        return false;
    return U16_IS_TRAIL(run[index + 1]);
}

TextRun constructTextRun(LayoutSVGInlineText& text, unsigned position, unsigned length, TextDirection textDirection)
{
    const ComputedStyle& style = text.styleRef();

    TextRun run(static_cast<const LChar*>(nullptr) // characters, will be set below if non-zero.
        , 0 // length, will be set below if non-zero.
        , 0 // xPos, only relevant with allowTabs=true
        , 0 // padding, only relevant for justified text, not relevant for SVG
        , TextRun::AllowTrailingExpansion
        , textDirection
        , isOverride(style.unicodeBidi()) /* directionalOverride */);

    if (length) {
        if (text.is8Bit())
            run.setText(text.characters8() + position, length);
        else
            run.setText(text.characters16() + position, length);
    }

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(text.textLength() - position);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

// TODO(pdr): We only have per-glyph data so we need to synthesize per-grapheme
// data. E.g., if 'fi' is shaped into a single glyph, we do not know the 'i'
// position. The code below synthesizes an average glyph width when characters
// share a single position. This will incorrectly split combining diacritics.
// See: https://crbug.com/473476.
void synthesizeGraphemeWidths(const TextRun& run, Vector<CharacterRange>& ranges)
{
    unsigned distributeCount = 0;
    for (int rangeIndex = static_cast<int>(ranges.size()) - 1; rangeIndex >= 0; --rangeIndex) {
        CharacterRange& currentRange = ranges[rangeIndex];
        if (currentRange.width() == 0) {
            distributeCount++;
        } else if (distributeCount != 0) {
            // Only count surrogate pairs as a single character.
            bool surrogatePair = isValidSurrogatePair(run, rangeIndex);
            if (!surrogatePair)
                distributeCount++;

            float newWidth = currentRange.width() / distributeCount;
            currentRange.end = currentRange.start + newWidth;
            float lastEndPosition = currentRange.end;
            for (unsigned distribute = 1; distribute < distributeCount; distribute++) {
                // This surrogate pair check will skip processing of the second
                // character forming the surrogate pair.
                unsigned distributeIndex = rangeIndex + distribute + (surrogatePair ? 1 : 0);
                ranges[distributeIndex].start = lastEndPosition;
                ranges[distributeIndex].end = lastEndPosition + newWidth;
                lastEndPosition = ranges[distributeIndex].end;
            }

            distributeCount = 0;
        }
    }
}

} // namespace

void LayoutSVGInlineText::addMetricsFromRun(
    const TextRun& run, bool& lastCharacterWasWhiteSpace)
{
    Vector<CharacterRange> charRanges = scaledFont().individualCharacterRanges(run);
    synthesizeGraphemeWidths(run, charRanges);

    const float cachedFontHeight = scaledFont().getFontMetrics().floatHeight() / m_scalingFactor;
    const bool preserveWhiteSpace = styleRef().whiteSpace() == PRE;
    const unsigned runLength = run.length();

    // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
    // should be unified under a single concept. See: https://crbug.com/593570
    unsigned characterIndex = 0;
    while (characterIndex < runLength) {
        bool currentCharacterIsWhiteSpace = run[characterIndex] == ' ';
        if (!preserveWhiteSpace && lastCharacterWasWhiteSpace && currentCharacterIsWhiteSpace) {
            m_metrics.append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            characterIndex++;
            continue;
        }

        unsigned length = isValidSurrogatePair(run, characterIndex) ? 2 : 1;
        float width = charRanges[characterIndex].width() / m_scalingFactor;

        m_metrics.append(SVGTextMetrics(length, width, cachedFontHeight));

        lastCharacterWasWhiteSpace = currentCharacterIsWhiteSpace;
        characterIndex += length;
    }
}

void LayoutSVGInlineText::updateMetricsList(bool& lastCharacterWasWhiteSpace)
{
    m_metrics.clear();

    if (!textLength())
        return;

    TextRun run = constructTextRun(*this, 0, textLength(), styleRef().direction());
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bool bidiOverride = isOverride(styleRef().unicodeBidi());
    BidiStatus status(LTR, bidiOverride);
    if (run.is8Bit() || bidiOverride) {
        WTF::Unicode::CharDirection direction = WTF::Unicode::LeftToRight;
        // If BiDi override is in effect, use the specified direction.
        if (bidiOverride && !styleRef().isLeftToRightDirection())
            direction = WTF::Unicode::RightToLeft;
        bidiRuns.addRun(new BidiCharacterRun(0, run.charactersLength(), status.context.get(), direction));
    } else {
        status.last = status.lastStrong = WTF::Unicode::OtherNeutral;
        bidiResolver.setStatus(status);
        bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
        const bool hardLineBreak = false;
        const bool reorderRuns = false;
        bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()), NoVisualOverride, hardLineBreak, reorderRuns);
    }

    for (const BidiCharacterRun* bidiRun = bidiRuns.firstRun(); bidiRun; bidiRun = bidiRun->next()) {
        TextRun subRun = constructTextRun(*this, bidiRun->start(), bidiRun->stop() - bidiRun->start(),
            bidiRun->direction());
        addMetricsFromRun(subRun, lastCharacterWasWhiteSpace);
    }

    bidiResolver.runs().deleteRuns();
}

void LayoutSVGInlineText::updateScaledFont()
{
    computeNewScaledFontForStyle(this, m_scalingFactor, m_scaledFont);
}

void LayoutSVGInlineText::computeNewScaledFontForStyle(LayoutObject* layoutObject, float& scalingFactor, Font& scaledFont)
{
    const ComputedStyle* style = layoutObject->style();
    ASSERT(style);
    ASSERT(layoutObject);

    // Alter font-size to the right on-screen value to avoid scaling the glyphs themselves, except when GeometricPrecision is specified.
    scalingFactor = SVGLayoutSupport::calculateScreenFontSizeScalingFactor(layoutObject);
    if (style->effectiveZoom() == 1 && (scalingFactor == 1 || !scalingFactor)) {
        scalingFactor = 1;
        scaledFont = style->font();
        return;
    }

    if (style->getFontDescription().textRendering() == GeometricPrecision)
        scalingFactor = 1;

    FontDescription fontDescription(style->getFontDescription());

    Document& document = layoutObject->document();
    // FIXME: We need to better handle the case when we compute very small fonts below (below 1pt).
    fontDescription.setComputedSize(FontSize::getComputedSizeFromSpecifiedSize(&document, scalingFactor, fontDescription.isAbsoluteSize(), fontDescription.specifiedSize(), DoNotUseSmartMinimumForFontSize));

    scaledFont = Font(fontDescription);
    scaledFont.update(document.styleEngine().fontSelector());
}

LayoutRect LayoutSVGInlineText::absoluteClippedOverflowRect() const
{
    return parent()->absoluteClippedOverflowRect();
}

FloatRect LayoutSVGInlineText::paintInvalidationRectInLocalSVGCoordinates() const
{
    return parent()->paintInvalidationRectInLocalSVGCoordinates();
}

PassRefPtr<StringImpl> LayoutSVGInlineText::originalText() const
{
    RefPtr<StringImpl> result = LayoutText::originalText();
    if (!result)
        return nullptr;
    return normalizeWhitespace(result);
}

} // namespace blink
