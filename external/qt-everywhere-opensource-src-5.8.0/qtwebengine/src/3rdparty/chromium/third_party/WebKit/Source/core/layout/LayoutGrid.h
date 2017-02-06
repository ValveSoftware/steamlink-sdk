/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutGrid_h
#define LayoutGrid_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/OrderIterator.h"
#include "core/style/GridPositionsResolver.h"
#include <memory>

namespace blink {

struct ContentAlignmentData;
struct GridArea;
struct GridSpan;
class GridTrack;

enum TrackSizeComputationPhase {
    ResolveIntrinsicMinimums,
    ResolveContentBasedMinimums,
    ResolveMaxContentMinimums,
    ResolveIntrinsicMaximums,
    ResolveMaxContentMaximums,
    MaximizeTracks,
};
enum GridAxisPosition {GridAxisStart, GridAxisEnd, GridAxisCenter};

class LayoutGrid final : public LayoutBlock {
public:
    explicit LayoutGrid(Element*);
    ~LayoutGrid() override;

    const char* name() const override { return "LayoutGrid"; }

    void layoutBlock(bool relayoutChildren) override;

    void dirtyGrid();

    const Vector<LayoutUnit>& columnPositions() const
    {
        ASSERT(!m_gridIsDirty);
        return m_columnPositions;
    }

    const Vector<LayoutUnit>& rowPositions() const
    {
        ASSERT(!m_gridIsDirty);
        return m_rowPositions;
    }

    LayoutUnit guttersSize(GridTrackSizingDirection, size_t span) const;

    LayoutUnit offsetBetweenTracks(GridTrackSizingDirection direction) const
    {
        return direction == ForColumns ? m_offsetBetweenColumns : m_offsetBetweenRows;
    }

    typedef Vector<LayoutBox*, 1> GridCell;
    const GridCell& gridCell(int row, int column) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!m_gridIsDirty);
        return m_grid[row][column];
    }

    const Vector<LayoutBox*>& itemsOverflowingGridArea() const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!m_gridIsDirty);
        return m_gridItemsOverflowingGridArea;
    }

    int paintIndexForGridItem(const LayoutBox* layoutBox) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!m_gridIsDirty);
        return m_gridItemsIndexesMap.get(layoutBox);
    }

    size_t autoRepeatCountForDirection(GridTrackSizingDirection direction) const
    {
        return direction == ForColumns ? m_autoRepeatColumns : m_autoRepeatRows;
    }

    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutGrid || LayoutBlock::isOfType(type); }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    LayoutUnit computeIntrinsicLogicalContentHeightUsing(const Length& logicalHeightLength, LayoutUnit intrinsicContentHeight, LayoutUnit borderAndPadding) const override;

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;

    void styleDidChange(StyleDifference, const ComputedStyle*) override;

    bool explicitGridDidResize(const ComputedStyle&) const;
    bool namedGridLinesDefinitionDidChange(const ComputedStyle&) const;

    class GridIterator;
    struct GridSizingData;
    enum SizingOperation { TrackSizing, IntrinsicSizeComputation };
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization);
    LayoutUnit computeUsedBreadthOfMinLength(const GridLength&, LayoutUnit maxBreadth) const;
    LayoutUnit computeUsedBreadthOfMaxLength(const GridLength&, LayoutUnit usedBreadth, LayoutUnit maxBreadth) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&);

    void ensureGridSize(size_t maximumRowSize, size_t maximumColumnSize);
    void insertItemIntoGrid(LayoutBox&, const GridArea&);

    size_t computeAutoRepeatTracksCount(GridTrackSizingDirection) const;

    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    std::unique_ptr<GridArea> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const LayoutBox&, GridTrackSizingDirection, const GridSpan& specifiedPositions) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<LayoutBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<LayoutBox*>&);
    void placeAutoMajorAxisItemOnGrid(LayoutBox&, std::pair<size_t, size_t>& autoPlacementCursor);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void computeIntrinsicLogicalHeight(GridSizingData&);
    LayoutUnit computeTrackBasedLogicalHeight(const GridSizingData&) const;
    void computeTrackSizesForDirection(GridTrackSizingDirection, GridSizingData&, LayoutUnit freeSpace);

    void repeatTracksSizingIfNeeded(GridSizingData&, LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows);

    void layoutGridItems(GridSizingData&);
    void prepareChildForPositionedLayout(LayoutBox&);
    void layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior = DefaultLayout);
    void offsetAndBreadthForPositionedChild(const LayoutBox&, GridTrackSizingDirection, LayoutUnit& offset, LayoutUnit& breadth);
    void populateGridPositionsForDirection(GridSizingData&, GridTrackSizingDirection);

    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    LayoutUnit currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, LayoutBox&, GridTrackSizingDirection, GridSizingData&);
    void resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection, const GridSpan&, LayoutBox& gridItem, GridTrack&, GridSizingData&);
    template <TrackSizeComputationPhase> void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, const GridItemsSpanGroupRange&);
    template <TrackSizeComputationPhase> void distributeSpaceToTracks(Vector<GridTrack*>&, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, GridSizingData&, LayoutUnit& availableLogicalSpace);

    typedef HashSet<size_t, DefaultHash<size_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<size_t>> TrackIndexSet;
    double computeFlexFactorUnitSize(const Vector<GridTrack>&, GridTrackSizingDirection, double flexFactorSum, LayoutUnit& leftOverSpace, const Vector<size_t, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
    double findFlexFactorUnitSize(const Vector<GridTrack>&, const GridSpan&, GridTrackSizingDirection, LayoutUnit leftOverSpace) const;

    const GridTrackSize& rawGridTrackSize(GridTrackSizingDirection, size_t) const;
    GridTrackSize gridTrackSize(GridTrackSizingDirection, size_t, SizingOperation = TrackSizing) const;

    bool updateOverrideContainingBlockContentSizeForChild(LayoutBox&, GridTrackSizingDirection, GridSizingData&);
    LayoutUnit logicalHeightForChild(LayoutBox&, GridSizingData&);
    LayoutUnit minSizeForChild(LayoutBox&, GridTrackSizingDirection, GridSizingData&);
    LayoutUnit minContentForChild(LayoutBox&, GridTrackSizingDirection, GridSizingData&);
    LayoutUnit maxContentForChild(LayoutBox&, GridTrackSizingDirection, GridSizingData&);
    GridAxisPosition columnAxisPositionForChild(const LayoutBox&) const;
    GridAxisPosition rowAxisPositionForChild(const LayoutBox&) const;
    LayoutUnit rowAxisOffsetForChild(const LayoutBox&, GridSizingData&) const;
    LayoutUnit columnAxisOffsetForChild(const LayoutBox&, GridSizingData&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    LayoutPoint findChildLogicalPosition(const LayoutBox&, GridSizingData&) const;
    GridArea cachedGridArea(const LayoutBox&) const;
    GridSpan cachedGridSpan(const LayoutBox&, GridTrackSizingDirection) const;

    LayoutUnit gridAreaBreadthForChild(const LayoutBox& child, GridTrackSizingDirection, const GridSizingData&) const;
    LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(const LayoutBox&, GridTrackSizingDirection, const GridSizingData&) const;
    LayoutUnit assumedRowsSizeForOrthogonalChild(const LayoutBox&, SizingOperation) const;

    void applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection, GridSizingData&);

    void paintChildren(const PaintInfo&, const LayoutPoint&) const override;

    LayoutUnit marginLogicalHeightForChild(const LayoutBox&) const;
    LayoutUnit computeMarginLogicalSizeForChild(MarginDirection, const LayoutBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const LayoutBox&) const;
    void applyStretchAlignmentToChildIfNeeded(LayoutBox&);
    bool hasAutoMarginsInColumnAxis(const LayoutBox&) const;
    bool hasAutoMarginsInRowAxis(const LayoutBox&) const;
    void updateAutoMarginsInColumnAxisIfNeeded(LayoutBox&);
    void updateAutoMarginsInRowAxisIfNeeded(LayoutBox&);

#if ENABLE(ASSERT)
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, GridSizingData&);
#endif

    size_t gridItemSpan(const LayoutBox&, GridTrackSizingDirection);
    bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&, GridTrackSizingDirection, SizingOperation) const;

    size_t gridColumnCount() const;
    size_t gridRowCount() const;

    bool isOrthogonalChild(const LayoutBox&) const;
    GridTrackSizingDirection flowAwareDirectionForChild(const LayoutBox&, GridTrackSizingDirection) const;

    typedef Vector<Vector<GridCell>> GridRepresentation;
    GridRepresentation m_grid;
    bool m_gridIsDirty;
    Vector<LayoutUnit> m_rowPositions;
    Vector<LayoutUnit> m_columnPositions;
    LayoutUnit m_offsetBetweenColumns;
    LayoutUnit m_offsetBetweenRows;
    HashMap<const LayoutBox*, GridArea> m_gridItemArea;
    OrderIterator m_orderIterator;
    Vector<LayoutBox*> m_gridItemsOverflowingGridArea;
    HashMap<const LayoutBox*, size_t> m_gridItemsIndexesMap;

    LayoutUnit m_minContentHeight { -1 };
    LayoutUnit m_maxContentHeight { -1 };

    int m_smallestRowStart;
    int m_smallestColumnStart;

    size_t m_autoRepeatColumns { 0 };
    size_t m_autoRepeatRows { 0 };

    bool m_hasAnyOrthogonalChild;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutGrid, isLayoutGrid());

} // namespace blink

#endif // LayoutGrid_h
