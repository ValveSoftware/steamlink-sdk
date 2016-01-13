/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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

#ifndef RenderSVGInlineText_h
#define RenderSVGInlineText_h

#include "core/rendering/RenderText.h"
#include "core/rendering/svg/SVGTextLayoutAttributes.h"

namespace WebCore {

class RenderSVGInlineText FINAL : public RenderText {
public:
    RenderSVGInlineText(Node*, PassRefPtr<StringImpl>);

    bool characterStartsNewTextChunk(int position) const;
    SVGTextLayoutAttributes* layoutAttributes() { return &m_layoutAttributes; }

    float scalingFactor() const { return m_scalingFactor; }
    const Font& scaledFont() const { return m_scaledFont; }
    void updateScaledFont();
    static void computeNewScaledFontForStyle(RenderObject*, const RenderStyle*, float& scalingFactor, Font& scaledFont);

    // Preserves floating point precision for the use in DRT. It knows how to round and does a better job than enclosingIntRect.
    FloatRect floatLinesBoundingBox() const;

private:
    virtual const char* renderName() const OVERRIDE { return "RenderSVGInlineText"; }

    virtual void setTextInternal(PassRefPtr<StringImpl>) OVERRIDE;
    virtual void styleDidChange(StyleDifference, const RenderStyle*) OVERRIDE;

    virtual FloatRect objectBoundingBox() const OVERRIDE { return floatLinesBoundingBox(); }

    virtual bool isSVGInlineText() const OVERRIDE { return true; }
    virtual bool isSVG() const OVERRIDE { return true; }

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) OVERRIDE;
    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) OVERRIDE;
    virtual IntRect linesBoundingBox() const OVERRIDE;
    virtual InlineTextBox* createTextBox() OVERRIDE;

    float m_scalingFactor;
    Font m_scaledFont;
    SVGTextLayoutAttributes m_layoutAttributes;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGInlineText, isSVGInlineText());

}

#endif // RenderSVGInlineText_h
