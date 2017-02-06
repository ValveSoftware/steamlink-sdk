/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef LayoutView_h
#define LayoutView_h

#include "core/CoreExport.h"
#include "core/layout/HitTestCache.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutState.h"
#include "core/layout/PaintInvalidationState.h"
#include "platform/PODFreeListArena.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"
#include <memory>

namespace blink {

class FrameView;
class PaintLayerCompositor;
class LayoutQuote;
class LayoutMedia;
class ViewFragmentationContext;

// LayoutView is the root of the layout tree and the Document's LayoutObject.
//
// It corresponds to the CSS concept of 'initial containing block' (or ICB).
// http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
//
// Its dimensions match that of the layout viewport. This viewport is used to
// size elements, in particular fixed positioned elements.
// LayoutView is always at position (0,0) relative to the document (and so isn't
// necessarily in view).
// See
// https://www.chromium.org/developers/design-documents/blink-coordinate-spaces
// about the different viewports.
//
// Because there is one LayoutView per rooted layout tree (or Frame), this class
// is used to add members shared by this tree (e.g. m_layoutState or
// m_layoutQuoteHead).
class CORE_EXPORT LayoutView final : public LayoutBlockFlow {
public:
    explicit LayoutView(Document*);
    ~LayoutView() override;
    void willBeDestroyed() override;

    // hitTest() will update layout, style and compositing first while hitTestNoLifecycleUpdate() does not.
    bool hitTest(HitTestResult&);
    bool hitTestNoLifecycleUpdate(HitTestResult&);

    // Returns the total count of calls to HitTest, for testing.
    unsigned hitTestCount() const { return m_hitTestCount; }
    unsigned hitTestCacheHits() const { return m_hitTestCacheHits; }

    void clearHitTestCache();

    const char* name() const override { return "LayoutView"; }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutView || LayoutBlockFlow::isOfType(type); }

    PaintLayerType layerTypeRequired() const override { return NormalPaintLayer; }

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    void layout() override;
    void updateLogicalWidth() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    // Based on FrameView::layoutSize, but:
    // - checks for null FrameView
    // - returns 0x0 if using printing layout
    // - scrollbar exclusion is compatible with root layer scrolling
    IntSize layoutSize(IncludeScrollbarsInRect = ExcludeScrollbars) const;

    int viewHeight(IncludeScrollbarsInRect scrollbarInclusion = ExcludeScrollbars) const { return layoutSize(scrollbarInclusion).height(); }
    int viewWidth(IncludeScrollbarsInRect scrollbarInclusion = ExcludeScrollbars) const { return layoutSize(scrollbarInclusion).width(); }

    int viewLogicalWidth(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewLogicalHeight(IncludeScrollbarsInRect = ExcludeScrollbars) const;

    LayoutUnit viewLogicalHeightForPercentages() const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    bool mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, MapCoordinatesFlags, VisualRectFlags) const;
    bool mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, VisualRectFlags = DefaultVisualRectFlags) const override;
    void adjustOffsetForFixedPosition(LayoutRect&) const;

    void invalidatePaintForViewAndCompositedLayers();

    void paint(const PaintInfo&, const LayoutPoint&) const override;
    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&) const override;

    enum SelectionPaintInvalidationMode { PaintInvalidationNewXOROld, PaintInvalidationNewMinusOld };
    void setSelection(LayoutObject* start, int startPos, LayoutObject*, int endPos, SelectionPaintInvalidationMode = PaintInvalidationNewXOROld);
    void clearSelection();
    bool hasPendingSelection() const;
    void commitPendingSelection();
    LayoutObject* selectionStart();
    LayoutObject* selectionEnd();
    IntRect selectionBounds();
    void selectionStartEnd(int& startPos, int& endPos);
    void invalidatePaintForSelection();

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&) const override;

    LayoutRect viewRect() const override;
    LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const override;

    LayoutState* layoutState() const { return m_layoutState; }

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    ViewFragmentationContext* fragmentationContext() const { return m_fragmentationContext.get(); }

    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    void setPageLogicalHeight(LayoutUnit height)
    {
        if (m_pageLogicalHeight != height) {
            m_pageLogicalHeight = height;
            m_pageLogicalHeightChanged = true;
        }
    }
    bool pageLogicalHeightChanged() const { return m_pageLogicalHeightChanged; }

    // Notification that this view moved into or out of a native window.
    void setIsInWindow(bool);

    PaintLayerCompositor* compositor();
    bool usesCompositing() const;

    LayoutRect backgroundRect(LayoutBox* backgroundLayoutObject) const;

    IntRect documentRect() const;

    // LayoutObject that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    IntervalArena* intervalArena();

    void setLayoutQuoteHead(LayoutQuote* head) { m_layoutQuoteHead = head; }
    LayoutQuote* layoutQuoteHead() const { return m_layoutQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addLayoutCounter() { m_layoutCounterCount++; }
    void removeLayoutCounter() { ASSERT(m_layoutCounterCount > 0); m_layoutCounterCount--; }
    bool hasLayoutCounters() { return m_layoutCounterCount; }

    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const override;

    // Returns the viewport size in (CSS pixels) that vh and vw units are calculated from.
    FloatSize viewportSizeForViewportUnits() const;

    void pushLayoutState(LayoutState& layoutState) { m_layoutState = &layoutState; }
    void popLayoutState() { ASSERT(m_layoutState); m_layoutState = m_layoutState->next(); }

    LayoutRect visualOverflowRect() const override;
    LayoutRect localOverflowRectForPaintInvalidation() const override;

    // Invalidates paint for the entire view, including composited descendants, but not including child frames.
    // It is very likely you do not want to call this method.
    void setShouldDoFullPaintInvalidationForViewAndAllDescendants();

    // The document scrollbar is always on the right, even in RTL. This is to prevent it from moving around on navigations.
    // TODO(skobes): This is not quite the ideal behavior, see http://crbug.com/250514 and http://crbug.com/249860.
    bool shouldPlaceBlockDirectionScrollbarOnLogicalLeft() const override { return false; }

    // Some LayoutMedias want to know about their viewport visibility for
    // crbug.com/487345,402044 .  This facility will be removed once those
    // experiments complete.
    // TODO(ojan): Merge this with IntersectionObserver once it lands.
    void registerMediaForPositionChangeNotification(LayoutMedia&);
    void unregisterMediaForPositionChangeNotification(LayoutMedia&);
    // Notify all registered LayoutMedias that their position on-screen might
    // have changed.  visibleRect is the clipping boundary.
    void sendMediaPositionChangeNotifications(const IntRect& visibleRect);

    // The rootLayerScrolls setting will ultimately determine whether FrameView
    // or PaintLayerScrollableArea handle the scroll.
    ScrollResult scroll(ScrollGranularity, const FloatSize&) override;

private:
    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const override;

    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const override;
    void mapAncestorToLocal(const LayoutBoxModelObject*, TransformState&, MapCoordinatesFlags) const override;
    void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const override;

    void layoutContent();
#if ENABLE(ASSERT)
    void checkLayoutState();
#endif

    void setShouldDoFullPaintInvalidationOnResizeIfNeeded();

    void updateFromStyle() override;
    bool allowsOverflowClip() const override;

    bool shouldUsePrintingLayout() const;

    int viewLogicalWidthForBoxSizing() const;
    int viewLogicalHeightForBoxSizing() const;

    UntracedMember<FrameView> m_frameView;

    // The current selection represented as 2 boundaries.
    // Selection boundaries are represented in LayoutView by a tuple
    // (LayoutObject, DOM node offset).
    // See http://www.w3.org/TR/dom/#range for more information.
    //
    // |m_selectionStartPos| and |m_selectionEndPos| are only valid for
    // |Text| node without 'transform' or 'first-letter'.
    //
    // Those are used for selection painting and paint invalidation upon
    // selection change.
    LayoutObject* m_selectionStart;
    LayoutObject* m_selectionEnd;

    // TODO(yosin): Clarify the meaning of these variables. editing/ passes
    // them as offsets in the DOM tree  but layout uses them as offset in the
    // layout tree.
    int m_selectionStartPos;
    int m_selectionEndPos;

    // The page logical height.
    // This is only used during printing to split the content into pages.
    // Outside of printing, this is 0.
    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;

    // LayoutState is an optimization used during layout.
    // |m_layoutState| will be nullptr outside of layout.
    //
    // See the class comment for more details.
    LayoutState* m_layoutState;

    std::unique_ptr<ViewFragmentationContext> m_fragmentationContext;
    std::unique_ptr<PaintLayerCompositor> m_compositor;
    RefPtr<IntervalArena> m_intervalArena;

    LayoutQuote* m_layoutQuoteHead;
    unsigned m_layoutCounterCount;

    unsigned m_hitTestCount;
    unsigned m_hitTestCacheHits;
    Persistent<HitTestCache> m_hitTestCache;

    Vector<LayoutMedia*> m_mediaForPositionNotification;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutView, isLayoutView());

} // namespace blink

#endif // LayoutView_h
