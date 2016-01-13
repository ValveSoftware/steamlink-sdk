/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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

#ifndef RenderBR_h
#define RenderBR_h

#include "core/rendering/RenderText.h"

/*
 * The whole class here is a hack to get <br> working, as long as we don't have support for
 * CSS2 :before and :after pseudo elements
 */
namespace WebCore {

class RenderBR FINAL : public RenderText {
public:
    explicit RenderBR(Node*);
    virtual ~RenderBR();

    virtual const char* renderName() const OVERRIDE { return "RenderBR"; }

    virtual LayoutRect selectionRectForPaintInvalidation(const RenderLayerModelObject* /*paintInvalidationContainer*/, bool /*clipToVisibleContent*/) OVERRIDE { return LayoutRect(); }

    virtual float width(unsigned /*from*/, unsigned /*len*/, const Font&, float /*xPos*/, TextDirection, HashSet<const SimpleFontData*>* = 0 /*fallbackFonts*/ , GlyphOverflow* = 0) const OVERRIDE { return 0; }
    virtual float width(unsigned /*from*/, unsigned /*len*/, float /*xpos*/, TextDirection, bool = false /*firstLine*/, HashSet<const SimpleFontData*>* = 0 /*fallbackFonts*/, GlyphOverflow* = 0) const OVERRIDE { return 0; }

    int lineHeight(bool firstLine) const;

    // overrides
    virtual bool isBR() const OVERRIDE { return true; }

    virtual int caretMinOffset() const OVERRIDE;
    virtual int caretMaxOffset() const OVERRIDE;

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) OVERRIDE FINAL;

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderBR, isBR());

} // namespace WebCore

#endif // RenderBR_h
