/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 *
 */

#include "core/layout/LayoutTextCombine.h"

#include "platform/graphics/GraphicsContext.h"

namespace blink {

const float textCombineMargin = 1.1f; // Allow em + 10% margin

LayoutTextCombine::LayoutTextCombine(Node* node, PassRefPtr<StringImpl> string)
    : LayoutText(node, string)
    , m_combinedTextWidth(0)
    , m_scaleX(1.0f)
    , m_isCombined(false)
    , m_needsFontUpdate(false)
{
}

void LayoutTextCombine::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    setStyleInternal(ComputedStyle::clone(styleRef()));
    LayoutText::styleDidChange(diff, oldStyle);

    updateIsCombined();
}

void LayoutTextCombine::setTextInternal(PassRefPtr<StringImpl> text)
{
    LayoutText::setTextInternal(text);

    updateIsCombined();
}

float LayoutTextCombine::width(unsigned from, unsigned length, const Font& font, LayoutUnit xPosition, TextDirection direction, HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* glyphBounds) const
{
    if (!length)
        return 0;

    if (hasEmptyText())
        return 0;

    if (m_isCombined)
        return font.getFontDescription().computedSize();

    return LayoutText::width(from, length, font, xPosition, direction, fallbackFonts, glyphBounds);
}

void scaleHorizontallyAndTranslate(GraphicsContext& context, float scaleX, float centerX, float offsetX, float offsetY)
{
    context.concatCTM(AffineTransform(scaleX, 0, 0, 1, centerX * (1.0f - scaleX) + offsetX * scaleX, offsetY));
}

void LayoutTextCombine::transformToInlineCoordinates(GraphicsContext& context, const LayoutRect& boxRect, bool clip) const
{
    ASSERT(!m_needsFontUpdate);
    ASSERT(m_isCombined);

    // On input, the |boxRect| is:
    // 1. Horizontal flow, rotated from the main vertical flow coordinate using
    //    TextPainter::rotation().
    // 2. height() is cell-height, which includes internal leading. This equals
    //    to A+D, and to em+internal leading.
    // 3. width() is the same as m_combinedTextWidth.
    // 4. Left is (right-edge - height()).
    // 5. Top is where char-top (not include internal leading) should be.
    // See https://support.microsoft.com/en-us/kb/32667.
    // We move it so that it comes to the center of em excluding internal
    // leading.

    float cellHeight = boxRect.height();
    float internalLeading = styleRef().font().primaryFont()->internalLeading();
    float offsetY = -internalLeading / 2;
    float width;
    if (m_scaleX >= 1.0f) {
        // Fast path, more than 90% of cases
        ASSERT(m_scaleX == 1.0f);
        float offsetX = (cellHeight - m_combinedTextWidth) / 2;
        context.concatCTM(AffineTransform::translation(offsetX, offsetY));
        width = boxRect.width();
    } else {
        ASSERT(m_scaleX > 0.0f);
        float centerX = boxRect.x() + cellHeight / 2;
        width = m_combinedTextWidth / m_scaleX;
        float offsetX = (cellHeight - width) / 2;
        scaleHorizontallyAndTranslate(context, m_scaleX, centerX, offsetX, offsetY);
    }

    if (clip)
        context.clip(FloatRect(boxRect.x(), boxRect.y(), width, cellHeight));
}

void LayoutTextCombine::updateIsCombined()
{
    // CSS3 spec says text-combine works only in vertical writing mode.
    m_isCombined = !style()->isHorizontalWritingMode()
        // Nothing to combine.
        && !hasEmptyText();

    if (m_isCombined)
        m_needsFontUpdate = true;
}

void LayoutTextCombine::updateFont()
{
    if (!m_needsFontUpdate)
        return;

    m_needsFontUpdate = false;

    if (!m_isCombined)
        return;

    unsigned offset = 0;
    TextRun run = constructTextRun(originalFont(), this, offset, textLength(), styleRef(), style()->direction());
    FontDescription description = originalFont().getFontDescription();
    float emWidth = description.computedSize();
    if (!(style()->textDecorationsInEffect() & (TextDecorationUnderline | TextDecorationOverline)))
        emWidth *= textCombineMargin;

    description.setOrientation(FontOrientation::Horizontal); // We are going to draw combined text horizontally.
    m_combinedTextWidth = originalFont().width(run);

    FontSelector* fontSelector = style()->font().getFontSelector();

    bool shouldUpdateFont = mutableStyleRef().setFontDescription(description); // Need to change font orientation to horizontal.

    if (m_combinedTextWidth <= emWidth) {
        m_scaleX = 1.0f;
    } else {
        // Need to try compressed glyphs.
        static const FontWidthVariant widthVariants[] = { HalfWidth, ThirdWidth, QuarterWidth };
        for (size_t i = 0 ; i < WTF_ARRAY_LENGTH(widthVariants) ; ++i) {
            description.setWidthVariant(widthVariants[i]);
            Font compressedFont = Font(description);
            compressedFont.update(fontSelector);
            float runWidth = compressedFont.width(run);
            if (runWidth <= emWidth) {
                m_combinedTextWidth = runWidth;

                // Replace my font with the new one.
                shouldUpdateFont = mutableStyleRef().setFontDescription(description);
                break;
            }
        }

        // If width > ~1em, shrink to fit within ~1em, otherwise render without scaling (no expansion)
        // http://dev.w3.org/csswg/css-writing-modes-3/#text-combine-compression
        if (m_combinedTextWidth > emWidth) {
            m_scaleX = emWidth / m_combinedTextWidth;
            m_combinedTextWidth = emWidth;
        } else {
            m_scaleX = 1.0f;
        }
    }

    if (shouldUpdateFont)
        style()->font().update(fontSelector);
}

} // namespace blink
