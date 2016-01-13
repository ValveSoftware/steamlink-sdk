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

#ifndef SVGGlyphRefElement_h
#define SVGGlyphRefElement_h

#if ENABLE(SVG_FONTS)
#include "core/SVGNames.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

class SVGGlyphRefElement FINAL : public SVGElement,
                                 public SVGURIReference {
public:
    DECLARE_NODE_FACTORY(SVGGlyphRefElement);

    bool hasValidGlyphElement(AtomicString& glyphName) const;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    // DOM interface
    const AtomicString& glyphRef() const;
    void setGlyphRef(const AtomicString&);
    float x() const { return m_x; }
    void setX(float);
    float y() const { return m_y; }
    void setY(float);
    float dx() const { return m_dx; }
    void setDx(float);
    float dy() const { return m_dy; }
    void setDy(float);

private:
    explicit SVGGlyphRefElement(Document&);

    template<typename CharType>
    void parseAttributeInternal(const QualifiedName&, const AtomicString&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }


    float m_x;
    float m_y;
    float m_dx;
    float m_dy;
};

}

#endif
#endif
