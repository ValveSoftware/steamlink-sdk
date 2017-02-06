/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef LayoutBlockFlow_h
#define LayoutBlockFlow_h

#include "core/CoreExport.h"
#include "core/layout/FloatingObjects.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/LineBoxList.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/layout/line/TrailingObjects.h"
#include <memory>

namespace blink {

class BlockChildrenLayoutInfo;
class MarginInfo;
class LayoutInline;
class LineInfo;
class LineLayoutState;
class LineWidth;
class LayoutMultiColumnFlowThread;
class LayoutMultiColumnSpannerPlaceholder;
class LayoutRubyRun;
template <class Run> class BidiRunList;

enum IndentTextOrNot { DoNotIndentText, IndentText };

// LayoutBlockFlow is the class that implements a block container in CSS 2.1.
// http://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// LayoutBlockFlows are the only LayoutObject allowed to own floating objects
// (aka floats): http://www.w3.org/TR/CSS21/visuren.html#floats .
//
// Floats are inserted into |m_floatingObjects| (see FloatingObjects for more
// information on how floats are modelled) during layout. This happens either as
// part of laying out blocks (layoutBlockChildren) or line layout (LineBreaker
// class). This is because floats can be part of an inline or a block context.
//
// An interesting feature of floats is that they can intrude into the next
// block(s). This means that |m_floatingObjects| can potentially contain
// pointers to a previous sibling LayoutBlockFlow's float.
//
// LayoutBlockFlow is also the only LayoutObject to own a line box tree and
// perform inline layout. See LayoutBlockFlowLine.cpp for these parts.
//
// TODO(jchaffraix): We need some float and line box expert to expand on this.
//
// LayoutBlockFlow enforces the following invariant:
//
// All in-flow children (ie excluding floating and out-of-flow positioned) are
// either all blocks or all inline boxes.
//
// This is suggested by CSS to correctly the layout mixed inlines and blocks
// lines (http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level). See
// LayoutBlock::addChild about how the invariant is enforced.
class CORE_EXPORT LayoutBlockFlow : public LayoutBlock {
public:
    explicit LayoutBlockFlow(ContainerNode*);
    ~LayoutBlockFlow() override;

    static LayoutBlockFlow* createAnonymous(Document*);
    bool beingDestroyed() const { return m_beingDestroyed; }

    bool isLayoutBlockFlow() const final { return true; }

    void layoutBlock(bool relayoutChildren) override;

    void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) override;

    void deleteLineBoxTree();

    LayoutUnit availableLogicalWidthForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return (logicalRightOffsetForLine(position, indentText, logicalHeight) - logicalLeftOffsetForLine(position, indentText, logicalHeight)).clampNegativeToZero();
    }
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(), indentText, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(), indentText, logicalHeight);
    }
    LayoutUnit startOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return style()->isLeftToRightDirection() ? logicalLeftOffsetForLine(position, indentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, indentText, logicalHeight);
    }
    LayoutUnit endOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return !style()->isLeftToRightDirection() ? logicalLeftOffsetForLine(position, indentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, indentText, logicalHeight);
    }

    const LineBoxList& lineBoxes() const { return m_lineBoxes; }
    LineBoxList* lineBoxes() { return &m_lineBoxes; }
    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(firstLineBox()); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(lastLineBox()); }

    LayoutUnit logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const override;
    LayoutUnit logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const override;

    RootInlineBox* createAndAppendRootInlineBox();

    // Return the number of lines in *this* block flow. Does not recurse into block flow children.
    // Will start counting from the first line, and stop counting right after |stopRootInlineBox|,
    // if specified.
    int lineCount(const RootInlineBox* stopRootInlineBox = nullptr) const;

    int firstLineBoxBaseline() const override;
    int inlineBlockBaseline(LineDirectionMode) const override;

    void removeFloatingObjectsFromDescendants();
    void markAllDescendantsWithFloatsForLayout(LayoutBox* floatToRemove = nullptr, bool inLayout = true);
    void markSiblingsWithFloatsForLayout(LayoutBox* floatToRemove = nullptr);

    bool containsFloats() const { return m_floatingObjects && !m_floatingObjects->set().isEmpty(); }
    bool containsFloat(LayoutBox*) const;

    void removeFloatingObjects();

    LayoutBoxModelObject* virtualContinuation() const final { return continuation(); }
    bool isAnonymousBlockContinuation() const { return continuation() && isAnonymousBlock(); }

    using LayoutBoxModelObject::continuation;
    using LayoutBoxModelObject::setContinuation;

    LayoutInline* inlineElementContinuation() const;

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;

    void moveAllChildrenIncludingFloatsTo(LayoutBlock* toBlock, bool fullRemoveInsert);

    void childBecameFloatingOrOutOfFlow(LayoutBox* child);
    void collapseAnonymousBlockChild(LayoutBlockFlow* child);

    bool generatesLineBoxesForInlineChild(LayoutObject*);

    LayoutUnit logicalTopForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.y() : floatingObject.x(); }
    LayoutUnit logicalBottomForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.maxY() : floatingObject.maxX(); }
    LayoutUnit logicalLeftForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.x() : floatingObject.y(); }
    LayoutUnit logicalRightForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.maxX() : floatingObject.maxY(); }
    LayoutUnit logicalWidthForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.width() : floatingObject.height(); }

    void setLogicalTopForFloat(FloatingObject& floatingObject, LayoutUnit logicalTop)
    {
        if (isHorizontalWritingMode())
            floatingObject.setY(logicalTop);
        else
            floatingObject.setX(logicalTop);
    }
    void setLogicalLeftForFloat(FloatingObject& floatingObject, LayoutUnit logicalLeft)
    {
        if (isHorizontalWritingMode())
            floatingObject.setX(logicalLeft);
        else
            floatingObject.setY(logicalLeft);
    }
    void setLogicalHeightForFloat(FloatingObject& floatingObject, LayoutUnit logicalHeight)
    {
        if (isHorizontalWritingMode())
            floatingObject.setHeight(logicalHeight);
        else
            floatingObject.setWidth(logicalHeight);
    }
    void setLogicalWidthForFloat(FloatingObject& floatingObject, LayoutUnit logicalWidth)
    {
        if (isHorizontalWritingMode())
            floatingObject.setWidth(logicalWidth);
        else
            floatingObject.setHeight(logicalWidth);
    }

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot);

    void setStaticInlinePositionForChild(LayoutBox&, LayoutUnit inlinePosition);
    void updateStaticInlinePositionForChild(LayoutBox&, LayoutUnit logicalTop, IndentTextOrNot = DoNotIndentText);

    static bool shouldSkipCreatingRunsForObject(LineLayoutItem obj)
    {
        return obj.isFloating() || (obj.isOutOfFlowPositioned() && !obj.style()->isOriginalDisplayInlineType() && !obj.container().isLayoutInline());
    }

    LayoutMultiColumnFlowThread* multiColumnFlowThread() const { return m_rareData ? m_rareData->m_multiColumnFlowThread : 0; }
    void resetMultiColumnFlowThread()
    {
        if (m_rareData)
            m_rareData->m_multiColumnFlowThread = nullptr;
    }

    void addOverflowFromInlineChildren();

    // FIXME: This should be const to avoid a const_cast, but can modify child dirty bits and LayoutTextCombine
    void computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth);

    bool allowsPaginationStrut() const;
    // Pagination strut caused by the first line or child block inside this block-level object.
    //
    // When the first piece of content (first child block or line) inside an object wants to insert
    // a soft page or column break, rather than setting a pagination strut on itself it normally
    // propagates the strut to its containing block (|this|), as long as our implementation can
    // handle it. The idea is that we want to push the entire object to the next page or column
    // along with the child content that caused the break, instead of leaving unusable space at the
    // beginning of the object at the end of one column or page and just push the first line or
    // block to the next column or page. That would waste space in the container for no good
    // reason, and it would also be a spec violation, since there is no break opportunity defined
    // between the content logical top of an object and its first child or line (only *between*
    // blocks or lines).
    LayoutUnit paginationStrutPropagatedFromChild() const { return m_rareData ? m_rareData->m_paginationStrutPropagatedFromChild : LayoutUnit(); }
    void setPaginationStrutPropagatedFromChild(LayoutUnit);

    void positionSpannerDescendant(LayoutMultiColumnSpannerPlaceholder& child);

    bool avoidsFloats() const override;

    using LayoutBoxModelObject::moveChildrenTo;
    void moveChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* startChild, LayoutObject* endChild, LayoutObject* beforeChild, bool fullRemoveInsert = false) override;

    LayoutUnit xPositionForFloatIncludingMargin(const FloatingObject& child) const
    {
        if (isHorizontalWritingMode())
            return child.x() + child.layoutObject()->marginLeft();

        return child.x() + marginBeforeForChild(*child.layoutObject());
    }

    LayoutUnit yPositionForFloatIncludingMargin(const FloatingObject& child) const
    {
        if (isHorizontalWritingMode())
            return child.y() + marginBeforeForChild(*child.layoutObject());

        return child.y() + child.layoutObject()->marginTop();
    }

    LayoutPoint flipFloatForWritingModeForChild(const FloatingObject&, const LayoutPoint&) const;

    const char* name() const override { return "LayoutBlockFlow"; }

    FloatingObject* insertFloatingObject(LayoutBox&);

    // Called from lineWidth, to position the floats added in the last line.
    // Returns true if and only if it has positioned any floats.
    bool positionNewFloats(LineWidth* = nullptr);

    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit) const;
    LayoutUnit nextFloatLogicalBottomBelowForBlock(LayoutUnit) const;

    FloatingObject* lastFloatFromPreviousLine() const
    {
        return containsFloats() ? m_floatingObjects->set().last().get() : nullptr;
    }

    void setShouldDoFullPaintInvalidationForFirstLine();

    void simplifiedNormalFlowInlineLayout();
    bool recalcInlineChildrenOverflowAfterStyleChange();

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    LayoutUnit lowestFloatLogicalBottom(FloatingObject::Type = FloatingObject::FloatLeftRight) const;

#ifndef NDEBUG
    void showLineTreeAndMark(const InlineBox* = nullptr, const char* = nullptr, const InlineBox* = nullptr, const char* = nullptr, const LayoutObject* = nullptr) const;
#endif

protected:
    void rebuildFloatsFromIntruding();
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit afterEdge);
    void addLowestFloatFromChildren(LayoutBlockFlow*);

    void createFloatingObjects();

    void willBeDestroyed() override;
    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox&);

    void addOverflowFromFloats();

    void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const override;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&) const override;
    LayoutObject* hoverAncestor() const final;
    void updateDragState(bool dragOn) final;

    LayoutUnit logicalRightOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, IndentTextOrNot applyTextIndent, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return adjustLogicalRightOffsetForLine(logicalRightFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, IndentTextOrNot applyTextIndent, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return adjustLogicalLeftOffsetForLine(logicalLeftFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }

    virtual LayoutObject* layoutSpecialExcludedChild(bool /*relayoutChildren*/, SubtreeLayoutScope&);
    bool updateLogicalWidthAndColumnWidth() override;

    void setLogicalLeftForChild(LayoutBox& child, LayoutUnit logicalLeft);
    void setLogicalTopForChild(LayoutBox& child, LayoutUnit logicalTop);
    void determineLogicalLeftPositionForChild(LayoutBox& child);

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const override;

    PaintInvalidationReason invalidatePaintIfNeeded(const PaintInvalidationState&) override;

    Node* nodeForHitTest() const final;
    bool hitTestChildren(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    LayoutSize accumulateInFlowPositionOffsets() const override;

private:
    bool layoutBlockFlow(bool relayoutChildren, LayoutUnit& pageLogicalHeight, SubtreeLayoutScope&);
    void layoutBlockChildren(bool relayoutChildren, SubtreeLayoutScope&, LayoutUnit beforeEdge, LayoutUnit afterEdge);

    void markDescendantsWithFloatsForLayoutIfNeeded(LayoutBlockFlow& child, LayoutUnit newLogicalTop, LayoutUnit previousFloatLogicalBottom);
    bool positionAndLayoutOnceIfNeeded(LayoutBox& child, LayoutUnit newLogicalTop, BlockChildrenLayoutInfo&);

    // Handle breaking policy before the child, and insert a forced break in front of it if needed.
    // Returns true if a forced break was inserted.
    bool insertForcedBreakBeforeChildIfNeeded(LayoutBox& child, BlockChildrenLayoutInfo&);

    void layoutBlockChild(LayoutBox& child, BlockChildrenLayoutInfo&);
    void adjustPositionedBlock(LayoutBox& child, const BlockChildrenLayoutInfo&);
    void adjustFloatingBlock(const MarginInfo&);

    LayoutPoint computeLogicalLocationForFloat(const FloatingObject&, LayoutUnit logicalTopOffset) const;

    void removeFloatingObject(LayoutBox*);
    void removeFloatingObjectsBelow(FloatingObject*, int logicalOffset);

    LayoutUnit getClearDelta(LayoutBox* child, LayoutUnit yPos);

    bool hasOverhangingFloats() { return parent() && containsFloats() && lowestFloatLogicalBottom() > logicalHeight(); }
    bool hasOverhangingFloat(LayoutBox*);
    void addIntrudingFloats(LayoutBlockFlow* prev, LayoutUnit xoffset, LayoutUnit yoffset);
    void addOverhangingFloats(LayoutBlockFlow* child, bool makeChildPaintOtherFloats);
    bool isOverhangingFloat(const FloatingObject& floatObject) const { return logicalBottomForFloat(floatObject) > logicalHeight(); }

    bool hitTestFloats(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

    void invalidatePaintForOverhangingFloats(bool paintAllDescendants) final;
    void invalidateDisplayItemClients(PaintInvalidationReason) const override;

    void clearFloats(EClear);

    LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const;
    LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const;

    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const;
    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const;

    LayoutUnit adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, IndentTextOrNot applyTextIndent) const;
    LayoutUnit adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, IndentTextOrNot applyTextIndent) const;

    virtual RootInlineBox* createRootInlineBox(); // Subclassed by SVG

    void dirtyLinesFromChangedChild(LayoutObject* child, MarkingBehavior markingBehaviour = MarkContainerChain) final { m_lineBoxes.dirtyLinesFromChangedChild(LineLayoutItem(this), LineLayoutItem(child), markingBehaviour == MarkContainerChain); }

    bool isPagedOverflow(const ComputedStyle&);

    enum FlowThreadType {
        NoFlowThread,
        MultiColumnFlowThread,
        PagedFlowThread
    };

    FlowThreadType getFlowThreadType(const ComputedStyle&);

    LayoutMultiColumnFlowThread* createMultiColumnFlowThread(FlowThreadType);
    void createOrDestroyMultiColumnFlowThreadIfNeeded(const ComputedStyle* oldStyle);

    // Merge children of |siblingThatMayBeDeleted| into this object if possible, and delete
    // |siblingThatMayBeDeleted|. Returns true if we were able to merge. In that case,
    // |siblingThatMayBeDeleted| will be dead. We'll only be able to merge if both blocks are
    // anonymous.
    bool mergeSiblingContiguousAnonymousBlock(LayoutBlockFlow* siblingThatMayBeDeleted);

    // Reparent subsequent or preceding adjacent floating or out-of-flow siblings into this object.
    void reparentSubsequentFloatingOrOutOfFlowSiblings();
    void reparentPrecedingFloatingOrOutOfFlowSiblings();

    void makeChildrenInlineIfPossible();

    void makeChildrenNonInline(LayoutObject* insertionPoint = nullptr);
    void childBecameNonInline(LayoutObject* child) final;

    void updateLogicalWidthForAlignment(const ETextAlign&, const RootInlineBox*, BidiRun* trailingSpaceRun, LayoutUnit& logicalLeft, LayoutUnit& totalLogicalWidth, LayoutUnit& availableLogicalWidth, unsigned expansionOpportunityCount);
    void checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight);

    bool shouldBreakAtLineToAvoidWidow() const { return m_rareData && m_rareData->m_lineBreakToAvoidWidow >= 0; }
    void clearShouldBreakAtLineToAvoidWidow() const;
    int lineBreakToAvoidWidow() const { return m_rareData ? m_rareData->m_lineBreakToAvoidWidow : -1; }
    void setBreakAtLineToAvoidWidow(int);
    void clearDidBreakAtLineToAvoidWidow();
    void setDidBreakAtLineToAvoidWidow();
    bool didBreakAtLineToAvoidWidow() const { return m_rareData && m_rareData->m_didBreakAtLineToAvoidWidow; }

public:
    struct FloatWithRect {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        FloatWithRect(LayoutBox* f)
            : object(f)
            , rect(f->frameRect())
            , everHadLayout(f->everHadLayout())
        {
            rect.expand(f->marginBoxOutsets());
        }

        LayoutBox* object;
        LayoutRect rect;
        bool everHadLayout;
    };

    // MarginValues holds the margins in the block direction
    // used during collapsing margins computation.
    // CSS mandates to keep track of both positive and negative margins:
    // "When two or more margins collapse, the resulting margin width is the
    // maximum of the collapsing margins' widths. In the case of negative
    // margins, the maximum of the absolute values of the negative adjoining
    // margins is deducted from the maximum of the positive adjoining margins.
    // If there are no positive margins, the maximum of the absolute values of
    // the adjoining margins is deducted from zero."
    // https://drafts.csswg.org/css2/box.html#collapsing-margins
    class MarginValues {
        DISALLOW_NEW();
    public:
        MarginValues(LayoutUnit beforePos, LayoutUnit beforeNeg, LayoutUnit afterPos, LayoutUnit afterNeg)
            : m_positiveMarginBefore(beforePos)
            , m_negativeMarginBefore(beforeNeg)
            , m_positiveMarginAfter(afterPos)
            , m_negativeMarginAfter(afterNeg)
        { }

        LayoutUnit positiveMarginBefore() const { return m_positiveMarginBefore; }
        LayoutUnit negativeMarginBefore() const { return m_negativeMarginBefore; }
        LayoutUnit positiveMarginAfter() const { return m_positiveMarginAfter; }
        LayoutUnit negativeMarginAfter() const { return m_negativeMarginAfter; }

        void setPositiveMarginBefore(LayoutUnit pos) { m_positiveMarginBefore = pos; }
        void setNegativeMarginBefore(LayoutUnit neg) { m_negativeMarginBefore = neg; }
        void setPositiveMarginAfter(LayoutUnit pos) { m_positiveMarginAfter = pos; }
        void setNegativeMarginAfter(LayoutUnit neg) { m_negativeMarginAfter = neg; }

    private:
        LayoutUnit m_positiveMarginBefore;
        LayoutUnit m_negativeMarginBefore;
        LayoutUnit m_positiveMarginAfter;
        LayoutUnit m_negativeMarginAfter;
    };
    MarginValues marginValuesForChild(LayoutBox& child) const;

    // Allocated only when some of these fields have non-default values
    struct LayoutBlockFlowRareData {
        WTF_MAKE_NONCOPYABLE(LayoutBlockFlowRareData); USING_FAST_MALLOC(LayoutBlockFlowRareData);
    public:
        LayoutBlockFlowRareData(const LayoutBlockFlow* block)
            : m_margins(positiveMarginBeforeDefault(block), negativeMarginBeforeDefault(block), positiveMarginAfterDefault(block), negativeMarginAfterDefault(block))
            , m_multiColumnFlowThread(nullptr)
            , m_breakBefore(BreakAuto)
            , m_breakAfter(BreakAuto)
            , m_lineBreakToAvoidWidow(-1)
            , m_didBreakAtLineToAvoidWidow(false)
            , m_discardMarginBefore(false)
            , m_discardMarginAfter(false)
        {
        }

        static LayoutUnit positiveMarginBeforeDefault(const LayoutBlockFlow* block)
        {
            return block->marginBefore().clampNegativeToZero();
        }
        static LayoutUnit negativeMarginBeforeDefault(const LayoutBlockFlow* block)
        {
            return (-block->marginBefore()).clampNegativeToZero();
        }
        static LayoutUnit positiveMarginAfterDefault(const LayoutBlockFlow* block)
        {
            return block->marginAfter().clampNegativeToZero();
        }
        static LayoutUnit negativeMarginAfterDefault(const LayoutBlockFlow* block)
        {
            return (-block->marginAfter()).clampNegativeToZero();
        }

        MarginValues m_margins;
        LayoutUnit m_paginationStrutPropagatedFromChild;

        LayoutMultiColumnFlowThread* m_multiColumnFlowThread;

        unsigned m_breakBefore : 4;
        unsigned m_breakAfter : 4;
        int m_lineBreakToAvoidWidow;
        bool m_didBreakAtLineToAvoidWidow : 1;
        bool m_discardMarginBefore : 1;
        bool m_discardMarginAfter : 1;
    };

    const FloatingObjects* floatingObjects() const { return m_floatingObjects.get(); }

    static void setAncestorShouldPaintFloatingObject(const LayoutBox& floatBox, bool shouldPaint);

protected:
    LayoutUnit maxPositiveMarginBefore() const { return m_rareData ? m_rareData->m_margins.positiveMarginBefore() : LayoutBlockFlowRareData::positiveMarginBeforeDefault(this); }
    LayoutUnit maxNegativeMarginBefore() const { return m_rareData ? m_rareData->m_margins.negativeMarginBefore() : LayoutBlockFlowRareData::negativeMarginBeforeDefault(this); }
    LayoutUnit maxPositiveMarginAfter() const { return m_rareData ? m_rareData->m_margins.positiveMarginAfter() : LayoutBlockFlowRareData::positiveMarginAfterDefault(this); }
    LayoutUnit maxNegativeMarginAfter() const { return m_rareData ? m_rareData->m_margins.negativeMarginAfter() : LayoutBlockFlowRareData::negativeMarginAfterDefault(this); }

    void setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
    void setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

    void setMustDiscardMarginBefore(bool = true);
    void setMustDiscardMarginAfter(bool = true);

    bool mustDiscardMarginBefore() const;
    bool mustDiscardMarginAfter() const;

    bool mustDiscardMarginBeforeForChild(const LayoutBox&) const;
    bool mustDiscardMarginAfterForChild(const LayoutBox&) const;

    bool mustSeparateMarginBeforeForChild(const LayoutBox&) const;
    bool mustSeparateMarginAfterForChild(const LayoutBox&) const;

    void initMaxMarginValues()
    {
        if (m_rareData) {
            m_rareData->m_margins = MarginValues(LayoutBlockFlowRareData::positiveMarginBeforeDefault(this) , LayoutBlockFlowRareData::negativeMarginBeforeDefault(this),
                LayoutBlockFlowRareData::positiveMarginAfterDefault(this), LayoutBlockFlowRareData::negativeMarginAfterDefault(this));

            m_rareData->m_discardMarginBefore = false;
            m_rareData->m_discardMarginAfter = false;
        }
    }

    virtual ETextAlign textAlignmentForLine(bool endsWithSoftBreak) const;
private:
    LayoutUnit collapsedMarginBefore() const final { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    LayoutUnit collapsedMarginAfter() const final { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    // Floats' margins do not collapse with page or column boundaries, and we therefore need to
    // treat them specially in some cases.
    LayoutUnit marginBeforeIfFloating() const { return isFloating() ? marginBefore() : LayoutUnit(); }

    LayoutUnit collapseMargins(LayoutBox& child, MarginInfo&, bool childIsSelfCollapsing, bool childDiscardMarginBefore, bool childDiscardMarginAfter);
    LayoutUnit clearFloatsIfNeeded(LayoutBox& child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos, bool childIsSelfCollapsing, bool childDiscardMargin);
    LayoutUnit estimateLogicalTopPosition(LayoutBox& child, const BlockChildrenLayoutInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(LayoutBox&, LayoutUnit&, LayoutUnit&, bool&) const;
    void handleAfterSideOfBlock(LayoutBox* lastChild, LayoutUnit top, LayoutUnit bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);

    // Apply any forced fragmentainer break that's set on the current class A break point.
    LayoutUnit applyForcedBreak(LayoutUnit logicalOffset, EBreak);

    void setBreakBefore(EBreak);
    void setBreakAfter(EBreak);
    EBreak breakBefore() const override;
    EBreak breakAfter() const override;

    LayoutUnit adjustBlockChildForPagination(LayoutUnit logicalTop, LayoutBox& child, BlockChildrenLayoutInfo&, bool atBeforeSideOfBlock);
    // Computes a deltaOffset value that put a line at the top of the next page if it doesn't fit on the current page.
    void adjustLinePositionForPagination(RootInlineBox&, LayoutUnit& deltaOffset);
    // If the child is unsplittable and can't fit on the current page, return the top of the next page/column.
    LayoutUnit adjustForUnsplittableChild(LayoutBox&, LayoutUnit logicalOffset) const;

    // Used to store state between styleWillChange and styleDidChange
    static bool s_canPropagateFloatIntoSibling;

    LineBoxList m_lineBoxes; // All of the root line boxes created for this block flow.  For example, <div>Hello<br>world.</div> will have two total lines for the <div>.

    LayoutBlockFlowRareData& ensureRareData();

    bool isSelfCollapsingBlock() const override;
    bool checkIfIsSelfCollapsingBlock() const;

protected:
    std::unique_ptr<LayoutBlockFlowRareData> m_rareData;
    std::unique_ptr<FloatingObjects> m_floatingObjects;

    friend class MarginInfo;
    friend class LineWidth; // needs to know FloatingObject

    // LayoutRubyBase objects need to be able to split and merge, moving their children around
    // (calling makeChildrenNonInline).
    // TODO(mstensho): Try to get rid of this friendship.
    friend class LayoutRubyBase;

// FIXME-BLOCKFLOW: These methods have implementations in
// LayoutBlockFlowLine. They should be moved to the proper header once the
// line layout code is separated from LayoutBlock and LayoutBlockFlow.
// START METHODS DEFINED IN LayoutBlockFlowLine
private:
    InlineFlowBox* createLineBoxes(LineLayoutItem, const LineInfo&, InlineBox* childBox);
    RootInlineBox* constructLine(BidiRunList<BidiRun>&, const LineInfo&);
    void setMarginsForRubyRun(BidiRun*, LayoutRubyRun*, LayoutObject*, const LineInfo&);
    void computeInlineDirectionPositionsForLine(RootInlineBox*, const LineInfo&, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    BidiRun* computeInlineDirectionPositionsForSegment(RootInlineBox*, const LineInfo&, ETextAlign, LayoutUnit& logicalLeft,
        LayoutUnit& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache&, WordMeasurements&);
    void computeBlockDirectionPositionsForLine(RootInlineBox*, BidiRun*, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    void appendFloatingObjectToLastLine(FloatingObject&);
    void appendFloatsToLastLine(LineLayoutState&, const InlineIterator& cleanLineStart, const InlineBidiResolver&, const BidiStatus& cleanLineBidiStatus);
    // Helper function for layoutInlineChildren()
    RootInlineBox* createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>&, const InlineIterator& end, LineInfo&, VerticalPositionCache&, BidiRun* trailingSpaceRun, WordMeasurements&);
    void layoutRunsAndFloats(LineLayoutState&);
    const InlineIterator& restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver&,  const InlineIterator&);
    void layoutRunsAndFloatsInRange(LineLayoutState&, InlineBidiResolver&,
        const InlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus);
    void linkToEndLineIfNeeded(LineLayoutState&);
    void markDirtyFloatsForPaintInvalidation(Vector<FloatWithRect>& floats);
    RootInlineBox* determineStartPosition(LineLayoutState&, InlineBidiResolver&);
    void determineEndPosition(LineLayoutState&, RootInlineBox* startBox, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus);
    bool lineBoxHasBRWithClearance(RootInlineBox*);
    bool checkPaginationAndFloatsAtEndLine(LineLayoutState&);
    bool matchedEndLine(LineLayoutState&, const InlineBidiResolver&, const InlineIterator& endLineStart, const BidiStatus& endLineStatus);
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    void markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest = nullptr);
    // Positions new floats and also adjust all floats encountered on the line if any of them
    // have to move to the next page/column.
    void positionDialog();

// END METHODS DEFINED IN LayoutBlockFlowLine

};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBlockFlow, isLayoutBlockFlow());

} // namespace blink

#endif // LayoutBlockFlow_h
