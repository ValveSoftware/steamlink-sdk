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

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGGlyphElement.h"
#include "core/svg/SVGHKernElement.h"
#include "core/svg/SVGMissingGlyphElement.h"
#include "core/svg/SVGVKernElement.h"
#include "wtf/ASCIICType.h"

namespace WebCore {

inline SVGFontElement::SVGFontElement(Document& document)
    : SVGElement(SVGNames::fontTag, document)
    , m_missingGlyph(0)
    , m_isGlyphCacheValid(false)
{
    ScriptWrappable::init(this);

    UseCounter::count(document, UseCounter::SVGFontElement);
}

DEFINE_NODE_FACTORY(SVGFontElement)

void SVGFontElement::invalidateGlyphCache()
{
    if (m_isGlyphCacheValid) {
        m_glyphMap.clear();
        m_horizontalKerningTable.clear();
        m_verticalKerningTable.clear();
    }
    m_isGlyphCacheValid = false;
}

void SVGFontElement::registerLigaturesInGlyphCache(Vector<String>& ligatures)
{
    ASSERT(!ligatures.isEmpty());

    // Register each character of a ligature in the map, if not present.
    // Eg. If only a "fi" ligature is present, but not "f" and "i", the
    // GlyphPage will not contain any entries for "f" and "i", so the
    // SVGFont is not used to render the text "fi1234". Register an
    // empty SVGGlyph with the character, so the SVG Font will be used
    // to render the text. If someone tries to render "f2" the SVG Font
    // will not be able to find a glyph for "f", but handles the fallback
    // character substitution properly through glyphDataForCharacter().
    Vector<SVGGlyph> glyphs;
    size_t ligaturesSize = ligatures.size();
    for (size_t i = 0; i < ligaturesSize; ++i) {
        const String& unicode = ligatures[i];

        unsigned unicodeLength = unicode.length();
        ASSERT(unicodeLength > 1);

        for (unsigned i = 0; i < unicodeLength; ++i) {
            String lookupString = unicode.substring(i, 1);
            m_glyphMap.collectGlyphsForString(lookupString, glyphs);
            if (!glyphs.isEmpty()) {
                glyphs.clear();
                continue;
            }

            // This glyph is never meant to be used for rendering, only as identifier as a part of a ligature.
            SVGGlyph newGlyphPart;
            newGlyphPart.isPartOfLigature = true;
            m_glyphMap.addGlyph(String(), lookupString, newGlyphPart);
        }
    }
}

static inline KerningPairKey makeKerningPairKey(Glyph glyphId1, Glyph glyphId2)
{
    return glyphId1 << 16 | glyphId2;
}

Vector<SVGGlyph> SVGFontElement::buildGlyphList(const UnicodeRanges& unicodeRanges, const HashSet<String>& unicodeNames, const HashSet<String>& glyphNames) const
{
    Vector<SVGGlyph> glyphs;
    if (!unicodeRanges.isEmpty()) {
        const UnicodeRanges::const_iterator end = unicodeRanges.end();
        for (UnicodeRanges::const_iterator it = unicodeRanges.begin(); it != end; ++it)
            m_glyphMap.collectGlyphsForUnicodeRange(*it, glyphs);
    }
    if (!unicodeNames.isEmpty()) {
        const HashSet<String>::const_iterator end = unicodeNames.end();
        for (HashSet<String>::const_iterator it = unicodeNames.begin(); it != end; ++it)
            m_glyphMap.collectGlyphsForStringExact(*it, glyphs);
    }
    if (!glyphNames.isEmpty()) {
        const HashSet<String>::const_iterator end = glyphNames.end();
        for (HashSet<String>::const_iterator it = glyphNames.begin(); it != end; ++it) {
            const SVGGlyph& glyph = m_glyphMap.glyphIdentifierForGlyphName(*it);
            if (glyph.tableEntry)
                glyphs.append(glyph);
        }
    }
    return glyphs;
}

void SVGFontElement::addPairsToKerningTable(const SVGKerningPair& kerningPair, KerningTable& kerningTable)
{
    Vector<SVGGlyph> glyphsLhs = buildGlyphList(kerningPair.unicodeRange1, kerningPair.unicodeName1, kerningPair.glyphName1);
    Vector<SVGGlyph> glyphsRhs = buildGlyphList(kerningPair.unicodeRange2, kerningPair.unicodeName2, kerningPair.glyphName2);
    if (glyphsLhs.isEmpty() || glyphsRhs.isEmpty())
        return;
    size_t glyphsLhsSize = glyphsLhs.size();
    size_t glyphsRhsSize = glyphsRhs.size();
    // Enumerate all the valid kerning pairs, and add them to the table.
    for (size_t lhsIndex = 0; lhsIndex < glyphsLhsSize; ++lhsIndex) {
        for (size_t rhsIndex = 0; rhsIndex < glyphsRhsSize; ++rhsIndex) {
            Glyph glyph1 = glyphsLhs[lhsIndex].tableEntry;
            Glyph glyph2 = glyphsRhs[rhsIndex].tableEntry;
            ASSERT(glyph1 && glyph2);
            kerningTable.add(makeKerningPairKey(glyph1, glyph2), kerningPair.kerning);
        }
    }
}

void SVGFontElement::buildKerningTable(const KerningPairVector& kerningPairs, KerningTable& kerningTable)
{
    size_t kerningPairsSize = kerningPairs.size();
    for (size_t i = 0; i < kerningPairsSize; ++i)
        addPairsToKerningTable(kerningPairs[i], kerningTable);
}

void SVGFontElement::ensureGlyphCache()
{
    if (m_isGlyphCacheValid)
        return;

    KerningPairVector horizontalKerningPairs;
    KerningPairVector verticalKerningPairs;

    SVGMissingGlyphElement* firstMissingGlyphElement = 0;
    Vector<String> ligatures;
    for (SVGElement* element = Traversal<SVGElement>::firstChild(*this); element; element = Traversal<SVGElement>::nextSibling(*element)) {
        if (isSVGGlyphElement(*element)) {
            SVGGlyphElement& glyph = toSVGGlyphElement(*element);
            AtomicString unicode = glyph.fastGetAttribute(SVGNames::unicodeAttr);
            AtomicString glyphId = glyph.getIdAttribute();
            if (glyphId.isEmpty() && unicode.isEmpty())
                continue;

            m_glyphMap.addGlyph(glyphId, unicode, glyph.buildGlyphIdentifier());

            // Register ligatures, if needed, don't mix up with surrogate pairs though!
            if (unicode.length() > 1 && !U16_IS_SURROGATE(unicode[0]))
                ligatures.append(unicode.string());
        } else if (isSVGHKernElement(*element)) {
            toSVGHKernElement(*element).buildHorizontalKerningPair(horizontalKerningPairs);
        } else if (isSVGVKernElement(*element)) {
            toSVGVKernElement(*element).buildVerticalKerningPair(verticalKerningPairs);
        } else if (isSVGMissingGlyphElement(*element) && !firstMissingGlyphElement) {
            firstMissingGlyphElement = toSVGMissingGlyphElement(element);
        }
    }

    // Build the kerning tables.
    buildKerningTable(horizontalKerningPairs, m_horizontalKerningTable);
    buildKerningTable(verticalKerningPairs, m_verticalKerningTable);

    // The glyph-name->glyph-id map won't be needed/used after having built the kerning table(s).
    m_glyphMap.dropNamedGlyphMap();

    // Register each character of each ligature, if needed.
    if (!ligatures.isEmpty())
        registerLigaturesInGlyphCache(ligatures);

    // Register missing-glyph element, if present.
    if (firstMissingGlyphElement) {
        SVGGlyph svgGlyph = SVGGlyphElement::buildGenericGlyphIdentifier(firstMissingGlyphElement);
        m_glyphMap.appendToGlyphTable(svgGlyph);
        m_missingGlyph = svgGlyph.tableEntry;
        ASSERT(m_missingGlyph > 0);
    }

    m_isGlyphCacheValid = true;
}

static float kerningForPairOfGlyphs(const KerningTable& kerningTable, Glyph glyphId1, Glyph glyphId2)
{
    KerningTable::const_iterator result = kerningTable.find(makeKerningPairKey(glyphId1, glyphId2));
    if (result != kerningTable.end())
        return result->value;

    return 0;
}

float SVGFontElement::horizontalKerningForPairOfGlyphs(Glyph glyphId1, Glyph glyphId2) const
{
    if (m_horizontalKerningTable.isEmpty())
        return 0;

    return kerningForPairOfGlyphs(m_horizontalKerningTable, glyphId1, glyphId2);
}

float SVGFontElement::verticalKerningForPairOfGlyphs(Glyph glyphId1, Glyph glyphId2) const
{
    if (m_verticalKerningTable.isEmpty())
        return 0;

    return kerningForPairOfGlyphs(m_verticalKerningTable, glyphId1, glyphId2);
}

void SVGFontElement::collectGlyphsForString(const String& string, Vector<SVGGlyph>& glyphs)
{
    ensureGlyphCache();
    m_glyphMap.collectGlyphsForString(string, glyphs);
}

void SVGFontElement::collectGlyphsForAltGlyphReference(const String& glyphIdentifier, Vector<SVGGlyph>& glyphs)
{
    ensureGlyphCache();
    // FIXME: We only support glyphName -> single glyph mapping so far.
    glyphs.append(m_glyphMap.glyphIdentifierForAltGlyphReference(glyphIdentifier));
}

SVGGlyph SVGFontElement::svgGlyphForGlyph(Glyph glyph)
{
    ensureGlyphCache();
    return m_glyphMap.svgGlyphForGlyph(glyph);
}

Glyph SVGFontElement::missingGlyph()
{
    ensureGlyphCache();
    return m_missingGlyph;
}

}

#endif // ENABLE(SVG_FONTS)
