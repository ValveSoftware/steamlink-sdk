/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGAltGlyphElement_h
#define SVGAltGlyphElement_h

#if ENABLE(SVG_FONTS)
#include "core/SVGNames.h"
#include "core/svg/SVGTextPositioningElement.h"
#include "core/svg/SVGURIReference.h"
#include "wtf/Vector.h"

namespace WebCore {

class ExceptionState;

class SVGAltGlyphElement FINAL : public SVGTextPositioningElement,
                                 public SVGURIReference {
public:
    DECLARE_NODE_FACTORY(SVGAltGlyphElement);

    const AtomicString& glyphRef() const;
    void setGlyphRef(const AtomicString&, ExceptionState&);
    const AtomicString& format() const;
    void setFormat(const AtomicString&, ExceptionState&);

    bool hasValidGlyphElements(Vector<AtomicString>& glyphNames) const;

private:
    explicit SVGAltGlyphElement(Document&);

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
};

} // namespace WebCore

#endif
#endif
