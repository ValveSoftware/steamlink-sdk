// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/InlineFlowBoxPainter.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void InlineFlowBoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutUnit lineTop, const LayoutUnit lineBottom)
{
    ASSERT(!shouldPaintSelfOutline(paintInfo.phase) && !shouldPaintDescendantOutlines(paintInfo.phase));

    LayoutRect overflowRect(m_inlineFlowBox.visualOverflowRect(lineTop, lineBottom));
    m_inlineFlowBox.flipForWritingMode(overflowRect);
    overflowRect.moveBy(paintOffset);

    if (!paintInfo.cullRect().intersectsCullRect(overflowRect))
        return;

    if (paintInfo.phase == PaintPhaseMask) {
        if (DrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_inlineFlowBox, DisplayItem::paintPhaseToDrawingType(paintInfo.phase)))
            return;
        DrawingRecorder recorder(paintInfo.context, m_inlineFlowBox, DisplayItem::paintPhaseToDrawingType(paintInfo.phase), pixelSnappedIntRect(overflowRect));
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        // Paint our background, border and box-shadow.
        paintBoxDecorationBackground(paintInfo, paintOffset, overflowRect);
    }

    // Paint our children.
    PaintInfo childInfo(paintInfo);
    for (InlineBox* curr = m_inlineFlowBox.firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->getLineLayoutItem().isText() || !curr->boxModelObject().hasSelfPaintingLayer())
            curr->paint(childInfo, paintOffset, lineTop, lineBottom);
    }
}

void InlineFlowBoxPainter::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect, SkXfermode::Mode op)
{
    // FIXME: This should be a for loop or similar. It's a little non-trivial to do so, however, since the layers need to be
    // painted in reverse order.
    if (fillLayer.next())
        paintFillLayers(paintInfo, c, *fillLayer.next(), rect, op);
    paintFillLayer(paintInfo, c, fillLayer, rect, op);
}

void InlineFlowBoxPainter::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect, SkXfermode::Mode op)
{
    LayoutBoxModelObject* boxModel = toLayoutBoxModelObject(LineLayoutAPIShim::layoutObjectFrom(m_inlineFlowBox.boxModelObject()));
    StyleImage* img = fillLayer.image();
    bool hasFillImage = img && img->canRender();
    if ((!hasFillImage && !m_inlineFlowBox.getLineLayoutItem().style()->hasBorderRadius()) || (!m_inlineFlowBox.prevLineBox() && !m_inlineFlowBox.nextLineBox()) || !m_inlineFlowBox.parent()) {
        BoxPainter::paintFillLayer(*boxModel, paintInfo, c, fillLayer, rect, BackgroundBleedNone, &m_inlineFlowBox, rect.size(), op);
    } else if (m_inlineFlowBox.getLineLayoutItem().style()->boxDecorationBreak() == BoxDecorationBreakClone) {
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        paintInfo.context.clip(pixelSnappedIntRect(rect));
        BoxPainter::paintFillLayer(*boxModel, paintInfo, c, fillLayer, rect, BackgroundBleedNone, &m_inlineFlowBox, rect.size(), op);
    } else {
        // We have a fill image that spans multiple lines.
        // FIXME: frameSize ought to be the same as rect.size().
        LayoutSize frameSize(m_inlineFlowBox.width(), m_inlineFlowBox.height());
        LayoutRect imageStripPaintRect = paintRectForImageStrip(rect.location(), frameSize, m_inlineFlowBox.getLineLayoutItem().style()->direction());
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        // TODO(chrishtr): this should likely be pixel-snapped.
        paintInfo.context.clip(pixelSnappedIntRect(rect));
        BoxPainter::paintFillLayer(*boxModel, paintInfo, c, fillLayer, imageStripPaintRect, BackgroundBleedNone, &m_inlineFlowBox, rect.size(), op);
    }
}

void InlineFlowBoxPainter::paintBoxShadow(const PaintInfo& info, const ComputedStyle& s, ShadowStyle shadowStyle, const LayoutRect& paintRect)
{
    if ((!m_inlineFlowBox.prevLineBox() && !m_inlineFlowBox.nextLineBox()) || !m_inlineFlowBox.parent()) {
        BoxPainter::paintBoxShadow(info, paintRect, s, shadowStyle);
    } else {
        // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
        // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
        BoxPainter::paintBoxShadow(info, paintRect, s, shadowStyle, m_inlineFlowBox.includeLogicalLeftEdge(), m_inlineFlowBox.includeLogicalRightEdge());
    }
}

static LayoutRect clipRectForNinePieceImageStrip(const InlineFlowBox& box, const NinePieceImage& image, const LayoutRect& paintRect)
{
    LayoutRect clipRect(paintRect);
    const ComputedStyle& style = box.getLineLayoutItem().styleRef();
    LayoutRectOutsets outsets = style.imageOutsets(image);
    if (box.isHorizontal()) {
        clipRect.setY(paintRect.y() - outsets.top());
        clipRect.setHeight(paintRect.height() + outsets.top() + outsets.bottom());
        if (box.includeLogicalLeftEdge()) {
            clipRect.setX(paintRect.x() - outsets.left());
            clipRect.setWidth(paintRect.width() + outsets.left());
        }
        if (box.includeLogicalRightEdge())
            clipRect.setWidth(clipRect.width() + outsets.right());
    } else {
        clipRect.setX(paintRect.x() - outsets.left());
        clipRect.setWidth(paintRect.width() + outsets.left() + outsets.right());
        if (box.includeLogicalLeftEdge()) {
            clipRect.setY(paintRect.y() - outsets.top());
            clipRect.setHeight(paintRect.height() + outsets.top());
        }
        if (box.includeLogicalRightEdge())
            clipRect.setHeight(clipRect.height() + outsets.bottom());
    }
    return clipRect;
}

LayoutRect InlineFlowBoxPainter::paintRectForImageStrip(const LayoutPoint& paintOffset, const LayoutSize& frameSize, TextDirection direction) const
{
    // We have a fill/border/mask image that spans multiple lines.
    // We need to adjust the offset by the width of all previous lines.
    // Think of background painting on inlines as though you had one long line, a single continuous
    // strip. Even though that strip has been broken up across multiple lines, you still paint it
    // as though you had one single line. This means each line has to pick up the background where
    // the previous line left off.
    LayoutUnit logicalOffsetOnLine;
    LayoutUnit totalLogicalWidth;
    if (direction == LTR) {
        for (const InlineFlowBox* curr = m_inlineFlowBox.prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (const InlineFlowBox* curr = &m_inlineFlowBox; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
    } else {
        for (const InlineFlowBox* curr = m_inlineFlowBox.nextLineBox(); curr; curr = curr->nextLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (const InlineFlowBox* curr = &m_inlineFlowBox; curr; curr = curr->prevLineBox())
            totalLogicalWidth += curr->logicalWidth();
    }
    LayoutUnit stripX = paintOffset.x() - (m_inlineFlowBox.isHorizontal() ? logicalOffsetOnLine : LayoutUnit());
    LayoutUnit stripY = paintOffset.y() - (m_inlineFlowBox.isHorizontal() ? LayoutUnit() : logicalOffsetOnLine);
    LayoutUnit stripWidth = m_inlineFlowBox.isHorizontal() ? totalLogicalWidth : frameSize.width();
    LayoutUnit stripHeight = m_inlineFlowBox.isHorizontal() ? frameSize.height() : totalLogicalWidth;
    return LayoutRect(stripX, stripY, stripWidth, stripHeight);
}


InlineFlowBoxPainter::BorderPaintingType InlineFlowBoxPainter::getBorderPaintType(const LayoutRect& adjustedFrameRect, IntRect& adjustedClipRect) const
{
    adjustedClipRect = pixelSnappedIntRect(adjustedFrameRect);
    if (m_inlineFlowBox.parent() && m_inlineFlowBox.getLineLayoutItem().style()->hasBorderDecoration()) {
        const NinePieceImage& borderImage = m_inlineFlowBox.getLineLayoutItem().style()->borderImage();
        StyleImage* borderImageSource = borderImage.image();
        bool hasBorderImage = borderImageSource && borderImageSource->canRender();
        if (hasBorderImage && !borderImageSource->isLoaded())
            return DontPaintBorders;

        // The simple case is where we either have no border image or we are the only box for this object.
        // In those cases only a single call to draw is required.
        if (!hasBorderImage || (!m_inlineFlowBox.prevLineBox() && !m_inlineFlowBox.nextLineBox()))
            return PaintBordersWithoutClip;

        // We have a border image that spans multiple lines.
        adjustedClipRect = pixelSnappedIntRect(clipRectForNinePieceImageStrip(m_inlineFlowBox, borderImage, adjustedFrameRect));
        return PaintBordersWithClip;
    }
    return DontPaintBorders;
}

void InlineFlowBoxPainter::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutRect& cullRect)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground);
    if (m_inlineFlowBox.getLineLayoutItem().style()->visibility() != VISIBLE)
        return;

    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    LayoutObject* inlineFlowBoxLayoutObject = LineLayoutAPIShim::layoutObjectFrom(m_inlineFlowBox.getLineLayoutItem());
    const ComputedStyle* styleToUse = m_inlineFlowBox.getLineLayoutItem().style(m_inlineFlowBox.isFirstLineStyle());
    bool shouldPaintBoxDecorationBackground;
    if (m_inlineFlowBox.parent())
        shouldPaintBoxDecorationBackground = inlineFlowBoxLayoutObject->hasBoxDecorationBackground();
    else
        shouldPaintBoxDecorationBackground = m_inlineFlowBox.isFirstLineStyle() && styleToUse != m_inlineFlowBox.getLineLayoutItem().style();

    if (!shouldPaintBoxDecorationBackground)
        return;

    if (DrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_inlineFlowBox, DisplayItem::BoxDecorationBackground))
        return;

    DrawingRecorder recorder(paintInfo.context, m_inlineFlowBox, DisplayItem::BoxDecorationBackground, pixelSnappedIntRect(cullRect));

    LayoutRect frameRect = frameRectClampedToLineTopAndBottomIfNeeded();

    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    m_inlineFlowBox.flipForWritingMode(localRect);
    LayoutPoint adjustedPaintOffset = paintOffset + localRect.location();

    LayoutRect adjustedFrameRect = LayoutRect(adjustedPaintOffset, frameRect.size());

    IntRect adjustedClipRect;
    BorderPaintingType borderPaintingType = getBorderPaintType(adjustedFrameRect, adjustedClipRect);

    // Shadow comes first and is behind the background and border.
    if (!m_inlineFlowBox.boxModelObject().boxShadowShouldBeAppliedToBackground(BackgroundBleedNone, &m_inlineFlowBox))
        paintBoxShadow(paintInfo, *styleToUse, Normal, adjustedFrameRect);

    Color backgroundColor = inlineFlowBoxLayoutObject->resolveColor(*styleToUse, CSSPropertyBackgroundColor);
    paintFillLayers(paintInfo, backgroundColor, styleToUse->backgroundLayers(), adjustedFrameRect);
    paintBoxShadow(paintInfo, *styleToUse, Inset, adjustedFrameRect);

    switch (borderPaintingType) {
    case DontPaintBorders:
        break;
    case PaintBordersWithoutClip:
        BoxPainter::paintBorder(*toLayoutBoxModelObject(LineLayoutAPIShim::layoutObjectFrom(m_inlineFlowBox.boxModelObject())), paintInfo, adjustedFrameRect, m_inlineFlowBox.getLineLayoutItem().styleRef(m_inlineFlowBox.isFirstLineStyle()), BackgroundBleedNone, m_inlineFlowBox.includeLogicalLeftEdge(), m_inlineFlowBox.includeLogicalRightEdge());
        break;
    case PaintBordersWithClip:
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        LayoutRect imageStripPaintRect = paintRectForImageStrip(adjustedPaintOffset, frameRect.size(), LTR);
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        paintInfo.context.clip(adjustedClipRect);
        BoxPainter::paintBorder(*toLayoutBoxModelObject(LineLayoutAPIShim::layoutObjectFrom(m_inlineFlowBox.boxModelObject())), paintInfo, imageStripPaintRect, m_inlineFlowBox.getLineLayoutItem().styleRef(m_inlineFlowBox.isFirstLineStyle()));
        break;
    }
}

void InlineFlowBoxPainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_inlineFlowBox.getLineLayoutItem().style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect frameRect = frameRectClampedToLineTopAndBottomIfNeeded();

    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    m_inlineFlowBox.flipForWritingMode(localRect);
    LayoutPoint adjustedPaintOffset = paintOffset + localRect.location();

    const NinePieceImage& maskNinePieceImage = m_inlineFlowBox.getLineLayoutItem().style()->maskBoxImage();
    StyleImage* maskBoxImage = m_inlineFlowBox.getLineLayoutItem().style()->maskBoxImage().image();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = m_inlineFlowBox.getLineLayoutItem().hasLayer() && m_inlineFlowBox.boxModelObject().layer()->hasCompositedMask();
    bool flattenCompositingLayers = paintInfo.getGlobalPaintFlags() & GlobalPaintFlattenCompositingLayers;
    SkXfermode::Mode compositeOp = SkXfermode::kSrcOver_Mode;
    if (!compositedMask || flattenCompositingLayers) {
        if ((maskBoxImage && m_inlineFlowBox.getLineLayoutItem().style()->maskLayers().hasImage()) || m_inlineFlowBox.getLineLayoutItem().style()->maskLayers().next()) {
            pushTransparencyLayer = true;
            paintInfo.context.beginLayer(1.0f, SkXfermode::kDstIn_Mode);
        } else {
            // TODO(fmalita): passing a dst-in xfer mode down to paintFillLayers/paintNinePieceImage
            //   seems dangerous: it is only correct if applied atomically (single draw call). While
            //   the heuristic above presumably ensures that is the case, this approach seems super
            //   fragile. We should investigate dropping this optimization in favour of the more
            //   robust layer branch above.
            compositeOp = SkXfermode::kDstIn_Mode;
        }
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, frameRect.size());
    paintFillLayers(paintInfo, Color::transparent, m_inlineFlowBox.getLineLayoutItem().style()->maskLayers(), paintRect, compositeOp);

    bool hasBoxImage = maskBoxImage && maskBoxImage->canRender();
    if (!hasBoxImage || !maskBoxImage->isLoaded()) {
        if (pushTransparencyLayer)
            paintInfo.context.endLayer();
        return; // Don't paint anything while we wait for the image to load.
    }

    LayoutBoxModelObject* boxModel = toLayoutBoxModelObject(LineLayoutAPIShim::layoutObjectFrom(m_inlineFlowBox.boxModelObject()));
    // The simple case is where we are the only box for this object. In those
    // cases only a single call to draw is required.
    if (!m_inlineFlowBox.prevLineBox() && !m_inlineFlowBox.nextLineBox()) {
        BoxPainter::paintNinePieceImage(*boxModel, paintInfo.context, paintRect, m_inlineFlowBox.getLineLayoutItem().styleRef(), maskNinePieceImage, compositeOp);
    } else {
        // We have a mask image that spans multiple lines.
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        LayoutRect imageStripPaintRect = paintRectForImageStrip(adjustedPaintOffset, frameRect.size(), LTR);
        FloatRect clipRect(clipRectForNinePieceImageStrip(m_inlineFlowBox, maskNinePieceImage, paintRect));
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        // TODO(chrishtr): this should be pixel-snapped.
        paintInfo.context.clip(clipRect);
        BoxPainter::paintNinePieceImage(*boxModel, paintInfo.context, imageStripPaintRect, m_inlineFlowBox.getLineLayoutItem().styleRef(), maskNinePieceImage, compositeOp);
    }

    if (pushTransparencyLayer)
        paintInfo.context.endLayer();
}

// This method should not be needed. See crbug.com/530659.
LayoutRect InlineFlowBoxPainter::frameRectClampedToLineTopAndBottomIfNeeded() const
{
    LayoutRect rect(m_inlineFlowBox.frameRect());

    bool noQuirksMode = m_inlineFlowBox.getLineLayoutItem().document().inNoQuirksMode();
    if (!noQuirksMode && !m_inlineFlowBox.hasTextChildren() && !(m_inlineFlowBox.descendantsHaveSameLineHeightAndBaseline() && m_inlineFlowBox.hasTextDescendants())) {
        const RootInlineBox& rootBox = m_inlineFlowBox.root();
        LayoutUnit logicalTop = m_inlineFlowBox.isHorizontal() ? rect.y() : rect.x();
        LayoutUnit logicalHeight = m_inlineFlowBox.isHorizontal() ? rect.height() : rect.width();
        LayoutUnit bottom = std::min(rootBox.lineBottom(), logicalTop + logicalHeight);
        logicalTop = std::max(rootBox.lineTop(), logicalTop);
        logicalHeight = bottom - logicalTop;
        if (m_inlineFlowBox.isHorizontal()) {
            rect.setY(logicalTop);
            rect.setHeight(logicalHeight);
        } else {
            rect.setX(logicalTop);
            rect.setWidth(logicalHeight);
        }
    }
    return rect;
}

} // namespace blink
