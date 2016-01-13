/*
 * Copyright (C) 2011 Leo Yang <leoyang@webkit.org>
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
#include "core/svg/SVGAltGlyphItemElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/svg/SVGGlyphRefElement.h"

namespace WebCore {

inline SVGAltGlyphItemElement::SVGAltGlyphItemElement(Document& document)
    : SVGElement(SVGNames::altGlyphItemTag, document)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGAltGlyphItemElement)

bool SVGAltGlyphItemElement::hasValidGlyphElements(Vector<AtomicString>& glyphNames) const
{
    // Spec: http://www.w3.org/TR/SVG/text.html#AltGlyphItemElement
    // The ‘altGlyphItem’ element defines a candidate set of possible glyph substitutions.
    // The first ‘altGlyphItem’ element whose referenced glyphs are all available is chosen.
    // Its glyphs are rendered instead of the character(s) that are inside of the referencing
    // ‘altGlyph’ element.
    //
    // Here we fill glyphNames and return true only if all referenced glyphs are valid and
    // there is at least one glyph.
    for (SVGGlyphRefElement* glyph = Traversal<SVGGlyphRefElement>::firstChild(*this); glyph; glyph = Traversal<SVGGlyphRefElement>::nextSibling(*glyph)) {
        AtomicString referredGlyphName;
        if (glyph->hasValidGlyphElement(referredGlyphName)) {
            glyphNames.append(referredGlyphName);
        } else {
            glyphNames.clear();
            return false;
        }
    }
    return !glyphNames.isEmpty();
}

}

#endif
