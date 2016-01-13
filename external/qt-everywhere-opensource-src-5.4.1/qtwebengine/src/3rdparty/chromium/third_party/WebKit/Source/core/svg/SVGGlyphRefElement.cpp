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
#include "core/svg/SVGGlyphRefElement.h"

#include "core/XLinkNames.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

inline SVGGlyphRefElement::SVGGlyphRefElement(Document& document)
    : SVGElement(SVGNames::glyphRefTag, document)
    , SVGURIReference(this)
    , m_x(0)
    , m_y(0)
    , m_dx(0)
    , m_dy(0)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGGlyphRefElement)

bool SVGGlyphRefElement::hasValidGlyphElement(AtomicString& glyphName) const
{
    // FIXME: We only support xlink:href so far.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    Element* element = targetElementFromIRIString(getAttribute(XLinkNames::hrefAttr), document(), &glyphName);
    return isSVGGlyphElement(element);
}

template<typename CharType>
void SVGGlyphRefElement::parseAttributeInternal(const QualifiedName& name, const AtomicString& value)
{
    const CharType* ptr = value.isEmpty() ? 0 : value.string().getCharacters<CharType>();
    const CharType* end = ptr + value.length();

    // FIXME: We need some error handling here.
    SVGParsingError parseError = NoError;
    if (name == SVGNames::xAttr) {
        parseNumber(ptr, end, m_x);
    } else if (name == SVGNames::yAttr) {
        parseNumber(ptr, end, m_y);
    } else if (name == SVGNames::dxAttr) {
        parseNumber(ptr, end, m_dx);
    } else if (name == SVGNames::dyAttr) {
        parseNumber(ptr, end, m_dy);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else {
        SVGElement::parseAttribute(name, value);
    }
    reportAttributeParsingError(parseError, name, value);
}

void SVGGlyphRefElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (value.isEmpty() || value.is8Bit())
        parseAttributeInternal<LChar>(name, value);
    else
        parseAttributeInternal<UChar>(name, value);
}

const AtomicString& SVGGlyphRefElement::glyphRef() const
{
    return fastGetAttribute(SVGNames::glyphRefAttr);
}

void SVGGlyphRefElement::setGlyphRef(const AtomicString&)
{
    // FIXME: Set and honor attribute change.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
}

void SVGGlyphRefElement::setX(float x)
{
    // FIXME: Honor attribute change.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    m_x = x;
}

void SVGGlyphRefElement::setY(float y)
{
    // FIXME: Honor attribute change.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    m_y = y;
}

void SVGGlyphRefElement::setDx(float dx)
{
    // FIXME: Honor attribute change.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    m_dx = dx;
}

void SVGGlyphRefElement::setDy(float dy)
{
    // FIXME: Honor attribute change.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    m_dy = dy;
}

}

#endif
