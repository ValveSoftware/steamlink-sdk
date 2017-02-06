// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ScrollableAreaPainter.h"

#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"

namespace blink {

void ScrollableAreaPainter::paintResizer(GraphicsContext& context, const IntPoint& paintOffset, const CullRect& cullRect)
{
    if (getScrollableArea().box().style()->resize() == RESIZE_NONE)
        return;

    IntRect absRect = getScrollableArea().resizerCornerRect(getScrollableArea().box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    if (absRect.isEmpty())
        return;
    absRect.moveBy(paintOffset);

    if (getScrollableArea().resizer()) {
        if (!cullRect.intersectsCullRect(absRect))
            return;
        ScrollbarPainter::paintIntoRect(*getScrollableArea().resizer(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, getScrollableArea().box(), DisplayItem::Resizer))
        return;

    LayoutObjectDrawingRecorder recorder(context, getScrollableArea().box(), DisplayItem::Resizer, absRect);

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!getScrollableArea().hasOverlayScrollbars() && getScrollableArea().hasScrollbar()) {
        GraphicsContextStateSaver stateSaver(context);
        context.clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context.setStrokeColor(Color(217, 217, 217));
        context.setStrokeThickness(1.0f);
        context.setFillColor(Color::transparent);
        context.drawRect(largerCorner);
    }
}

void ScrollableAreaPainter::drawPlatformResizerImage(GraphicsContext& context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = blink::deviceScaleFactor(getScrollableArea().box().frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(Image, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x")));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_REF(Image, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner")));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (getScrollableArea().box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context.save();
        context.translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context.scale(-1.0, 1.0);
        context.drawImage(resizeCornerImage.get(), IntRect(IntPoint(), cornerResizerSize));
        context.restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context.drawImage(resizeCornerImage.get(), imageRect);
}

void ScrollableAreaPainter::paintOverflowControls(GraphicsContext& context, const IntPoint& paintOffset, const CullRect& cullRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!getScrollableArea().box().hasOverflowClip())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = getScrollableArea().cachedOverlayScrollbarOffset();

    CullRect adjustedCullRect(cullRect, -adjustedPaintOffset);

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the LayoutTree to get it right.
    if (getScrollableArea().hasOverlayScrollbars() && !paintingOverlayControls) {
        getScrollableArea().setCachedOverlayScrollbarOffset(paintOffset);
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((getScrollableArea().horizontalScrollbar() && getScrollableArea().layerForHorizontalScrollbar()) || (getScrollableArea().verticalScrollbar() && getScrollableArea().layerForVerticalScrollbar()))
            return;
        if (!overflowControlsIntersectRect(adjustedCullRect))
            return;

        LayoutView* layoutView = getScrollableArea().box().view();

        PaintLayer* paintingRoot = getScrollableArea().layer()->enclosingLayerWithCompositedLayerMapping(IncludeSelf);
        if (!paintingRoot)
            paintingRoot = layoutView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !getScrollableArea().hasOverlayScrollbars())
        return;

    {
        Optional<ScopedPaintChunkProperties> scopedTransformProperty;
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            const auto* objectProperties = getScrollableArea().box().objectPaintProperties();
            if (objectProperties && objectProperties->scrollbarPaintOffset()) {
                PaintChunkProperties properties(context.getPaintController().currentPaintChunkProperties());
                properties.transform = objectProperties->scrollbarPaintOffset();
                scopedTransformProperty.emplace(context.getPaintController(), properties);
            }
        }
        if (getScrollableArea().horizontalScrollbar() && !getScrollableArea().layerForHorizontalScrollbar()) {
            TransformRecorder translateRecorder(context, *getScrollableArea().horizontalScrollbar(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()));
            getScrollableArea().horizontalScrollbar()->paint(context, adjustedCullRect);
        }
        if (getScrollableArea().verticalScrollbar() && !getScrollableArea().layerForVerticalScrollbar()) {
            TransformRecorder translateRecorder(context, *getScrollableArea().verticalScrollbar(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()));
            getScrollableArea().verticalScrollbar()->paint(context, adjustedCullRect);
        }
    }

    if (getScrollableArea().layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, cullRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, cullRect);
}

bool ScrollableAreaPainter::overflowControlsIntersectRect(const CullRect& cullRect) const
{
    const IntRect borderBox = getScrollableArea().box().pixelSnappedBorderBoxRect();

    if (cullRect.intersectsCullRect(getScrollableArea().rectForHorizontalScrollbar(borderBox)))
        return true;

    if (cullRect.intersectsCullRect(getScrollableArea().rectForVerticalScrollbar(borderBox)))
        return true;

    if (cullRect.intersectsCullRect(getScrollableArea().scrollCornerRect()))
        return true;

    if (cullRect.intersectsCullRect(getScrollableArea().resizerCornerRect(borderBox, ResizerForPointer)))
        return true;

    return false;
}

void ScrollableAreaPainter::paintScrollCorner(GraphicsContext& context, const IntPoint& paintOffset, const CullRect& adjustedCullRect)
{
    IntRect absRect = getScrollableArea().scrollCornerRect();
    if (absRect.isEmpty())
        return;
    absRect.moveBy(paintOffset);

    if (getScrollableArea().scrollCorner()) {
        if (!adjustedCullRect.intersectsCullRect(absRect))
            return;
        ScrollbarPainter::paintIntoRect(*getScrollableArea().scrollCorner(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (getScrollableArea().hasOverlayScrollbars())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, getScrollableArea().box(), DisplayItem::ScrollbarCorner))
        return;

    LayoutObjectDrawingRecorder recorder(context, getScrollableArea().box(), DisplayItem::ScrollbarCorner, absRect);
    context.fillRect(absRect, Color::white);
}

PaintLayerScrollableArea& ScrollableAreaPainter::getScrollableArea() const
{
    return *m_scrollableArea;
}

} // namespace blink
