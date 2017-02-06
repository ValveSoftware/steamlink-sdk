// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FramePainter.h"

#include "core/editing/markers/DocumentMarkerController.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

bool FramePainter::s_inPaintContents = false;

void FramePainter::paint(GraphicsContext& context, const GlobalPaintFlags globalPaintFlags, const CullRect& rect)
{
    frameView().notifyPageThatContentAreaWillPaint();

    IntRect documentDirtyRect = rect.m_rect;
    IntRect visibleAreaWithoutScrollbars(frameView().location(), frameView().visibleContentRect().size());
    documentDirtyRect.intersect(visibleAreaWithoutScrollbars);

    bool shouldPaintContents = !documentDirtyRect.isEmpty();
    bool shouldPaintScrollbars = !frameView().scrollbarsSuppressed() && (frameView().horizontalScrollbar() || frameView().verticalScrollbar());
    if (!shouldPaintContents && !shouldPaintScrollbars)
        return;

    if (shouldPaintContents) {
        // TODO(pdr): Creating frame paint properties here will not be needed once
        // settings()->rootLayerScrolls() is enabled.
        // TODO(pdr): Make this conditional on the rootLayerScrolls setting.
        Optional<ScopedPaintChunkProperties> scopedPaintChunkProperties;
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            TransformPaintPropertyNode* transform = m_frameView->scrollTranslation() ? m_frameView->scrollTranslation() : m_frameView->preTranslation();
            ClipPaintPropertyNode* clip = m_frameView->contentClip();
            if (transform || clip) {
                PaintChunkProperties properties(context.getPaintController().currentPaintChunkProperties());
                if (transform)
                    properties.transform = transform;
                if (clip)
                    properties.clip = clip;
                scopedPaintChunkProperties.emplace(context.getPaintController(), properties);
            }
        }

        TransformRecorder transformRecorder(context, *frameView().layoutView(),
            AffineTransform::translation(frameView().x() - frameView().scrollX(), frameView().y() - frameView().scrollY()));

        ClipRecorder recorder(context, *frameView().layoutView(), DisplayItem::ClipFrameToVisibleContentRect, frameView().visibleContentRect());

        documentDirtyRect.moveBy(-frameView().location() + frameView().scrollPosition());
        paintContents(context, globalPaintFlags, documentDirtyRect);
    }

    if (shouldPaintScrollbars) {
        IntRect scrollViewDirtyRect = rect.m_rect;
        IntRect visibleAreaWithScrollbars(frameView().location(), frameView().visibleContentRect(IncludeScrollbars).size());
        scrollViewDirtyRect.intersect(visibleAreaWithScrollbars);
        scrollViewDirtyRect.moveBy(-frameView().location());

        Optional<ScopedPaintChunkProperties> scopedPaintChunkProperties;
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            if (TransformPaintPropertyNode* transform = m_frameView->preTranslation()) {
                PaintChunkProperties properties(context.getPaintController().currentPaintChunkProperties());
                properties.transform = transform;
                scopedPaintChunkProperties.emplace(context.getPaintController(), properties);
            }
        }

        TransformRecorder transformRecorder(context, *frameView().layoutView(),
            AffineTransform::translation(frameView().x(), frameView().y()));

        ClipRecorder recorder(context, *frameView().layoutView(), DisplayItem::ClipFrameScrollbars, IntRect(IntPoint(), visibleAreaWithScrollbars.size()));

        paintScrollbars(context, scrollViewDirtyRect);
    }
}

void FramePainter::paintContents(GraphicsContext& context, const GlobalPaintFlags globalPaintFlags, const IntRect& rect)
{
    Document* document = frameView().frame().document();

    if (frameView().shouldThrottleRendering())
        return;

#ifndef NDEBUG
    bool fillWithRed;
    if (document->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (frameView().frame().owner())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (frameView().isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (globalPaintFlags & GlobalPaintSelectionOnly)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else
        fillWithRed = true;

    if (fillWithRed && !LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, *frameView().layoutView(), DisplayItem::DebugRedFill)) {
        IntRect contentRect(IntPoint(), frameView().contentsSize());
        LayoutObjectDrawingRecorder drawingRecorder(context, *frameView().layoutView(), DisplayItem::DebugRedFill, contentRect);
    }
#endif

    LayoutView* layoutView = frameView().layoutView();
    if (!layoutView) {
        DLOG(ERROR) << "called FramePainter::paint with nil layoutObject";
        return;
    }

    // TODO(crbug.com/590856): It's still broken when we choose not to crash when the check fails.
    if (!frameView().checkDoesNotNeedLayout())
        return;

    ASSERT(document->lifecycle().state() >= DocumentLifecycle::CompositingClean);

    TRACE_EVENT1("devtools.timeline", "Paint", "data", InspectorPaintEvent::data(layoutView, LayoutRect(rect), 0));

    bool isTopLevelPainter = !s_inPaintContents;
    s_inPaintContents = true;

    FontCachePurgePreventer fontCachePurgePreventer;

    // TODO(jchaffraix): GlobalPaintFlags should be const during a paint
    // phase. Thus we should set this flag upfront (crbug.com/510280).
    GlobalPaintFlags localPaintFlags = globalPaintFlags;
    if (document->printing())
        localPaintFlags |= GlobalPaintFlattenCompositingLayers | GlobalPaintPrinting;

    PaintLayer* rootLayer = layoutView->layer();

#if ENABLE(ASSERT)
    layoutView->assertSubtreeIsLaidOut();
    LayoutObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(*rootLayer->layoutObject());
#endif

    PaintLayerPainter layerPainter(*rootLayer);

    float deviceScaleFactor = blink::deviceScaleFactor(rootLayer->layoutObject()->frame());
    context.setDeviceScaleFactor(deviceScaleFactor);

    layerPainter.paint(context, LayoutRect(rect), localPaintFlags);

    if (rootLayer->containsDirtyOverlayScrollbars())
        layerPainter.paintOverlayScrollbars(context, LayoutRect(rect), localPaintFlags);

    // Regions may have changed as a result of the visibility/z-index of element changing.
    if (document->annotatedRegionsDirty())
        frameView().updateDocumentAnnotatedRegions();

    if (isTopLevelPainter) {
        // Everything that happens after paintContents completions is considered
        // to be part of the next frame.
        memoryCache()->updateFramePaintTimestamp();
        s_inPaintContents = false;
    }

    InspectorInstrumentation::didPaint(layoutView->frame(), 0, context, LayoutRect(rect));
}

void FramePainter::paintScrollbars(GraphicsContext& context, const IntRect& rect)
{
    if (frameView().horizontalScrollbar() && !frameView().layerForHorizontalScrollbar())
        paintScrollbar(context, *frameView().horizontalScrollbar(), rect);
    if (frameView().verticalScrollbar() && !frameView().layerForVerticalScrollbar())
        paintScrollbar(context, *frameView().verticalScrollbar(), rect);

    if (frameView().layerForScrollCorner())
        return;

    paintScrollCorner(context, frameView().scrollCornerRect());
}

void FramePainter::paintScrollCorner(GraphicsContext& context, const IntRect& cornerRect)
{
    if (frameView().scrollCorner()) {
        bool needsBackground = frameView().frame().isMainFrame();
        if (needsBackground && !LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, *frameView().layoutView(), DisplayItem::ScrollbarCorner)) {
            LayoutObjectDrawingRecorder drawingRecorder(context, *frameView().layoutView(), DisplayItem::ScrollbarCorner, FloatRect(cornerRect));
            context.fillRect(cornerRect, frameView().baseBackgroundColor());

        }
        ScrollbarPainter::paintIntoRect(*frameView().scrollCorner(), context, cornerRect.location(), LayoutRect(cornerRect));
        return;
    }

    ScrollbarTheme::theme().paintScrollCorner(context, *frameView().layoutView(), cornerRect);
}

void FramePainter::paintScrollbar(GraphicsContext& context, Scrollbar& bar, const IntRect& rect)
{
    bool needsBackground = bar.isCustomScrollbar() && frameView().frame().isMainFrame();
    if (needsBackground) {
        IntRect toFill = bar.frameRect();
        toFill.intersect(rect);
        context.fillRect(toFill, frameView().baseBackgroundColor());
    }

    bar.paint(context, CullRect(rect));
}

const FrameView& FramePainter::frameView()
{
    ASSERT(m_frameView);
    return *m_frameView;
}

} // namespace blink
