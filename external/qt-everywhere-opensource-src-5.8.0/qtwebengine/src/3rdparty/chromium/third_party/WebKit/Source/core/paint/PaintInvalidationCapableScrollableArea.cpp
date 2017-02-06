// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInvalidationCapableScrollableArea.h"

#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"

namespace blink {

void PaintInvalidationCapableScrollableArea::willRemoveScrollbar(Scrollbar& scrollbar, ScrollbarOrientation orientation)
{
    if (!scrollbar.isCustomScrollbar()
        && !(orientation == HorizontalScrollbar ? layerForHorizontalScrollbar() : layerForVerticalScrollbar()))
        boxForScrollControlPaintInvalidation().slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(scrollbar, PaintInvalidationScroll);

    ScrollableArea::willRemoveScrollbar(scrollbar, orientation);
}

static LayoutRect scrollControlPaintInvalidationRect(const IntRect& scrollControlRect, const LayoutBox& box, const PaintInvalidationState& paintInvalidationState)
{
    LayoutRect paintInvalidationRect(scrollControlRect);
    if (!paintInvalidationRect.isEmpty())
        paintInvalidationState.mapLocalRectToPaintInvalidationBacking(paintInvalidationRect);
    return paintInvalidationRect;
}

// Returns true if the scroll control is invalidated.
static bool invalidatePaintOfScrollControlIfNeeded(const LayoutRect& newPaintInvalidationRect, const LayoutRect& previousPaintInvalidationRect, bool needsPaintInvalidation, LayoutBox& box, const LayoutBoxModelObject& paintInvalidationContainer)
{
    bool shouldInvalidateNewRect = needsPaintInvalidation;
    if (newPaintInvalidationRect != previousPaintInvalidationRect) {
        box.invalidatePaintUsingContainer(paintInvalidationContainer, previousPaintInvalidationRect, PaintInvalidationScroll);
        shouldInvalidateNewRect = true;
    }
    if (shouldInvalidateNewRect) {
        box.invalidatePaintUsingContainer(paintInvalidationContainer, newPaintInvalidationRect, PaintInvalidationScroll);
        box.enclosingLayer()->setNeedsRepaint();
        return true;
    }
    return false;
}

static void invalidatePaintOfScrollbarIfNeeded(Scrollbar* scrollbar, GraphicsLayer* graphicsLayer, bool& previouslyWasOverlay, LayoutRect& previousPaintInvalidationRect, bool needsPaintInvalidationArg, LayoutBox& box, const PaintInvalidationState& paintInvalidationState)
{
    bool isOverlay = scrollbar && scrollbar->isOverlayScrollbar();

    // Calculate paint invalidation rect of the scrollbar, except overlay composited scrollbars because we invalidate the graphics layer only.
    LayoutRect newPaintInvalidationRect;
    if (scrollbar && !(graphicsLayer && isOverlay))
        newPaintInvalidationRect = scrollControlPaintInvalidationRect(scrollbar->frameRect(), box, paintInvalidationState);

    bool needsPaintInvalidation = needsPaintInvalidationArg;
    if (needsPaintInvalidation && graphicsLayer) {
        // If the scrollbar needs paint invalidation but didn't change location/size or the scrollbar is an
        // overlay scrollbar (paint invalidation rect is empty), invalidating the graphics layer is enough
        // (which has been done in ScrollableArea::setScrollbarNeedsPaintInvalidation()).
        // Otherwise invalidatePaintOfScrollControlIfNeeded() below will invalidate the old and new location
        // of the scrollbar on the box's paint invalidation container to ensure newly expanded/shrunk areas
        // of the box to be invalidated.
        needsPaintInvalidation = false;
        DCHECK(!graphicsLayer->drawsContent() || graphicsLayer->getPaintController().cacheIsEmpty());
    }

    // Invalidate the box's display item client if the box's padding box size is affected by change of the
    // non-overlay scrollbar width. We detect change of paint invalidation rect size instead of change of
    // scrollbar width change, which may have some false-positives (e.g. the scrollbar changed length but
    // not width) but won't invalidate more than expected because in the false-positive case the box must
    // have changed size and have been invalidated.
    const LayoutBoxModelObject& paintInvalidationContainer = paintInvalidationState.paintInvalidationContainer();
    LayoutSize newScrollbarUsedSpaceInBox;
    if (!isOverlay)
        newScrollbarUsedSpaceInBox = newPaintInvalidationRect.size();
    LayoutSize previousScrollbarUsedSpaceInBox;
    if (!previouslyWasOverlay)
        previousScrollbarUsedSpaceInBox= previousPaintInvalidationRect.size();
    if (newScrollbarUsedSpaceInBox != previousScrollbarUsedSpaceInBox)
        box.setPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(paintInvalidationState, box, PaintInvalidationScroll);

    bool invalidated = invalidatePaintOfScrollControlIfNeeded(newPaintInvalidationRect, previousPaintInvalidationRect, needsPaintInvalidation, box, paintInvalidationContainer);

    previousPaintInvalidationRect = newPaintInvalidationRect;
    previouslyWasOverlay = isOverlay;

    if (!invalidated || !scrollbar || graphicsLayer)
        return;

    box.setPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(paintInvalidationState, *scrollbar, PaintInvalidationScroll);
    if (scrollbar->isCustomScrollbar())
        toLayoutScrollbar(scrollbar)->invalidateDisplayItemClientsOfScrollbarParts();
}

void PaintInvalidationCapableScrollableArea::invalidatePaintOfScrollControlsIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    LayoutBox& box = boxForScrollControlPaintInvalidation();
    invalidatePaintOfScrollbarIfNeeded(horizontalScrollbar(), layerForHorizontalScrollbar(), m_horizontalScrollbarPreviouslyWasOverlay, m_horizontalScrollbarPreviousPaintInvalidationRect, horizontalScrollbarNeedsPaintInvalidation(), box, paintInvalidationState);
    invalidatePaintOfScrollbarIfNeeded(verticalScrollbar(), layerForVerticalScrollbar(), m_verticalScrollbarPreviouslyWasOverlay, m_verticalScrollbarPreviousPaintInvalidationRect, verticalScrollbarNeedsPaintInvalidation(), box, paintInvalidationState);

    LayoutRect scrollCornerPaintInvalidationRect = scrollControlPaintInvalidationRect(scrollCornerAndResizerRect(), box, paintInvalidationState);
    const LayoutBoxModelObject& paintInvalidationContainer = paintInvalidationState.paintInvalidationContainer();
    if (invalidatePaintOfScrollControlIfNeeded(scrollCornerPaintInvalidationRect, m_scrollCornerAndResizerPreviousPaintInvalidationRect, scrollCornerNeedsPaintInvalidation(), box, paintInvalidationContainer)) {
        m_scrollCornerAndResizerPreviousPaintInvalidationRect = scrollCornerPaintInvalidationRect;
        if (LayoutScrollbarPart* scrollCorner = this->scrollCorner())
            scrollCorner->invalidateDisplayItemClientsIncludingNonCompositingDescendants(PaintInvalidationScroll);
        if (LayoutScrollbarPart* resizer = this->resizer())
            resizer->invalidateDisplayItemClientsIncludingNonCompositingDescendants(PaintInvalidationScroll);
    }

    clearNeedsPaintInvalidationForScrollControls();
}

void PaintInvalidationCapableScrollableArea::clearPreviousPaintInvalidationRects()
{
    m_horizontalScrollbarPreviousPaintInvalidationRect = LayoutRect();
    m_verticalScrollbarPreviousPaintInvalidationRect = LayoutRect();
    m_scrollCornerAndResizerPreviousPaintInvalidationRect = LayoutRect();
}

LayoutRect PaintInvalidationCapableScrollableArea::visualRectForScrollbarParts() const
{
    LayoutRect fullBounds(m_horizontalScrollbarPreviousPaintInvalidationRect);
    fullBounds.unite(m_verticalScrollbarPreviousPaintInvalidationRect);
    fullBounds.unite(m_scrollCornerAndResizerPreviousPaintInvalidationRect);
    return fullBounds;
}

void PaintInvalidationCapableScrollableArea::scrollControlWasSetNeedsPaintInvalidation()
{
    boxForScrollControlPaintInvalidation().setMayNeedPaintInvalidation();
}

} // namespace blink
