/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SVGGlyphMap_h
#define SVGGlyphMap_h

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGParserUtilities.h"
#include "platform/fonts/Latin1TextIterator.h"
#include "platform/fonts/SVGGlyph.h"
#include "platform/text/SurrogatePairAwareTextIterator.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace WebCore {

struct GlyphMapNode;

typedef HashMap<UChar32, RefPtr<GlyphMapNode> > GlyphMapLayer;

struct GlyphMapNode : public RefCounted<GlyphMapNode> {
private:
    GlyphMapNode() { }
public:
    static PassRefPtr<GlyphMapNode> create() { return adoptRef(new GlyphMapNode); }

    Vector<SVGGlyph> glyphs;

    GlyphMapLayer children;
};

class SVGGlyphMap {
public:
    SVGGlyphMap() : m_currentPriority(0) { }

    void addGlyph(const String& glyphIdentifier, const String& unicodeString, SVGGlyph glyph)
    {
        ASSERT(!glyphIdentifier.isEmpty() || !unicodeString.isEmpty());

        bool hasGlyphIdentifier = !glyphIdentifier.isEmpty();
        if (unicodeString.isEmpty()) {
            // Register glyphs with 'id's in the id glyph map and in the glyph table.
            ASSERT(hasGlyphIdentifier);
            appendToGlyphTable(glyph);
            m_idGlyphs.add(glyphIdentifier, glyph.tableEntry);
            return;
        }

        unsigned length = unicodeString.length();

        RefPtr<GlyphMapNode> node;
        if (unicodeString.is8Bit()) {
            Latin1TextIterator textIterator(unicodeString.characters8(), 0, length, length);
            node = findOrCreateNode(textIterator);
        } else {
            SurrogatePairAwareTextIterator textIterator(unicodeString.characters16(), 0, length, length);
            node = findOrCreateNode(textIterator);
        }
        if (!node)
            return;

        // Register glyph associated with an unicode string into the glyph map.
        node->glyphs.append(glyph);
        SVGGlyph& lastGlyph = node->glyphs.last();
        lastGlyph.priority = m_currentPriority++;
        lastGlyph.unicodeStringLength = length;

        // If the glyph is named, also add it to the named glyph name, and to the glyph table in both cases.
        appendToGlyphTable(lastGlyph);
        if (!lastGlyph.glyphName.isEmpty())
            m_namedGlyphs.add(lastGlyph.glyphName, lastGlyph.tableEntry);
        if (hasGlyphIdentifier)
            m_idGlyphs.add(glyphIdentifier, lastGlyph.tableEntry);
    }

    void appendToGlyphTable(SVGGlyph& glyph)
    {
        size_t tableEntry = m_glyphTable.size();
        ASSERT(tableEntry < std::numeric_limits<unsigned short>::max());

        // The first table entry starts with 1. 0 denotes an unknown glyph.
        glyph.tableEntry = tableEntry + 1;
        m_glyphTable.append(glyph);
    }

    static inline bool compareGlyphPriority(const SVGGlyph& first, const SVGGlyph& second)
    {
        return first.priority < second.priority;
    }

    void collectGlyphsForString(const String& string, Vector<SVGGlyph>& glyphs)
    {
        unsigned length = string.length();

        if (!length)
            return;

        if (string.is8Bit()) {
            Latin1TextIterator textIterator(string.characters8(), 0, length, length);
            collectGlyphsForIterator(textIterator, glyphs);
        } else {
            SurrogatePairAwareTextIterator textIterator(string.characters16(), 0, length, length);
            collectGlyphsForIterator(textIterator, glyphs);
        }

        std::sort(glyphs.begin(), glyphs.end(), compareGlyphPriority);
    }

    void collectGlyphsForStringExact(const String& string, Vector<SVGGlyph>& glyphs) const
    {
        unsigned length = string.length();

        if (!length)
            return;

        RefPtr<GlyphMapNode> node;
        if (string.is8Bit()) {
            Latin1TextIterator textIterator(string.characters8(), 0, length, length);
            node = findNode(textIterator);
        } else {
            SurrogatePairAwareTextIterator textIterator(string.characters16(), 0, length, length);
            node = findNode(textIterator);
        }

        if (node)
            glyphs.appendVector(node->glyphs);
    }

    void collectGlyphsForUnicodeRange(const UnicodeRange& unicodeRange, Vector<SVGGlyph>& glyphs) const
    {
        for (unsigned character = unicodeRange.first; character <= unicodeRange.second; ++character) {
            if (RefPtr<GlyphMapNode> node = m_rootLayer.get(character))
                glyphs.appendVector(node->glyphs);
        }
    }

    void clear()
    {
        m_rootLayer.clear();
        m_glyphTable.clear();
        m_idGlyphs.clear();
        m_namedGlyphs.clear();
        m_currentPriority = 0;
    }

    void dropNamedGlyphMap()
    {
        m_namedGlyphs.clear();
    }

    const SVGGlyph& svgGlyphForGlyph(Glyph glyph) const
    {
        if (!glyph || glyph > m_glyphTable.size()) {
            DEFINE_STATIC_LOCAL(SVGGlyph, defaultGlyph, ());
            return defaultGlyph;
        }
        return m_glyphTable[glyph - 1];
    }

    const SVGGlyph& glyphIdentifierForAltGlyphReference(const String& glyphIdentifier) const
    {
        return svgGlyphForGlyph(m_idGlyphs.get(glyphIdentifier));
    }

    const SVGGlyph& glyphIdentifierForGlyphName(const String& glyphName) const
    {
        return svgGlyphForGlyph(m_namedGlyphs.get(glyphName));
    }

private:
    template<typename Iterator>
    PassRefPtr<GlyphMapNode> findOrCreateNode(Iterator& textIterator)
    {
        GlyphMapLayer* currentLayer = &m_rootLayer;

        RefPtr<GlyphMapNode> node;
        UChar32 character = 0;
        unsigned clusterLength = 0;
        while (textIterator.consume(character, clusterLength)) {
            node = currentLayer->get(character);
            if (!node) {
                node = GlyphMapNode::create();
                currentLayer->set(character, node);
            }
            currentLayer = &node->children;
            textIterator.advance(clusterLength);
        }

        return node.release();
    }

    template<typename Iterator>
    PassRefPtr<GlyphMapNode> findNode(Iterator& textIterator) const
    {
        const GlyphMapLayer* currentLayer = &m_rootLayer;

        RefPtr<GlyphMapNode> node;
        UChar32 character = 0;
        unsigned clusterLength = 0;
        while (textIterator.consume(character, clusterLength)) {
            node = currentLayer->get(character);
            if (!node)
                break;
            currentLayer = &node->children;
            textIterator.advance(clusterLength);
        }

        return node.release();
    }

    template<typename Iterator>
    void collectGlyphsForIterator(Iterator& textIterator, Vector<SVGGlyph>& glyphs)
    {
        GlyphMapLayer* currentLayer = &m_rootLayer;

        UChar32 character = 0;
        unsigned clusterLength = 0;
        while (textIterator.consume(character, clusterLength)) {
            RefPtr<GlyphMapNode> node = currentLayer->get(character);
            if (!node)
                break;
            glyphs.appendVector(node->glyphs);
            currentLayer = &node->children;
            textIterator.advance(clusterLength);
        }
    }

    GlyphMapLayer m_rootLayer;
    Vector<SVGGlyph> m_glyphTable;
    HashMap<String, Glyph> m_namedGlyphs;
    HashMap<String, Glyph> m_idGlyphs;
    int m_currentPriority;
};

}

#endif // ENABLE(SVG_FONTS)
#endif // SVGGlyphMap_h
