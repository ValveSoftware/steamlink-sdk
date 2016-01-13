/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGInlineTextBox_h
#define SVGInlineTextBox_h

#include "core/rendering/InlineTextBox.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGTextLayoutEngine.h"

namespace WebCore {

class RenderSVGResource;

class SVGInlineTextBox FINAL : public InlineTextBox {
public:
    SVGInlineTextBox(RenderObject&);

    virtual bool isSVGInlineTextBox() const OVERRIDE { return true; }

    virtual float virtualLogicalHeight() const OVERRIDE { return m_logicalHeight; }
    void setLogicalHeight(float height) { m_logicalHeight = height; }

    virtual int offsetForPosition(float x, bool includePartialGlyphs = true) const OVERRIDE;
    virtual float positionForOffset(int offset) const OVERRIDE;

    void paintSelectionBackground(PaintInfo&);
    virtual void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) OVERRIDE;
    virtual LayoutRect localSelectionRect(int startPosition, int endPosition) OVERRIDE;

    bool mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment&, int& startPosition, int& endPosition) const;

    virtual FloatRect calculateBoundaries() const OVERRIDE;

    void clearTextFragments() { m_textFragments.clear(); }
    Vector<SVGTextFragment>& textFragments() { return m_textFragments; }
    const Vector<SVGTextFragment>& textFragments() const { return m_textFragments; }

    virtual void dirtyLineBoxes() OVERRIDE;

    bool startsNewTextChunk() const { return m_startsNewTextChunk; }
    void setStartsNewTextChunk(bool newTextChunk) { m_startsNewTextChunk = newTextChunk; }

    int offsetForPositionInFragment(const SVGTextFragment&, float position, bool includePartialGlyphs) const;
    FloatRect selectionRectForTextFragment(const SVGTextFragment&, int fragmentStartPosition, int fragmentEndPosition, RenderStyle*);

private:
    TextRun constructTextRun(RenderStyle*, const SVGTextFragment&) const;

    bool acquirePaintingResource(GraphicsContext*&, float scalingFactor, RenderObject*,
        RenderStyle*, RenderSVGResourceModeFlags);
    void releasePaintingResource(GraphicsContext*&, const Path*, RenderSVGResourceModeFlags);

    bool prepareGraphicsContextForTextPainting(GraphicsContext*&, float scalingFactor, TextRun&,
        RenderStyle*, RenderSVGResourceModeFlags);
    void restoreGraphicsContextAfterTextPainting(GraphicsContext*&, TextRun&, RenderSVGResourceModeFlags);

    void paintDecoration(GraphicsContext*, TextDecoration, const SVGTextFragment&);
    void paintDecorationWithStyle(GraphicsContext*, TextDecoration, const SVGTextFragment&,
        RenderObject* decorationRenderer, RenderSVGResourceModeFlags);
    void paintTextWithShadows(GraphicsContext*, RenderStyle*, TextRun&, const SVGTextFragment&,
        int startPosition, int endPosition, RenderSVGResourceModeFlags);
    void paintText(GraphicsContext*, RenderStyle*, RenderStyle* selectionStyle, const SVGTextFragment&,
        RenderSVGResourceModeFlags, bool hasSelection, bool paintSelectedTextOnly);

    virtual void paintDocumentMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, RenderStyle*, const Font&, bool) OVERRIDE FINAL;
    virtual void paintTextMatchMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, RenderStyle*, const Font&) OVERRIDE FINAL;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) OVERRIDE;

private:
    float m_logicalHeight;
    bool m_startsNewTextChunk : 1;
    RenderSVGResource* m_paintingResource;
    Vector<SVGTextFragment> m_textFragments;
};

DEFINE_INLINE_BOX_TYPE_CASTS(SVGInlineTextBox);

} // namespace WebCore

#endif // SVGInlineTextBox_h
