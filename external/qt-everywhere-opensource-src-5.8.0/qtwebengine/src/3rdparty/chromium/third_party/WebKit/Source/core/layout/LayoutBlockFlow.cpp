/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutBlockFlow.h"

#include "core/editing/Editor.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDialogElement.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutPagedFlowThread.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/line/GlyphOverflow.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/LineWidth.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"
#include "core/paint/PaintLayer.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

bool LayoutBlockFlow::s_canPropagateFloatIntoSibling = false;

struct SameSizeAsLayoutBlockFlow : public LayoutBlock {
    LineBoxList lineBoxes;
    void* pointers[2];
};

static_assert(sizeof(LayoutBlockFlow) == sizeof(SameSizeAsLayoutBlockFlow), "LayoutBlockFlow should stay small");

struct SameSizeAsMarginInfo {
    uint16_t bitfields;
    LayoutUnit margins[2];
};

static_assert(sizeof(LayoutBlockFlow::MarginValues) == sizeof(LayoutUnit[4]), "MarginValues should stay small");

// Caches all our current margin collapsing state.
class MarginInfo {
    // Collapsing flags for whether we can collapse our margins with our children's margins.
    bool m_canCollapseWithChildren : 1;
    bool m_canCollapseMarginBeforeWithChildren : 1;
    bool m_canCollapseMarginAfterWithChildren : 1;
    bool m_canCollapseMarginAfterWithLastChild: 1;

    // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
    // margins in our container. Table cells and the body are the common examples. We
    // also have a custom style property for Safari RSS to deal with TypePad blog articles.
    bool m_quirkContainer : 1;

    // This flag tracks whether we are still looking at child margins that can all collapse together at the beginning of a block.
    // They may or may not collapse with the top margin of the block (|m_canCollapseTopWithChildren| tells us that), but they will
    // always be collapsing with one another. This variable can remain set to true through multiple iterations
    // as long as we keep encountering self-collapsing blocks.
    bool m_atBeforeSideOfBlock : 1;

    // This flag is set when we know we're examining bottom margins and we know we're at the bottom of the block.
    bool m_atAfterSideOfBlock : 1;

    // These variables are used to detect quirky margins that we need to collapse away (in table cells
    // and in the body element).
    bool m_hasMarginBeforeQuirk : 1;
    bool m_hasMarginAfterQuirk : 1;
    bool m_determinedMarginBeforeQuirk : 1;

    bool m_discardMargin : 1;
    bool m_lastChildIsSelfCollapsingBlockWithClearance : 1;

    // These flags track the previous maximal positive and negative margins.
    LayoutUnit m_positiveMargin;
    LayoutUnit m_negativeMargin;

public:
    MarginInfo(LayoutBlockFlow*, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding);

    void setAtBeforeSideOfBlock(bool b) { m_atBeforeSideOfBlock = b; }
    void setAtAfterSideOfBlock(bool b) { m_atAfterSideOfBlock = b; }
    void clearMargin()
    {
        m_positiveMargin = LayoutUnit();
        m_negativeMargin = LayoutUnit();
    }
    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }
    void setDeterminedMarginBeforeQuirk(bool b) { m_determinedMarginBeforeQuirk = b; }
    void setPositiveMargin(LayoutUnit p) { ASSERT(!m_discardMargin); m_positiveMargin = p; }
    void setNegativeMargin(LayoutUnit n) { ASSERT(!m_discardMargin); m_negativeMargin = n; }
    void setPositiveMarginIfLarger(LayoutUnit p)
    {
        ASSERT(!m_discardMargin);
        if (p > m_positiveMargin)
            m_positiveMargin = p;
    }
    void setNegativeMarginIfLarger(LayoutUnit n)
    {
        ASSERT(!m_discardMargin);
        if (n > m_negativeMargin)
            m_negativeMargin = n;
    }

    void setMargin(LayoutUnit p, LayoutUnit n) { ASSERT(!m_discardMargin); m_positiveMargin = p; m_negativeMargin = n; }
    void setCanCollapseMarginAfterWithChildren(bool collapse) { m_canCollapseMarginAfterWithChildren = collapse; }
    void setCanCollapseMarginAfterWithLastChild(bool collapse) { m_canCollapseMarginAfterWithLastChild = collapse; }
    void setDiscardMargin(bool value) { m_discardMargin = value; }

    bool atBeforeSideOfBlock() const { return m_atBeforeSideOfBlock; }
    bool canCollapseWithMarginBefore() const { return m_atBeforeSideOfBlock && m_canCollapseMarginBeforeWithChildren; }
    bool canCollapseWithMarginAfter() const { return m_atAfterSideOfBlock && m_canCollapseMarginAfterWithChildren; }
    bool canCollapseMarginBeforeWithChildren() const { return m_canCollapseMarginBeforeWithChildren; }
    bool canCollapseMarginAfterWithChildren() const { return m_canCollapseMarginAfterWithChildren; }
    bool canCollapseMarginAfterWithLastChild() const { return m_canCollapseMarginAfterWithLastChild; }
    bool quirkContainer() const { return m_quirkContainer; }
    bool determinedMarginBeforeQuirk() const { return m_determinedMarginBeforeQuirk; }
    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }
    LayoutUnit positiveMargin() const { return m_positiveMargin; }
    LayoutUnit negativeMargin() const { return m_negativeMargin; }
    bool discardMargin() const { return m_discardMargin; }
    LayoutUnit margin() const { return m_positiveMargin - m_negativeMargin; }
    void setLastChildIsSelfCollapsingBlockWithClearance(bool value) { m_lastChildIsSelfCollapsingBlockWithClearance = value; }
    bool lastChildIsSelfCollapsingBlockWithClearance() const { return m_lastChildIsSelfCollapsingBlockWithClearance; }
};

// Some features, such as floats, margin collapsing and fragmentation, require some knowledge about
// things that happened when laying out previous block child siblings. Only looking at the object
// currently being laid out isn't always enough.
class BlockChildrenLayoutInfo {
public:
    BlockChildrenLayoutInfo(LayoutBlockFlow* blockFlow, LayoutUnit beforeEdge, LayoutUnit afterEdge)
        : m_marginInfo(blockFlow, beforeEdge, afterEdge)
        , m_previousBreakAfterValue(BreakAuto)
        , m_isAtFirstInFlowChild(true) { }

    const MarginInfo& marginInfo() const { return m_marginInfo; }
    MarginInfo& marginInfo() { return m_marginInfo; }
    LayoutUnit& previousFloatLogicalBottom() { return m_previousFloatLogicalBottom; }

    EBreak previousBreakAfterValue() const { return m_previousBreakAfterValue; }
    void setPreviousBreakAfterValue(EBreak value) { m_previousBreakAfterValue = value; }

    bool isAtFirstInFlowChild() const { return m_isAtFirstInFlowChild; }
    void clearIsAtFirstInFlowChild() { m_isAtFirstInFlowChild = false; }

private:
    MarginInfo m_marginInfo;
    LayoutUnit m_previousFloatLogicalBottom;
    EBreak m_previousBreakAfterValue;
    bool m_isAtFirstInFlowChild;
};

LayoutBlockFlow::LayoutBlockFlow(ContainerNode* node)
    : LayoutBlock(node)
{
    static_assert(sizeof(MarginInfo) == sizeof(SameSizeAsMarginInfo), "MarginInfo should stay small");
    setChildrenInline(true);
}

LayoutBlockFlow::~LayoutBlockFlow()
{
}

LayoutBlockFlow* LayoutBlockFlow::createAnonymous(Document* document)
{
    LayoutBlockFlow* layoutBlockFlow = new LayoutBlockFlow(nullptr);
    layoutBlockFlow->setDocumentForAnonymous(document);
    return layoutBlockFlow;
}

LayoutObject* LayoutBlockFlow::layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread();
    if (!flowThread)
        return nullptr;
    setLogicalTopForChild(*flowThread, borderBefore() + paddingBefore());
    flowThread->layoutColumns(layoutScope);
    determineLogicalLeftPositionForChild(*flowThread);
    return flowThread;
}

bool LayoutBlockFlow::updateLogicalWidthAndColumnWidth()
{
    bool relayoutChildren = LayoutBlock::updateLogicalWidthAndColumnWidth();
    if (LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread()) {
        if (flowThread->needsNewWidth())
            return true;
    }
    return relayoutChildren;
}

void LayoutBlockFlow::checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight)
{
    if (LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread()) {
        // Calculate the non-auto content box height, or set it to 0 if it's auto. We need to know
        // this before layout, so that we can figure out where to insert column breaks. We also
        // treat LayoutView (which may be paginated, which uses the multicol implmentation) as
        // having non-auto height, since its height is deduced from the viewport height. We use
        // computeLogicalHeight() to calculate the content box height. That method will clamp
        // against max-height and min-height. Since we're now at the beginning of layout, and we
        // don't know the actual height of the content yet, only call that method when height is
        // definite, or we might fool ourselves into believing that columns have a definite height
        // when they in fact don't.
        // To check if we've a definite height we verify that percentage height is resolvable
        // on the first in-flow child.
        LayoutUnit columnHeight;
        if (!firstInFlowChildBox() || firstInFlowChildBox()->percentageLogicalHeightIsResolvable() || isLayoutView()) {
            LogicalExtentComputedValues computedValues;
            computeLogicalHeight(LayoutUnit(), logicalTop(), computedValues);
            columnHeight = computedValues.m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight();
        }
        pageLogicalHeightChanged = columnHeight != flowThread->columnHeightAvailable();
        flowThread->setColumnHeightAvailable(std::max(columnHeight, LayoutUnit()));
    } else if (isLayoutFlowThread()) {
        LayoutFlowThread* flowThread = toLayoutFlowThread(this);

        // FIXME: This is a hack to always make sure we have a page logical height, if said height
        // is known. The page logical height thing in LayoutState is meaningless for flow
        // thread-based pagination (page height isn't necessarily uniform throughout the flow
        // thread), but as long as it is used universally as a means to determine whether page
        // height is known or not, we need this. Page height is unknown when column balancing is
        // enabled and flow thread height is still unknown (i.e. during the first layout pass). When
        // it's unknown, we need to prevent the pagination code from assuming page breaks everywhere
        // and thereby eating every top margin. It should be trivial to clean up and get rid of this
        // hack once the old multicol implementation is gone.
        pageLogicalHeight = flowThread->isPageLogicalHeightKnown() ? LayoutUnit(1) : LayoutUnit();

        pageLogicalHeightChanged = flowThread->pageLogicalSizeChanged();
    }
}

void LayoutBlockFlow::setBreakAtLineToAvoidWidow(int lineToBreak)
{
    ASSERT(lineToBreak >= 0);
    ensureRareData();
    ASSERT(!m_rareData->m_didBreakAtLineToAvoidWidow);
    m_rareData->m_lineBreakToAvoidWidow = lineToBreak;
}

void LayoutBlockFlow::setDidBreakAtLineToAvoidWidow()
{
    ASSERT(!shouldBreakAtLineToAvoidWidow());

    // This function should be called only after a break was applied to avoid widows
    // so assert |m_rareData| exists.
    ASSERT(m_rareData);

    m_rareData->m_didBreakAtLineToAvoidWidow = true;
}

void LayoutBlockFlow::clearDidBreakAtLineToAvoidWidow()
{
    if (!m_rareData)
        return;

    m_rareData->m_didBreakAtLineToAvoidWidow = false;
}

void LayoutBlockFlow::clearShouldBreakAtLineToAvoidWidow() const
{
    ASSERT(shouldBreakAtLineToAvoidWidow());
    if (!m_rareData)
        return;

    m_rareData->m_lineBreakToAvoidWidow = -1;
}

bool LayoutBlockFlow::isSelfCollapsingBlock() const
{
    if (needsLayout()) {
        // Sometimes we don't lay out objects in DOM order (column spanners being one such relevant
        // type of object right here). As long as the object in question establishes a new
        // formatting context, that's nothing to worry about, though.
        ASSERT(createsNewFormattingContext());
        return false;
    }
    ASSERT(!m_isSelfCollapsing == !checkIfIsSelfCollapsingBlock());
    return m_isSelfCollapsing;
}

bool LayoutBlockFlow::checkIfIsSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) have border/padding,
    // (c) have a min-height
    // (d) have specified that one of our margins can't collapse using a CSS extension
    // (e) establish a new block formatting context.

    // The early exit must be done before we check for clean layout.
    // We should be able to give a quick answer if the box is a relayout boundary.
    // Being a relayout boundary implies a block formatting context, and also
    // our internal layout shouldn't affect our container in any way.
    if (createsNewFormattingContext())
        return false;

    // Placeholder elements are not laid out until the dimensions of their parent text control are known, so they
    // don't get layout until their parent has had layout - this is unique in the layout tree and means
    // when we call isSelfCollapsingBlock on them we find that they still need layout.
    ASSERT(!needsLayout() || (node() && node()->isElementNode() && toElement(node())->shadowPseudoId() == "-webkit-input-placeholder"));

    if (logicalHeight() > LayoutUnit()
        || borderAndPaddingLogicalHeight()
        || style()->logicalMinHeight().isPositive()
        || style()->marginBeforeCollapse() == MarginCollapseSeparate || style()->marginAfterCollapse() == MarginCollapseSeparate)
        return false;

    Length logicalHeightLength = style()->logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.hasPercent() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (LayoutBlock* cb = containingBlock(); !cb->isLayoutView(); cb = cb->containingBlock()) {
            if (cb->style()->logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.hasPercent()) && logicalHeightLength.isZero())) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !firstLineBox();

        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isFloatingOrOutOfFlowPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void LayoutBlockFlow::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());
    ASSERT(isInlineBlockOrInlineTable() || !isInline());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutAnalyzer::BlockScope analyzer(*this);
    SubtreeLayoutScope layoutScope(*this);

    // Multiple passes might be required for column based layout.
    // The number of passes could be as high as the number of columns.
    bool done = false;
    LayoutUnit pageLogicalHeight;
    while (!done)
        done = layoutBlockFlow(relayoutChildren, pageLogicalHeight, layoutScope);

    LayoutView* layoutView = view();
    if (layoutView->layoutState()->pageLogicalHeight())
        setPageLogicalOffset(layoutView->layoutState()->pageLogicalOffset(*this, logicalTop()));

    updateLayerTransformAfterLayout();

    updateAfterLayout();

    if (isHTMLDialogElement(node()) && isOutOfFlowPositioned())
        positionDialog();

    clearNeedsLayout();
    m_isSelfCollapsing = checkIfIsSelfCollapsingBlock();
}

inline bool LayoutBlockFlow::layoutBlockFlow(bool relayoutChildren, LayoutUnit &pageLogicalHeight, SubtreeLayoutScope& layoutScope)
{
    LayoutUnit oldLeft = logicalLeft();
    bool logicalWidthChanged = updateLogicalWidthAndColumnWidth();
    relayoutChildren |= logicalWidthChanged;

    rebuildFloatsFromIntruding();

    bool pageLogicalHeightChanged = false;
    bool hasSpecifiedPageLogicalHeight = false;
    checkForPaginationLogicalHeightChange(pageLogicalHeight, pageLogicalHeightChanged, hasSpecifiedPageLogicalHeight);
    if (pageLogicalHeightChanged)
        relayoutChildren = true;

    LayoutState state(*this, locationOffset(), pageLogicalHeight, pageLogicalHeightChanged, logicalWidthChanged);

    // We use four values, maxTopPos, maxTopNeg, maxBottomPos, and maxBottomNeg, to track
    // our current maximal positive and negative margins. These values are used when we
    // are collapsed with adjacent blocks, so for example, if you have block A and B
    // collapsing together, then you'd take the maximal positive margin from both A and B
    // and subtract it from the maximal negative margin from both A and B to get the
    // true collapsed margin. This algorithm is recursive, so when we finish layout()
    // our block knows its current maximal positive/negative values.
    //
    // Start out by setting our margin values to our current margins. Table cells have
    // no margins, so we don't fill in the values for table cells.
    if (!isTableCell()) {
        initMaxMarginValues();
        setHasMarginBeforeQuirk(style()->hasMarginBeforeQuirk());
        setHasMarginAfterQuirk(style()->hasMarginAfterQuirk());
    }

    if (state.isPaginated()) {
        setPaginationStrutPropagatedFromChild(LayoutUnit());

        // Start with any applicable computed break-after and break-before values for this
        // object. During child layout, breakBefore will be joined with the breakBefore value of
        // the first in-flow child, and breakAfter will be joined with the breakAfter value of the
        // last in-flow child. This is done in order to honor the requirement that a class A break
        // point [1] may only exists *between* in-flow siblings (i.e. not before the first child
        // and not after the last child).
        //
        // [1] https://drafts.csswg.org/css-break/#possible-breaks
        setBreakBefore(LayoutBlock::breakBefore());
        setBreakAfter(LayoutBlock::breakAfter());
    }

    LayoutUnit beforeEdge = borderBefore() + paddingBefore();
    LayoutUnit afterEdge = borderAfter() + paddingAfter() + scrollbarLogicalHeight();
    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(beforeEdge);

    if (!firstChild() && !isAnonymousBlock())
        setChildrenInline(true);

    TextAutosizer::LayoutScope textAutosizerLayoutScope(this, &layoutScope);

    bool preferredLogicalWidthsWereDirty = preferredLogicalWidthsDirty();

    // Reset the flag here instead of in layoutInlineChildren() in case that
    // all inline children are removed from this block.
    setContainsInlineWithOutlineAndContinuation(false);
    if (childrenInline())
        layoutInlineChildren(relayoutChildren, afterEdge);
    else
        layoutBlockChildren(relayoutChildren, layoutScope, beforeEdge, afterEdge);

    bool preferredLogicalWidthsBecameDirty = !preferredLogicalWidthsWereDirty && preferredLogicalWidthsDirty();
    if (preferredLogicalWidthsBecameDirty) {
        // The only thing that should dirty preferred widths at this point is the addition of
        // overflow:auto scrollbars in a descendant.  To avoid a potential infinite loop,
        // run layout again with auto scrollbars frozen in their current state.
        PaintLayerScrollableArea::FreezeScrollbarsScope freezeScrollbars;
        return layoutBlockFlow(relayoutChildren, pageLogicalHeight, layoutScope);
    }

    // Expand our intrinsic height to encompass floats.
    if (lowestFloatLogicalBottom() > (logicalHeight() - afterEdge) && createsNewFormattingContext())
        setLogicalHeight(lowestFloatLogicalBottom() + afterEdge);

    if (LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread()) {
        if (flowThread->columnHeightsChanged()) {
            setChildNeedsLayout(MarkOnlyThis);
            return false;
        }
    }

    if (shouldBreakAtLineToAvoidWidow()) {
        setEverHadLayout();
        return false;
    }

    // Calculate our new height.
    LayoutUnit oldHeight = logicalHeight();
    LayoutUnit oldClientAfterEdge = clientLogicalBottom();

    updateLogicalHeight();
    LayoutUnit newHeight = logicalHeight();
    if (!childrenInline()) {
        LayoutBlockFlow* lowestBlock = nullptr;
        bool addedOverhangingFloats = false;
        // One of our children's floats may have become an overhanging float for us.
        for (LayoutObject* child = lastChild(); child; child = child->previousSibling()) {
            // TODO(robhogan): We should exclude blocks that create formatting contexts, not just out of flow or floating blocks.
            if (child->isLayoutBlockFlow() && !child->isFloatingOrOutOfFlowPositioned()) {
                LayoutBlockFlow* block = toLayoutBlockFlow(child);
                if (!block->containsFloats())
                    continue;
                lowestBlock = block;
                if (oldHeight <= newHeight || block->lowestFloatLogicalBottom() + block->logicalTop() <= newHeight)
                    break;
                addOverhangingFloats(block, false);
                addedOverhangingFloats = true;
            }
        }
        // If we have no overhanging floats we still pass a record of the lowest non-overhanging float up the tree so we can enclose it if
        // we are a formatting context and allow siblings to avoid it if they have negative margin and find themselves in its vicinity.
        if (!addedOverhangingFloats)
            addLowestFloatFromChildren(lowestBlock);
    }

    bool heightChanged = (previousHeight != newHeight);
    if (heightChanged)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isDocumentElement(), oldLeft != logicalLeft() ? ForcedLayoutAfterContainingBlockMoved : DefaultLayout);

    // Add overflow from children (unless we're multi-column, since in that case all our child overflow is clipped anyway).
    computeOverflow(oldClientAfterEdge);

    m_descendantsWithFloatsMarkedForLayout = false;
    return true;
}

void LayoutBlockFlow::addLowestFloatFromChildren(LayoutBlockFlow* block)
{
    // TODO(robhogan): Make createsNewFormattingContext an ASSERT.
    if (!block || !block->containsFloats() || block->createsNewFormattingContext())
        return;

    FloatingObject* floatingObject = block->m_floatingObjects->lowestFloatingObject();
    if (!floatingObject || containsFloat(floatingObject->layoutObject()))
        return;

    LayoutSize offset(-block->logicalLeft(), -block->logicalTop());
    if (!isHorizontalWritingMode())
        offset = offset.transposedSize();

    if (!m_floatingObjects)
        createFloatingObjects();
    FloatingObject* newFloatingObject = m_floatingObjects->add(floatingObject->copyToNewContainer(offset, false, true));
    newFloatingObject->setIsLowestNonOverhangingFloatInChild(true);
}

void LayoutBlockFlow::determineLogicalLeftPositionForChild(LayoutBox& child)
{
    LayoutUnit startPosition = borderStart() + paddingStart();
    LayoutUnit initialStartPosition = startPosition;
    if (shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        startPosition -= verticalScrollbarWidth();
    LayoutUnit totalAvailableLogicalWidth = borderAndPaddingLogicalWidth() + availableLogicalWidth();

    LayoutUnit childMarginStart = marginStartForChild(child);
    LayoutUnit newPosition = startPosition + childMarginStart;

    if (child.avoidsFloats() && containsFloats()) {
        LayoutUnit positionToAvoidFloats = startOffsetForLine(logicalTopForChild(child), DoNotIndentText, logicalHeightForChild(child));

        // If the child has an offset from the content edge to avoid floats then use that, otherwise let any negative
        // margin pull it back over the content edge or any positive margin push it out.
        // If the child is being centred then the margin calculated to do that has factored in any offset required to
        // avoid floats, so use it if necessary.
        if (style()->textAlign() == WEBKIT_CENTER || child.style()->marginStartUsing(style()).isAuto())
            newPosition = std::max(newPosition, positionToAvoidFloats + childMarginStart);
        else if (positionToAvoidFloats > initialStartPosition)
            newPosition = std::max(newPosition, positionToAvoidFloats);
    }

    setLogicalLeftForChild(child, style()->isLeftToRightDirection() ? newPosition : totalAvailableLogicalWidth - newPosition - logicalWidthForChild(child));
}

void LayoutBlockFlow::setLogicalLeftForChild(LayoutBox& child, LayoutUnit logicalLeft)
{
    if (isHorizontalWritingMode()) {
        child.setX(logicalLeft);
    } else {
        child.setY(logicalLeft);
    }
}

void LayoutBlockFlow::setLogicalTopForChild(LayoutBox& child, LayoutUnit logicalTop)
{
    if (isHorizontalWritingMode()) {
        child.setY(logicalTop);
    } else {
        child.setX(logicalTop);
    }
}

void LayoutBlockFlow::markDescendantsWithFloatsForLayoutIfNeeded(LayoutBlockFlow& child, LayoutUnit newLogicalTop, LayoutUnit previousFloatLogicalBottom)
{
    // TODO(mstensho): rework the code to return early when there is no need for marking, instead
    // of this |markDescendantsWithFloats| flag.
    bool markDescendantsWithFloats = false;
    if (newLogicalTop != child.logicalTop() && !child.avoidsFloats() && child.containsFloats()) {
        markDescendantsWithFloats = true;
    } else if (UNLIKELY(newLogicalTop.mightBeSaturated())) {
        // The logical top might be saturated for very large elements. Comparing with the old
        // logical top might then yield a false negative, as adding and removing margins, borders
        // etc. from a saturated number might yield incorrect results. If this is the case, always
        // mark for layout.
        markDescendantsWithFloats = true;
    } else if (!child.avoidsFloats() || child.shrinkToAvoidFloats()) {
        // If an element might be affected by the presence of floats, then always mark it for
        // layout.
        if (std::max(previousFloatLogicalBottom, lowestFloatLogicalBottom()) > newLogicalTop)
            markDescendantsWithFloats = true;
    }

    if (markDescendantsWithFloats)
        child.markAllDescendantsWithFloatsForLayout();
}

bool LayoutBlockFlow::positionAndLayoutOnceIfNeeded(LayoutBox& child, LayoutUnit newLogicalTop, BlockChildrenLayoutInfo& layoutInfo)
{
    if (child.isLayoutBlockFlow()) {
        LayoutUnit& previousFloatLogicalBottom = layoutInfo.previousFloatLogicalBottom();
        LayoutBlockFlow& childBlockFlow = toLayoutBlockFlow(child);
        if (childBlockFlow.containsFloats() || containsFloats())
            markDescendantsWithFloatsForLayoutIfNeeded(childBlockFlow, newLogicalTop, previousFloatLogicalBottom);

        // TODO(mstensho): A writing mode root is one thing, but we should be able to skip anything
        // that establishes a new block formatting context here. Their floats don't affect us.
        if (!childBlockFlow.isWritingModeRoot())
            previousFloatLogicalBottom = std::max(previousFloatLogicalBottom, childBlockFlow.logicalTop() + childBlockFlow.lowestFloatLogicalBottom());
    }

    LayoutUnit oldLogicalTop = logicalTopForChild(child);
    setLogicalTopForChild(child, newLogicalTop);

    SubtreeLayoutScope layoutScope(child);
    if (!child.needsLayout()) {
        if (newLogicalTop != oldLogicalTop && child.shrinkToAvoidFloats()) {
            // The child's width is affected by adjacent floats. When the child shifts to clear an
            // item, its width can change (because it has more available width).
            layoutScope.setChildNeedsLayout(&child);
        } else {
            child.markForPaginationRelayoutIfNeeded(layoutScope);
        }
    }

    if (!child.needsLayout())
        return false;
    child.layout();
    return true;
}

bool LayoutBlockFlow::insertForcedBreakBeforeChildIfNeeded(LayoutBox& child, BlockChildrenLayoutInfo& layoutInfo)
{
    if (layoutInfo.isAtFirstInFlowChild()) {
        // There's no class A break point before the first child (only *between* siblings), so
        // steal its break value and join it with what we already have here.
        setBreakBefore(joinFragmentainerBreakValues(breakBefore(), child.breakBefore()));
        return false;
    }

    // Figure out if a forced break should be inserted in front of the child. If we insert a forced
    // break, the margins on this child may not collapse with those preceding the break.
    EBreak classABreakPointValue = child.classABreakPointValue(layoutInfo.previousBreakAfterValue());
    if (isForcedFragmentainerBreakValue(classABreakPointValue)) {
        layoutInfo.marginInfo().clearMargin();
        LayoutUnit oldLogicalTop = logicalHeight();
        LayoutUnit newLogicalTop = applyForcedBreak(oldLogicalTop, classABreakPointValue);
        setLogicalHeight(newLogicalTop);
        LayoutUnit paginationStrut = newLogicalTop - oldLogicalTop;
        child.setPaginationStrut(paginationStrut);
        return true;
    }
    return false;
}

void LayoutBlockFlow::layoutBlockChild(LayoutBox& child, BlockChildrenLayoutInfo& layoutInfo)
{
    MarginInfo& marginInfo = layoutInfo.marginInfo();
    LayoutBlockFlow* childLayoutBlockFlow = child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child) : nullptr;
    LayoutUnit oldPosMarginBefore = maxPositiveMarginBefore();
    LayoutUnit oldNegMarginBefore = maxNegativeMarginBefore();

    // The child is a normal flow object. Compute the margins we will use for collapsing now.
    child.computeAndSetBlockDirectionMargins(this);

    // Try to guess our correct logical top position. In most cases this guess will
    // be correct. Only if we're wrong (when we compute the real logical top position)
    // will we have to potentially relayout.
    LayoutUnit estimateWithoutPagination;
    LayoutUnit logicalTopEstimate = estimateLogicalTopPosition(child, layoutInfo, estimateWithoutPagination);

    // Cache our old rect so that we can dirty the proper paint invalidation rects if the child moves.
    LayoutRect oldRect = child.frameRect();

    // Use the estimated block position and lay out the child if needed. After child layout, when
    // we have enough information to perform proper margin collapsing, float clearing and
    // pagination, we may have to reposition and lay out again if the estimate was wrong.
    bool childNeededLayout = positionAndLayoutOnceIfNeeded(child, logicalTopEstimate, layoutInfo);

    // Cache if we are at the top of the block right now.
    bool atBeforeSideOfBlock = marginInfo.atBeforeSideOfBlock();
    bool childIsSelfCollapsing = child.isSelfCollapsingBlock();
    bool childDiscardMarginBefore = mustDiscardMarginBeforeForChild(child);
    bool childDiscardMarginAfter = mustDiscardMarginAfterForChild(child);
    bool paginated = view()->layoutState()->isPaginated();

    // If there should be a forced break before the child, we need to insert it before attempting
    // to collapse margins or apply clearance.
    bool forcedBreakWasInserted = paginated && insertForcedBreakBeforeChildIfNeeded(child, layoutInfo);

    // Now determine the correct ypos based off examination of collapsing margin
    // values.
    LayoutUnit logicalTopBeforeClear = collapseMargins(child, marginInfo, childIsSelfCollapsing, childDiscardMarginBefore, childDiscardMarginAfter);

    // Now check for clear.
    bool childDiscardMargin = childDiscardMarginBefore || childDiscardMarginAfter;
    LayoutUnit newLogicalTop = clearFloatsIfNeeded(child, marginInfo, oldPosMarginBefore, oldNegMarginBefore, logicalTopBeforeClear, childIsSelfCollapsing, childDiscardMargin);

    // If there's a forced break in front of this child, its final position has already been
    // determined. Otherwise, see if there are other reasons for breaking before it
    // (break-inside:avoid, or not enough space for the first piece of child content to fit in the
    // current fragmentainer), and adjust the position accordingly.
    if (paginated && !forcedBreakWasInserted) {
        if (estimateWithoutPagination != newLogicalTop) {
            // We got a new position due to clearance or margin collapsing. Before we attempt to
            // paginate (which may result in the position changing again), let's try again at the
            // new position (since a new position may result in a new logical height).
            positionAndLayoutOnceIfNeeded(child, newLogicalTop, layoutInfo);
        }

        newLogicalTop = adjustBlockChildForPagination(newLogicalTop, child, layoutInfo, atBeforeSideOfBlock && logicalTopBeforeClear == newLogicalTop);
    }

    // Clearance, margin collapsing or pagination may have given us a new logical top, in which
    // case we may have to reposition and possibly relayout as well. If we determined during child
    // layout that we need to insert a break to honor widows, we also need to relayout.
    if (newLogicalTop != logicalTopEstimate
        || child.needsLayout()
        || (paginated && childLayoutBlockFlow && childLayoutBlockFlow->shouldBreakAtLineToAvoidWidow())) {
        positionAndLayoutOnceIfNeeded(child, newLogicalTop, layoutInfo);
    }

    // If we previously encountered a self-collapsing sibling of this child that had clearance then
    // we set this bit to ensure we would not collapse the child's margins, and those of any subsequent
    // self-collapsing siblings, with our parent. If this child is not self-collapsing then it can
    // collapse its margins with the parent so reset the bit.
    if (!marginInfo.canCollapseMarginAfterWithLastChild() && !childIsSelfCollapsing)
        marginInfo.setCanCollapseMarginAfterWithLastChild(true);

    // We are no longer at the top of the block if we encounter a non-empty child.
    // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
    if (marginInfo.atBeforeSideOfBlock() && !childIsSelfCollapsing)
        marginInfo.setAtBeforeSideOfBlock(false);

    // Now place the child in the correct left position
    determineLogicalLeftPositionForChild(child);

    LayoutSize childOffset = child.location() - oldRect.location();

    // Update our height now that the child has been placed in the correct position.
    setLogicalHeight(logicalHeight() + logicalHeightForChild(child));
    if (mustSeparateMarginAfterForChild(child)) {
        setLogicalHeight(logicalHeight() + marginAfterForChild(child));
        marginInfo.clearMargin();
    }
    // If the child has overhanging floats that intrude into following siblings (or possibly out
    // of this block), then the parent gets notified of the floats now.
    if (childLayoutBlockFlow)
        addOverhangingFloats(childLayoutBlockFlow, !childNeededLayout);

    // If the child moved, we have to invalidate its paint as well as any floating/positioned
    // descendants. An exception is if we need a layout. In this case, we know we're going to
    // invalidate our paint (and the child) anyway.
    if (!selfNeedsLayout() && (childOffset.width() || childOffset.height()))
        child.invalidatePaintForOverhangingFloats(true);

    if (paginated) {
        // Keep track of the break-after value of the child, so that it can be joined with the
        // break-before value of the next in-flow object at the next class A break point.
        layoutInfo.setPreviousBreakAfterValue(child.breakAfter());

        paginatedContentWasLaidOut(child.logicalBottom());
    }

    if (child.isLayoutMultiColumnSpannerPlaceholder()) {
        // The actual column-span:all element is positioned by this placeholder child.
        positionSpannerDescendant(toLayoutMultiColumnSpannerPlaceholder(child));
    }
}

LayoutUnit LayoutBlockFlow::adjustBlockChildForPagination(LayoutUnit logicalTop, LayoutBox& child, BlockChildrenLayoutInfo& layoutInfo, bool atBeforeSideOfBlock)
{
    // Forced breaks trumps unforced ones, and if we have a forced break, we shouldn't even be here.
    ASSERT(layoutInfo.isAtFirstInFlowChild() || !isForcedFragmentainerBreakValue(child.classABreakPointValue(layoutInfo.previousBreakAfterValue())));

    LayoutBlockFlow* childBlockFlow = child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child) : 0;

    // See if we need a soft (unforced) break in front of this child, and set the pagination strut
    // in that case. An unforced break may come from two sources:
    // 1. The first piece of content inside the child doesn't fit in the current page or column
    // 2. The child itself has breaking restrictions (break-inside:avoid, replaced content, etc.)
    // and doesn't fully fit in the current page or column.
    //
    // No matter which source, if we need to insert a strut, it should always take us to the exact
    // top of a page or column further ahead, or be zero.

    // We're now going to calculate the child's final pagination strut. We may end up propagating
    // it to its containing block (|this|), so reset it first.
    child.resetPaginationStrut();

    // The first piece of content inside the child may have set a strut during layout. Currently,
    // only block flows support strut propagation, but this may (and should) change in the future.
    // See crbug.com/539873
    LayoutUnit strutFromContent = childBlockFlow ? childBlockFlow->paginationStrutPropagatedFromChild() : LayoutUnit();
    LayoutUnit logicalTopWithContentStrut = logicalTop + strutFromContent;

    // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
    LayoutUnit logicalTopAfterUnsplittable = adjustForUnsplittableChild(child, logicalTop);

    // Pick the largest offset. Tall unsplittable content may take us to a page or column further
    // ahead than the next one.
    LayoutUnit logicalTopAfterPagination = std::max(logicalTopWithContentStrut, logicalTopAfterUnsplittable);
    LayoutUnit newLogicalTop = logicalTop;
    if (LayoutUnit paginationStrut = logicalTopAfterPagination - logicalTop) {
        ASSERT(paginationStrut > 0);
        // If we're not at the first in-flow child, there's a class A break point before the child. If we *are* at the
        // first in-flow child, but the child isn't flush with the content edge of its container, due to e.g. clearance,
        // there's a class C break point before the child. Otherwise we should propagate the strut to our parent block,
        // and attempt to break there instead. See https://drafts.csswg.org/css-break/#possible-breaks
        if (layoutInfo.isAtFirstInFlowChild() && atBeforeSideOfBlock && allowsPaginationStrut()) {
            // FIXME: Should really check if we're exceeding the page height before propagating the strut, but we don't
            // have all the information to do so (the strut only has the remaining amount to push). Gecko gets this wrong too
            // and pushes to the next page anyway, so not too concerned about it.
            paginationStrut += logicalTop + marginBeforeIfFloating();
            setPaginationStrutPropagatedFromChild(paginationStrut);
            if (childBlockFlow)
                childBlockFlow->setPaginationStrutPropagatedFromChild(LayoutUnit());
        } else {
            child.setPaginationStrut(paginationStrut);
            newLogicalTop += paginationStrut;
        }
    }

    // Similar to how we apply clearance. Go ahead and boost height() to be the place where we're going to position the child.
    setLogicalHeight(logicalHeight() + (newLogicalTop - logicalTop));

    // Return the final adjusted logical top.
    return newLogicalTop;
}

static bool shouldSetStrutOnBlock(const LayoutBlockFlow& block, const RootInlineBox& lineBox, LayoutUnit lineLogicalOffset, int lineIndex, LayoutUnit pageLogicalHeight)
{
    if (lineBox == block.firstRootBox()) {
        // This is the first line in the block. We can take the whole block with us to the next page
        // or column, rather than keeping a content-less portion of it in the previous one. Only do
        // this if the line is flush with the content edge of the block, though. If it isn't, it
        // means that the line was pushed downwards by preceding floats that didn't fit beside the
        // line, and we don't want to move all that, since it has already been established that it
        // fits nicely where it is. In this case we have a class "C" break point [1] in front of
        // this line.
        //
        // [1] https://drafts.csswg.org/css-break/#possible-breaks
        if (lineLogicalOffset > block.borderAndPaddingBefore())
            return false;

        LayoutUnit lineHeight = lineBox.lineBottomWithLeading() - lineBox.lineTopWithLeading();
        LayoutUnit totalLogicalHeight = lineHeight + lineLogicalOffset.clampNegativeToZero();
        // It's rather pointless to break before the block if the current line isn't going to
        // fit in the same column or page, so check that as well.
        if (totalLogicalHeight > pageLogicalHeight)
            return false;
    } else {
        if (lineIndex > block.style()->orphans())
            return false;

        // Not enough orphans here. Push the entire block to the next column / page as an attempt to
        // better satisfy the orphans requirement.
        //
        // Note that we should ideally check if the first line in the block is flush with the
        // content edge of the block here, because if it isn't, we should break at the class "C"
        // break point in front of the first line, rather than before the entire block.
    }
    return block.allowsPaginationStrut();
}

void LayoutBlockFlow::adjustLinePositionForPagination(RootInlineBox& lineBox, LayoutUnit& delta)
{
    // TODO(mstensho): Pay attention to line overflow. It should be painted in the same column as
    // the rest of the line, possibly overflowing the column. We currently only allow overflow above
    // the first column. We clip at all other column boundaries, and that's how it has to be for
    // now. The paint we have to do when a column has overflow has to be special. We need to exclude
    // content that paints in a previous column (and content that paints in the following column).
    //
    // FIXME: Another problem with simply moving lines is that the available line width may change (because of floats).
    // Technically if the location we move the line to has a different line width than our old position, then we need to dirty the
    // line and all following lines.
    LayoutUnit logicalOffset = lineBox.lineTopWithLeading();
    LayoutUnit lineHeight = lineBox.lineBottomWithLeading() - logicalOffset;
    logicalOffset += delta;
    lineBox.setPaginationStrut(LayoutUnit());
    lineBox.setIsFirstAfterPageBreak(false);
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, AssociateWithLatterPage);
    int lineIndex = lineCount(&lineBox);
    if (remainingLogicalHeight < lineHeight || (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex)) {
        LayoutUnit paginationStrut = calculatePaginationStrutToFitContent(logicalOffset, remainingLogicalHeight, lineHeight);
        LayoutUnit newLogicalOffset = logicalOffset + paginationStrut;
        // Moving to a different page or column may mean that its height is different.
        pageLogicalHeight = pageLogicalHeightForOffset(newLogicalOffset);
        if (lineHeight > pageLogicalHeight) {
            // Too tall to fit in one page / column. Give up. Don't push to the next page / column.
            // TODO(mstensho): Get rid of this. This is just utter weirdness, but the other browsers
            // also do something slightly similar, although in much more specific cases than we do here,
            // and printing Google Docs depends on it.
            paginatedContentWasLaidOut(logicalOffset + lineHeight);
            return;
        }

        // We need to insert a break now, either because there's no room for the line in the
        // current column / page, or because we have determined that we need a break to satisfy
        // widow requirements.
        if (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex) {
            clearShouldBreakAtLineToAvoidWidow();
            setDidBreakAtLineToAvoidWidow();
        }
        if (shouldSetStrutOnBlock(*this, lineBox, logicalOffset, lineIndex, pageLogicalHeight)) {
            // Note that when setting the strut on a block, it may be propagated to parent blocks
            // later on, if a block's logical top is flush with that of its parent. We don't want
            // content-less portions (struts) at the beginning of a block before a break, if it can
            // be avoided. After all, that's the reason for setting struts on blocks and not lines
            // in the first place.
            LayoutUnit strut = paginationStrut + logicalOffset + marginBeforeIfFloating();
            setPaginationStrutPropagatedFromChild(strut);
        } else {
            delta += paginationStrut;
            lineBox.setPaginationStrut(paginationStrut);
            lineBox.setIsFirstAfterPageBreak(true);
        }
        paginatedContentWasLaidOut(newLogicalOffset + lineHeight);
        return;
    }

    if (remainingLogicalHeight == pageLogicalHeight) {
        // We're at the very top of a page or column.
        if (lineBox != firstRootBox())
            lineBox.setIsFirstAfterPageBreak(true);
        // If this is the first line in the block, and the block has a top border, padding, or (in
        // case it's a float) margin, we may want to set a strut on the block, so that everything
        // ends up in the next column or page. Setting a strut on the block is also important when
        // it comes to satisfying orphan requirements.
        if (shouldSetStrutOnBlock(*this, lineBox, logicalOffset, lineIndex, pageLogicalHeight)) {
            LayoutUnit strut = logicalOffset + marginBeforeIfFloating();
            setPaginationStrutPropagatedFromChild(strut);
        }
    } else if (lineBox == firstRootBox() && allowsPaginationStrut()) {
        // This is the first line in the block. The block may still start in the previous column or
        // page, and if that's the case, attempt to pull it over to where this line is, so that we
        // don't split the top border, padding, or (in case it's a float) margin.
        LayoutUnit totalLogicalOffset = logicalOffset + marginBeforeIfFloating();
        LayoutUnit strut = remainingLogicalHeight + totalLogicalOffset - pageLogicalHeight;
        if (strut > 0) {
            // The block starts in a previous column or page. Set a strut on the block if there's
            // room for the top border, padding and (if it's a float) margin and the line in one
            // column or page.
            if (totalLogicalOffset + lineHeight <= pageLogicalHeight)
                setPaginationStrutPropagatedFromChild(strut);
        }
    }

    paginatedContentWasLaidOut(logicalOffset + lineHeight);
}

LayoutUnit LayoutBlockFlow::adjustForUnsplittableChild(LayoutBox& child, LayoutUnit logicalOffset) const
{
    if (child.getPaginationBreakability() == AllowAnyBreaks)
        return logicalOffset;
    LayoutUnit childLogicalHeight = logicalHeightForChild(child);
    // Floats' margins do not collapse with page or column boundaries.
    if (child.isFloating())
        childLogicalHeight += marginBeforeForChild(child) + marginAfterForChild(child);
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, AssociateWithLatterPage);
    if (remainingLogicalHeight >= childLogicalHeight)
        return logicalOffset; // It fits fine where it is. No need to break.
    LayoutUnit paginationStrut = calculatePaginationStrutToFitContent(logicalOffset, remainingLogicalHeight, childLogicalHeight);
    if (paginationStrut == remainingLogicalHeight && remainingLogicalHeight == pageLogicalHeight) {
        // Don't break if we were at the top of a page, and we failed to fit the content
        // completely. No point in leaving a page completely blank.
        return logicalOffset;
    }
    return logicalOffset + paginationStrut;
}

void LayoutBlockFlow::rebuildFloatsFromIntruding()
{
    if (m_floatingObjects)
        m_floatingObjects->setHorizontalWritingMode(isHorizontalWritingMode());

    HashSet<LayoutBox*> oldIntrudingFloatSet;
    if (!childrenInline() && m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator end = floatingObjectSet.end();
        for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
            const FloatingObject& floatingObject = *it->get();
            if (!floatingObject.isDescendant())
                oldIntrudingFloatSet.add(floatingObject.layoutObject());
        }
    }

    // Inline blocks are covered by the isAtomicInlineLevel() check in the avoidFloats method.
    if (avoidsFloats() || isDocumentElement() || isLayoutView() || isFloatingOrOutOfFlowPositioned() || isTableCell()) {
        if (m_floatingObjects) {
            m_floatingObjects->clear();
        }
        if (!oldIntrudingFloatSet.isEmpty())
            markAllDescendantsWithFloatsForLayout();
        return;
    }

    LayoutBoxToFloatInfoMap floatMap;

    if (m_floatingObjects) {
        if (childrenInline())
            m_floatingObjects->moveAllToFloatInfoMap(floatMap);
        else
            m_floatingObjects->clear();
    }

    // We should not process floats if the parent node is not a LayoutBlockFlow. Otherwise, we will add
    // floats in an invalid context. This will cause a crash arising from a bad cast on the parent.
    // See <rdar://problem/8049753>, where float property is applied on a text node in a SVG.
    if (!parent() || !parent()->isLayoutBlockFlow())
        return;

    // Attempt to locate a previous sibling with overhanging floats. We skip any elements that
    // may have shifted to avoid floats, and any objects whose floats cannot interact with objects
    // outside it (i.e. objects that create a new block formatting context).
    LayoutBlockFlow* parentBlockFlow = toLayoutBlockFlow(parent());
    bool siblingFloatMayIntrude = false;
    LayoutObject* prev = previousSibling();
    while (prev && (!prev->isBox() || !prev->isLayoutBlock() || toLayoutBlock(prev)->avoidsFloats() || toLayoutBlock(prev)->createsNewFormattingContext())) {
        if (prev->isFloating())
            siblingFloatMayIntrude = true;
        prev = prev->previousSibling();
    }

    // First add in floats from the parent. Self-collapsing blocks let their parent track any floats that intrude into
    // them (as opposed to floats they contain themselves) so check for those here too. If margin collapsing has moved
    // us up past the top a previous sibling then we need to check for floats from the parent too.
    bool parentFloatsMayIntrude = !siblingFloatMayIntrude && (!prev || toLayoutBlockFlow(prev)->isSelfCollapsingBlock() || toLayoutBlock(prev)->logicalTop() > logicalTop())
        && parentBlockFlow->lowestFloatLogicalBottom() > logicalTop();
    if (siblingFloatMayIntrude || parentFloatsMayIntrude)
        addIntrudingFloats(parentBlockFlow, parentBlockFlow->logicalLeftOffsetForContent(), logicalTop());

    // Add overhanging floats from the previous LayoutBlockFlow, but only if it has a float that intrudes into our space.
    if (prev) {
        LayoutBlockFlow* previousBlockFlow = toLayoutBlockFlow(prev);
        if (logicalTop() < previousBlockFlow->logicalTop() + previousBlockFlow->lowestFloatLogicalBottom())
            addIntrudingFloats(previousBlockFlow, LayoutUnit(), logicalTop() - previousBlockFlow->logicalTop());
    }

    if (childrenInline()) {
        LayoutUnit changeLogicalTop = LayoutUnit::max();
        LayoutUnit changeLogicalBottom = LayoutUnit::min();
        if (m_floatingObjects) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            FloatingObjectSetIterator end = floatingObjectSet.end();
            for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
                const FloatingObject& floatingObject = *it->get();
                FloatingObject* oldFloatingObject = floatMap.get(floatingObject.layoutObject());
                LayoutUnit logicalBottom = logicalBottomForFloat(floatingObject);
                if (oldFloatingObject) {
                    LayoutUnit oldLogicalBottom = logicalBottomForFloat(*oldFloatingObject);
                    if (logicalWidthForFloat(floatingObject) != logicalWidthForFloat(*oldFloatingObject) || logicalLeftForFloat(floatingObject) != logicalLeftForFloat(*oldFloatingObject)) {
                        changeLogicalTop = LayoutUnit();
                        changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                    } else {
                        if (logicalBottom != oldLogicalBottom) {
                            changeLogicalTop = std::min(changeLogicalTop, std::min(logicalBottom, oldLogicalBottom));
                            changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                        }
                        LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                        LayoutUnit oldLogicalTop = logicalTopForFloat(*oldFloatingObject);
                        if (logicalTop != oldLogicalTop) {
                            changeLogicalTop = std::min(changeLogicalTop, std::min(logicalTop, oldLogicalTop));
                            changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalTop, oldLogicalTop));
                        }
                    }

                    if (oldFloatingObject->originatingLine() && !selfNeedsLayout()) {
                        ASSERT(oldFloatingObject->originatingLine()->getLineLayoutItem().isEqual(this));
                        oldFloatingObject->originatingLine()->markDirty();
                    }

                    floatMap.remove(floatingObject.layoutObject());
                } else {
                    changeLogicalTop = LayoutUnit();
                    changeLogicalBottom = std::max(changeLogicalBottom, logicalBottom);
                }
            }
        }

        LayoutBoxToFloatInfoMap::iterator end = floatMap.end();
        for (LayoutBoxToFloatInfoMap::iterator it = floatMap.begin(); it != end; ++it) {
            std::unique_ptr<FloatingObject>& floatingObject = it->value;
            if (!floatingObject->isDescendant()) {
                changeLogicalTop = LayoutUnit();
                changeLogicalBottom = std::max(changeLogicalBottom, logicalBottomForFloat(*floatingObject));
            }
        }

        markLinesDirtyInBlockRange(changeLogicalTop, changeLogicalBottom);
    } else if (!oldIntrudingFloatSet.isEmpty()) {
        // If there are previously intruding floats that no longer intrude, then children with floats
        // should also get layout because they might need their floating object lists cleared.
        if (m_floatingObjects->set().size() < oldIntrudingFloatSet.size()) {
            markAllDescendantsWithFloatsForLayout();
        } else {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            FloatingObjectSetIterator end = floatingObjectSet.end();
            for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end && !oldIntrudingFloatSet.isEmpty(); ++it)
                oldIntrudingFloatSet.remove((*it)->layoutObject());
            if (!oldIntrudingFloatSet.isEmpty())
                markAllDescendantsWithFloatsForLayout();
        }
    }
}

void LayoutBlockFlow::layoutBlockChildren(bool relayoutChildren, SubtreeLayoutScope& layoutScope, LayoutUnit beforeEdge, LayoutUnit afterEdge)
{
    dirtyForLayoutFromPercentageHeightDescendants(layoutScope);

    BlockChildrenLayoutInfo layoutInfo(this, beforeEdge, afterEdge);
    MarginInfo& marginInfo = layoutInfo.marginInfo();

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout. The same is true for ruby text.
    // It doesn't get included in the normal layout process but is instead skipped.
    LayoutObject* childToExclude = layoutSpecialExcludedChild(relayoutChildren, layoutScope);

    LayoutBox* next = firstChildBox();
    LayoutBox* lastNormalFlowChild = nullptr;

    while (next) {
        LayoutBox* child = next;
        next = child->nextSiblingBox();

        child->setMayNeedPaintInvalidation();

        if (childToExclude == child)
            continue; // Skip this child, since it will be positioned by the specialized subclass (fieldsets and ruby runs).

        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *child);

        if (child->isOutOfFlowPositioned()) {
            child->containingBlock()->insertPositionedObject(child);
            adjustPositionedBlock(*child, layoutInfo);
            continue;
        }
        if (child->isFloating()) {
            insertFloatingObject(*child);
            adjustFloatingBlock(marginInfo);
            continue;
        }
        if (child->isColumnSpanAll()) {
            // This is not the containing block of the spanner. The spanner's placeholder will lay
            // it out in due course. For now we just need to consult our flow thread, so that the
            // columns (if any) preceding and following the spanner are laid out correctly. But
            // first we apply the pending margin, so that it's taken into consideration and doesn't
            // end up on the other side of the spanner.
            setLogicalHeight(logicalHeight() + marginInfo.margin());
            marginInfo.clearMargin();

            child->spannerPlaceholder()->flowThread()->skipColumnSpanner(child, offsetFromLogicalTopOfFirstPage() + logicalHeight());
            continue;
        }

        // Lay out the child.
        layoutBlockChild(*child, layoutInfo);
        layoutInfo.clearIsAtFirstInFlowChild();
        lastNormalFlowChild = child;
    }

    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    handleAfterSideOfBlock(lastNormalFlowChild, beforeEdge, afterEdge, marginInfo);
}

// Our MarginInfo state used when laying out block children.
MarginInfo::MarginInfo(LayoutBlockFlow* blockFlow, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding)
    : m_canCollapseMarginAfterWithLastChild(true)
    , m_atBeforeSideOfBlock(true)
    , m_atAfterSideOfBlock(false)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_determinedMarginBeforeQuirk(false)
    , m_discardMargin(false)
    , m_lastChildIsSelfCollapsingBlockWithClearance(false)
{
    const ComputedStyle& blockStyle = blockFlow->styleRef();
    ASSERT(blockFlow->isLayoutView() || blockFlow->parent());
    m_canCollapseWithChildren = !blockFlow->createsNewFormattingContext() && !blockFlow->isLayoutFlowThread() && !blockFlow->isLayoutView();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !beforeBorderPadding && blockStyle.marginBeforeCollapse() != MarginCollapseSeparate;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    m_canCollapseMarginAfterWithChildren = m_canCollapseWithChildren && !afterBorderPadding
        && (blockStyle.logicalHeight().isAuto() && !blockStyle.logicalHeight().value()) && blockStyle.marginAfterCollapse() != MarginCollapseSeparate;

    m_quirkContainer = blockFlow->isTableCell() || blockFlow->isBody();

    m_discardMargin = m_canCollapseMarginBeforeWithChildren && blockFlow->mustDiscardMarginBefore();

    m_positiveMargin = (m_canCollapseMarginBeforeWithChildren && !blockFlow->mustDiscardMarginBefore()) ? blockFlow->maxPositiveMarginBefore() : LayoutUnit();
    m_negativeMargin = (m_canCollapseMarginBeforeWithChildren && !blockFlow->mustDiscardMarginBefore()) ? blockFlow->maxNegativeMarginBefore() : LayoutUnit();
}

LayoutBlockFlow::MarginValues LayoutBlockFlow::marginValuesForChild(LayoutBox& child) const
{
    LayoutUnit childBeforePositive;
    LayoutUnit childBeforeNegative;
    LayoutUnit childAfterPositive;
    LayoutUnit childAfterNegative;

    LayoutUnit beforeMargin;
    LayoutUnit afterMargin;

    LayoutBlockFlow* childLayoutBlockFlow = child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child) : 0;

    // If the child has the same directionality as we do, then we can just return its
    // margins in the same direction.
    if (!child.isWritingModeRoot()) {
        if (childLayoutBlockFlow) {
            childBeforePositive = childLayoutBlockFlow->maxPositiveMarginBefore();
            childBeforeNegative = childLayoutBlockFlow->maxNegativeMarginBefore();
            childAfterPositive = childLayoutBlockFlow->maxPositiveMarginAfter();
            childAfterNegative = childLayoutBlockFlow->maxNegativeMarginAfter();
        } else {
            beforeMargin = child.marginBefore();
            afterMargin = child.marginAfter();
        }
    } else if (child.isHorizontalWritingMode() == isHorizontalWritingMode()) {
        // The child has a different directionality. If the child is parallel, then it's just
        // flipped relative to us. We can use the margins for the opposite edges.
        if (childLayoutBlockFlow) {
            childBeforePositive = childLayoutBlockFlow->maxPositiveMarginAfter();
            childBeforeNegative = childLayoutBlockFlow->maxNegativeMarginAfter();
            childAfterPositive = childLayoutBlockFlow->maxPositiveMarginBefore();
            childAfterNegative = childLayoutBlockFlow->maxNegativeMarginBefore();
        } else {
            beforeMargin = child.marginAfter();
            afterMargin = child.marginBefore();
        }
    } else {
        // The child is perpendicular to us, which means its margins don't collapse but are on the
        // "logical left/right" sides of the child box. We can just return the raw margin in this case.
        beforeMargin = marginBeforeForChild(child);
        afterMargin = marginAfterForChild(child);
    }

    // Resolve uncollapsing margins into their positive/negative buckets.
    if (beforeMargin) {
        if (beforeMargin > 0)
            childBeforePositive = beforeMargin;
        else
            childBeforeNegative = -beforeMargin;
    }
    if (afterMargin) {
        if (afterMargin > 0)
            childAfterPositive = afterMargin;
        else
            childAfterNegative = -afterMargin;
    }

    return LayoutBlockFlow::MarginValues(childBeforePositive, childBeforeNegative, childAfterPositive, childAfterNegative);
}

LayoutUnit LayoutBlockFlow::collapseMargins(LayoutBox& child, MarginInfo& marginInfo, bool childIsSelfCollapsing, bool childDiscardMarginBefore, bool childDiscardMarginAfter)
{
    // The child discards the before margin when the the after margin has discard in the case of a self collapsing block.
    childDiscardMarginBefore = childDiscardMarginBefore || (childDiscardMarginAfter && childIsSelfCollapsing);

    // Get the four margin values for the child and cache them.
    const LayoutBlockFlow::MarginValues childMargins = marginValuesForChild(child);

    // Get our max pos and neg top margins.
    LayoutUnit posTop = childMargins.positiveMarginBefore();
    LayoutUnit negTop = childMargins.negativeMarginBefore();

    // For self-collapsing blocks, collapse our bottom margins into our
    // top to get new posTop and negTop values.
    if (childIsSelfCollapsing) {
        posTop = std::max(posTop, childMargins.positiveMarginAfter());
        negTop = std::max(negTop, childMargins.negativeMarginAfter());
    }

    // See if the top margin is quirky. We only care if this child has
    // margins that will collapse with us.
    bool topQuirk = hasMarginBeforeQuirk(&child);

    if (marginInfo.canCollapseWithMarginBefore()) {
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child is collapsing with the top of the
            // block. If it has larger margin values, then we need to update
            // our own maximal values.
            if (!document().inQuirksMode() || !marginInfo.quirkContainer() || !topQuirk)
                setMaxMarginBeforeValues(std::max(posTop, maxPositiveMarginBefore()), std::max(negTop, maxNegativeMarginBefore()));

            // The minute any of the margins involved isn't a quirk, don't
            // collapse it away, even if the margin is smaller (www.webreference.com
            // has an example of this, a <dt> with 0.8em author-specified inside
            // a <dl> inside a <td>.
            if (!marginInfo.determinedMarginBeforeQuirk() && !topQuirk && (posTop - negTop)) {
                setHasMarginBeforeQuirk(false);
                marginInfo.setDeterminedMarginBeforeQuirk(true);
            }

            if (!marginInfo.determinedMarginBeforeQuirk() && topQuirk && !marginBefore()) {
                // We have no top margin and our top child has a quirky margin.
                // We will pick up this quirky margin and pass it through.
                // This deals with the <td><div><p> case.
                // Don't do this for a block that split two inlines though. You do
                // still apply margins in this case.
                setHasMarginBeforeQuirk(true);
            }
        } else {
            // The before margin of the container will also discard all the margins it is collapsing with.
            setMustDiscardMarginBefore();
        }
    }

    // Once we find a child with discardMarginBefore all the margins collapsing with us must also discard.
    if (childDiscardMarginBefore) {
        marginInfo.setDiscardMargin(true);
        marginInfo.clearMargin();
    }

    if (marginInfo.quirkContainer() && marginInfo.atBeforeSideOfBlock() && (posTop - negTop))
        marginInfo.setHasMarginBeforeQuirk(topQuirk);

    LayoutUnit beforeCollapseLogicalTop = logicalHeight();
    LayoutUnit logicalTop = beforeCollapseLogicalTop;

    LayoutObject* prev = child.previousSibling();
    LayoutBlockFlow* previousBlockFlow =  prev && prev->isLayoutBlockFlow() && !prev->isFloatingOrOutOfFlowPositioned() ? toLayoutBlockFlow(prev) : 0;
    // If the child's previous sibling is a self-collapsing block that cleared a float then its top border edge has been set at the bottom border edge
    // of the float. Since we want to collapse the child's top margin with the self-collapsing block's top and bottom margins we need to adjust our parent's height to match the
    // margin top of the self-collapsing block. If the resulting collapsed margin leaves the child still intruding into the float then we will want to clear it.
    if (!marginInfo.canCollapseWithMarginBefore() && previousBlockFlow && marginInfo.lastChildIsSelfCollapsingBlockWithClearance())
        setLogicalHeight(logicalHeight() - marginValuesForChild(*previousBlockFlow).positiveMarginBefore());

    if (childIsSelfCollapsing) {
        // For a self collapsing block both the before and after margins get discarded. The block doesn't contribute anything to the height of the block.
        // Also, the child's top position equals the logical height of the container.
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child has no height. We need to compute our
            // position before we collapse the child's margins together,
            // so that we can get an accurate position for the zero-height block.
            LayoutUnit collapsedBeforePos = std::max(marginInfo.positiveMargin(), childMargins.positiveMarginBefore());
            LayoutUnit collapsedBeforeNeg = std::max(marginInfo.negativeMargin(), childMargins.negativeMarginBefore());
            marginInfo.setMargin(collapsedBeforePos, collapsedBeforeNeg);

            // Now collapse the child's margins together, which means examining our
            // bottom margin values as well.
            marginInfo.setPositiveMarginIfLarger(childMargins.positiveMarginAfter());
            marginInfo.setNegativeMarginIfLarger(childMargins.negativeMarginAfter());

            if (!marginInfo.canCollapseWithMarginBefore()) {
                // We need to make sure that the position of the self-collapsing block
                // is correct, since it could have overflowing content
                // that needs to be positioned correctly (e.g., a block that
                // had a specified height of 0 but that actually had subcontent).
                logicalTop = logicalHeight() + collapsedBeforePos - collapsedBeforeNeg;
            }
        }
    } else {
        if (mustSeparateMarginBeforeForChild(child)) {
            ASSERT(!marginInfo.discardMargin() || (marginInfo.discardMargin() && !marginInfo.margin()));
            // If we are at the before side of the block and we collapse, ignore the computed margin
            // and just add the child margin to the container height. This will correctly position
            // the child inside the container.
            LayoutUnit separateMargin = !marginInfo.canCollapseWithMarginBefore() ? marginInfo.margin() : LayoutUnit();
            setLogicalHeight(logicalHeight() + separateMargin + marginBeforeForChild(child));
            logicalTop = logicalHeight();
        } else if (!marginInfo.discardMargin() && (!marginInfo.atBeforeSideOfBlock()
            || (!marginInfo.canCollapseMarginBeforeWithChildren()
            && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginBeforeQuirk())))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            setLogicalHeight(logicalHeight() + std::max(marginInfo.positiveMargin(), posTop) - std::max(marginInfo.negativeMargin(), negTop));
            logicalTop = logicalHeight();
        }

        marginInfo.setDiscardMargin(childDiscardMarginAfter);

        if (!marginInfo.discardMargin()) {
            marginInfo.setPositiveMargin(childMargins.positiveMarginAfter());
            marginInfo.setNegativeMargin(childMargins.negativeMarginAfter());
        } else {
            marginInfo.clearMargin();
        }

        if (marginInfo.margin())
            marginInfo.setHasMarginAfterQuirk(hasMarginAfterQuirk(&child));
    }

    // If margins would pull us past the top of the next page, then we need to pull back and pretend like the margins
    // collapsed into the page edge.
    LayoutState* layoutState = view()->layoutState();
    if (layoutState->isPaginated() && isPageLogicalHeightKnown(beforeCollapseLogicalTop) && logicalTop > beforeCollapseLogicalTop) {
        LayoutUnit oldLogicalTop = logicalTop;
        logicalTop = std::min(logicalTop, nextPageLogicalTop(beforeCollapseLogicalTop, AssociateWithLatterPage));
        setLogicalHeight(logicalHeight() + (logicalTop - oldLogicalTop));
    }

    if (previousBlockFlow) {
        // If |child| is a self-collapsing block it may have collapsed into a previous sibling and although it hasn't reduced the height of the parent yet
        // any floats from the parent will now overhang.
        LayoutUnit oldLogicalHeight = logicalHeight();
        setLogicalHeight(logicalTop);
        if (!previousBlockFlow->avoidsFloats() && (previousBlockFlow->logicalTop() + previousBlockFlow->lowestFloatLogicalBottom()) > logicalTop)
            addOverhangingFloats(previousBlockFlow, false);
        setLogicalHeight(oldLogicalHeight);

        // If |child|'s previous sibling is or contains a self-collapsing block that cleared a float and margin collapsing resulted in |child| moving up
        // into the margin area of the self-collapsing block then the float it clears is now intruding into |child|. Layout again so that we can look for
        // floats in the parent that overhang |child|'s new logical top.
        bool logicalTopIntrudesIntoFloat = logicalTop < beforeCollapseLogicalTop;
        if (logicalTopIntrudesIntoFloat && containsFloats() && !child.avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
            child.setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::AncestorMarginCollapsing);
    }

    return logicalTop;
}

void LayoutBlockFlow::adjustPositionedBlock(LayoutBox& child, const BlockChildrenLayoutInfo& layoutInfo)
{
    LayoutUnit logicalTop = logicalHeight();

    // Forced breaks are only specified on in-flow objects, but auto-positioned out-of-flow objects
    // may be affected by a break-after value of the previous in-flow object.
    if (view()->layoutState()->isPaginated())
        logicalTop = applyForcedBreak(logicalTop, layoutInfo.previousBreakAfterValue());

    updateStaticInlinePositionForChild(child, logicalTop);

    const MarginInfo& marginInfo = layoutInfo.marginInfo();
    if (!marginInfo.canCollapseWithMarginBefore()) {
        // Positioned blocks don't collapse margins, so add the margin provided by
        // the container now. The child's own margin is added later when calculating its logical top.
        LayoutUnit collapsedBeforePos = marginInfo.positiveMargin();
        LayoutUnit collapsedBeforeNeg = marginInfo.negativeMargin();
        logicalTop += collapsedBeforePos - collapsedBeforeNeg;
    }

    PaintLayer* childLayer = child.layer();
    if (childLayer->staticBlockPosition() != logicalTop)
        childLayer->setStaticBlockPosition(logicalTop);
}

LayoutUnit LayoutBlockFlow::clearFloatsIfNeeded(LayoutBox& child, MarginInfo& marginInfo, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos, bool childIsSelfCollapsing, bool childDiscardMargin)
{
    LayoutUnit heightIncrease = getClearDelta(&child, yPos);
    marginInfo.setLastChildIsSelfCollapsingBlockWithClearance(false);

    if (!heightIncrease)
        return yPos;

    if (childIsSelfCollapsing) {
        marginInfo.setLastChildIsSelfCollapsingBlockWithClearance(true);
        marginInfo.setDiscardMargin(childDiscardMargin);

        // For self-collapsing blocks that clear, they can still collapse their
        // margins with following siblings. Reset the current margins to represent
        // the self-collapsing block's margins only.
        // If DISCARD is specified for -webkit-margin-collapse, reset the margin values.
        LayoutBlockFlow::MarginValues childMargins = marginValuesForChild(child);
        if (!childDiscardMargin) {
            marginInfo.setPositiveMargin(std::max(childMargins.positiveMarginBefore(), childMargins.positiveMarginAfter()));
            marginInfo.setNegativeMargin(std::max(childMargins.negativeMarginBefore(), childMargins.negativeMarginAfter()));
        } else {
            marginInfo.clearMargin();
        }

        // CSS2.1 states:
        // "If the top and bottom margins of an element with clearance are adjoining, its margins collapse with
        // the adjoining margins of following siblings but that resulting margin does not collapse with the bottom margin of the parent block."
        // So the parent's bottom margin cannot collapse through this block or any subsequent self-collapsing blocks. Set a bit to ensure
        // this happens; it will get reset if we encounter an in-flow sibling that is not self-collapsing.
        marginInfo.setCanCollapseMarginAfterWithLastChild(false);

        // For now set the border-top of |child| flush with the bottom border-edge of the float so it can layout any floating or positioned children of
        // its own at the correct vertical position. If subsequent siblings attempt to collapse with |child|'s margins in |collapseMargins| we will
        // adjust the height of the parent to |child|'s margin top (which if it is positive sits up 'inside' the float it's clearing) so that all three
        // margins can collapse at the correct vertical position.
        // Per CSS2.1 we need to ensure that any negative margin-top clears |child| beyond the bottom border-edge of the float so that the top border edge of the child
        // (i.e. its clearance)  is at a position that satisfies the equation: "the amount of clearance is set so that clearance + margin-top = [height of float],
        // i.e., clearance = [height of float] - margin-top".
        setLogicalHeight(child.logicalTop() + childMargins.negativeMarginBefore());
    } else {
        // Increase our height by the amount we had to clear.
        setLogicalHeight(logicalHeight() + heightIncrease);
    }

    if (marginInfo.canCollapseWithMarginBefore()) {
        // We can no longer collapse with the top of the block since a clear
        // occurred. The empty blocks collapse into the cleared block.
        setMaxMarginBeforeValues(oldTopPosMargin, oldTopNegMargin);
        marginInfo.setAtBeforeSideOfBlock(false);

        // In case the child discarded the before margin of the block we need to reset the mustDiscardMarginBefore flag to the initial value.
        setMustDiscardMarginBefore(style()->marginBeforeCollapse() == MarginCollapseDiscard);
    }

    return yPos + heightIncrease;
}

void LayoutBlockFlow::setCollapsedBottomMargin(const MarginInfo& marginInfo)
{
    if (marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()) {
        // Update the after side margin of the container to discard if the after margin of the last child also discards and we collapse with it.
        // Don't update the max margin values because we won't need them anyway.
        if (marginInfo.discardMargin()) {
            setMustDiscardMarginAfter();
            return;
        }

        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        setMaxMarginAfterValues(std::max(maxPositiveMarginAfter(), marginInfo.positiveMargin()), std::max(maxNegativeMarginAfter(), marginInfo.negativeMargin()));

        if (!marginInfo.hasMarginAfterQuirk())
            setHasMarginAfterQuirk(false);

        if (marginInfo.hasMarginAfterQuirk() && !marginAfter()) {
            // We have no bottom margin and our last child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            setHasMarginAfterQuirk(true);
        }
    }
}

void LayoutBlockFlow::marginBeforeEstimateForChild(LayoutBox& child, LayoutUnit& positiveMarginBefore, LayoutUnit& negativeMarginBefore, bool& discardMarginBefore) const
{
    // Give up if in quirks mode and we're a body/table cell and the top margin of the child box is quirky.
    // Give up if the child specified -webkit-margin-collapse: separate that prevents collapsing.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if ((document().inQuirksMode() && hasMarginBeforeQuirk(&child) && (isTableCell() || isBody())) || child.style()->marginBeforeCollapse() == MarginCollapseSeparate)
        return;

    // The margins are discarded by a child that specified -webkit-margin-collapse: discard.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if (child.style()->marginBeforeCollapse() == MarginCollapseDiscard) {
        positiveMarginBefore = LayoutUnit();
        negativeMarginBefore = LayoutUnit();
        discardMarginBefore = true;
        return;
    }

    LayoutUnit beforeChildMargin = marginBeforeForChild(child);
    positiveMarginBefore = std::max(positiveMarginBefore, beforeChildMargin);
    negativeMarginBefore = std::max(negativeMarginBefore, -beforeChildMargin);

    if (!child.isLayoutBlockFlow())
        return;

    LayoutBlockFlow* childBlockFlow = toLayoutBlockFlow(&child);
    if (childBlockFlow->childrenInline() || childBlockFlow->isWritingModeRoot())
        return;

    MarginInfo childMarginInfo(childBlockFlow, childBlockFlow->borderBefore() + childBlockFlow->paddingBefore(), childBlockFlow->borderAfter() + childBlockFlow->paddingAfter());
    if (!childMarginInfo.canCollapseMarginBeforeWithChildren())
        return;

    LayoutBox* grandchildBox = childBlockFlow->firstChildBox();
    for ( ; grandchildBox; grandchildBox = grandchildBox->nextSiblingBox()) {
        if (!grandchildBox->isFloatingOrOutOfFlowPositioned() && !grandchildBox->isColumnSpanAll())
            break;
    }

    if (!grandchildBox)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(this);
        if (grandchildBox->isLayoutBlock()) {
            LayoutBlock* grandchildBlock = toLayoutBlock(grandchildBox);
            grandchildBlock->setHasMarginBeforeQuirk(grandchildBox->style()->hasMarginBeforeQuirk());
            grandchildBlock->setHasMarginAfterQuirk(grandchildBox->style()->hasMarginAfterQuirk());
        }
    }

    // If we have a 'clear' value but also have a margin we may not actually require clearance to move past any floats.
    // If that's the case we want to be sure we estimate the correct position including margins after any floats rather
    // than use 'clearance' later which could give us the wrong position.
    if (grandchildBox->style()->clear() != ClearNone && childBlockFlow->marginBeforeForChild(*grandchildBox) == 0)
        return;

    // Collapse the margin of the grandchild box with our own to produce an estimate.
    childBlockFlow->marginBeforeEstimateForChild(*grandchildBox, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
}

LayoutUnit LayoutBlockFlow::estimateLogicalTopPosition(LayoutBox& child, const BlockChildrenLayoutInfo& layoutInfo, LayoutUnit& estimateWithoutPagination)
{
    const MarginInfo& marginInfo = layoutInfo.marginInfo();
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    LayoutUnit logicalTopEstimate = logicalHeight();
    LayoutUnit positiveMarginBefore;
    LayoutUnit negativeMarginBefore;
    bool discardMarginBefore = false;
    if (!marginInfo.canCollapseWithMarginBefore()) {
        if (child.selfNeedsLayout()) {
            // Try to do a basic estimation of how the collapse is going to go.
            marginBeforeEstimateForChild(child, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
        } else {
            // Use the cached collapsed margin values from a previous layout. Most of the time they
            // will be right.
            LayoutBlockFlow::MarginValues marginValues = marginValuesForChild(child);
            positiveMarginBefore = std::max(positiveMarginBefore, marginValues.positiveMarginBefore());
            negativeMarginBefore = std::max(negativeMarginBefore, marginValues.negativeMarginBefore());
            discardMarginBefore = mustDiscardMarginBeforeForChild(child);
        }

        // Collapse the result with our current margins.
        if (!discardMarginBefore)
            logicalTopEstimate += std::max(marginInfo.positiveMargin(), positiveMarginBefore) - std::max(marginInfo.negativeMargin(), negativeMarginBefore);
    }

    // Adjust logicalTopEstimate down to the next page if the margins are so large that we don't fit on the current
    // page.
    LayoutState* layoutState = view()->layoutState();
    if (layoutState->isPaginated() && isPageLogicalHeightKnown(logicalHeight()) && logicalTopEstimate > logicalHeight())
        logicalTopEstimate = std::min(logicalTopEstimate, nextPageLogicalTop(logicalHeight(), AssociateWithLatterPage));

    logicalTopEstimate += getClearDelta(&child, logicalTopEstimate);

    estimateWithoutPagination = logicalTopEstimate;

    if (layoutState->isPaginated()) {
        if (!layoutInfo.isAtFirstInFlowChild()) {
            // Estimate the need for a forced break in front of this child. The final break policy
            // at this class A break point isn't known until we have laid out the children of
            // |child|. There may be forced break-before values set on first-children inside that
            // get propagated up to the child. Just make an estimate with what we know so far.
            EBreak breakValue = child.classABreakPointValue(layoutInfo.previousBreakAfterValue());
            if (isForcedFragmentainerBreakValue(breakValue)) {
                logicalTopEstimate = applyForcedBreak(logicalHeight(), breakValue);
                // Disregard previous margins, since they will collapse with the fragmentainer
                // boundary, due to the forced break. Only apply margins that have been specified
                // on the child or its descendants.
                if (!discardMarginBefore)
                    logicalTopEstimate += positiveMarginBefore - negativeMarginBefore;

                // Clearance may already have taken us past the beginning of the next
                // fragmentainer.
                return std::max(estimateWithoutPagination, logicalTopEstimate);
            }
        }

        // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
        logicalTopEstimate = adjustForUnsplittableChild(child, logicalTopEstimate);
    }

    return logicalTopEstimate;
}

void LayoutBlockFlow::adjustFloatingBlock(const MarginInfo& marginInfo)
{
    // The float should be positioned taking into account the bottom margin
    // of the previous flow. We add that margin into the height, get the
    // float positioned properly, and then subtract the margin out of the
    // height again. In the case of self-collapsing blocks, we always just
    // use the top margins, since the self-collapsing block collapsed its
    // own bottom margin into its top margin.
    //
    // Note also that the previous flow may collapse its margin into the top of
    // our block. If this is the case, then we do not add the margin in to our
    // height when computing the position of the float. This condition can be tested
    // for by simply calling canCollapseWithMarginBefore. See
    // http://www.hixie.ch/tests/adhoc/css/box/block/margin-collapse/046.html for
    // an example of this scenario.
    LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? LayoutUnit() : marginInfo.margin();
    setLogicalHeight(logicalHeight() + marginOffset);
    positionNewFloats();
    setLogicalHeight(logicalHeight() - marginOffset);
}

void LayoutBlockFlow::handleAfterSideOfBlock(LayoutBox* lastChild, LayoutUnit beforeSide, LayoutUnit afterSide, MarginInfo& marginInfo)
{
    marginInfo.setAtAfterSideOfBlock(true);

    // If our last child was a self-collapsing block with clearance then our logical height is flush with the
    // bottom edge of the float that the child clears. The correct vertical position for the margin-collapsing we want
    // to perform now is at the child's margin-top - so adjust our height to that position.
    if (marginInfo.lastChildIsSelfCollapsingBlockWithClearance()) {
        ASSERT(lastChild);
        setLogicalHeight(logicalHeight() - marginValuesForChild(*lastChild).positiveMarginBefore());
    }

    if (marginInfo.canCollapseMarginAfterWithChildren() && !marginInfo.canCollapseMarginAfterWithLastChild())
        marginInfo.setCanCollapseMarginAfterWithChildren(false);

    // If we can't collapse with children then go ahead and add in the bottom margin.
    if (!marginInfo.discardMargin() && (!marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()
        && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginAfterQuirk())))
        setLogicalHeight(logicalHeight() + marginInfo.margin());

    // Now add in our bottom border/padding.
    setLogicalHeight(logicalHeight() + afterSide);

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    setLogicalHeight(std::max(logicalHeight(), beforeSide + afterSide));

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);

    // There's no class A break point right after the last child, only *between* siblings. So
    // propagate the break-after value, and keep looking for a class A break point (at the next
    // in-flow block-level object), where we'll join this break-after value with the break-before
    // value there.
    if (view()->layoutState()->isPaginated() && lastChild)
        setBreakAfter(joinFragmentainerBreakValues(breakAfter(), lastChild->breakAfter()));
}

void LayoutBlockFlow::setMustDiscardMarginBefore(bool value)
{
    if (style()->marginBeforeCollapse() == MarginCollapseDiscard) {
        ASSERT(value);
        return;
    }

    if (!m_rareData && !value)
        return;

    if (!m_rareData)
        m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));

    m_rareData->m_discardMarginBefore = value;
}

void LayoutBlockFlow::setMustDiscardMarginAfter(bool value)
{
    if (style()->marginAfterCollapse() == MarginCollapseDiscard) {
        ASSERT(value);
        return;
    }

    if (!m_rareData && !value)
        return;

    if (!m_rareData)
        m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));

    m_rareData->m_discardMarginAfter = value;
}

bool LayoutBlockFlow::mustDiscardMarginBefore() const
{
    return style()->marginBeforeCollapse() == MarginCollapseDiscard || (m_rareData && m_rareData->m_discardMarginBefore);
}

bool LayoutBlockFlow::mustDiscardMarginAfter() const
{
    return style()->marginAfterCollapse() == MarginCollapseDiscard || (m_rareData && m_rareData->m_discardMarginAfter);
}

bool LayoutBlockFlow::mustDiscardMarginBeforeForChild(const LayoutBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child)->mustDiscardMarginBefore() : (child.style()->marginBeforeCollapse() == MarginCollapseDiscard);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child)->mustDiscardMarginAfter() : (child.style()->marginAfterCollapse() == MarginCollapseDiscard);

    // FIXME: We return false here because the implementation is not geometrically complete. We have values only for before/after, not start/end.
    // In case the boxes are perpendicular we assume the property is not specified.
    return false;
}

bool LayoutBlockFlow::mustDiscardMarginAfterForChild(const LayoutBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child)->mustDiscardMarginAfter() : (child.style()->marginAfterCollapse() == MarginCollapseDiscard);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isLayoutBlockFlow() ? toLayoutBlockFlow(&child)->mustDiscardMarginBefore() : (child.style()->marginBeforeCollapse() == MarginCollapseDiscard);

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

void LayoutBlockFlow::setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!m_rareData) {
        if (pos == LayoutBlockFlowRareData::positiveMarginBeforeDefault(this) && neg == LayoutBlockFlowRareData::negativeMarginBeforeDefault(this))
            return;
        m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));
    }
    m_rareData->m_margins.setPositiveMarginBefore(pos);
    m_rareData->m_margins.setNegativeMarginBefore(neg);
}

void LayoutBlockFlow::setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!m_rareData) {
        if (pos == LayoutBlockFlowRareData::positiveMarginAfterDefault(this) && neg == LayoutBlockFlowRareData::negativeMarginAfterDefault(this))
            return;
        m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));
    }
    m_rareData->m_margins.setPositiveMarginAfter(pos);
    m_rareData->m_margins.setNegativeMarginAfter(neg);
}

bool LayoutBlockFlow::mustSeparateMarginBeforeForChild(const LayoutBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const ComputedStyle& childStyle = child.styleRef();
    if (!child.isWritingModeRoot())
        return childStyle.marginBeforeCollapse() == MarginCollapseSeparate;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginAfterCollapse() == MarginCollapseSeparate;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool LayoutBlockFlow::mustSeparateMarginAfterForChild(const LayoutBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const ComputedStyle& childStyle = child.styleRef();
    if (!child.isWritingModeRoot())
        return childStyle.marginAfterCollapse() == MarginCollapseSeparate;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginBeforeCollapse() == MarginCollapseSeparate;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

LayoutUnit LayoutBlockFlow::applyForcedBreak(LayoutUnit logicalOffset, EBreak breakValue)
{
    // TODO(mstensho): honor breakValue. There are different types of forced breaks. We currently
    // just assume that we want to break to the top of the next fragmentainer of the fragmentation
    // context we're in. However, we may want to find the next left or right page - even if we're
    // inside a multicol container when printing.
    if (isForcedFragmentainerBreakValue(breakValue))
        return nextPageLogicalTop(logicalOffset, AssociateWithFormerPage);
    return logicalOffset;
}

void LayoutBlockFlow::setBreakBefore(EBreak breakValue)
{
    if (breakValue != BreakAuto && !isBreakBetweenControllable(breakValue))
        breakValue = BreakAuto;
    if (breakValue == BreakAuto && !m_rareData)
        return;
    ensureRareData().m_breakBefore = breakValue;
}

void LayoutBlockFlow::setBreakAfter(EBreak breakValue)
{
    if (breakValue != BreakAuto && !isBreakBetweenControllable(breakValue))
        breakValue = BreakAuto;
    if (breakValue == BreakAuto && !m_rareData)
        return;
    ensureRareData().m_breakAfter = breakValue;
}

EBreak LayoutBlockFlow::breakBefore() const
{
    return m_rareData ? static_cast<EBreak>(m_rareData->m_breakBefore) : BreakAuto;
}

EBreak LayoutBlockFlow::breakAfter() const
{
    return m_rareData ? static_cast<EBreak>(m_rareData->m_breakAfter) : BreakAuto;
}

void LayoutBlockFlow::addOverflowFromFloats()
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
        const FloatingObject& floatingObject = *it->get();
        if (floatingObject.isDescendant())
            addOverflowFromChild(floatingObject.layoutObject(), LayoutSize(xPositionForFloatIncludingMargin(floatingObject), yPositionForFloatIncludingMargin(floatingObject)));
    }
}

void LayoutBlockFlow::computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats)
{
    LayoutBlock::computeOverflow(oldClientAfterEdge, recomputeFloats);
    if (recomputeFloats || createsNewFormattingContext() || hasSelfPaintingLayer())
        addOverflowFromFloats();
}

void LayoutBlockFlow::computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const
{
    LayoutBlock::computeSelfHitTestRects(rects, layerOffset);

    if (!hasHorizontalLayoutOverflow() && !hasVerticalLayoutOverflow())
        return;

    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
        LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
        LayoutRect rect(layerOffset.x() + curr->x(), layerOffset.y() + top, curr->width(), bottom - top);
        // It's common for this rect to be entirely contained in our box, so exclude that simple case.
        if (!rect.isEmpty() && (rects.isEmpty() || !rects[0].contains(rect)))
            rects.append(rect);
    }
}

void LayoutBlockFlow::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    if (!isAnonymousBlockContinuation()) {
        LayoutBlock::absoluteRects(rects, accumulatedOffset);
        return;
    }
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    // FIXME: This is wrong for vertical writing-modes.
    // https://bugs.webkit.org/show_bug.cgi?id=46781
    LayoutRect rect(accumulatedOffset, size());
    rect.expand(collapsedMarginBoxLogicalOutsets());
    rects.append(pixelSnappedIntRect(rect));
    continuation()->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() + inlineElementContinuation()->containingBlock()->location()));
}

void LayoutBlockFlow::absoluteQuads(Vector<FloatQuad>& quads) const
{
    if (!isAnonymousBlockContinuation()) {
        LayoutBlock::absoluteQuads(quads);
        return;
    }
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    // FIXME: This is wrong for vertical writing-modes.
    // https://bugs.webkit.org/show_bug.cgi?id=46781
    LayoutRect localRect(LayoutPoint(), size());
    localRect.expand(collapsedMarginBoxLogicalOutsets());
    quads.append(localToAbsoluteQuad(FloatRect(localRect)));
    continuation()->absoluteQuads(quads);
}

LayoutObject* LayoutBlockFlow::hoverAncestor() const
{
    return isAnonymousBlockContinuation() ? continuation() : LayoutBlock::hoverAncestor();
}

void LayoutBlockFlow::updateDragState(bool dragOn)
{
    LayoutBlock::updateDragState(dragOn);
    if (LayoutBoxModelObject* continuation = this->continuation())
        continuation->updateDragState(dragOn);
}

RootInlineBox* LayoutBlockFlow::createAndAppendRootInlineBox()
{
    RootInlineBox* rootBox = createRootInlineBox();
    m_lineBoxes.appendLineBox(rootBox);

    return rootBox;
}

void LayoutBlockFlow::deleteLineBoxTree()
{
    if (containsFloats())
        m_floatingObjects->clearLineBoxTreePointers();

    m_lineBoxes.deleteLineBoxTree();
}

int LayoutBlockFlow::lineCount(const RootInlineBox* stopRootInlineBox) const
{
#ifndef NDEBUG
    ASSERT(!stopRootInlineBox || stopRootInlineBox->block().debugPointer() == this);
#endif
    if (!childrenInline())
        return 0;

    int count = 0;
    for (const RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
        count++;
        if (box == stopRootInlineBox)
            break;
    }
    return count;
}

int LayoutBlockFlow::firstLineBoxBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;
    if (!childrenInline())
        return LayoutBlock::firstLineBoxBaseline();
    if (firstLineBox())
        return firstLineBox()->logicalTop() + style(true)->getFontMetrics().ascent(firstRootBox()->baselineType());
    return -1;
}

int LayoutBlockFlow::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    // CSS2.1 states that the baseline of an 'inline-block' is:
    // the baseline of the last line box in the normal flow, unless it has
    // either no in-flow line boxes or if its 'overflow' property has a computed
    // value other than 'visible', in which case the baseline is the bottom
    // margin edge.
    // We likewise avoid using the last line box in the case of size containment,
    // where the block's contents shouldn't be considered when laying out its
    // ancestors or siblings.

    if ((!style()->isOverflowVisible() && !shouldIgnoreOverflowPropertyForInlineBlockBaseline()) || style()->containsSize()) {
        // We are not calling baselinePosition here because the caller should add the margin-top/margin-right, not us.
        return lineDirection == HorizontalLine ? size().height() + marginBottom() : size().width() + marginLeft();
    }
    if (isWritingModeRoot() && !isRubyRun())
        return -1;
    if (!childrenInline())
        return LayoutBlock::inlineBlockBaseline(lineDirection);
    if (lastLineBox())
        return lastLineBox()->logicalTop() + style(lastLineBox() == firstLineBox())->getFontMetrics().ascent(lastRootBox()->baselineType());
    if (!hasLineIfEmpty())
        return -1;
    const FontMetrics& fontMetrics = firstLineStyle()->getFontMetrics();
    return fontMetrics.ascent()
        + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
        + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
}

void LayoutBlockFlow::removeFloatingObjectsFromDescendants()
{
    if (!containsFloats())
        return;
    removeFloatingObjects();
    setChildNeedsLayout(MarkOnlyThis);

    // If our children are inline, then the only boxes which could contain floats are atomic inlines (e.g. inline-block, float etc.)
    // and these create formatting contexts, so can't pick up intruding floats from ancestors/siblings - making them safe to skip.
    if (childrenInline())
        return;
    for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
        // We don't skip blocks that create formatting contexts as they may have only recently
        // changed style and their float lists may still contain floats from siblings and ancestors.
        if (child->isLayoutBlockFlow())
            toLayoutBlockFlow(child)->removeFloatingObjectsFromDescendants();
    }
}

void LayoutBlockFlow::markAllDescendantsWithFloatsForLayout(LayoutBox* floatToRemove, bool inLayout)
{
    if (!everHadLayout() && !containsFloats())
        return;

    if (m_descendantsWithFloatsMarkedForLayout && !floatToRemove)
        return;
    m_descendantsWithFloatsMarkedForLayout |= !floatToRemove;

    MarkingBehavior markParents = inLayout ? MarkOnlyThis : MarkContainerChain;
    setChildNeedsLayout(markParents);

    if (floatToRemove)
        removeFloatingObject(floatToRemove);

    // Iterate over our children and mark them as needed. If our children are inline, then the
    // only boxes which could contain floats are atomic inlines (e.g. inline-block, float etc.) and these create formatting
    // contexts, so can't pick up intruding floats from ancestors/siblings - making them safe to skip.
    if (!childrenInline()) {
        for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
            if ((!floatToRemove && child->isFloatingOrOutOfFlowPositioned()) || !child->isLayoutBlock())
                continue;
            if (!child->isLayoutBlockFlow()) {
                LayoutBlock* childBlock = toLayoutBlock(child);
                if (childBlock->shrinkToAvoidFloats() && childBlock->everHadLayout())
                    childBlock->setChildNeedsLayout(markParents);
                continue;
            }
            LayoutBlockFlow* childBlockFlow = toLayoutBlockFlow(child);
            if ((floatToRemove ? childBlockFlow->containsFloat(floatToRemove) : childBlockFlow->containsFloats()) || childBlockFlow->shrinkToAvoidFloats())
                childBlockFlow->markAllDescendantsWithFloatsForLayout(floatToRemove, inLayout);
        }
    }
}

void LayoutBlockFlow::markSiblingsWithFloatsForLayout(LayoutBox* floatToRemove)
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();

    for (LayoutObject* next = nextSibling(); next; next = next->nextSibling()) {
        if (!next->isLayoutBlockFlow() || (!floatToRemove && (next->isFloatingOrOutOfFlowPositioned() || toLayoutBlockFlow(next)->avoidsFloats())))
            continue;

        LayoutBlockFlow* nextBlock = toLayoutBlockFlow(next);
        for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
            LayoutBox* floatingBox = (*it)->layoutObject();
            if (floatToRemove && floatingBox != floatToRemove)
                continue;
            if (nextBlock->containsFloat(floatingBox))
                nextBlock->markAllDescendantsWithFloatsForLayout(floatingBox);
        }
    }
}

LayoutUnit LayoutBlockFlow::getClearDelta(LayoutBox* child, LayoutUnit logicalTop)
{
    // There is no need to compute clearance if we have no floats.
    if (!containsFloats())
        return LayoutUnit();

    // At least one float is present. We need to perform the clearance computation.
    bool clearSet = child->style()->clear() != ClearNone;
    LayoutUnit logicalBottom;
    switch (child->style()->clear()) {
    case ClearNone:
        break;
    case ClearLeft:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case ClearRight:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case ClearBoth:
        logicalBottom = lowestFloatLogicalBottom();
        break;
    }

    // We also clear floats if we are too big to sit on the same line as a float (and wish to avoid floats by default).
    LayoutUnit result = clearSet ? (logicalBottom - logicalTop).clampNegativeToZero() : LayoutUnit();
    if (!result && child->avoidsFloats()) {
        LayoutUnit newLogicalTop = logicalTop;
        LayoutRect borderBox = child->borderBoxRect();
        LayoutUnit childLogicalWidthAtOldLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();
        while (true) {
            LayoutUnit availableLogicalWidthAtNewLogicalTopOffset = availableLogicalWidthForLine(newLogicalTop, DoNotIndentText, logicalHeightForChild(*child));
            if (availableLogicalWidthAtNewLogicalTopOffset == availableLogicalWidthForContent())
                return newLogicalTop - logicalTop;

            LogicalExtentComputedValues computedValues;
            child->logicalExtentAfterUpdatingLogicalWidth(newLogicalTop, computedValues);
            LayoutUnit childLogicalWidthAtNewLogicalTopOffset = computedValues.m_extent;

            if (childLogicalWidthAtNewLogicalTopOffset <= availableLogicalWidthAtNewLogicalTopOffset) {
                // Even though we may not be moving, if the logical width did shrink because of the presence of new floats, then
                // we need to force a relayout as though we shifted. This happens because of the dynamic addition of overhanging floats
                // from previous siblings when negative margins exist on a child (see the addOverhangingFloats call at the end of collapseMargins).
                if (childLogicalWidthAtOldLogicalTopOffset != childLogicalWidthAtNewLogicalTopOffset)
                    child->setChildNeedsLayout(MarkOnlyThis);
                return newLogicalTop - logicalTop;
            }

            newLogicalTop = nextFloatLogicalBottomBelowForBlock(newLogicalTop);
            ASSERT(newLogicalTop >= logicalTop);
            if (newLogicalTop < logicalTop)
                break;
        }
        ASSERT_NOT_REACHED();
    }
    return result;
}

void LayoutBlockFlow::createFloatingObjects()
{
    m_floatingObjects = wrapUnique(new FloatingObjects(this, isHorizontalWritingMode()));
}

void LayoutBlockFlow::willBeDestroyed()
{
    // Mark as being destroyed to avoid trouble with merges in removeChild().
    m_beingDestroyed = true;

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from. Effects that do :before/:after only on hover could crash otherwise.
    children()->destroyLeftoverChildren();

    // Destroy our continuation before anything other than anonymous children.
    // The reason we don't destroy it before anonymous children is that they may
    // have continuations of their own that are anonymous children of our continuation.
    LayoutBoxModelObject* continuation = this->continuation();
    if (continuation) {
        continuation->destroy();
        setContinuation(nullptr);
    }

    if (!documentBeingDestroyed()) {
        // TODO(mstensho): figure out if we need this. We have no test coverage for it. It looks
        // like all line boxes have been removed at this point.
        if (firstLineBox()) {
            // We can't wait for LayoutBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            // FIXME: The FrameSelection should be responsible for this when it
            // is notified of DOM mutations.
            if (isSelectionBorder())
                view()->clearSelection();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (InlineFlowBox* box = firstLineBox(); box; box = box->nextLineBox()) {
                    while (InlineBox* childBox = box->firstChild())
                        childBox->remove();
                }
            }
        }
    }

    m_lineBoxes.deleteLineBoxes();

    LayoutBlock::willBeDestroyed();
}

void LayoutBlockFlow::styleWillChange(StyleDifference diff, const ComputedStyle& newStyle)
{
    const ComputedStyle* oldStyle = style();
    s_canPropagateFloatIntoSibling = oldStyle ? !isFloatingOrOutOfFlowPositioned() && !avoidsFloats() : false;
    if (oldStyle && parent() && diff.needsFullLayout() && oldStyle->position() != newStyle.position()
        && containsFloats() && !isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
            markAllDescendantsWithFloatsForLayout();

    LayoutBlock::styleWillChange(diff, newStyle);
}

void LayoutBlockFlow::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    bool hadSelfPaintingLayer = hasSelfPaintingLayer();
    LayoutBlock::styleDidChange(diff, oldStyle);

    // After our style changed, if we lose our ability to propagate floats into next sibling
    // blocks, then we need to find the top most parent containing that overhanging float and
    // then mark its descendants with floats for layout and clear all floats from its next
    // sibling blocks that exist in our floating objects list. See bug 56299 and 62875.
    bool canPropagateFloatIntoSibling = !isFloatingOrOutOfFlowPositioned() && !avoidsFloats();
    bool siblingFloatPropagationChanged = diff.needsFullLayout() && s_canPropagateFloatIntoSibling && !canPropagateFloatIntoSibling && hasOverhangingFloats();

    // When this object's self-painting layer status changed, we should update FloatingObjects::shouldPaint() flags for
    // descendant overhanging floats in ancestors.
    bool needsUpdateAncestorFloatObjectShouldPaintFlags = false;
    if (hasSelfPaintingLayer() != hadSelfPaintingLayer && hasOverhangingFloats()) {
        setNeedsLayout(LayoutInvalidationReason::StyleChange);
        if (hadSelfPaintingLayer)
            markAllDescendantsWithFloatsForLayout();
        else
            needsUpdateAncestorFloatObjectShouldPaintFlags = true;
    }

    if (siblingFloatPropagationChanged || needsUpdateAncestorFloatObjectShouldPaintFlags) {
        LayoutBlockFlow* parentBlockFlow = this;
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator end = floatingObjectSet.end();

        for (LayoutObject* curr = parent(); curr && !curr->isLayoutView(); curr = curr->parent()) {
            if (curr->isLayoutBlockFlow()) {
                LayoutBlockFlow* currBlock = toLayoutBlockFlow(curr);

                if (currBlock->hasOverhangingFloats()) {
                    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
                        LayoutBox* layoutBox = (*it)->layoutObject();
                        if (currBlock->hasOverhangingFloat(layoutBox)) {
                            parentBlockFlow = currBlock;
                            break;
                        }
                    }
                }
            }
        }

        parentBlockFlow->markAllDescendantsWithFloatsForLayout();
        if (siblingFloatPropagationChanged)
            parentBlockFlow->markSiblingsWithFloatsForLayout();
    }

    if (diff.needsFullLayout() || !oldStyle)
        createOrDestroyMultiColumnFlowThreadIfNeeded(oldStyle);
    if (oldStyle) {
        if (LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread()) {
            if (!style()->columnRuleEquivalent(oldStyle)) {
                // Column rules are painted by anonymous column set children of the multicol
                // container. We need to notify them.
                flowThread->columnRuleStyleDidChange();
            }
        }
    }
}

void LayoutBlockFlow::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox& child)
{
    if (child.isLayoutMultiColumnSpannerPlaceholder())
        toLayoutMultiColumnSpannerPlaceholder(child).markForLayoutIfObjectInFlowThreadNeedsLayout();
    LayoutBlock::updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);
}

void LayoutBlockFlow::updateStaticInlinePositionForChild(LayoutBox& child, LayoutUnit logicalTop, IndentTextOrNot indentText)
{
    if (child.style()->isOriginalDisplayInlineType())
        setStaticInlinePositionForChild(child, startAlignedOffsetForLine(logicalTop, indentText));
    else
        setStaticInlinePositionForChild(child, startOffsetForContent());
}

void LayoutBlockFlow::setStaticInlinePositionForChild(LayoutBox& child, LayoutUnit inlinePosition)
{
    child.layer()->setStaticInlinePosition(inlinePosition);
}

LayoutInline* LayoutBlockFlow::inlineElementContinuation() const
{
    LayoutBoxModelObject* continuation = this->continuation();
    return continuation && continuation->isInline() ? toLayoutInline(continuation) : nullptr;
}

void LayoutBlockFlow::addChild(LayoutObject* newChild, LayoutObject* beforeChild)
{
    if (LayoutMultiColumnFlowThread* flowThread = multiColumnFlowThread()) {
        if (beforeChild == flowThread)
            beforeChild = flowThread->firstChild();
        ASSERT(!beforeChild || beforeChild->isDescendantOf(flowThread));
        flowThread->addChild(newChild, beforeChild);
        return;
    }

    if (beforeChild && beforeChild->parent() != this) {
        addChildBeforeDescendant(newChild, beforeChild);
        return;
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    bool childIsBlockLevel = !newChild->isInline() && !newChild->isFloatingOrOutOfFlowPositioned();
    if (childrenInline()) {
        if (childIsBlockLevel) {
            // Wrap the inline content in anonymous blocks, to allow for the new block child to be
            // inserted.
            makeChildrenNonInline(beforeChild);
            madeBoxesNonInline = true;

            if (beforeChild && beforeChild->parent() != this) {
                beforeChild = beforeChild->parent();
                ASSERT(beforeChild->isAnonymousBlock());
                ASSERT(beforeChild->parent() == this);
            }
        }
    } else if (!childIsBlockLevel) {
        // This block has block children. We may want to put the new child into an anomyous
        // block. Floats and out-of-flow children may live among either block or inline children,
        // so for such children, only put them inside an anonymous block if one already exists. If
        // the child is inline, on the other hand, we *have to* put it inside an anonymous block,
        // so create a new one if there is none for us there already.
        LayoutObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            afterChild->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            LayoutBlockFlow* newBlock = toLayoutBlockFlow(createAnonymousBlock());
            LayoutBox::addChild(newBlock, beforeChild);
            // Reparent adjacent floating or out-of-flow siblings to the new box.
            newBlock->reparentPrecedingFloatingOrOutOfFlowSiblings();
            newBlock->addChild(newChild);
            newBlock->reparentSubsequentFloatingOrOutOfFlowSiblings();
            return;
        }
    }

    // Skip the LayoutBlock override, since that one deals with anonymous child insertion in a way
    // that isn't sufficient for us, and can only cause trouble at this point.
    LayoutBox::addChild(newChild, beforeChild);

    if (madeBoxesNonInline && parent() && isAnonymousBlock() && parent()->isLayoutBlock()) {
        toLayoutBlock(parent())->removeLeftoverAnonymousBlock(this);
        // |this| may be dead now.
    }
}

void LayoutBlockFlow::removeChild(LayoutObject* oldChild)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (documentBeingDestroyed()) {
        LayoutBox::removeChild(oldChild);
        return;
    }

    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    LayoutObject* prev = oldChild->previousSibling();
    LayoutObject* next = oldChild->nextSibling();
    bool mergedAnonymousBlocks = false;
    if (prev && next && !oldChild->isInline() && !oldChild->virtualContinuation() && prev->isLayoutBlockFlow() && next->isLayoutBlockFlow()) {
        if (toLayoutBlockFlow(prev)->mergeSiblingContiguousAnonymousBlock(toLayoutBlockFlow(next))) {
            mergedAnonymousBlocks = true;
            next = nullptr;
        }
    }

    LayoutBlock::removeChild(oldChild);

    LayoutObject* child = prev ? prev : next;
    if (mergedAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        collapseAnonymousBlockChild(toLayoutBlockFlow(child));
    }

    if (!firstChild()) {
        // If this was our last child be sure to clear out our line boxes.
        if (childrenInline())
            deleteLineBoxTree();

        // If we are an empty anonymous block in the continuation chain,
        // we need to remove ourself and fix the continuation chain.
        if (!beingDestroyed() && isAnonymousBlockContinuation() && !oldChild->isListMarker()) {
            LayoutObject* containingBlockIgnoringAnonymous = containingBlock();
            while (containingBlockIgnoringAnonymous && containingBlockIgnoringAnonymous->isAnonymous())
                containingBlockIgnoringAnonymous = containingBlockIgnoringAnonymous->containingBlock();
            for (LayoutObject* curr = this; curr; curr = curr->previousInPreOrder(containingBlockIgnoringAnonymous)) {
                if (curr->virtualContinuation() != this)
                    continue;

                // Found our previous continuation. We just need to point it to
                // |this|'s next continuation.
                LayoutBoxModelObject* nextContinuation = continuation();
                if (curr->isLayoutInline())
                    toLayoutInline(curr)->setContinuation(nextContinuation);
                else if (curr->isLayoutBlockFlow())
                    toLayoutBlockFlow(curr)->setContinuation(nextContinuation);
                else
                    ASSERT_NOT_REACHED();

                break;
            }
            setContinuation(nullptr);
            destroy();
        }
    } else if (!beingDestroyed() && !oldChild->isFloatingOrOutOfFlowPositioned() && !oldChild->isAnonymousBlock()) {
        // If the child we're removing means that we can now treat all children as inline without the need for anonymous blocks, then do that.
        makeChildrenInlineIfPossible();
    }
}

void LayoutBlockFlow::moveAllChildrenIncludingFloatsTo(LayoutBlock* toBlock, bool fullRemoveInsert)
{
    LayoutBlockFlow* toBlockFlow = toLayoutBlockFlow(toBlock);
    moveAllChildrenTo(toBlockFlow, fullRemoveInsert);

    // When a portion of the layout tree is being detached, anonymous blocks
    // will be combined as their children are deleted. In this process, the
    // anonymous block later in the tree is merged into the one preceding it.
    // It can happen that the later block (this) contains floats that the
    // previous block (toBlockFlow) did not contain, and thus are not in the
    // floating objects list for toBlockFlow. This can result in toBlockFlow containing
    // floats that are not in it's floating objects list, but are in the
    // floating objects lists of siblings and parents. This can cause problems
    // when the float itself is deleted, since the deletion code assumes that
    // if a float is not in it's containing block's floating objects list, it
    // isn't in any floating objects list. In order to preserve this condition
    // (removing it has serious performance implications), we need to copy the
    // floating objects from the old block (this) to the new block (toBlockFlow).
    // The float's metrics will likely all be wrong, but since toBlockFlow is
    // already marked for layout, this will get fixed before anything gets
    // displayed.
    // See bug https://code.google.com/p/chromium/issues/detail?id=230907
    if (m_floatingObjects) {
        if (!toBlockFlow->m_floatingObjects)
            toBlockFlow->createFloatingObjects();

        const FloatingObjectSet& fromFloatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator end = fromFloatingObjectSet.end();

        for (FloatingObjectSetIterator it = fromFloatingObjectSet.begin(); it != end; ++it) {
            const FloatingObject& floatingObject = *it->get();

            // Don't insert the object again if it's already in the list
            if (toBlockFlow->containsFloat(floatingObject.layoutObject()))
                continue;

            toBlockFlow->m_floatingObjects->add(floatingObject.unsafeClone());
        }
    }
}

void LayoutBlockFlow::childBecameFloatingOrOutOfFlow(LayoutBox* child)
{
    makeChildrenInlineIfPossible();

    // Reparent the child to an adjacent anonymous block if one is available.
    LayoutObject* prev = child->previousSibling();
    if (prev && prev->isAnonymousBlock() && prev->isLayoutBlockFlow()) {
        LayoutBlockFlow* newContainer = toLayoutBlockFlow(prev);
        moveChildTo(newContainer, child, nullptr, false);
        // The anonymous block we've moved to may now be adjacent to former siblings of ours
        // that it can contain also.
        newContainer->reparentSubsequentFloatingOrOutOfFlowSiblings();
        return;
    }
    LayoutObject* next = child->nextSibling();
    if (next && next->isAnonymousBlock() && next->isLayoutBlockFlow()) {
        LayoutBlockFlow* newContainer = toLayoutBlockFlow(next);
        moveChildTo(newContainer, child, newContainer->firstChild(), false);
    }
}

void LayoutBlockFlow::collapseAnonymousBlockChild(LayoutBlockFlow* child)
{
    // It's possible that this block's destruction may have been triggered by the
    // child's removal. Just bail if the anonymous child block is already being
    // destroyed. See crbug.com/282088
    if (child->beingDestroyed())
        return;
    if (child->continuation())
        return;
    // Ruby elements use anonymous wrappers for ruby runs and ruby bases by design, so we don't remove them.
    if (child->isRubyRun() || child->isRubyBase())
        return;
    setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::ChildAnonymousBlockChanged);

    child->moveAllChildrenTo(this, child->nextSibling(), child->hasLayer());
    setChildrenInline(child->childrenInline());

    children()->removeChildNode(this, child, child->hasLayer());
    child->destroy();
}

static bool isMergeableAnonymousBlock(const LayoutBlockFlow* block)
{
    return block->isAnonymousBlock() && !block->continuation() && !block->beingDestroyed() && !block->isRubyRun() && !block->isRubyBase();
}

bool LayoutBlockFlow::mergeSiblingContiguousAnonymousBlock(LayoutBlockFlow* siblingThatMayBeDeleted)
{
    // Note: |this| and |siblingThatMayBeDeleted| may not be adjacent siblings at this point. There
    // may be an object between them which is about to be removed.

    if (!isMergeableAnonymousBlock(this) || !isMergeableAnonymousBlock(siblingThatMayBeDeleted))
        return false;

    setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::AnonymousBlockChange);

    // If the inlineness of children of the two block don't match, we'd need special code here
    // (but there should be no need for it).
    ASSERT(siblingThatMayBeDeleted->childrenInline() == childrenInline());
    // Take all the children out of the |next| block and put them in
    // the |prev| block.
    siblingThatMayBeDeleted->moveAllChildrenIncludingFloatsTo(this, siblingThatMayBeDeleted->hasLayer() || hasLayer());
    // Delete the now-empty block's lines and nuke it.
    siblingThatMayBeDeleted->deleteLineBoxTree();
    siblingThatMayBeDeleted->destroy();
    return true;
}

void LayoutBlockFlow::reparentSubsequentFloatingOrOutOfFlowSiblings()
{
    if (!parent() || !parent()->isLayoutBlockFlow())
        return;
    if (beingDestroyed() || documentBeingDestroyed())
        return;
    LayoutBlockFlow* parentBlockFlow = toLayoutBlockFlow(parent());
    LayoutObject* child = nextSibling();
    while (child && child->isFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = child->nextSibling();
        parentBlockFlow->moveChildTo(this, child, nullptr, false);
        child = sibling;
    }

    if (LayoutObject* next = nextSibling()) {
        if (next->isLayoutBlockFlow())
            mergeSiblingContiguousAnonymousBlock(toLayoutBlockFlow(next));
    }
}

void LayoutBlockFlow::reparentPrecedingFloatingOrOutOfFlowSiblings()
{
    if (!parent() || !parent()->isLayoutBlockFlow())
        return;
    if (beingDestroyed() || documentBeingDestroyed())
        return;
    LayoutBlockFlow* parentBlockFlow = toLayoutBlockFlow(parent());
    LayoutObject* child = previousSibling();
    while (child && child->isFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = child->previousSibling();
        parentBlockFlow->moveChildTo(this, child, firstChild(), false);
        child = sibling;
    }
}

void LayoutBlockFlow::makeChildrenInlineIfPossible()
{
    // Collapsing away anonymous wrappers isn't relevant for the children of anonymous blocks, unless they are ruby bases.
    if (isAnonymousBlock() && !isRubyBase())
        return;

    Vector<LayoutBlockFlow*, 3> blocksToRemove;
    for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isFloating())
            continue;
        if (child->isOutOfFlowPositioned())
            continue;

        // There are still block children in the container, so any anonymous wrappers are still needed.
        if (!child->isAnonymousBlock() || !child->isLayoutBlockFlow())
            return;
        // If one of the children is being destroyed then it is unsafe to clean up anonymous wrappers as the
        // entire branch may be being destroyed.
        if (toLayoutBlockFlow(child)->beingDestroyed())
            return;
        // We can't remove anonymous wrappers if they contain continuations as this means there are block children present.
        if (toLayoutBlockFlow(child)->continuation())
            return;
        // We are only interested in removing anonymous wrappers if there are inline siblings underneath them.
        if (!child->childrenInline())
            return;
        // Ruby elements use anonymous wrappers for ruby runs and ruby bases by design, so we don't remove them.
        if (child->isRubyRun() || child->isRubyBase())
            return;

        blocksToRemove.append(toLayoutBlockFlow(child));
    }

    // If we make an object's children inline we are going to frustrate any future attempts to remove
    // floats from its children's float-lists before the next layout happens so clear down all the floatlists
    // now - they will be rebuilt at layout.
    removeFloatingObjectsFromDescendants();

    for (size_t i = 0; i < blocksToRemove.size(); i++)
        collapseAnonymousBlockChild(blocksToRemove[i]);
    setChildrenInline(true);
}

static void getInlineRun(LayoutObject* start, LayoutObject* boundary,
    LayoutObject*& inlineRunStart,
    LayoutObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.

    // Start by skipping as many non-inlines as we can.
    LayoutObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()))
            curr = curr->nextSibling();

        inlineRunStart = inlineRunEnd = curr;

        if (!curr)
            return; // No more inline children to be found.

        sawInline = curr->isInline();

        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void LayoutBlockFlow::makeChildrenNonInline(LayoutObject *insertionPoint)
{
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(!isInline() || isAtomicInlineLevel());
    ASSERT(!insertionPoint || insertionPoint->parent() == this);

    setChildrenInline(false);

    LayoutObject* child = firstChild();
    if (!child)
        return;

    deleteLineBoxTree();

    while (child) {
        LayoutObject* inlineRunStart;
        LayoutObject* inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        LayoutBlock* block = createAnonymousBlock();
        children()->insertChildNode(this, block, inlineRunStart);
        moveChildrenTo(block, inlineRunStart, child);
    }

#if ENABLE(ASSERT)
    for (LayoutObject *c = firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif

    setShouldDoFullPaintInvalidation();
}

void LayoutBlockFlow::childBecameNonInline(LayoutObject*)
{
    makeChildrenNonInline();
    if (isAnonymousBlock() && parent() && parent()->isLayoutBlock())
        toLayoutBlock(parent())->removeLeftoverAnonymousBlock(this);
    // |this| may be dead here
}

void LayoutBlockFlow::invalidatePaintForOverhangingFloats(bool paintAllDescendants)
{
    // Invalidate paint of any overhanging floats (if we know we're the one to paint them).
    // Otherwise, bail out.
    if (!hasOverhangingFloats())
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
        const FloatingObject& floatingObject = *it->get();
        // Only issue paint invaldiations for the object if it is overhanging, is not in its own layer, and
        // is our responsibility to paint (m_shouldPaint is set). When paintAllDescendants is true, the latter
        // condition is replaced with being a descendant of us.
        if (isOverhangingFloat(floatingObject)
            && !floatingObject.layoutObject()->hasSelfPaintingLayer()
            && (floatingObject.shouldPaint() || (paintAllDescendants && floatingObject.layoutObject()->isDescendantOf(this)))) {

            LayoutBox* floatingLayoutBox = floatingObject.layoutObject();
            floatingLayoutBox->setShouldDoFullPaintInvalidation();
            floatingLayoutBox->invalidatePaintForOverhangingFloats(false);
        }
    }
}

void LayoutBlockFlow::invalidateDisplayItemClients(PaintInvalidationReason invalidationReason) const
{
    LayoutBlock::invalidateDisplayItemClients(invalidationReason);

    // If the block is a continuation or containing block of an inline continuation, invalidate the
    // start object of the continuations if it has focus ring because change of continuation may change
    // the shape of the focus ring.
    if (!isAnonymous())
        return;

    LayoutObject* startOfContinuations = nullptr;
    if (LayoutInline* inlineElementContinuation = this->inlineElementContinuation()) {
        // This block is an anonymous block continuation.
        startOfContinuations = inlineElementContinuation->node()->layoutObject();
    } else if (LayoutObject* firstChild = this->firstChild()) {
        // This block is the anonymous containing block of an inline element continuation.
        if (firstChild->isElementContinuation())
            startOfContinuations = firstChild->node()->layoutObject();
    }
    if (startOfContinuations && startOfContinuations->styleRef().outlineStyleIsAuto())
        startOfContinuations->slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(*startOfContinuations, invalidationReason);
}

void LayoutBlockFlow::clearFloats(EClear clear)
{
    positionNewFloats();
    // set y position
    LayoutUnit newY;
    switch (clear) {
    case ClearLeft:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case ClearRight:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case ClearBoth:
        newY = lowestFloatLogicalBottom();
    default:
        break;
    }
    if (size().height() < newY)
        setLogicalHeight(newY);
}

bool LayoutBlockFlow::containsFloat(LayoutBox* layoutBox) const
{
    return m_floatingObjects && m_floatingObjects->set().contains<FloatingObjectHashTranslator>(layoutBox);
}

void LayoutBlockFlow::removeFloatingObjects()
{
    if (!m_floatingObjects)
        return;

    markSiblingsWithFloatsForLayout();

    m_floatingObjects->clear();
}

LayoutPoint LayoutBlockFlow::flipFloatForWritingModeForChild(const FloatingObject& child, const LayoutPoint& point) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return point;

    // This is similar to LayoutBox::flipForWritingModeForChild. We have to subtract out our left offsets twice, since
    // it's going to get added back in. We hide this complication here so that the calling code looks normal for the unflipped
    // case.
    return LayoutPoint(point.x() + size().width() - child.layoutObject()->size().width() - 2 * xPositionForFloatIncludingMargin(child), point.y());
}

LayoutUnit LayoutBlockFlow::logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        offset = m_floatingObjects->logicalLeftOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalLeftOffsetForLine(offset, DoNotIndentText);
}

LayoutUnit LayoutBlockFlow::logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        offset = m_floatingObjects->logicalRightOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalRightOffsetForLine(offset, DoNotIndentText);
}

LayoutUnit LayoutBlockFlow::adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, IndentTextOrNot applyTextIndent) const
{
    LayoutUnit left = offsetFromFloats;

    if (applyTextIndent == IndentText && style()->isLeftToRightDirection())
        left += textIndentOffset();

    return left;
}

LayoutUnit LayoutBlockFlow::adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, IndentTextOrNot applyTextIndent) const
{
    LayoutUnit right = offsetFromFloats;

    if (applyTextIndent == IndentText && !style()->isLeftToRightDirection())
        right -= textIndentOffset();

    return right;
}

LayoutPoint LayoutBlockFlow::computeLogicalLocationForFloat(const FloatingObject& floatingObject, LayoutUnit logicalTopOffset) const
{
    LayoutBox* childBox = floatingObject.layoutObject();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
    LayoutUnit logicalRightOffset; // Constant part of right offset.
    logicalRightOffset = logicalRightOffsetForContent();

    LayoutUnit floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    bool insideFlowThread = flowThreadContainingBlock();

    if (childBox->style()->floating() == LeftFloat) {
        LayoutUnit heightRemainingLeft = LayoutUnit(1);
        LayoutUnit heightRemainingRight = LayoutUnit(1);
        floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft);
        while (logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight) - floatLogicalLeft < floatLogicalWidth) {
            logicalTopOffset += std::min<LayoutUnit>(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        floatLogicalLeft = std::max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
    } else {
        LayoutUnit heightRemainingLeft = LayoutUnit(1);
        LayoutUnit heightRemainingRight = LayoutUnit(1);
        floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight);
        while (floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft) < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        // Use the original width of the float here, since the local variable
        // |floatLogicalWidth| was capped to the available line width. See
        // fast/block/float/clamped-right-float.html.
        floatLogicalLeft -= logicalWidthForFloat(floatingObject);
    }

    return LayoutPoint(floatLogicalLeft, logicalTopOffset);
}

FloatingObject* LayoutBlockFlow::insertFloatingObject(LayoutBox& floatBox)
{
    ASSERT(floatBox.isFloating());

    // Create the list of special objects if we don't aleady have one
    if (!m_floatingObjects) {
        createFloatingObjects();
    } else {
        // Don't insert the object again if it's already in the list
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator it = floatingObjectSet.find<FloatingObjectHashTranslator>(&floatBox);
        if (it != floatingObjectSet.end())
            return it->get();
    }

    // Create the special object entry & append it to the list

    std::unique_ptr<FloatingObject> newObj = FloatingObject::create(&floatBox);

    // Our location is irrelevant if we're unsplittable or no pagination is in effect.
    // Just go ahead and lay out the float.
    bool isChildLayoutBlock = floatBox.isLayoutBlock();
    if (isChildLayoutBlock && !floatBox.needsLayout() && view()->layoutState()->pageLogicalHeightChanged())
        floatBox.setChildNeedsLayout(MarkOnlyThis);

    floatBox.layoutIfNeeded();

    setLogicalWidthForFloat(*newObj, logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));

    return m_floatingObjects->add(std::move(newObj));
}

void LayoutBlockFlow::removeFloatingObject(LayoutBox* floatBox)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
        if (it != floatingObjectSet.end()) {
            FloatingObject& floatingObject = *it->get();
            if (childrenInline()) {
                LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                LayoutUnit logicalBottom = logicalBottomForFloat(floatingObject);

                // Fix for https://bugs.webkit.org/show_bug.cgi?id=54995.
                if (logicalBottom < 0 || logicalBottom < logicalTop || logicalTop == LayoutUnit::max()) {
                    logicalBottom = LayoutUnit::max();
                } else {
                    // Special-case zero- and less-than-zero-height floats: those don't touch
                    // the line that they're on, but it still needs to be dirtied. This is
                    // accomplished by pretending they have a height of 1.
                    logicalBottom = std::max(logicalBottom, logicalTop + 1);
                }
                if (floatingObject.originatingLine()) {
                    if (!selfNeedsLayout()) {
                        ASSERT(floatingObject.originatingLine()->getLineLayoutItem().isEqual(this));
                        floatingObject.originatingLine()->markDirty();
                    }
#if ENABLE(ASSERT)
                    floatingObject.setOriginatingLine(nullptr);
#endif
                }
                markLinesDirtyInBlockRange(LayoutUnit(), logicalBottom);
            }
            m_floatingObjects->remove(&floatingObject);
        }
    }
}

void LayoutBlockFlow::removeFloatingObjectsBelow(FloatingObject* lastFloat, int logicalOffset)
{
    if (!containsFloats())
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObject* curr = floatingObjectSet.last().get();
    while (curr != lastFloat && (!curr->isPlaced() || logicalTopForFloat(*curr) >= logicalOffset)) {
        m_floatingObjects->remove(curr);
        if (floatingObjectSet.isEmpty())
            break;
        curr = floatingObjectSet.last().get();
    }
}

bool LayoutBlockFlow::positionNewFloats(LineWidth* width)
{
    if (!m_floatingObjects)
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    if (floatingObjectSet.isEmpty())
        return false;

    // If all floats have already been positioned, then we have no work to do.
    if (floatingObjectSet.last()->isPlaced())
        return false;

    // Move backwards through our floating object list until we find a float that has
    // already been positioned. Then we'll be able to move forward, positioning all of
    // the new floats that need it.
    FloatingObjectSetIterator it = floatingObjectSet.end();
    --it; // Go to last item.
    FloatingObjectSetIterator begin = floatingObjectSet.begin();
    FloatingObject* lastPlacedFloatingObject = nullptr;
    while (it != begin) {
        --it;
        if ((*it)->isPlaced()) {
            lastPlacedFloatingObject = it->get();
            ++it;
            break;
        }
    }

    LayoutUnit logicalTop = logicalHeight();

    // The float cannot start above the top position of the last positioned float.
    if (lastPlacedFloatingObject)
        logicalTop = std::max(logicalTopForFloat(*lastPlacedFloatingObject), logicalTop);

    FloatingObjectSetIterator end = floatingObjectSet.end();
    // Now walk through the set of unpositioned floats and place them.
    for (; it != end; ++it) {
        FloatingObject& floatingObject = *it->get();
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        if (floatingObject.layoutObject()->containingBlock() != this)
            continue;

        LayoutBox* childBox = floatingObject.layoutObject();

        // FIXME Investigate if this can be removed. crbug.com/370006
        childBox->setMayNeedPaintInvalidation();

        LayoutUnit childLogicalLeftMargin = style()->isLeftToRightDirection() ? marginStartForChild(*childBox) : marginEndForChild(*childBox);
        if (childBox->style()->clear() & ClearLeft)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatLeft), logicalTop);
        if (childBox->style()->clear() & ClearRight)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatRight), logicalTop);

        bool isPaginated = view()->layoutState()->isPaginated();
        if (isPaginated && !childrenInline()) {
            // Forced breaks are inserted at class A break points. Floats may be affected by a
            // break-after value on the previous in-flow sibling.
            if (LayoutBox* previousInFlowBox = childBox->previousInFlowSiblingBox())
                logicalTop = applyForcedBreak(logicalTop, previousInFlowBox->breakAfter());
        }

        LayoutPoint floatLogicalLocation = computeLogicalLocationForFloat(floatingObject, logicalTop);

        setLogicalLeftForFloat(floatingObject, floatLogicalLocation.x());

        setLogicalLeftForChild(*childBox, floatLogicalLocation.x() + childLogicalLeftMargin);
        setLogicalTopForChild(*childBox, floatLogicalLocation.y() + marginBeforeForChild(*childBox));

        SubtreeLayoutScope layoutScope(*childBox);
        if (isPaginated && !childBox->needsLayout())
            childBox->markForPaginationRelayoutIfNeeded(layoutScope);

        childBox->layoutIfNeeded();

        if (isPaginated) {
            LayoutBlockFlow* childBlockFlow = childBox->isLayoutBlockFlow() ? toLayoutBlockFlow(childBox) : nullptr;
            // The first piece of content inside the child may have set a strut during layout.
            LayoutUnit strut = childBlockFlow ? childBlockFlow->paginationStrutPropagatedFromChild() : LayoutUnit();
            if (!strut) {
                // Otherwise, if we are unsplittable and don't fit, move to the next page or column
                // if that helps the situation.
                strut = adjustForUnsplittableChild(*childBox, floatLogicalLocation.y()) - floatLogicalLocation.y();
            }

            childBox->setPaginationStrut(strut);
            if (strut) {
                floatLogicalLocation = computeLogicalLocationForFloat(floatingObject, floatLogicalLocation.y() + strut);
                setLogicalLeftForFloat(floatingObject, floatLogicalLocation.x());

                setLogicalLeftForChild(*childBox, floatLogicalLocation.x() + childLogicalLeftMargin);
                setLogicalTopForChild(*childBox, floatLogicalLocation.y() + marginBeforeForChild(*childBox));

                if (childBox->isLayoutBlock())
                    childBox->setChildNeedsLayout(MarkOnlyThis);
                childBox->layoutIfNeeded();
            }
        }

        setLogicalTopForFloat(floatingObject, floatLogicalLocation.y());

        setLogicalHeightForFloat(floatingObject, logicalHeightForChild(*childBox) + marginBeforeForChild(*childBox) + marginAfterForChild(*childBox));

        m_floatingObjects->addPlacedObject(floatingObject);

        if (ShapeOutsideInfo* shapeOutside = childBox->shapeOutsideInfo())
            shapeOutside->setReferenceBoxLogicalSize(logicalSizeForChild(*childBox));

        if (width)
            width->shrinkAvailableWidthForNewFloatIfNeeded(floatingObject);
    }
    return true;
}

bool LayoutBlockFlow::hasOverhangingFloat(LayoutBox* layoutBox)
{
    if (!m_floatingObjects || !parent())
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator it = floatingObjectSet.find<FloatingObjectHashTranslator>(layoutBox);
    if (it == floatingObjectSet.end())
        return false;

    return isOverhangingFloat(**it);
}

void LayoutBlockFlow::addIntrudingFloats(LayoutBlockFlow* prev, LayoutUnit logicalLeftOffset, LayoutUnit logicalTopOffset)
{
    ASSERT(!avoidsFloats());

    // If we create our own block formatting context then our contents don't interact with floats outside it, even those from our parent.
    if (createsNewFormattingContext())
        return;

    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!prev->m_floatingObjects)
        return;

    logicalLeftOffset += marginLogicalLeft();

    const FloatingObjectSet& prevSet = prev->m_floatingObjects->set();
    FloatingObjectSetIterator prevEnd = prevSet.end();
    for (FloatingObjectSetIterator prevIt = prevSet.begin(); prevIt != prevEnd; ++prevIt) {
        FloatingObject& floatingObject = *prevIt->get();
        if (logicalBottomForFloat(floatingObject) > logicalTopOffset) {
            if (!m_floatingObjects || !m_floatingObjects->set().contains(&floatingObject)) {
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since this margin was added already through the modification of the |logicalLeftOffset| variable
                // above. |logicalLeftOffset| will equal the margin in this case, so it's already been taken
                // into account. Only apply this code if prev is the parent, since otherwise the left margin
                // will get applied twice.
                LayoutSize offset = isHorizontalWritingMode()
                    ? LayoutSize(logicalLeftOffset - (prev != parent() ? prev->marginLeft() : LayoutUnit()), logicalTopOffset)
                    : LayoutSize(logicalTopOffset, logicalLeftOffset - (prev != parent() ? prev->marginTop() : LayoutUnit()));

                m_floatingObjects->add(floatingObject.copyToNewContainer(offset));
            }
        }
    }
}

void LayoutBlockFlow::addOverhangingFloats(LayoutBlockFlow* child, bool makeChildPaintOtherFloats)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (!child->containsFloats() || child->createsNewFormattingContext())
        return;

    LayoutUnit childLogicalTop = child->logicalTop();
    LayoutUnit childLogicalLeft = child->logicalLeft();

    // Floats that will remain the child's responsibility to paint should factor into its
    // overflow.
    FloatingObjectSetIterator childEnd = child->m_floatingObjects->set().end();
    for (FloatingObjectSetIterator childIt = child->m_floatingObjects->set().begin(); childIt != childEnd; ++childIt) {
        FloatingObject& floatingObject = *childIt->get();
        LayoutUnit logicalBottomForFloat = std::min(this->logicalBottomForFloat(floatingObject), LayoutUnit::max() - childLogicalTop);
        LayoutUnit logicalBottom = childLogicalTop + logicalBottomForFloat;

        if (logicalBottom > logicalHeight()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(floatingObject.layoutObject())) {
                LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(-childLogicalLeft, -childLogicalTop) : LayoutSize(-childLogicalTop, -childLogicalLeft);
                bool shouldPaint = false;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly). We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a self-painting layer boundary.
                if (floatingObject.layoutObject()->enclosingFloatPaintingLayer() == enclosingFloatPaintingLayer() && !floatingObject.isLowestNonOverhangingFloatInChild()) {
                    floatingObject.setShouldPaint(false);
                    shouldPaint = true;
                }
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                m_floatingObjects->add(floatingObject.copyToNewContainer(offset, shouldPaint, true));
            }
        } else {
            if (makeChildPaintOtherFloats && !floatingObject.shouldPaint() && !floatingObject.layoutObject()->hasSelfPaintingLayer() && !floatingObject.isLowestNonOverhangingFloatInChild()
                && floatingObject.layoutObject()->isDescendantOf(child) && floatingObject.layoutObject()->enclosingFloatPaintingLayer() == child->enclosingFloatPaintingLayer()) {
                // The float is not overhanging from this block, so if it is a descendant of the child, the child should
                // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
                // layer.
                // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
                // it should paint.
                floatingObject.setShouldPaint(true);
            }

            // Since the float doesn't overhang, it didn't get put into our list. We need to go ahead and add its overflow in to the
            // child now.
            if (floatingObject.isDescendant())
                child->addOverflowFromChild(floatingObject.layoutObject(), LayoutSize(xPositionForFloatIncludingMargin(floatingObject), yPositionForFloatIncludingMargin(floatingObject)));
        }
    }
}

LayoutUnit LayoutBlockFlow::lowestFloatLogicalBottom(FloatingObject::Type floatType) const
{
    if (!m_floatingObjects)
        return LayoutUnit();

    return m_floatingObjects->lowestFloatLogicalBottom(floatType);
}

LayoutUnit LayoutBlockFlow::nextFloatLogicalBottomBelow(LayoutUnit logicalHeight) const
{
    if (!m_floatingObjects)
        return logicalHeight;
    return m_floatingObjects->findNextFloatLogicalBottomBelow(logicalHeight);
}

LayoutUnit LayoutBlockFlow::nextFloatLogicalBottomBelowForBlock(LayoutUnit logicalHeight) const
{
    if (!m_floatingObjects)
        return logicalHeight;

    return m_floatingObjects->findNextFloatLogicalBottomBelowForBlock(logicalHeight);
}

Node* LayoutBlockFlow::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    return isAnonymousBlockContinuation() ? continuation()->node() : node();
}

bool LayoutBlockFlow::hitTestChildren(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint scrolledOffset(hasOverflowClip() ? accumulatedOffset - scrolledContentOffset() : accumulatedOffset);
    if (childrenInline()) {
        if (m_lineBoxes.hitTest(LineLayoutBoxModel(this), result, locationInContainer, scrolledOffset, hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - accumulatedOffset)));
            return true;
        }
    } else if (LayoutBlock::hitTestChildren(result, locationInContainer, accumulatedOffset, hitTestAction)) {
        return true;
    }

    if (hitTestAction == HitTestFloat && hitTestFloats(result, locationInContainer, scrolledOffset))
        return true;

    return false;
}

bool LayoutBlockFlow::hitTestFloats(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!m_floatingObjects)
        return false;

    LayoutPoint adjustedLocation = accumulatedOffset;
    if (isLayoutView()) {
        DoublePoint position = toLayoutView(this)->frameView()->scrollPositionDouble();
        adjustedLocation.move(position.x(), position.y());
    }

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator begin = floatingObjectSet.begin();
    for (FloatingObjectSetIterator it = floatingObjectSet.end(); it != begin;) {
        --it;
        const FloatingObject& floatingObject = *it->get();
        if (floatingObject.shouldPaint()) {
            LayoutUnit xOffset = xPositionForFloatIncludingMargin(floatingObject) - floatingObject.layoutObject()->location().x();
            LayoutUnit yOffset = yPositionForFloatIncludingMargin(floatingObject) - floatingObject.layoutObject()->location().y();
            LayoutPoint childPoint = flipFloatForWritingModeForChild(floatingObject, adjustedLocation + LayoutSize(xOffset, yOffset));
            if (floatingObject.layoutObject()->hitTest(result, locationInContainer, childPoint)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(childPoint));
                return true;
            }
        }
    }

    return false;
}

LayoutSize LayoutBlockFlow::accumulateInFlowPositionOffsets() const
{
    if (!isAnonymousBlock() || !isInFlowPositioned())
        return LayoutSize();
    LayoutSize offset;
    for (const LayoutObject* p = inlineElementContinuation(); p && p->isLayoutInline(); p = p->parent()) {
        if (p->isInFlowPositioned())
            offset += toLayoutInline(p)->offsetForInFlowPosition();
    }
    return offset;
}

LayoutUnit LayoutBlockFlow::logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const
{
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        return m_floatingObjects->logicalLeftOffset(fixedOffset, logicalTop, logicalHeight);

    return fixedOffset;
}

LayoutUnit LayoutBlockFlow::logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const
{
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        return m_floatingObjects->logicalRightOffset(fixedOffset, logicalTop, logicalHeight);

    return fixedOffset;
}

void LayoutBlockFlow::setAncestorShouldPaintFloatingObject(const LayoutBox& floatBox, bool shouldPaint)
{
    ASSERT(floatBox.isFloating());
    ASSERT(!floatBox.hasSelfPaintingLayer());
    for (LayoutObject* ancestor = floatBox.parent(); ancestor && ancestor->isLayoutBlockFlow(); ancestor = ancestor->parent()) {
        LayoutBlockFlow* ancestorBlock = toLayoutBlockFlow(ancestor);
        FloatingObjects* ancestorFloatingObjects = ancestorBlock->m_floatingObjects.get();
        if (!ancestorFloatingObjects)
            break;
        FloatingObjectSet::iterator it = ancestorFloatingObjects->mutableSet().find<FloatingObjectHashTranslator>(const_cast<LayoutBox*>(&floatBox));
        if (it == ancestorFloatingObjects->mutableSet().end())
            break;

        FloatingObject& floatingObject = **it;
        if (shouldPaint) {
            ASSERT(!floatingObject.shouldPaint());
            // This repeats the logic in addOverhangingFloats() about shouldPaint flag:
            // - The nearest enclosing block in which the float doesn't overhang paints the float;
            // - Or even if the float overhangs, if the ancestor block has self-painting layer, it
            //   paints the float.
            if (ancestorBlock->hasSelfPaintingLayer() || !ancestorBlock->isOverhangingFloat(floatingObject)) {
                floatingObject.setShouldPaint(true);
                return;
            }
        } else if (floatingObject.shouldPaint()) {
            floatingObject.setShouldPaint(false);
            return;
        }
    }
    // We should have found the ancestor to update shouldPaint flag.
    ASSERT_NOT_REACHED();
}

bool LayoutBlockFlow::allowsPaginationStrut() const
{
    // The block needs to be contained by a LayoutBlockFlow (and not by e.g. a flexbox, grid, or a
    // table (the latter being the case for table cell or table caption)). The reason for this
    // limitation is simply that LayoutBlockFlow child layout code is the only place where we pick
    // up the struts and handle them. We handle floats and regular in-flow children, and that's
    // all. We could handle this in other layout modes as well (and even for out-of-flow children),
    // but currently we don't.
    // TODO(mstensho): But we *should*.
    if (isOutOfFlowPositioned())
        return false;
    if (isLayoutFlowThread()) {
        // Don't let the strut escape the fragmentation context and get lost.
        // TODO(mstensho): If we're in a nested fragmentation context, we should ideally convert
        // and propagate the strut to the outer fragmentation context, so that the inner one is
        // fully pushed to the next outer fragmentainer, instead of taking up unusable space in the
        // previous one. But currently we have no mechanism in place to handle this.
        return false;
    }
    LayoutBlock* containingBlock = this->containingBlock();
    return containingBlock && containingBlock->isLayoutBlockFlow();
}

void LayoutBlockFlow::setPaginationStrutPropagatedFromChild(LayoutUnit strut)
{
    strut = std::max(strut, LayoutUnit());
    if (!m_rareData) {
        if (!strut)
            return;
        m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));
    }
    m_rareData->m_paginationStrutPropagatedFromChild = strut;
}

void LayoutBlockFlow::positionSpannerDescendant(LayoutMultiColumnSpannerPlaceholder& child)
{
    LayoutBox& spanner = *child.layoutObjectInFlowThread();
    // FIXME: |spanner| is a descendant, but never a direct child, so the names here are bad, if
    // nothing else.
    setLogicalTopForChild(spanner, child.logicalTop());
    determineLogicalLeftPositionForChild(spanner);
}

bool LayoutBlockFlow::avoidsFloats() const
{
    // Floats can't intrude into our box if we have a non-auto column count or width.
    // Note: we need to use LayoutBox::avoidsFloats here since LayoutBlock::avoidsFloats is always true.
    return LayoutBox::avoidsFloats() || !style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth();
}

void LayoutBlockFlow::moveChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* startChild, LayoutObject* endChild, LayoutObject* beforeChild, bool fullRemoveInsert)
{
    if (childrenInline())
        deleteLineBoxTree();
    LayoutBoxModelObject::moveChildrenTo(toBoxModelObject, startChild, endChild, beforeChild, fullRemoveInsert);
}

LayoutUnit LayoutBlockFlow::logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    LayoutUnit logicalLeft = logicalLeftOffsetForLine(position, DoNotIndentText);
    if (logicalLeft == logicalLeftOffsetForContent())
        return LayoutBlock::logicalLeftSelectionOffset(rootBlock, position);

    const LayoutBlock* cb = this;
    while (cb != rootBlock) {
        logicalLeft += cb->logicalLeft();
        cb = cb->containingBlock();
    }
    return logicalLeft;
}

LayoutUnit LayoutBlockFlow::logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    LayoutUnit logicalRight = logicalRightOffsetForLine(position, DoNotIndentText);
    if (logicalRight == logicalRightOffsetForContent())
        return LayoutBlock::logicalRightSelectionOffset(rootBlock, position);

    const LayoutBlock* cb = this;
    while (cb != rootBlock) {
        logicalRight += cb->logicalLeft();
        cb = cb->containingBlock();
    }
    return logicalRight;
}

RootInlineBox* LayoutBlockFlow::createRootInlineBox()
{
    return new RootInlineBox(LineLayoutItem(this));
}

bool LayoutBlockFlow::isPagedOverflow(const ComputedStyle& style)
{
    return style.isOverflowPaged() && node() != document().viewportDefiningElement();
}

LayoutBlockFlow::FlowThreadType LayoutBlockFlow::getFlowThreadType(const ComputedStyle& style)
{
    if (isPagedOverflow(style))
        return PagedFlowThread;
    if (style.specifiesColumns())
        return MultiColumnFlowThread;
    return NoFlowThread;
}

LayoutMultiColumnFlowThread* LayoutBlockFlow::createMultiColumnFlowThread(FlowThreadType type)
{
    switch (type) {
    case MultiColumnFlowThread:
        return LayoutMultiColumnFlowThread::createAnonymous(document(), styleRef());
    case PagedFlowThread:
        // Paged overflow is currently done using the multicol implementation.
        return LayoutPagedFlowThread::createAnonymous(document(), styleRef());
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

void LayoutBlockFlow::createOrDestroyMultiColumnFlowThreadIfNeeded(const ComputedStyle* oldStyle)
{
    // Paged overflow trumps multicol in this implementation. Ideally, it should be possible to have
    // both paged overflow and multicol on the same element, but then we need two flow
    // threads. Anyway, this is nothing to worry about until we can actually nest multicol properly
    // inside other fragmentation contexts.
    FlowThreadType type = getFlowThreadType(styleRef());

    if (multiColumnFlowThread()) {
        ASSERT(oldStyle);
        if (type != getFlowThreadType(*oldStyle)) {
            // If we're no longer to be multicol/paged, destroy the flow thread. Also destroy it
            // when switching between multicol and paged, since that affects the column set
            // structure (multicol containers may have spanners, paged containers may not).
            multiColumnFlowThread()->evacuateAndDestroy();
            ASSERT(!multiColumnFlowThread());
        }
    }

    if (type == NoFlowThread || multiColumnFlowThread())
        return;

    // Ruby elements manage child insertion in a special way, and would mess up insertion of the
    // flow thread. The flow thread needs to be a direct child of the multicol block (|this|).
    if (isRuby())
        return;

    // Fieldsets look for a legend special child (layoutSpecialExcludedChild()). We currently only
    // support one special child per layout object, and the flow thread would make for a second one.
    if (isFieldset())
        return;

    // Form controls are replaced content, and are therefore not supposed to support multicol.
    if (isFileUploadControl() || isTextControl() || isListBox())
        return;

    LayoutMultiColumnFlowThread* flowThread = createMultiColumnFlowThread(type);
    addChild(flowThread);

    // Check that addChild() put the flow thread as a direct child, and didn't do fancy things.
    ASSERT(flowThread->parent() == this);

    flowThread->populate();
    LayoutBlockFlowRareData& rareData = ensureRareData();
    ASSERT(!rareData.m_multiColumnFlowThread);
    rareData.m_multiColumnFlowThread = flowThread;
}

LayoutBlockFlow::LayoutBlockFlowRareData& LayoutBlockFlow::ensureRareData()
{
    if (m_rareData)
        return *m_rareData;

    m_rareData = wrapUnique(new LayoutBlockFlowRareData(this));
    return *m_rareData;
}

void LayoutBlockFlow::positionDialog()
{
    HTMLDialogElement* dialog = toHTMLDialogElement(node());
    if (dialog->getCenteringMode() == HTMLDialogElement::NotCentered)
        return;

    bool canCenterDialog = (style()->position() == AbsolutePosition || style()->position() == FixedPosition)
        && style()->hasAutoTopAndBottom();

    if (dialog->getCenteringMode() == HTMLDialogElement::Centered) {
        if (canCenterDialog)
            setY(dialog->centeredPosition());
        return;
    }

    ASSERT(dialog->getCenteringMode() == HTMLDialogElement::NeedsCentering);
    if (!canCenterDialog) {
        dialog->setNotCentered();
        return;
    }

    FrameView* frameView = document().view();
    LayoutUnit top = LayoutUnit((style()->position() == FixedPosition) ? 0 : frameView->scrollOffset().height());
    int visibleHeight = frameView->visibleContentRect(IncludeScrollbars).height();
    if (size().height() < visibleHeight)
        top += (visibleHeight - size().height()) / 2;
    setY(top);
    dialog->setCentered(top);
}

void LayoutBlockFlow::simplifiedNormalFlowInlineLayout()
{
    ASSERT(childrenInline());
    ListHashSet<RootInlineBox*> lineBoxes;
    for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.atEnd(); walker.advance()) {
        LayoutObject* o = walker.current().layoutObject();
        if (!o->isOutOfFlowPositioned() && (o->isAtomicInlineLevel() || o->isFloating())) {
            o->layoutIfNeeded();
            if (toLayoutBox(o)->inlineBoxWrapper()) {
                RootInlineBox& box = toLayoutBox(o)->inlineBoxWrapper()->root();
                lineBoxes.add(&box);
            }
        } else if (o->isText() || (o->isLayoutInline() && !walker.atEndOfInline())) {
            o->clearNeedsLayout();
        }
    }

    // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
        RootInlineBox* box = *it;
        box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
    }
}

bool LayoutBlockFlow::recalcInlineChildrenOverflowAfterStyleChange()
{
    ASSERT(childrenInline());
    bool childrenOverflowChanged = false;
    ListHashSet<RootInlineBox*> lineBoxes;
    for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.atEnd(); walker.advance()) {
        LayoutObject* layoutObject = walker.current().layoutObject();
        if (recalcNormalFlowChildOverflowIfNeeded(layoutObject)) {
            childrenOverflowChanged = true;
            if (InlineBox* inlineBoxWrapper = toLayoutBlock(layoutObject)->inlineBoxWrapper())
                lineBoxes.add(&inlineBoxWrapper->root());
        }
    }

    // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
        RootInlineBox* box = *it;
        box->clearKnownToHaveNoOverflow();
        box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
    }
    return childrenOverflowChanged;
}

PositionWithAffinity LayoutBlockFlow::positionForPoint(const LayoutPoint& point)
{
    if (isAtomicInlineLevel()) {
        PositionWithAffinity position = positionForPointIfOutsideAtomicInlineLevel(point);
        if (!position.isNull())
            return position;
    }
    if (!childrenInline())
        return LayoutBlock::positionForPoint(point);

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (!firstRootBox())
        return createPositionWithAffinity(0);

    bool linesAreFlipped = style()->isFlippedLinesWritingMode();
    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineBox* closestBox = nullptr;
    RootInlineBox* firstRootBoxWithChildren = nullptr;
    RootInlineBox* lastRootBoxWithChildren = nullptr;
    for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
        if (!root->firstLeafChild())
            continue;
        if (!firstRootBoxWithChildren)
            firstRootBoxWithChildren = root;

        if (!linesAreFlipped && root->isFirstAfterPageBreak() && (pointInLogicalContents.y() < root->lineTopWithLeading()
            || (blocksAreFlipped && pointInLogicalContents.y() == root->lineTopWithLeading())))
            break;

        lastRootBoxWithChildren = root;

        // check if this root line box is located at this y coordinate
        if (pointInLogicalContents.y() < root->selectionBottom() || (blocksAreFlipped && pointInLogicalContents.y() == root->selectionBottom())) {
            if (linesAreFlipped) {
                RootInlineBox* nextRootBoxWithChildren = root->nextRootBox();
                while (nextRootBoxWithChildren && !nextRootBoxWithChildren->firstLeafChild())
                    nextRootBoxWithChildren = nextRootBoxWithChildren->nextRootBox();

                if (nextRootBoxWithChildren && nextRootBoxWithChildren->isFirstAfterPageBreak() && (pointInLogicalContents.y() > nextRootBoxWithChildren->lineTopWithLeading()
                    || (!blocksAreFlipped && pointInLogicalContents.y() == nextRootBoxWithChildren->lineTopWithLeading())))
                    continue;
            }
            closestBox = root->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
            if (closestBox)
                break;
        }
    }

    bool moveCaretToBoundary = document().frame()->editor().behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastRootBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = lastRootBoxWithChildren->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            LayoutUnit firstRootBoxWithChildrenTop = std::min<LayoutUnit>(firstRootBoxWithChildren->selectionTop(), firstRootBoxWithChildren->logicalTop());
            if (pointInLogicalContents.y() < firstRootBoxWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstRootBoxWithChildrenTop)) {
                InlineBox* box = firstRootBoxWithChildren->firstLeafChild();
                if (box->isLineBreak()) {
                    if (InlineBox* newBox = box->nextLeafChildIgnoringLineBreak())
                        box = newBox;
                }
                // y coordinate is above first root line box, so return the start of the first
                return PositionWithAffinity(positionForBox(box, true));
            }
        }

        // pass the box a top position that is inside it
        LayoutPoint point(pointInLogicalContents.x(), closestBox->root().blockDirectionPointInLine());
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->getLineLayoutItem().isAtomicInlineLevel())
            return positionForPointRespectingEditingBoundaries(LineLayoutBox(closestBox->getLineLayoutItem()), point);
        return closestBox->getLineLayoutItem().positionForPoint(point);
    }

    if (lastRootBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineBox* logicallyLastBox;
        if (lastRootBoxWithChildren->getLogicalEndBoxWithNode(logicallyLastBox))
            return PositionWithAffinity(positionForBox(logicallyLastBox, false));
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createPositionWithAffinity(0);
}

#ifndef NDEBUG

void LayoutBlockFlow::showLineTreeAndMark(const InlineBox* markedBox1, const char* markedLabel1, const InlineBox* markedBox2, const char* markedLabel2, const LayoutObject* obj) const
{
    showLayoutObject();
    for (const RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->showLineTreeAndMark(markedBox1, markedLabel1, markedBox2, markedLabel2, obj, 1);
}

#endif

void LayoutBlockFlow::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot includeBlockOverflows) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    const LayoutInline* inlineElementContinuation = this->inlineElementContinuation();
    if (inlineElementContinuation) {
        // FIXME: This check really isn't accurate.
        bool nextInlineHasLineBox = inlineElementContinuation->firstLineBox();
        // FIXME: This is wrong. The principal layoutObject may not be the continuation preceding this block.
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = toLayoutInline(inlineElementContinuation->node()->layoutObject())->firstLineBox();
        LayoutUnit topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        LayoutUnit bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        if (topMargin || bottomMargin) {
            LayoutRect rect(additionalOffset, size());
            rect.expandEdges(topMargin, LayoutUnit(), bottomMargin, LayoutUnit());
            rects.append(rect);
        }
    }

    LayoutBlock::addOutlineRects(rects, additionalOffset, includeBlockOverflows);

    if (includeBlockOverflows == IncludeBlockVisualOverflow && !hasOverflowClip() && !hasControlClip()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
            if (!rect.isEmpty())
                rects.append(rect);
        }
    }

    if (inlineElementContinuation)
        inlineElementContinuation->addOutlineRects(rects, additionalOffset + (inlineElementContinuation->containingBlock()->location() - location()), includeBlockOverflows);
}

} // namespace blink
