/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGFontElement_h
#define SVGFontElement_h

#if ENABLE(SVG_FONTS)
#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGGlyphMap.h"
#include "core/svg/SVGParserUtilities.h"

namespace WebCore {

// Describe an SVG <hkern>/<vkern> element
struct SVGKerningPair {
    float kerning;
    UnicodeRanges unicodeRange1;
    UnicodeRanges unicodeRange2;
    HashSet<String> unicodeName1;
    HashSet<String> unicodeName2;
    HashSet<String> glyphName1;
    HashSet<String> glyphName2;

    SVGKerningPair()
        : kerning(0)
    {
    }
};

typedef unsigned KerningPairKey;
typedef Vector<SVGKerningPair> KerningPairVector;
typedef HashMap<KerningPairKey, float> KerningTable;

class SVGFontElement FINAL : public SVGElement {
public:
    DECLARE_NODE_FACTORY(SVGFontElement);

    void invalidateGlyphCache();
    void collectGlyphsForString(const String&, Vector<SVGGlyph>&);
    void collectGlyphsForAltGlyphReference(const String&, Vector<SVGGlyph>&);

    float horizontalKerningForPairOfGlyphs(Glyph, Glyph) const;
    float verticalKerningForPairOfGlyphs(Glyph, Glyph) const;

    // Used by SimpleFontData/WidthIterator.
    SVGGlyph svgGlyphForGlyph(Glyph);
    Glyph missingGlyph();

private:
    explicit SVGFontElement(Document&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    void ensureGlyphCache();
    void registerLigaturesInGlyphCache(Vector<String>&);
    Vector<SVGGlyph> buildGlyphList(const UnicodeRanges&, const HashSet<String>& unicodeNames, const HashSet<String>& glyphNames) const;
    void addPairsToKerningTable(const SVGKerningPair&, KerningTable&);
    void buildKerningTable(const KerningPairVector&, KerningTable&);


    KerningTable m_horizontalKerningTable;
    KerningTable m_verticalKerningTable;
    SVGGlyphMap m_glyphMap;
    Glyph m_missingGlyph;
    bool m_isGlyphCacheValid;
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif
