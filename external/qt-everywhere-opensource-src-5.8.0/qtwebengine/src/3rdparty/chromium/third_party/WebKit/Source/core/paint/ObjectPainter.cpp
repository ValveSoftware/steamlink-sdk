// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

namespace {

void paintSingleRectangleOutline(const PaintInfo& paintInfo, const IntRect& rect, const ComputedStyle& style, const Color& color)
{
    ASSERT(!style.outlineStyleIsAuto());

    LayoutRect inner(rect);
    inner.inflate(style.outlineOffset());
    LayoutRect outer(inner);
    outer.inflate(style.outlineWidth());
    const BorderEdge commonEdgeInfo(style.outlineWidth(), color, style.outlineStyle());
    BoxBorderPainter(style, outer, inner, commonEdgeInfo).paintBorder(paintInfo, outer);
}

struct OutlineEdgeInfo {
    int x1;
    int y1;
    int x2;
    int y2;
    BoxSide side;
};

// Adjust length of edges if needed. Returns the width of the joint.
int adjustJoint(int outlineWidth, OutlineEdgeInfo& edge1, OutlineEdgeInfo& edge2)
{
    // A clockwise joint:
    // - needs no adjustment of edge length because our edges are along the clockwise outer edge of the outline;
    // - needs a positive adjacent joint width (required by drawLineForBoxSide).
    // A counterclockwise joint:
    // - needs to increase the edge length to include the joint;
    // - needs a negative adjacent joint width (required by drawLineForBoxSide).
    switch (edge1.side) {
    case BSTop:
        switch (edge2.side) {
        case BSRight: // Clockwise
            return outlineWidth;
        case BSLeft: // Counterclockwise
            edge1.x2 += outlineWidth;
            edge2.y2 += outlineWidth;
            return -outlineWidth;
        default: // Same side or no joint.
            return 0;
        }
    case BSRight:
        switch (edge2.side) {
        case BSBottom: // Clockwise
            return outlineWidth;
        case BSTop: // Counterclockwise
            edge1.y2 += outlineWidth;
            edge2.x1 -= outlineWidth;
            return -outlineWidth;
        default: // Same side or no joint.
            return 0;
        }
    case BSBottom:
        switch (edge2.side) {
        case BSLeft: // Clockwise
            return outlineWidth;
        case BSRight: // Counterclockwise
            edge1.x1 -= outlineWidth;
            edge2.y1 -= outlineWidth;
            return -outlineWidth;
        default: // Same side or no joint.
            return 0;
        }
    case BSLeft:
        switch (edge2.side) {
        case BSTop: // Clockwise
            return outlineWidth;
        case BSBottom: // Counterclockwise
            edge1.y1 -= outlineWidth;
            edge2.x2 += outlineWidth;
            return -outlineWidth;
        default: // Same side or no joint.
            return 0;
        }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

void paintComplexOutline(GraphicsContext& graphicsContext, const Vector<IntRect> rects, const ComputedStyle& style, const Color& color)
{
    ASSERT(!style.outlineStyleIsAuto());

    // Construct a clockwise path along the outer edge of the outline.
    SkRegion region;
    int width = style.outlineWidth();
    int outset = style.outlineOffset() + style.outlineWidth();
    for (auto& r : rects) {
        IntRect rect = r;
        rect.inflate(outset);
        region.op(rect, SkRegion::kUnion_Op);
    }
    SkPath path;
    if (!region.getBoundaryPath(&path))
        return;

    Vector<OutlineEdgeInfo, 4> edges;

    SkPath::Iter iter(path, false);
    SkPoint points[4];
    size_t count = 0;
    for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb; verb = iter.next(points, false)) {
        if (verb != SkPath::kLine_Verb)
            continue;

        edges.grow(++count);
        OutlineEdgeInfo& edge = edges.last();
        edge.x1 = SkScalarTruncToInt(points[0].x());
        edge.y1 = SkScalarTruncToInt(points[0].y());
        edge.x2 = SkScalarTruncToInt(points[1].x());
        edge.y2 = SkScalarTruncToInt(points[1].y());
        if (edge.x1 == edge.x2) {
            if (edge.y1 < edge.y2) {
                edge.x1 -= width;
                edge.side = BSRight;
            } else {
                std::swap(edge.y1, edge.y2);
                edge.x2 += width;
                edge.side = BSLeft;
            }
        } else {
            ASSERT(edge.y1 == edge.y2);
            if (edge.x1 < edge.x2) {
                edge.y2 += width;
                edge.side = BSTop;
            } else {
                std::swap(edge.x1, edge.x2);
                edge.y1 -= width;
                edge.side = BSBottom;
            }
        }
    }

    if (!count)
        return;

    Color outlineColor = color;
    bool useTransparencyLayer = color.hasAlpha();
    if (useTransparencyLayer) {
        graphicsContext.beginLayer(static_cast<float>(color.alpha()) / 255);
        outlineColor = Color(outlineColor.red(), outlineColor.green(), outlineColor.blue());
    }

    ASSERT(count >= 4 && edges.size() == count);
    int firstAdjacentWidth = adjustJoint(width, edges.last(), edges.first());

    // The width of the angled part of starting and ending joint of the current edge.
    int adjacentWidthStart = firstAdjacentWidth;
    int adjacentWidthEnd;
    for (size_t i = 0; i < count; ++i) {
        OutlineEdgeInfo& edge = edges[i];
        adjacentWidthEnd = i == count - 1 ? firstAdjacentWidth : adjustJoint(width, edge, edges[i + 1]);
        int adjacentWidth1 = adjacentWidthStart;
        int adjacentWidth2 = adjacentWidthEnd;
        if (edge.side == BSLeft || edge.side == BSBottom)
            std::swap(adjacentWidth1, adjacentWidth2);
        ObjectPainter::drawLineForBoxSide(graphicsContext, edge.x1, edge.y1, edge.x2, edge.y2, edge.side, outlineColor, style.outlineStyle(), adjacentWidth1, adjacentWidth2, false);
        adjacentWidthStart = adjacentWidthEnd;
    }

    if (useTransparencyLayer)
        graphicsContext.endLayer();
}


void fillQuad(GraphicsContext& context, const FloatPoint quad[], const Color& color, bool antialias)
{
    SkPath path;
    path.moveTo(quad[0]);
    path.lineTo(quad[1]);
    path.lineTo(quad[2]);
    path.lineTo(quad[3]);
    SkPaint paint(context.fillPaint());
    paint.setAntiAlias(antialias);
    paint.setColor(color.rgb());

    context.drawPath(path, paint);
}

} // namespace

void ObjectPainter::paintOutline(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(shouldPaintSelfOutline(paintInfo.phase));

    const ComputedStyle& styleToUse = m_layoutObject.styleRef();
    if (!styleToUse.hasOutline() || styleToUse.visibility() != VISIBLE)
        return;

    // Only paint the focus ring by hand if the theme isn't able to draw the focus ring.
    if (styleToUse.outlineStyleIsAuto() && !LayoutTheme::theme().shouldDrawDefaultFocusRing(m_layoutObject))
        return;

    Vector<LayoutRect> outlineRects;
    m_layoutObject.addOutlineRects(outlineRects, paintOffset, m_layoutObject.outlineRectsShouldIncludeBlockVisualOverflow());
    if (outlineRects.isEmpty())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutObject, paintInfo.phase))
        return;

    // The result rects are in coordinates of m_layoutObject's border box.
    // Block flipping is not applied yet if !m_layoutObject.isBox().
    if (!m_layoutObject.isBox() && m_layoutObject.styleRef().isFlippedBlocksWritingMode()) {
        LayoutBlock* container = m_layoutObject.containingBlock();
        if (container) {
            m_layoutObject.localToAncestorRects(outlineRects, container, -paintOffset, paintOffset);
            if (outlineRects.isEmpty())
                return;
        }
    }

    Vector<IntRect> pixelSnappedOutlineRects;
    for (auto& r : outlineRects)
        pixelSnappedOutlineRects.append(pixelSnappedIntRect(r));

    IntRect unitedOutlineRect = unionRectEvenIfEmpty(pixelSnappedOutlineRects);
    IntRect bounds = unitedOutlineRect;
    bounds.inflate(m_layoutObject.styleRef().outlineOutsetExtent());
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutObject, paintInfo.phase, bounds);

    Color color = m_layoutObject.resolveColor(styleToUse, CSSPropertyOutlineColor);
    if (styleToUse.outlineStyleIsAuto()) {
        paintInfo.context.drawFocusRing(pixelSnappedOutlineRects, styleToUse.outlineWidth(), styleToUse.outlineOffset(), color);
        return;
    }

    if (unitedOutlineRect == pixelSnappedOutlineRects[0]) {
        paintSingleRectangleOutline(paintInfo, unitedOutlineRect, styleToUse, color);
        return;
    }
    paintComplexOutline(paintInfo.context, pixelSnappedOutlineRects, styleToUse, color);
}

void ObjectPainter::paintInlineChildrenOutlines(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(shouldPaintDescendantOutlines(paintInfo.phase));

    PaintInfo paintInfoForDescendants = paintInfo.forDescendants();
    for (LayoutObject* child = m_layoutObject.slowFirstChild(); child; child = child->nextSibling()) {
        if (child->isLayoutInline() && !toLayoutInline(child)->hasSelfPaintingLayer())
            child->paint(paintInfoForDescendants, paintOffset);
    }
}

void ObjectPainter::addPDFURLRectIfNeeded(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.isPrinting());
    if (m_layoutObject.isElementContinuation() || !m_layoutObject.node() || !m_layoutObject.node()->isLink() || m_layoutObject.styleRef().visibility() != VISIBLE)
        return;

    KURL url = toElement(m_layoutObject.node())->hrefURL();
    if (!url.isValid())
        return;

    Vector<LayoutRect> visualOverflowRects;
    m_layoutObject.addElementVisualOverflowRects(visualOverflowRects, paintOffset);
    IntRect rect = pixelSnappedIntRect(unionRect(visualOverflowRects));
    if (rect.isEmpty())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutObject, DisplayItem::PrintedContentPDFURLRect))
        return;

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutObject, DisplayItem::PrintedContentPDFURLRect, rect);
    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, m_layoutObject.document().baseURL())) {
        String fragmentName = url.fragmentIdentifier();
        if (m_layoutObject.document().findAnchor(fragmentName))
            paintInfo.context.setURLFragmentForRect(fragmentName, rect);
        return;
    }
    paintInfo.context.setURLForRect(url, rect);
}

void ObjectPainter::drawLineForBoxSide(GraphicsContext& graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, EBorderStyle style,
    int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    int thickness;
    int length;
    if (side == BSTop || side == BSBottom) {
        thickness = y2 - y1;
        length = x2 - x1;
    } else {
        thickness = x2 - x1;
        length = y2 - y1;
    }

    // We would like this check to be an ASSERT as we don't want to draw empty borders. However
    // nothing guarantees that the following recursive calls to drawLineForBoxSide will have
    // positive thickness and length.
    if (length <= 0 || thickness <= 0)
        return;

    if (style == BorderStyleDouble && thickness < 3)
        style = BorderStyleSolid;

    switch (style) {
    case BorderStyleNone:
    case BorderStyleHidden:
        return;
    case BorderStyleDotted:
    case BorderStyleDashed:
        drawDashedOrDottedBoxSide(graphicsContext, x1, y1, x2, y2, side,
            color, thickness, style, antialias);
        break;
    case BorderStyleDouble:
        drawDoubleBoxSide(graphicsContext, x1, y1, x2, y2, length, side, color,
            thickness, adjacentWidth1, adjacentWidth2, antialias);
        break;
    case BorderStyleRidge:
    case BorderStyleGroove:
        drawRidgeOrGrooveBoxSide(graphicsContext, x1, y1, x2, y2, side, color,
            style, adjacentWidth1, adjacentWidth2, antialias);
        break;
    case BorderStyleInset:
        // FIXME: Maybe we should lighten the colors on one side like Firefox.
        // https://bugs.webkit.org/show_bug.cgi?id=58608
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        // fall through
    case BorderStyleOutset:
        if (style == BorderStyleOutset && (side == BSBottom || side == BSRight))
            color = color.dark();
        // fall through
    case BorderStyleSolid:
        drawSolidBoxSide(graphicsContext, x1, y1, x2, y2, side, color, adjacentWidth1, adjacentWidth2, antialias);
        break;
    }
}

void ObjectPainter::drawDashedOrDottedBoxSide(GraphicsContext& graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, int thickness, EBorderStyle style, bool antialias)
{
    ASSERT(thickness > 0);

    bool wasAntialiased = graphicsContext.shouldAntialias();
    StrokeStyle oldStrokeStyle = graphicsContext.getStrokeStyle();
    graphicsContext.setShouldAntialias(antialias);
    graphicsContext.setStrokeColor(color);
    graphicsContext.setStrokeThickness(thickness);
    graphicsContext.setStrokeStyle(style == BorderStyleDashed ? DashedStroke : DottedStroke);

    switch (side) {
    case BSBottom:
    case BSTop: {
        int midY = y1 + thickness / 2;
        graphicsContext.drawLine(IntPoint(x1, midY), IntPoint(x2, midY));
        break;
    }
    case BSRight:
    case BSLeft: {
        int midX = x1 + thickness / 2;
        graphicsContext.drawLine(IntPoint(midX, y1), IntPoint(midX, y2));
        break;
    }
    }
    graphicsContext.setShouldAntialias(wasAntialiased);
    graphicsContext.setStrokeStyle(oldStrokeStyle);
}

void ObjectPainter::drawDoubleBoxSide(GraphicsContext& graphicsContext, int x1, int y1, int x2, int y2,
    int length, BoxSide side, Color color, int thickness, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    int thirdOfThickness = (thickness + 1) / 3;
    ASSERT(thirdOfThickness > 0);

    if (!adjacentWidth1 && !adjacentWidth2) {
        StrokeStyle oldStrokeStyle = graphicsContext.getStrokeStyle();
        graphicsContext.setStrokeStyle(NoStroke);
        graphicsContext.setFillColor(color);

        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(antialias);

        switch (side) {
        case BSTop:
        case BSBottom:
            graphicsContext.drawRect(IntRect(x1, y1, length, thirdOfThickness));
            graphicsContext.drawRect(IntRect(x1, y2 - thirdOfThickness, length, thirdOfThickness));
            break;
        case BSLeft:
        case BSRight:
            graphicsContext.drawRect(IntRect(x1, y1, thirdOfThickness, length));
            graphicsContext.drawRect(IntRect(x2 - thirdOfThickness, y1, thirdOfThickness, length));
            break;
        }

        graphicsContext.setShouldAntialias(wasAntialiased);
        graphicsContext.setStrokeStyle(oldStrokeStyle);
        return;
    }

    int adjacent1BigThird = ((adjacentWidth1 > 0) ? adjacentWidth1 + 1 : adjacentWidth1 - 1) / 3;
    int adjacent2BigThird = ((adjacentWidth2 > 0) ? adjacentWidth2 + 1 : adjacentWidth2 - 1) / 3;

    switch (side) {
    case BSTop:
        drawLineForBoxSide(graphicsContext, x1 + std::max((-adjacentWidth1 * 2 + 1) / 3, 0),
            y1, x2 - std::max((-adjacentWidth2 * 2 + 1) / 3, 0), y1 + thirdOfThickness,
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x1 + std::max((adjacentWidth1 * 2 + 1) / 3, 0),
            y2 - thirdOfThickness, x2 - std::max((adjacentWidth2 * 2 + 1) / 3, 0), y2,
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSLeft:
        drawLineForBoxSide(graphicsContext, x1, y1 + std::max((-adjacentWidth1 * 2 + 1) / 3, 0),
            x1 + thirdOfThickness, y2 - std::max((-adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x2 - thirdOfThickness, y1 + std::max((adjacentWidth1 * 2 + 1) / 3, 0),
            x2, y2 - std::max((adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSBottom:
        drawLineForBoxSide(graphicsContext, x1 + std::max((adjacentWidth1 * 2 + 1) / 3, 0),
            y1, x2 - std::max((adjacentWidth2 * 2 + 1) / 3, 0), y1 + thirdOfThickness,
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x1 + std::max((-adjacentWidth1 * 2 + 1) / 3, 0),
            y2 - thirdOfThickness, x2 - std::max((-adjacentWidth2 * 2 + 1) / 3, 0), y2,
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSRight:
        drawLineForBoxSide(graphicsContext, x1, y1 + std::max((adjacentWidth1 * 2 + 1) / 3, 0),
            x1 + thirdOfThickness, y2 - std::max((adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x2 - thirdOfThickness, y1 + std::max((-adjacentWidth1 * 2 + 1) / 3, 0),
            x2, y2 - std::max((-adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, BorderStyleSolid, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    default:
        break;
    }
}

void ObjectPainter::drawRidgeOrGrooveBoxSide(GraphicsContext& graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, EBorderStyle style, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    EBorderStyle s1;
    EBorderStyle s2;
    if (style == BorderStyleGroove) {
        s1 = BorderStyleInset;
        s2 = BorderStyleOutset;
    } else {
        s1 = BorderStyleOutset;
        s2 = BorderStyleInset;
    }

    int adjacent1BigHalf = ((adjacentWidth1 > 0) ? adjacentWidth1 + 1 : adjacentWidth1 - 1) / 2;
    int adjacent2BigHalf = ((adjacentWidth2 > 0) ? adjacentWidth2 + 1 : adjacentWidth2 - 1) / 2;

    switch (side) {
    case BSTop:
        drawLineForBoxSide(graphicsContext, x1 + std::max(-adjacentWidth1, 0) / 2, y1, x2 - std::max(-adjacentWidth2, 0) / 2, (y1 + y2 + 1) / 2,
            side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, x1 + std::max(adjacentWidth1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - std::max(adjacentWidth2 + 1, 0) / 2, y2,
            side, color, s2, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSLeft:
        drawLineForBoxSide(graphicsContext, x1, y1 + std::max(-adjacentWidth1, 0) / 2, (x1 + x2 + 1) / 2, y2 - std::max(-adjacentWidth2, 0) / 2,
            side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, (x1 + x2 + 1) / 2, y1 + std::max(adjacentWidth1 + 1, 0) / 2, x2, y2 - std::max(adjacentWidth2 + 1, 0) / 2,
            side, color, s2, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSBottom:
        drawLineForBoxSide(graphicsContext, x1 + std::max(adjacentWidth1, 0) / 2, y1, x2 - std::max(adjacentWidth2, 0) / 2, (y1 + y2 + 1) / 2,
            side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, x1 + std::max(-adjacentWidth1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - std::max(-adjacentWidth2 + 1, 0) / 2, y2,
            side, color, s1, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSRight:
        drawLineForBoxSide(graphicsContext, x1, y1 + std::max(adjacentWidth1, 0) / 2, (x1 + x2 + 1) / 2, y2 - std::max(adjacentWidth2, 0) / 2,
            side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, (x1 + x2 + 1) / 2, y1 + std::max(-adjacentWidth1 + 1, 0) / 2, x2, y2 - std::max(-adjacentWidth2 + 1, 0) / 2,
            side, color, s1, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    }
}

void ObjectPainter::drawSolidBoxSide(GraphicsContext& graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    ASSERT(x2 >= x1);
    ASSERT(y2 >= y1);

    if (!adjacentWidth1 && !adjacentWidth2) {
        // Tweak antialiasing to match the behavior of fillQuad();
        // this matters for rects in transformed contexts.
        bool wasAntialiased = graphicsContext.shouldAntialias();
        if (antialias != wasAntialiased)
            graphicsContext.setShouldAntialias(antialias);
        graphicsContext.fillRect(IntRect(x1, y1, x2 - x1, y2 - y1), color);
        if (antialias != wasAntialiased)
            graphicsContext.setShouldAntialias(wasAntialiased);
        return;
    }

    FloatPoint quad[4];
    switch (side) {
    case BSTop:
        quad[0] = FloatPoint(x1 + std::max(-adjacentWidth1, 0), y1);
        quad[1] = FloatPoint(x1 + std::max(adjacentWidth1, 0), y2);
        quad[2] = FloatPoint(x2 - std::max(adjacentWidth2, 0), y2);
        quad[3] = FloatPoint(x2 - std::max(-adjacentWidth2, 0), y1);
        break;
    case BSBottom:
        quad[0] = FloatPoint(x1 + std::max(adjacentWidth1, 0), y1);
        quad[1] = FloatPoint(x1 + std::max(-adjacentWidth1, 0), y2);
        quad[2] = FloatPoint(x2 - std::max(-adjacentWidth2, 0), y2);
        quad[3] = FloatPoint(x2 - std::max(adjacentWidth2, 0), y1);
        break;
    case BSLeft:
        quad[0] = FloatPoint(x1, y1 + std::max(-adjacentWidth1, 0));
        quad[1] = FloatPoint(x1, y2 - std::max(-adjacentWidth2, 0));
        quad[2] = FloatPoint(x2, y2 - std::max(adjacentWidth2, 0));
        quad[3] = FloatPoint(x2, y1 + std::max(adjacentWidth1, 0));
        break;
    case BSRight:
        quad[0] = FloatPoint(x1, y1 + std::max(adjacentWidth1, 0));
        quad[1] = FloatPoint(x1, y2 - std::max(adjacentWidth2, 0));
        quad[2] = FloatPoint(x2, y2 - std::max(-adjacentWidth2, 0));
        quad[3] = FloatPoint(x2, y1 + std::max(-adjacentWidth1, 0));
        break;
    }

    fillQuad(graphicsContext, quad, color, antialias);
}

void ObjectPainter::paintAllPhasesAtomically(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Pass PaintPhaseSelection and PaintPhaseTextClip to the descendants so that they will paint
    // for selection and text clip respectively. We don't need complete painting for these phases.
    if (paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip) {
        m_layoutObject.paint(paintInfo, paintOffset);
        return;
    }

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    PaintInfo info(paintInfo);
    info.phase = PaintPhaseBlockBackground;
    m_layoutObject.paint(info, paintOffset);
    info.phase = PaintPhaseFloat;
    m_layoutObject.paint(info, paintOffset);
    info.phase = PaintPhaseForeground;
    m_layoutObject.paint(info, paintOffset);
    info.phase = PaintPhaseOutline;
    m_layoutObject.paint(info, paintOffset);
}

} // namespace blink
