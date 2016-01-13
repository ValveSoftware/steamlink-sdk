/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef RenderFlowThread_h
#define RenderFlowThread_h


#include "core/rendering/RenderBlockFlow.h"
#include "wtf/HashCountedSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

struct LayerFragment;
typedef Vector<LayerFragment, 1> LayerFragments;
class RenderFlowThread;
class RenderRegion;

typedef ListHashSet<RenderRegion*> RenderRegionList;

// RenderFlowThread is used to collect all the render objects that participate in a
// flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, RenderRegion objects will redirect their paint
// and nodeAtPoint methods to this object. Each RenderRegion will actually be a viewPort
// of the RenderFlowThread.

class RenderFlowThread: public RenderBlockFlow {
public:
    RenderFlowThread();
    virtual ~RenderFlowThread() { };

    virtual bool isRenderFlowThread() const OVERRIDE FINAL { return true; }
    virtual bool isRenderMultiColumnFlowThread() const { return false; }

    virtual void layout() OVERRIDE;

    // Always create a RenderLayer for the RenderFlowThread so that we
    // can easily avoid drawing the children directly.
    virtual LayerType layerTypeRequired() const OVERRIDE FINAL { return NormalLayer; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE FINAL;

    virtual void addRegionToThread(RenderRegion*) = 0;
    virtual void removeRegionFromThread(RenderRegion*);
    const RenderRegionList& renderRegionList() const { return m_regionList; }

    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;

    bool hasRegions() const { return m_regionList.size(); }

    void validateRegions();
    void invalidateRegions();
    bool hasValidRegionInfo() const { return !m_regionsInvalidated && !m_regionList.isEmpty(); }

    void repaintRectangleInRegions(const LayoutRect&) const;

    LayoutPoint adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject&, const LayoutPoint&);

    LayoutUnit pageLogicalTopForOffset(LayoutUnit);
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit);
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule = IncludePageBoundary);

    virtual void setPageBreak(LayoutUnit /*offset*/, LayoutUnit /*spaceShortage*/) { }
    virtual void updateMinimumPageHeight(LayoutUnit /*offset*/, LayoutUnit /*minHeight*/) { }

    virtual RenderRegion* regionAtBlockOffset(LayoutUnit) const;

    bool regionsHaveUniformLogicalHeight() const { return m_regionsHaveUniformLogicalHeight; }

    RenderRegion* firstRegion() const;
    RenderRegion* lastRegion() const;

    void setRegionRangeForBox(const RenderBox*, LayoutUnit offsetFromLogicalTopOfFirstPage);
    void getRegionRangeForBox(const RenderBox*, RenderRegion*& startRegion, RenderRegion*& endRegion) const;

    virtual bool addForcedRegionBreak(LayoutUnit, RenderObject* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) { return false; }

    virtual bool isPageLogicalHeightKnown() const { return true; }
    bool pageLogicalSizeChanged() const { return m_pageLogicalSizeChanged; }

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect);
    LayoutRect fragmentsBoundingBox(const LayoutRect& layerBoundingBox);

    void pushFlowThreadLayoutState(const RenderObject&);
    void popFlowThreadLayoutState();
    LayoutUnit offsetFromLogicalTopOfFirstRegion(const RenderBlock*) const;

    // Used to estimate the maximum height of the flow thread.
    static LayoutUnit maxLogicalHeight() { return LayoutUnit::max() / 2; }

protected:
    virtual const char* renderName() const = 0;

    void updateRegionsFlowThreadPortionRect();
    bool shouldRepaint(const LayoutRect&) const;

    bool cachedOffsetFromLogicalTopOfFirstRegion(const RenderBox*, LayoutUnit&) const;
    void setOffsetFromLogicalTopOfFirstRegion(const RenderBox*, LayoutUnit);
    void clearOffsetFromLogicalTopOfFirstRegion(const RenderBox*);

    const RenderBox* currentStatePusherRenderBox() const;

    RenderRegionList m_regionList;

    class RenderRegionRange {
    public:
        RenderRegionRange()
        {
            setRange(0, 0);
        }

        RenderRegionRange(RenderRegion* start, RenderRegion* end)
        {
            setRange(start, end);
        }

        void setRange(RenderRegion* start, RenderRegion* end)
        {
            m_startRegion = start;
            m_endRegion = end;
        }

        RenderRegion* startRegion() const { return m_startRegion; }
        RenderRegion* endRegion() const { return m_endRegion; }

    private:
        RenderRegion* m_startRegion;
        RenderRegion* m_endRegion;
    };

    typedef PODInterval<LayoutUnit, RenderRegion*> RegionInterval;
    typedef PODIntervalTree<LayoutUnit, RenderRegion*> RegionIntervalTree;

    class RegionSearchAdapter {
    public:
        RegionSearchAdapter(LayoutUnit offset)
            : m_offset(offset)
            , m_result(0)
        {
        }

        const LayoutUnit& lowValue() const { return m_offset; }
        const LayoutUnit& highValue() const { return m_offset; }
        void collectIfNeeded(const RegionInterval&);

        RenderRegion* result() const { return m_result; }

    private:
        LayoutUnit m_offset;
        RenderRegion* m_result;
    };

    // A maps from RenderBox
    typedef HashMap<const RenderBox*, RenderRegionRange> RenderRegionRangeMap;
    RenderRegionRangeMap m_regionRangeMap;

    // Stack of objects that pushed a LayoutState object on the RenderView. The
    // objects on the stack are the ones that are curently in the process of being
    // laid out.
    ListHashSet<const RenderObject*> m_statePusherObjectsStack;
    typedef HashMap<const RenderBox*, LayoutUnit> RenderBoxToOffsetMap;
    RenderBoxToOffsetMap m_boxesToOffsetMap;

    RegionIntervalTree m_regionIntervalTree;

    bool m_regionsInvalidated : 1;
    bool m_regionsHaveUniformLogicalHeight : 1;
    bool m_pageLogicalSizeChanged : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderFlowThread, isRenderFlowThread());

class CurrentRenderFlowThreadMaintainer {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadMaintainer);
public:
    CurrentRenderFlowThreadMaintainer(RenderFlowThread*);
    ~CurrentRenderFlowThreadMaintainer();
private:
    RenderFlowThread* m_renderFlowThread;
    RenderFlowThread* m_previousRenderFlowThread;
};

// These structures are used by PODIntervalTree for debugging.
#ifndef NDEBUG
template <> struct ValueToString<LayoutUnit> {
    static String string(const LayoutUnit value) { return String::number(value.toFloat()); }
};

template <> struct ValueToString<RenderRegion*> {
    static String string(const RenderRegion* value) { return String::format("%p", value); }
};
#endif

} // namespace WebCore

#endif // RenderFlowThread_h
