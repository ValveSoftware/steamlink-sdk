/*
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

#include "config.h"

#include "core/rendering/svg/SVGTextLayoutEngineSpacing.h"

#include "core/svg/SVGLengthContext.h"
#include "platform/fonts/Character.h"
#include "platform/fonts/Font.h"

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontData.h"
#include "core/svg/SVGFontElement.h"
#include "core/svg/SVGFontFaceElement.h"
#endif

namespace WebCore {

SVGTextLayoutEngineSpacing::SVGTextLayoutEngineSpacing(const Font& font, float effectiveZoom)
    : m_font(font)
    , m_lastCharacter(0)
    , m_effectiveZoom(effectiveZoom)
#if ENABLE(SVG_FONTS)
    , m_lastGlyph(0)
#endif
{
    ASSERT(m_effectiveZoom);
}

float SVGTextLayoutEngineSpacing::calculateSVGKerning(bool isVerticalText, Glyph currentGlyph)
{
#if ENABLE(SVG_FONTS)
    const SimpleFontData* fontData = m_font.primaryFont();
    if (!fontData->isSVGFont()) {
        m_lastGlyph = 0;
        return 0;
    }

    ASSERT(fontData->isCustomFont());
    ASSERT(fontData->isSVGFont());

    RefPtr<CustomFontData> customFontData = fontData->customFontData();
    const SVGFontData* svgFontData = static_cast<const SVGFontData*>(customFontData.get());
    SVGFontFaceElement* svgFontFace = svgFontData->svgFontFaceElement();
    ASSERT(svgFontFace);

    SVGFontElement* svgFont = svgFontFace->associatedFontElement();
    if (!svgFont) {
        m_lastGlyph = 0;
        return 0;
    }

    float kerning = 0;
    if (m_lastGlyph) {
        if (isVerticalText)
            kerning = svgFont->verticalKerningForPairOfGlyphs(m_lastGlyph, currentGlyph);
        else
            kerning = svgFont->horizontalKerningForPairOfGlyphs(m_lastGlyph, currentGlyph);

        kerning *= m_font.fontDescription().computedSize() / m_font.fontMetrics().unitsPerEm();
    }

    m_lastGlyph = currentGlyph;
    return kerning;
#else
    return 0;
#endif
}

float SVGTextLayoutEngineSpacing::calculateCSSSpacing(UChar currentCharacter)
{
    UChar lastCharacter = m_lastCharacter;
    m_lastCharacter = currentCharacter;

    if (!m_font.fontDescription().letterSpacing() && !m_font.fontDescription().wordSpacing())
        return 0;

    float spacing = m_font.fontDescription().letterSpacing();
    if (currentCharacter && lastCharacter && m_font.fontDescription().wordSpacing()) {
        if (Character::treatAsSpace(currentCharacter) && !Character::treatAsSpace(lastCharacter))
            spacing += m_font.fontDescription().wordSpacing();
    }

    if (m_effectiveZoom != 1)
        spacing = spacing / m_effectiveZoom;

    return spacing;
}

}
