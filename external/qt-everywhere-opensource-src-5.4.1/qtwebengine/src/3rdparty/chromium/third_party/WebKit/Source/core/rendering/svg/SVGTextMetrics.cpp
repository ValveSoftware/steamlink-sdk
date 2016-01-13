/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "config.h"

#include "core/rendering/svg/SVGTextMetrics.h"

#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/SVGTextRunRenderingContext.h"

namespace WebCore {

SVGTextMetrics::SVGTextMetrics()
    : m_width(0)
    , m_height(0)
    , m_length(0)
    , m_glyph(0)
{
}

SVGTextMetrics::SVGTextMetrics(SVGTextMetrics::MetricsType)
    : m_width(0)
    , m_height(0)
    , m_length(1)
    , m_glyph(0)
{
}

SVGTextMetrics::SVGTextMetrics(RenderSVGInlineText* textRenderer, const TextRun& run)
{
    ASSERT(textRenderer);

    float scalingFactor = textRenderer->scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textRenderer->scaledFont();
    int length = 0;

    // Calculate width/height using the scaled font, divide this result by the scalingFactor afterwards.
    m_width = scaledFont.width(run, length, m_glyph) / scalingFactor;
    m_height = scaledFont.fontMetrics().floatHeight() / scalingFactor;

    ASSERT(length >= 0);
    m_length = static_cast<unsigned>(length);
}

TextRun SVGTextMetrics::constructTextRun(RenderSVGInlineText* text, unsigned position, unsigned length)
{
    ASSERT(text->style());
    return constructTextRun(text, position, length, text->style()->direction());
}

TextRun SVGTextMetrics::constructTextRun(RenderSVGInlineText* text, unsigned position, unsigned length, TextDirection textDirection)
{
    RenderStyle* style = text->style();
    ASSERT(style);

    TextRun run(static_cast<const LChar*>(0) // characters, will be set below if non-zero.
                , 0 // length, will be set below if non-zero.
                , 0 // xPos, only relevant with allowTabs=true
                , 0 // padding, only relevant for justified text, not relevant for SVG
                , TextRun::AllowTrailingExpansion
                , textDirection
                , isOverride(style->unicodeBidi()) /* directionalOverride */);

    if (length) {
        if (text->is8Bit())
            run.setText(text->characters8() + position, length);
        else
            run.setText(text->characters16() + position, length);
    }

    if (textRunNeedsRenderingContext(style->font()))
        run.setRenderingContext(SVGTextRunRenderingContext::create(text));

    run.disableRoundingHacks();

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(text->textLength() - position);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

SVGTextMetrics SVGTextMetrics::measureCharacterRange(RenderSVGInlineText* text, unsigned position, unsigned length, TextDirection textDirection)
{
    ASSERT(text);
    return SVGTextMetrics(text, constructTextRun(text, position, length, textDirection));
}

SVGTextMetrics SVGTextMetrics::measureCharacterRange(RenderSVGInlineText* text, unsigned position, unsigned length)
{
    ASSERT(text);
    return SVGTextMetrics(text, constructTextRun(text, position, length));
}

SVGTextMetrics::SVGTextMetrics(RenderSVGInlineText* text, unsigned position, unsigned length, float width, Glyph glyphNameGlyphId)
{
    ASSERT(text);

    bool needsContext = textRunNeedsRenderingContext(text->style()->font());
    float scalingFactor = text->scalingFactor();
    ASSERT(scalingFactor);

    m_width = width / scalingFactor;
    m_height = text->scaledFont().fontMetrics().floatHeight() / scalingFactor;
    m_glyph = needsContext ? glyphNameGlyphId : 0;

    m_length = length;
}

}
