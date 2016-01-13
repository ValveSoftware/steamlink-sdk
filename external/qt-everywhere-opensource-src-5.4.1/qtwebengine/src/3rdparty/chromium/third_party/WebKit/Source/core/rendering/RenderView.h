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

#ifndef RenderView_h
#define RenderView_h

#include "core/frame/FrameView.h"
#include "core/rendering/LayoutState.h"
#include "core/rendering/RenderBlockFlow.h"
#include "platform/PODFreeListArena.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class FlowThreadController;
class RenderLayerCompositor;
class RenderQuote;

// The root of the render tree, corresponding to the CSS initial containing block.
// It's dimensions match that of the logical viewport (which may be different from
// the visible viewport in fixed-layout mode), and it is always at position (0,0)
// relative to the document (and so isn't necessarily in view).
class RenderView FINAL : public RenderBlockFlow {
public:
    explicit RenderView(Document*);
    virtual ~RenderView();

    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    virtual const char* renderName() const OVERRIDE { return "RenderView"; }

    virtual bool isRenderView() const OVERRIDE { return true; }

    virtual LayerType layerTypeRequired() const OVERRIDE { return NormalLayer; }

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    virtual void layout() OVERRIDE;
    virtual void updateLogicalWidth() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;

    virtual LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const OVERRIDE;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewWidth(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewLogicalWidth() const
    {
        return style()->isHorizontalWritingMode() ? viewWidth(ExcludeScrollbars) : viewHeight(ExcludeScrollbars);
    }
    int viewLogicalHeight() const;
    LayoutUnit viewLogicalHeightForPercentages() const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    virtual void mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer, LayoutRect&, bool fixed = false) const OVERRIDE;
    void repaintViewRectangle(const LayoutRect&) const;

    void repaintViewAndCompositedLayers();

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual void paintBoxDecorations(PaintInfo&, const LayoutPoint&) OVERRIDE;

    enum SelectionRepaintMode { RepaintNewXOROld, RepaintNewMinusOld, RepaintNothing };
    void setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode = RepaintNewXOROld);
    void getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const;
    void clearSelection();
    RenderObject* selectionStart() const { return m_selectionStart; }
    RenderObject* selectionEnd() const { return m_selectionEnd; }
    IntRect selectionBounds(bool clipToVisibleContent = true) const;
    void selectionStartEnd(int& startPos, int& endPos) const;
    void repaintSelection() const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const OVERRIDE;

    virtual LayoutRect viewRect() const OVERRIDE;

    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    LayoutSize layoutDelta() const
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        return m_layoutState ? m_layoutState->layoutDelta() : LayoutSize();
    }
    void addLayoutDelta(const LayoutSize& delta)
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        if (m_layoutState)
            m_layoutState->addLayoutDelta(delta);
    }

#if ASSERT_ENABLED
    bool layoutDeltaMatches(const LayoutSize& delta)
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        if (!m_layoutState)
            return false;
        return (delta.width() == m_layoutState->layoutDelta().width() || m_layoutState->layoutDeltaXSaturated()) && (delta.height() == m_layoutState->layoutDelta().height() || m_layoutState->layoutDeltaYSaturated());
    }
#endif

    bool shouldDoFullRepaintForNextLayout() const;
    bool doingFullRepaint() const { return m_frameView->needsFullPaintInvalidation(); }

    // Returns true if layoutState should be used for its cached offset and clip.
    bool layoutStateCachedOffsetsEnabled() const { return m_layoutState && m_layoutState->cachedOffsetsEnabled(); }
    LayoutState* layoutState() const { return m_layoutState; }

    bool canMapUsingLayoutStateForContainer(const RenderObject* repaintContainer) const
    {
        // FIXME: LayoutState should be enabled for other repaint containers than the RenderView. crbug.com/363834
        return layoutStateCachedOffsetsEnabled() && (repaintContainer == this);
    }

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) OVERRIDE;

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

    RenderLayerCompositor* compositor();
    bool usesCompositing() const;

    IntRect unscaledDocumentRect() const;
    LayoutRect backgroundRect(RenderBox* backgroundRenderer) const;

    IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    FlowThreadController* flowThreadController();

    IntervalArena* intervalArena();

    void setRenderQuoteHead(RenderQuote* head) { m_renderQuoteHead = head; }
    RenderQuote* renderQuoteHead() const { return m_renderQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addRenderCounter() { m_renderCounterCount++; }
    void removeRenderCounter() { ASSERT(m_renderCounterCount > 0); m_renderCounterCount--; }
    bool hasRenderCounters() { return m_renderCounterCount; }

    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const OVERRIDE;

    double layoutViewportWidth() const;
    double layoutViewportHeight() const;

    void pushLayoutState(LayoutState&);
    void popLayoutState();
private:
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const OVERRIDE;
    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const OVERRIDE;

    virtual void invalidateTreeAfterLayout(const RenderLayerModelObject& paintInvalidationContainer) OVERRIDE FINAL;

    bool shouldRepaint(const LayoutRect&) const;

    bool rootFillsViewportBackground(RenderBox* rootBox) const;

    void layoutContent();
#ifndef NDEBUG
    void checkLayoutState();
#endif

    void positionDialog(RenderBox*);
    void positionDialogs();

    friend class ForceHorriblySlowRectMapping;

    bool shouldUsePrintingLayout() const;

    RenderObject* backgroundRenderer() const;

    FrameView* m_frameView;

    RenderObject* m_selectionStart;
    RenderObject* m_selectionEnd;

    int m_selectionStartPos;
    int m_selectionEndPos;

    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;
    LayoutState* m_layoutState;
    OwnPtr<RenderLayerCompositor> m_compositor;
    OwnPtr<FlowThreadController> m_flowThreadController;
    RefPtr<IntervalArena> m_intervalArena;

    RenderQuote* m_renderQuoteHead;
    unsigned m_renderCounterCount;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderView, isRenderView());

// Suspends the LayoutState cached offset and clipRect optimization. Used under transforms
// that cannot be represented by LayoutState (common in SVG) and when manipulating the render
// tree during layout in ways that can trigger repaint of a non-child (e.g. when a list item
// moves its list marker around). Note that even when disabled, LayoutState is still used to
// store layoutDelta.
class ForceHorriblySlowRectMapping {
    WTF_MAKE_NONCOPYABLE(ForceHorriblySlowRectMapping);
public:
    ForceHorriblySlowRectMapping(const RenderObject& root)
        : m_view(*root.view())
        , m_didDisable(m_view.layoutState() && m_view.layoutState()->cachedOffsetsEnabled())
    {
        if (m_view.layoutState())
            m_view.layoutState()->m_cachedOffsetsEnabled = false;
#if ASSERT_ENABLED
        m_layoutState = m_view.layoutState();
#endif
    }

    ~ForceHorriblySlowRectMapping()
    {
        ASSERT(m_view.layoutState() == m_layoutState);
        if (m_didDisable)
            m_view.layoutState()->m_cachedOffsetsEnabled = true;
    }
private:
    RenderView& m_view;
    bool m_didDisable;
#if ASSERT_ENABLED
    LayoutState* m_layoutState;
#endif
};

} // namespace WebCore

#endif // RenderView_h
