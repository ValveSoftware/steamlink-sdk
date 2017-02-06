/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 */

#ifndef LayoutBlock_h
#define LayoutBlock_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "wtf/ListHashSet.h"
#include <memory>

namespace blink {

struct PaintInfo;
class LineLayoutBox;
class WordMeasurement;

typedef WTF::ListHashSet<LayoutBox*, 16> TrackedLayoutBoxListHashSet;
typedef WTF::HashMap<const LayoutBlock*, std::unique_ptr<TrackedLayoutBoxListHashSet>> TrackedDescendantsMap;
typedef WTF::HashMap<const LayoutBox*, LayoutBlock*> TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

// LayoutBlock is the class that is used by any LayoutObject
// that is a containing block.
// http://www.w3.org/TR/CSS2/visuren.html#containing-block
// See also LayoutObject::containingBlock() that is the function
// used to get the containing block of a LayoutObject.
//
// CSS is inconsistent and allows inline elements (LayoutInline) to be
// containing blocks, even though they are not blocks. Our
// implementation is as confused with inlines. See e.g.
// LayoutObject::containingBlock() vs LayoutObject::container().
//
// Containing blocks are a central concept for layout, in
// particular to the layout of out-of-flow positioned
// elements. They are used to determine the sizing as well
// as the positioning of the LayoutObjects.
//
// LayoutBlock is the class that handles out-of-flow positioned elements in
// Blink, in particular for layout (see layoutPositionedObjects()). That's why
// LayoutBlock keeps track of them through |gPositionedDescendantsMap| (see
// LayoutBlock.cpp).
// Note that this is a design decision made in Blink that doesn't reflect CSS:
// CSS allows relatively positioned inlines (LayoutInline) to be containing
// blocks, but they don't have the logic to handle out-of-flow positioned
// objects. This induces some complexity around choosing an enclosing
// LayoutBlock (for inserting out-of-flow objects during layout) vs the CSS
// containing block (for sizing, invalidation).
//
//
// ***** WHO LAYS OUT OUT-OF-FLOW POSITIONED OBJECTS? *****
// A positioned object gets inserted into an enclosing LayoutBlock's positioned
// map. This is determined by LayoutObject::containingBlock().
//
//
// ***** HANDLING OUT-OF-FLOW POSITIONED OBJECTS *****
// Care should be taken to handle out-of-flow positioned objects during
// certain tree walks (e.g. layout()). The rule is that anything that
// cares about containing blocks should skip the out-of-flow elements
// in the normal tree walk and do an optional follow-up pass for them
// using LayoutBlock::positionedObjects().
// Not doing so will result in passing the wrong containing
// block as tree walks will always pass the parent as the
// containing block.
//
// Sample code of how to handle positioned objects in LayoutBlock:
//
// for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
//     if (child->isOutOfFlowPositioned())
//         continue;
//
//     // Handle normal flow children.
//     ...
// }
// for (LayoutBox* positionedObject : positionedObjects()) {
//     // Handle out-of-flow positioned objects.
//     ...
// }
class CORE_EXPORT LayoutBlock : public LayoutBox {
protected:
    explicit LayoutBlock(ContainerNode*);
    ~LayoutBlock() override;

public:
    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a LayoutBlock, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    // These two functions are overridden for inline-block.
    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    LayoutUnit minLineHeightForReplacedObject(bool isFirstLine, LayoutUnit replacedHeight) const;

    bool createsNewFormattingContext() const;

    const char* name() const override;

protected:
    // Insert a child correctly into the tree when |beforeDescendant| isn't a direct child of
    // |this|. This happens e.g. when there's an anonymous block child of |this| and
    // |beforeDescendant| has been reparented into that one. Such things are invisible to the DOM,
    // and addChild() is typically called with the DOM tree (and not the layout tree) in mind.
    void addChildBeforeDescendant(LayoutObject* newChild, LayoutObject* beforeDescendant);

public:
    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;

    virtual void layoutBlock(bool relayoutChildren);

    void insertPositionedObject(LayoutBox*);
    static void removePositionedObject(LayoutBox*);
    void removePositionedObjects(LayoutBlock*, ContainingBlockState = SameContainingBlock);

    TrackedLayoutBoxListHashSet* positionedObjects() const { return hasPositionedObjects() ? positionedObjectsInternal() : nullptr; }
    bool hasPositionedObjects() const
    {
        ASSERT(m_hasPositionedObjects ? (positionedObjectsInternal() && !positionedObjectsInternal()->isEmpty()) : !positionedObjectsInternal());
        return m_hasPositionedObjects;
    }

    void addPercentHeightDescendant(LayoutBox*);
    void removePercentHeightDescendant(LayoutBox*);
    bool hasPercentHeightDescendant(LayoutBox* o) const { return hasPercentHeightDescendants() && percentHeightDescendantsInternal()->contains(o); }

    TrackedLayoutBoxListHashSet* percentHeightDescendants() const { return hasPercentHeightDescendants() ? percentHeightDescendantsInternal() : nullptr; }
    bool hasPercentHeightDescendants() const
    {
        ASSERT(m_hasPercentHeightDescendants ? (percentHeightDescendantsInternal() && !percentHeightDescendantsInternal()->isEmpty()) : !percentHeightDescendantsInternal());
        return m_hasPercentHeightDescendants;
    }

    void notifyScrollbarThicknessChanged() { m_widthAvailableToChildrenChanged = true; }

    void setHasMarkupTruncation(bool b) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }

    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }

    bool hasMarginBeforeQuirk(const LayoutBox* child) const;
    bool hasMarginAfterQuirk(const LayoutBox* child) const;

    void markPositionedObjectsForLayout();
    // FIXME: Do we really need this to be virtual? It's just so we can call this on
    // LayoutBoxes without needed to check whether they're LayoutBlocks first.
    void markForPaginationRelayoutIfNeeded(SubtreeLayoutScope&) final;

    LayoutUnit textIndentOffset() const;

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    LayoutUnit blockDirectionOffset(const LayoutSize& offsetFromBlock) const;
    LayoutUnit inlineDirectionOffset(const LayoutSize& offsetFromBlock) const;

    LayoutBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    void setSelectionState(SelectionState) override;

    static LayoutBlock* createAnonymousWithParentAndDisplay(const LayoutObject*, EDisplay = BLOCK);
    LayoutBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentAndDisplay(this, display); }

    LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const override;

    int columnGap() const;

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    LayoutUnit logicalWidthForChild(const LayoutBox& child) const { return logicalWidthForChildSize(child.size()); }
    LayoutUnit logicalWidthForChildSize(LayoutSize childSize) const { return isHorizontalWritingMode() ? childSize.width() : childSize.height(); }
    LayoutUnit logicalHeightForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.size().height() : child.size().width(); }
    LayoutSize logicalSizeForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.size() : child.size().transposedSize(); }
    LayoutUnit logicalTopForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.location().y() : child.location().x(); }
    LayoutUnit marginBeforeForChild(const LayoutBoxModelObject& child) const { return child.marginBefore(style()); }
    LayoutUnit marginAfterForChild(const LayoutBoxModelObject& child) const { return child.marginAfter(style()); }
    LayoutUnit marginStartForChild(const LayoutBoxModelObject& child) const { return child.marginStart(style()); }
    LayoutUnit marginEndForChild(const LayoutBoxModelObject& child) const { return child.marginEnd(style()); }
    void setMarginStartForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginStart(value, style()); }
    void setMarginEndForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginEnd(value, style()); }
    void setMarginBeforeForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginBefore(value, style()); }
    void setMarginAfterForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginAfter(value, style()); }
    LayoutUnit collapsedMarginBeforeForChild(const LayoutBox& child) const;
    LayoutUnit collapsedMarginAfterForChild(const LayoutBox& child) const;

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/);

    LayoutUnit availableLogicalWidthForContent() const { return (logicalRightOffsetForContent() - logicalLeftOffsetForContent()).clampNegativeToZero(); }
    LayoutUnit logicalLeftOffsetForContent() const { return isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
    LayoutUnit logicalRightOffsetForContent() const { return logicalLeftOffsetForContent() + availableLogicalWidth(); }
    LayoutUnit startOffsetForContent() const { return style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }
    LayoutUnit endOffsetForContent() const { return !style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }

    virtual LayoutUnit logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const;
    virtual LayoutUnit logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const;

#if ENABLE(ASSERT)
    void checkPositionedObjectsNeedLayout();
#endif

protected:
    bool recalcNormalFlowChildOverflowIfNeeded(LayoutObject*);
    bool recalcPositionedDescendantsOverflowAfterStyleChange();
public:
    virtual bool recalcChildOverflowAfterStyleChange();
    bool recalcOverflowAfterStyleChange();

    // An example explaining layout tree structure about first-line style:
    // <style>
    //   #enclosingFirstLineStyleBlock::first-line { ... }
    // </style>
    // <div id="enclosingFirstLineStyleBlock">
    //   <div>
    //     <div id="nearestInnerBlockWithFirstLine">
    //       [<span>]first line text[</span>]
    //     </div>
    //   </div>
    // </div>

    // Returns the nearest enclosing block (including this block) that contributes a first-line style to our first line.
    const LayoutBlock* enclosingFirstLineStyleBlock() const;
    // Returns this block or the nearest inner block containing the actual first line.
    LayoutBlockFlow* nearestInnerBlockWithFirstLine();

protected:
    void willBeDestroyed() override;

    void dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope&);

    void layout() override;

    enum PositionedLayoutBehavior {
        DefaultLayout,
        LayoutOnlyFixedPositionedObjects,
        ForcedLayoutAfterContainingBlockMoved
    };

    void layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior = DefaultLayout);
    void markFixedPositionObjectForLayoutIfNeeded(LayoutObject* child, SubtreeLayoutScope&);

    LayoutUnit marginIntrinsicLogicalWidthForChild(const LayoutBox& child) const;

    int beforeMarginInLineDirection(LineDirectionMode) const;

    void paint(const PaintInfo&, const LayoutPoint&) const override;
public:
    virtual void paintObject(const PaintInfo&, const LayoutPoint&) const;
    virtual void paintChildren(const PaintInfo&, const LayoutPoint&) const;

protected:
    virtual void adjustInlineDirectionLineBounds(unsigned /* expansionOpportunityCount */, LayoutUnit& /* logicalLeft */, LayoutUnit& /* logicalWidth */) const { }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    void computeChildPreferredLogicalWidths(LayoutObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const;

    int firstLineBoxBaseline() const override;
    int inlineBlockBaseline(LineDirectionMode) const override;

    // This function disables the 'overflow' check in inlineBlockBaseline.
    // For 'inline-block', CSS says that the baseline is the bottom margin edge
    // if 'overflow' is not visible. But some descendant classes want to ignore
    // this condition.
    virtual bool shouldIgnoreOverflowPropertyForInlineBlockBaseline() const { return false; }

    bool hitTestOverflowControl(HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation) override;
    bool hitTestChildren(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    void updateAfterLayout();

    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void updateFromStyle() override;

    // Returns true if non-visible overflow should be respected. Otherwise hasOverflowClip() will be
    // false and we won't create scrollable area for this object even if overflow is non-visible.
    virtual bool allowsOverflowClip() const;

    virtual bool hasLineIfEmpty() const;

    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool = false);
protected:
    virtual void addOverflowFromChildren();
    void addOverflowFromPositionedObjects();
    void addOverflowFromBlockChildren();
    void addVisualOverflowFromTheme();

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const override;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox&);

    // TODO(jchaffraix): We should rename this function as inline-flex and inline-grid as also covered.
    // Alternatively it should be removed as we clarify the meaning of isAtomicInlineLevel to imply
    // isInline.
    bool isInlineBlockOrInlineTable() const final { return isInline() && isAtomicInlineLevel(); }

private:
    LayoutObjectChildList* virtualChildren() final { return children(); }
    const LayoutObjectChildList* virtualChildren() const final { return children(); }

    bool isLayoutBlock() const final { return true; }

    virtual void removeLeftoverAnonymousBlock(LayoutBlock* child);

    TrackedLayoutBoxListHashSet* positionedObjectsInternal() const;
    TrackedLayoutBoxListHashSet* percentHeightDescendantsInternal() const;

    // Returns true if the positioned movement-only layout succeeded.
    bool tryLayoutDoingPositionedMovementOnly();

    bool avoidsFloats() const override { return true; }

    bool isInSelfHitTestingPhase(HitTestAction hitTestAction) const final
    {
        return hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground;
    }

    bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) const;

    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    bool isSelectionRoot() const;

public:
    bool hasCursorCaret() const;
    bool hasDragCaret() const;
    bool hasCaret() const { return hasCursorCaret() || hasDragCaret(); }

private:
    LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr) final;
    bool isInlineBoxWrapperActuallyChild() const;

    Position positionForBox(InlineBox*, bool start = true) const;

    // End helper functions and structs used by layoutBlockChildren.

    void removeFromGlobalMaps();
    bool widthAvailableToChildrenHasChanged();

protected:
    bool isPageLogicalHeightKnown(LayoutUnit logicalOffset) const { return pageLogicalHeightForOffset(logicalOffset); }

    // Returns the logical offset at the top of the next page, for a given offset.
    //
    // If the given offset is at a page boundary, using AssociateWithLatterPage as PageBoundaryRule
    // will move us one page ahead (since the offset is at the top of the "current" page). Using
    // AssociateWithFormerPage instead will keep us where we are (since the offset is at the bottom
    // of the "current" page, which is exactly the same offset as the top offset on the next page).
    //
    // For a page height of 800px, AssociateWithLatterPage will return 1600 if the value passed in
    // is 800. AssociateWithFormerPage will simply return 800.
    LayoutUnit nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule) const;

    // Paginated content inside this block was laid out.
    // |logicalBottomOffsetAfterPagination| is the logical bottom offset of the child content after
    // applying any forced or unforced breaks as needed.
    void paginatedContentWasLaidOut(LayoutUnit logicalBottomOffsetAfterPagination);

    // Adjust from painting offsets to the local coords of this layoutObject
    void offsetForContents(LayoutPoint&) const;

    PositionWithAffinity positionForPointRespectingEditingBoundaries(LineLayoutBox child, const LayoutPoint& pointInParentCoordinates);
    PositionWithAffinity positionForPointIfOutsideAtomicInlineLevel(const LayoutPoint&);

    virtual bool updateLogicalWidthAndColumnWidth();

    LayoutObjectChildList m_children;

    unsigned m_hasMarginBeforeQuirk : 1; // Note these quirk values can't be put in LayoutBlockRareData since they are set too frequently.
    unsigned m_hasMarginAfterQuirk : 1;
    unsigned m_beingDestroyed : 1;
    unsigned m_hasMarkupTruncation : 1;
    unsigned m_widthAvailableToChildrenChanged  : 1;
    unsigned m_heightAvailableToChildrenChanged  : 1;
    unsigned m_isSelfCollapsing : 1; // True if margin-before and margin-after are adjoining.
    unsigned m_descendantsWithFloatsMarkedForLayout : 1;

    unsigned m_hasPositionedObjects : 1;
    unsigned m_hasPercentHeightDescendants : 1;

    // FIXME: This is temporary as we move code that accesses block flow
    // member variables out of LayoutBlock and into LayoutBlockFlow.
    friend class LayoutBlockFlow;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBlock, isLayoutBlock());

} // namespace blink

#endif // LayoutBlock_h
