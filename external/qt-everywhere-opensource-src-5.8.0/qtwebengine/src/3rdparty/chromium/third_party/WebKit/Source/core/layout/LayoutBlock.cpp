/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/layout/LayoutBlock.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/DragCaretController.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMarqueeElement.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/Page.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

struct SameSizeAsLayoutBlock : public LayoutBox {
    LayoutObjectChildList children;
    uint32_t bitfields;
};

static_assert(sizeof(LayoutBlock) == sizeof(SameSizeAsLayoutBlock), "LayoutBlock should stay small");

// This map keeps track of the positioned objects associated with a containing
// block.
//
// This map is populated during layout. It is kept across layouts to handle
// that we skip unchanged sub-trees during layout, in such a way that we are
// able to lay out deeply nested out-of-flow descendants if their containing
// block got laid out. The map could be invalidated during style change but
// keeping track of containing blocks at that time is complicated (we are in
// the middle of recomputing the style so we can't rely on any of its
// information), which is why it's easier to just update it for every layout.
static TrackedDescendantsMap* gPositionedDescendantsMap = nullptr;
static TrackedContainerMap* gPositionedContainerMap = nullptr;

// This map keeps track of the descendants whose 'height' is percentage associated
// with a containing block. Like |gPositionedDescendantsMap|, it is also recomputed
// for every layout (see the comment above about why).
static TrackedDescendantsMap* gPercentHeightDescendantsMap = nullptr;

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_widthAvailableToChildrenChanged(false)
    , m_heightAvailableToChildrenChanged(false)
    , m_isSelfCollapsing(false)
    , m_descendantsWithFloatsMarkedForLayout(false)
    , m_hasPositionedObjects(false)
    , m_hasPercentHeightDescendants(false)
{
    // LayoutBlockFlow calls setChildrenInline(true).
    // By default, subclasses do not have inline children.
}

void LayoutBlock::removeFromGlobalMaps()
{
    if (hasPositionedObjects()) {
        std::unique_ptr<TrackedLayoutBoxListHashSet> descendants = gPositionedDescendantsMap->take(this);
        ASSERT(!descendants->isEmpty());
        for (LayoutBox* descendant : *descendants) {
            ASSERT(gPositionedContainerMap->get(descendant) == this);
            gPositionedContainerMap->remove(descendant);
        }
    }
    if (hasPercentHeightDescendants()) {
        std::unique_ptr<TrackedLayoutBoxListHashSet> descendants = gPercentHeightDescendantsMap->take(this);
        ASSERT(!descendants->isEmpty());
        for (LayoutBox* descendant : *descendants) {
            ASSERT(descendant->percentHeightContainer() == this);
            descendant->setPercentHeightContainer(nullptr);
        }
    }
}

LayoutBlock::~LayoutBlock()
{
    removeFromGlobalMaps();
}

void LayoutBlock::willBeDestroyed()
{
    if (!documentBeingDestroyed() && parent())
        parent()->dirtyLinesFromChangedChild(this);

    if (TextAutosizer* textAutosizer = document().textAutosizer())
        textAutosizer->destroy(this);

    LayoutBox::willBeDestroyed();
}

void LayoutBlock::styleWillChange(StyleDifference diff, const ComputedStyle& newStyle)
{
    const ComputedStyle* oldStyle = style();

    setIsAtomicInlineLevel(newStyle.isDisplayInlineType());

    if (oldStyle && parent()) {
        bool oldStyleContainsFixedPosition = oldStyle->canContainFixedPositionObjects();
        bool oldStyleContainsAbsolutePosition = oldStyleContainsFixedPosition || oldStyle->canContainAbsolutePositionObjects();
        bool newStyleContainsFixedPosition = newStyle.canContainFixedPositionObjects();
        bool newStyleContainsAbsolutePosition = newStyleContainsFixedPosition || newStyle.canContainAbsolutePositionObjects();

        if ((oldStyleContainsFixedPosition && !newStyleContainsFixedPosition)
            || (oldStyleContainsAbsolutePosition && !newStyleContainsAbsolutePosition)) {
            // Clear our positioned objects list. Our absolute and fixed positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(nullptr, NewContainingBlock);
        }
        if (!oldStyleContainsAbsolutePosition && newStyleContainsAbsolutePosition) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            if (LayoutBlock* cb = containingBlockForAbsolutePosition())
                cb->removePositionedObjects(this, NewContainingBlock);
        }
        if (!oldStyleContainsFixedPosition && newStyleContainsFixedPosition) {
            // Remove our fixed positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            if (LayoutBlock* cb = containerForFixedPosition())
                cb->removePositionedObjects(this, NewContainingBlock);
        }
    }

    LayoutBox::styleWillChange(diff, newStyle);
}

enum LogicalExtent { LogicalWidth, LogicalHeight };
static bool borderOrPaddingLogicalDimensionChanged(const ComputedStyle& oldStyle, const ComputedStyle& newStyle, LogicalExtent logicalExtent)
{
    if (newStyle.isHorizontalWritingMode() == (logicalExtent == LogicalWidth)) {
        return oldStyle.borderLeftWidth() != newStyle.borderLeftWidth()
            || oldStyle.borderRightWidth() != newStyle.borderRightWidth()
            || oldStyle.paddingLeft() != newStyle.paddingLeft()
            || oldStyle.paddingRight() != newStyle.paddingRight();
    }

    return oldStyle.borderTopWidth() != newStyle.borderTopWidth()
        || oldStyle.borderBottomWidth() != newStyle.borderBottomWidth()
        || oldStyle.paddingTop() != newStyle.paddingTop()
        || oldStyle.paddingBottom() != newStyle.paddingBottom();
}

void LayoutBlock::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutBox::styleDidChange(diff, oldStyle);

    const ComputedStyle& newStyle = styleRef();

    if (oldStyle && parent()) {
        if (oldStyle->position() != newStyle.position() && newStyle.position() != StaticPosition) {
            // In LayoutObject::styleWillChange() we already removed ourself from our old containing
            // block's positioned descendant list, and we will be inserted to the new containing
            // block's list during layout. However the positioned descendant layout logic assumes
            // layout objects to obey parent-child order in the list. Remove our descendants here
            // so they will be re-inserted after us.
            if (LayoutBlock* cb = containingBlock()) {
                cb->removePositionedObjects(this, NewContainingBlock);
                if (isOutOfFlowPositioned()) {
                    // Insert this object into containing block's positioned descendants list
                    // in case the parent won't layout. This is needed especially there are
                    // descendants scheduled for overflow recalc.
                    cb->insertPositionedObject(this);
                }
            }
        }
    }

    if (TextAutosizer* textAutosizer = document().textAutosizer())
        textAutosizer->record(this);

    propagateStyleToAnonymousChildren();

    // It's possible for our border/padding to change, but for the overall logical width or height of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    m_widthAvailableToChildrenChanged |= oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalDimensionChanged(*oldStyle, newStyle, LogicalWidth);
    m_heightAvailableToChildrenChanged |= oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalDimensionChanged(*oldStyle, newStyle, LogicalHeight);
}

void LayoutBlock::updateFromStyle()
{
    LayoutBox::updateFromStyle();

    bool shouldClipOverflow = !styleRef().isOverflowVisible() && allowsOverflowClip();
    if (shouldClipOverflow != hasOverflowClip()) {
        if (!shouldClipOverflow)
            getScrollableArea()->invalidateAllStickyConstraints();
        setMayNeedPaintInvalidationSubtree();
    }
    setHasOverflowClip(shouldClipOverflow);
}

bool LayoutBlock::allowsOverflowClip() const
{
    // If overflow has been propagated to the viewport, it has no effect here.
    return node() != document().viewportDefiningElement();
}

void LayoutBlock::addChildBeforeDescendant(LayoutObject* newChild, LayoutObject* beforeDescendant)
{
    ASSERT(beforeDescendant->parent() != this);
    LayoutObject* beforeDescendantContainer = beforeDescendant->parent();
    while (beforeDescendantContainer->parent() != this)
        beforeDescendantContainer = beforeDescendantContainer->parent();
    ASSERT(beforeDescendantContainer);

    // We really can't go on if what we have found isn't anonymous. We're not supposed to use some
    // random non-anonymous object and put the child there. That's a recipe for security issues.
    RELEASE_ASSERT(beforeDescendantContainer->isAnonymous());

    // If the requested insertion point is not one of our children, then this is because
    // there is an anonymous container within this object that contains the beforeDescendant.
    if (beforeDescendantContainer->isAnonymousBlock()
        // Full screen layoutObjects and full screen placeholders act as anonymous blocks, not tables:
        || beforeDescendantContainer->isLayoutFullScreen()
        || beforeDescendantContainer->isLayoutFullScreenPlaceholder()) {
        // Insert the child into the anonymous block box instead of here.
        if (newChild->isInline() || newChild->isFloatingOrOutOfFlowPositioned() || beforeDescendant->parent()->slowFirstChild() != beforeDescendant)
            beforeDescendant->parent()->addChild(newChild, beforeDescendant);
        else
            addChild(newChild, beforeDescendant->parent());
        return;
    }

    ASSERT(beforeDescendantContainer->isTable());
    if (newChild->isTablePart()) {
        // Insert into the anonymous table.
        beforeDescendantContainer->addChild(newChild, beforeDescendant);
        return;
    }

    LayoutObject* beforeChild = splitAnonymousBoxesAroundChild(beforeDescendant);

    ASSERT(beforeChild->parent() == this);
    if (beforeChild->parent() != this) {
        // We should never reach here. If we do, we need to use the
        // safe fallback to use the topmost beforeChild container.
        beforeChild = beforeDescendantContainer;
    }

    addChild(newChild, beforeChild);
}

void LayoutBlock::addChild(LayoutObject* newChild, LayoutObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != this) {
        addChildBeforeDescendant(newChild, beforeChild);
        return;
    }

    // Only LayoutBlockFlow should have inline children, and then we shouldn't be here.
    ASSERT(!childrenInline());

    if (newChild->isInline() || newChild->isFloatingOrOutOfFlowPositioned()) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        LayoutObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            afterChild->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            LayoutBlock* newBox = createAnonymousBlock();
            LayoutBox::addChild(newBox, beforeChild);
            newBox->addChild(newChild);
            return;
        }
    }

    LayoutBox::addChild(newChild, beforeChild);
}

void LayoutBlock::removeLeftoverAnonymousBlock(LayoutBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());
    ASSERT(child->parent() == this);

    if (child->continuation())
        return;

    // Promote all the leftover anonymous block's children (to become children of this block
    // instead). We still want to keep the leftover block in the tree for a moment, for notification
    // purposes done further below (flow threads and grids).
    child->moveAllChildrenTo(this, child->nextSibling());

    // Remove all the information in the flow thread associated with the leftover anonymous block.
    child->removeFromLayoutFlowThread();

    // LayoutGrid keeps track of its children, we must notify it about changes in the tree.
    if (child->parent()->isLayoutGrid())
        toLayoutGrid(child->parent())->dirtyGrid();

    // Now remove the leftover anonymous block from the tree, and destroy it. We'll rip it out
    // manually from the tree before destroying it, because we don't want to trigger any tree
    // adjustments with regards to anonymous blocks (or any other kind of undesired chain-reaction).
    children()->removeChildNode(this, child, false);
    child->destroy();
}

void LayoutBlock::updateAfterLayout()
{
    invalidateStickyConstraints();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->getScrollableArea()->updateAfterLayout();
}

void LayoutBlock::layout()
{
    LayoutAnalyzer::Scope analyzer(*this);

    bool needsScrollAnchoring = RuntimeEnabledFeatures::scrollAnchoringEnabled() && hasOverflowClip();
    if (needsScrollAnchoring)
        getScrollableArea()->scrollAnchor().save();

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);

    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();

    if (needsScrollAnchoring)
        getScrollableArea()->scrollAnchor().restore();

    m_heightAvailableToChildrenChanged = false;
}

bool LayoutBlock::widthAvailableToChildrenHasChanged()
{
    // TODO(robhogan): Does m_widthAvailableToChildrenChanged always get reset when it needs to?
    bool widthAvailableToChildrenHasChanged = m_widthAvailableToChildrenChanged;
    m_widthAvailableToChildrenChanged = false;

    // If we use border-box sizing, have percentage padding, and our parent has changed width then the width available to our children has changed even
    // though our own width has remained the same.
    widthAvailableToChildrenHasChanged |= style()->boxSizing() == BoxSizingBorderBox && needsPreferredWidthsRecalculation() && view()->layoutState()->containingBlockLogicalWidthChanged();

    return widthAvailableToChildrenHasChanged;
}

bool LayoutBlock::updateLogicalWidthAndColumnWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    updateLogicalWidth();
    return oldWidth != logicalWidth() || widthAvailableToChildrenHasChanged();
}

void LayoutBlock::layoutBlock(bool)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void LayoutBlock::addOverflowFromChildren()
{
    if (childrenInline())
        toLayoutBlockFlow(this)->addOverflowFromInlineChildren();
    else
        addOverflowFromBlockChildren();
}

void LayoutBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool)
{
    m_overflow.reset();

    // Add overflow from children.
    addOverflowFromChildren();

    // Add in the overflow from positioned objects.
    addOverflowFromPositionedObjects();

    if (hasOverflowClip()) {
        // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins
        // and bottom padding.  Set the axis we don't care about to be 1, since we want this overflow to always
        // be considered reachable.
        LayoutRect clientRect(noOverflowRect());
        LayoutRect rectToApply;
        if (isHorizontalWritingMode())
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), LayoutUnit(1), (oldClientAfterEdge - clientRect.y()).clampNegativeToZero());
        else
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), (oldClientAfterEdge - clientRect.x()).clampNegativeToZero(), LayoutUnit(1));
        addLayoutOverflow(rectToApply);
        if (hasOverflowModel())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }

    addVisualEffectOverflow();
    addVisualOverflowFromTheme();

    // An enclosing composited layer will need to update its bounds if we now overflow it.
    PaintLayer* layer = enclosingLayer();
    if (!needsLayout() && layer->hasCompositedLayerMapping() && !layer->visualRect().contains(visualOverflowRect()))
        layer->setNeedsCompositingInputsUpdate();
}

void LayoutBlock::addOverflowFromBlockChildren()
{
    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!child->isFloatingOrOutOfFlowPositioned() && !child->isColumnSpanAll())
            addOverflowFromChild(child);
    }
}

void LayoutBlock::addOverflowFromPositionedObjects()
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    for (auto* positionedObject : *positionedDescendants) {
        // Fixed positioned elements don't contribute to layout overflow, since they don't scroll with the content.
        if (positionedObject->style()->position() != FixedPosition)
            addOverflowFromChild(positionedObject, toLayoutSize(positionedObject->location()));
    }
}

void LayoutBlock::addVisualOverflowFromTheme()
{
    if (!style()->hasAppearance())
        return;

    IntRect inflatedRect = pixelSnappedBorderBoxRect();
    LayoutTheme::theme().addVisualOverflow(*this, inflatedRect);
    addSelfVisualOverflow(LayoutRect(inflatedRect));
}

bool LayoutBlock::createsNewFormattingContext() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasOverflowClip() || isFlexItemIncludingDeprecated()
        || style()->specifiesColumns() || isLayoutFlowThread() || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot()
        || isDocumentElement() || isColumnSpanAll() || isGridItem() || style()->containsPaint() || style()->containsLayout();
}

static inline bool changeInAvailableLogicalHeightAffectsChild(LayoutBlock* parent, LayoutBox& child)
{
    if (parent->style()->boxSizing() != BoxSizingBorderBox)
        return false;
    return parent->style()->isHorizontalWritingMode() && !child.style()->isHorizontalWritingMode();
}

void LayoutBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox& child)
{
    if (child.isOutOfFlowPositioned()) {
        // It's rather useless to mark out-of-flow children at this point. We may not be their
        // containing block (and if we are, it's just pure luck), so this would be the wrong place
        // for it. Furthermore, it would cause trouble for out-of-flow descendants of column
        // spanners, if the containing block is outside the spanner but inside the multicol container.
        return;
    }
    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    bool hasRelativeLogicalHeight = child.hasRelativeLogicalHeight()
        || (child.isAnonymous() && this->hasRelativeLogicalHeight())
        || child.stretchesToViewport();
    if (relayoutChildren || (hasRelativeLogicalHeight && !isLayoutView())
        || (m_heightAvailableToChildrenChanged && changeInAvailableLogicalHeightAffectsChild(this, child))) {
        child.setChildNeedsLayout(MarkOnlyThis);

        // If the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (child.needsPreferredWidthsRecalculation())
            child.setPreferredLogicalWidthsDirty(MarkOnlyThis);
    }
}

void LayoutBlock::simplifiedNormalFlowLayout()
{
    if (childrenInline()) {
        ASSERT_WITH_SECURITY_IMPLICATION(isLayoutBlockFlow());
        LayoutBlockFlow* blockFlow = toLayoutBlockFlow(this);
        blockFlow->simplifiedNormalFlowInlineLayout();
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned()) {
                if (box->isLayoutMultiColumnSpannerPlaceholder())
                    toLayoutMultiColumnSpannerPlaceholder(box)->markForLayoutIfObjectInFlowThreadNeedsLayout();
                box->layoutIfNeeded();
            }
        }
    }
}

bool LayoutBlock::simplifiedLayout()
{
    // Check if we need to do a full layout.
    if (normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    // Check that we actually need to do a simplified layout.
    if (!posChildNeedsLayout() && !(needsSimplifiedNormalFlowLayout() || needsPositionedMovementLayout()))
        return false;

    {
        // LayoutState needs this deliberate scope to pop before paint invalidation.
        LayoutState state(*this, locationOffset());

        if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly())
            return false;

        if (LayoutFlowThread* flowThread = flowThreadContainingBlock()) {
            if (!flowThread->canSkipLayout(*this))
                return false;
        }

        TextAutosizer::LayoutScope textAutosizerLayoutScope(this);

        // Lay out positioned descendants or objects that just need to recompute overflow.
        if (needsSimplifiedNormalFlowLayout())
            simplifiedNormalFlowLayout();

        // Lay out our positioned objects if our positioned child bit is set.
        // Also, if an absolute position element inside a relative positioned container moves, and the absolute element has a fixed position
        // child, neither the fixed element nor its container learn of the movement since posChildNeedsLayout() is only marked as far as the
        // relative positioned container. So if we can have fixed pos objects in our positioned objects list check if any of them
        // are statically positioned and thus need to move with their absolute ancestors.
        bool canContainFixedPosObjects = canContainFixedPositionObjects();
        if (posChildNeedsLayout() || needsPositionedMovementLayout() || canContainFixedPosObjects)
            layoutPositionedObjects(false, needsPositionedMovementLayout() ? ForcedLayoutAfterContainingBlockMoved : (!posChildNeedsLayout() && canContainFixedPosObjects ? LayoutOnlyFixedPositionedObjects : DefaultLayout));

        // Recompute our overflow information.
        // FIXME: We could do better here by computing a temporary overflow object from layoutPositionedObjects and only
        // updating our overflow if we either used to have overflow or if the new temporary object has overflow.
        // For now just always recompute overflow. This is no worse performance-wise than the old code that called rightmostPosition and
        // lowestPosition on every relayout so it's not a regression.
        // computeOverflow expects the bottom edge before we clamp our height. Since this information isn't available during
        // simplifiedLayout, we cache the value in m_overflow.
        LayoutUnit oldClientAfterEdge = hasOverflowModel() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
        computeOverflow(oldClientAfterEdge, true);
    }

    updateLayerTransformAfterLayout();

    updateAfterLayout();

    clearNeedsLayout();

    if (LayoutAnalyzer* analyzer = frameView()->layoutAnalyzer())
        analyzer->increment(LayoutAnalyzer::LayoutObjectsThatNeedSimplifiedLayout);

    return true;
}

void LayoutBlock::markFixedPositionObjectForLayoutIfNeeded(LayoutObject* child, SubtreeLayoutScope& layoutScope)
{
    if (child->style()->position() != FixedPosition)
        return;

    bool hasStaticBlockPosition = child->style()->hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = child->style()->hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    LayoutObject* o = child->parent();
    while (o && !o->isLayoutView() && o->style()->position() != AbsolutePosition)
        o = o->parent();
    if (o->style()->position() != AbsolutePosition)
        return;

    LayoutBox* box = toLayoutBox(child);
    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        box->computeLogicalWidth(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != box->logicalLeft())
            layoutScope.setChildNeedsLayout(child);
    } else if (hasStaticBlockPosition) {
        LayoutUnit oldTop = box->logicalTop();
        box->updateLogicalHeight();
        if (box->logicalTop() != oldTop)
            layoutScope.setChildNeedsLayout(child);
    }
}

LayoutUnit LayoutBlock::marginIntrinsicLogicalWidthForChild(const LayoutBox& child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child.style()->marginStartUsing(style());
    Length marginRight = child.style()->marginEndUsing(style());
    LayoutUnit margin;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

static bool needsLayoutDueToStaticPosition(LayoutBox* child)
{
    // When a non-positioned block element moves, it may have positioned children that are
    // implicitly positioned relative to the non-positioned block.
    const ComputedStyle* style = child->style();
    bool isHorizontal = style->isHorizontalWritingMode();
    if (style->hasStaticBlockPosition(isHorizontal)) {
        LayoutBox::LogicalExtentComputedValues computedValues;
        LayoutUnit currentLogicalTop = child->logicalTop();
        LayoutUnit currentLogicalHeight = child->logicalHeight();
        child->computeLogicalHeight(currentLogicalHeight, currentLogicalTop, computedValues);
        if (computedValues.m_position != currentLogicalTop || computedValues.m_extent != currentLogicalHeight)
            return true;
    }
    if (style->hasStaticInlinePosition(isHorizontal)) {
        LayoutBox::LogicalExtentComputedValues computedValues;
        LayoutUnit currentLogicalLeft = child->logicalLeft();
        LayoutUnit currentLogicalWidth = child->logicalWidth();
        child->computeLogicalWidth(computedValues);
        if (computedValues.m_position != currentLogicalLeft || computedValues.m_extent != currentLogicalWidth)
            return true;
    }
    return false;
}

void LayoutBlock::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    bool isPaginated = view()->layoutState()->isPaginated();

    for (auto* positionedObject : *positionedDescendants) {
        positionedObject->setMayNeedPaintInvalidation();

        SubtreeLayoutScope layoutScope(*positionedObject);
        // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
        // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e.
        // it has static position.
        markFixedPositionObjectForLayoutIfNeeded(positionedObject, layoutScope);
        if (info == LayoutOnlyFixedPositionedObjects) {
            positionedObject->layoutIfNeeded();
            continue;
        }

        if (!positionedObject->normalChildNeedsLayout() && (relayoutChildren || m_heightAvailableToChildrenChanged || needsLayoutDueToStaticPosition(positionedObject)))
            layoutScope.setChildNeedsLayout(positionedObject);

        // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (relayoutChildren && positionedObject->needsPreferredWidthsRecalculation())
            positionedObject->setPreferredLogicalWidthsDirty(MarkOnlyThis);

        LayoutUnit logicalTopEstimate;
        bool needsBlockDirectionLocationSetBeforeLayout = isPaginated && positionedObject->getPaginationBreakability() != ForbidBreaks;
        if (needsBlockDirectionLocationSetBeforeLayout) {
            // Out-of-flow objects are normally positioned after layout (while in-flow objects are
            // positioned before layout). If the child object is paginated in the same context as
            // we are, estimate its logical top now. We need to know this up-front, to correctly
            // evaluate if we need to mark for relayout, and, if our estimate is correct, we'll
            // even be able to insert correct pagination struts on the first attempt.
            LogicalExtentComputedValues computedValues;
            positionedObject->computeLogicalHeight(positionedObject->logicalHeight(), positionedObject->logicalTop(), computedValues);
            logicalTopEstimate = computedValues.m_position;
            positionedObject->setLogicalTop(logicalTopEstimate);
        }

        if (!positionedObject->needsLayout())
            positionedObject->markForPaginationRelayoutIfNeeded(layoutScope);

        // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout() here instead of a full layout. Need
        // to investigate why it does not trigger the correct invalidations in that case. crbug.com/350756
        if (info == ForcedLayoutAfterContainingBlockMoved)
            positionedObject->setNeedsLayout(LayoutInvalidationReason::AncestorMoved, MarkOnlyThis);

        positionedObject->layoutIfNeeded();

        LayoutObject* parent = positionedObject->parent();
        bool layoutChanged = false;
        if (parent->isFlexibleBox() && toLayoutFlexibleBox(parent)->setStaticPositionForPositionedLayout(*positionedObject)) {
            // The static position of an abspos child of a flexbox depends on its size (for example,
            // they can be centered). So we may have to reposition the item after layout.
            // TODO(cbiesinger): We could probably avoid a layout here and just reposition?
            positionedObject->forceChildLayout();
            layoutChanged = true;
        }

        // Lay out again if our estimate was wrong.
        if (!layoutChanged && needsBlockDirectionLocationSetBeforeLayout && logicalTopEstimate != logicalTopForChild(*positionedObject))
            positionedObject->forceChildLayout();
    }
}

void LayoutBlock::markPositionedObjectsForLayout()
{
    if (TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects()) {
        for (auto* descendant : *positionedDescendants)
            descendant->setChildNeedsLayout();
    }
}

void LayoutBlock::markForPaginationRelayoutIfNeeded(SubtreeLayoutScope& layoutScope)
{
    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    if (view()->layoutState()->pageLogicalHeightChanged() || (view()->layoutState()->pageLogicalHeight() && view()->layoutState()->pageLogicalOffset(*this, logicalTop()) != pageLogicalOffset()))
        layoutScope.setChildNeedsLayout(this);
}

void LayoutBlock::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paint(paintInfo, paintOffset);
}

void LayoutBlock::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paintChildren(paintInfo, paintOffset);
}

void LayoutBlock::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paintObject(paintInfo, paintOffset);
}

bool LayoutBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(node() || isAnonymous());

    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;

    if (isBody() || isDocumentElement() || hasOverflowClip()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransformRelatedProperty() || hasReflection() || hasMask() || isWritingModeRoot()
        || isLayoutFlowThread() || isFlexItemIncludingDeprecated())
        return true;

    if (view() && view()->selectionStart()) {
        Node* startElement = view()->selectionStart()->node();
        if (startElement && startElement->rootEditableElement() == node())
            return true;
    }

    return false;
}

LayoutUnit LayoutBlock::blockDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.height() : offsetFromBlock.width();
}

LayoutUnit LayoutBlock::inlineDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.width() : offsetFromBlock.height();
}

LayoutUnit LayoutBlock::logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalLeftSelectionOffset(rootBlock, position + logicalTop());
    return logicalLeftOffsetForContent();
}

LayoutUnit LayoutBlock::logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalRightSelectionOffset(rootBlock, position + logicalTop());
    return logicalRightOffsetForContent();
}

LayoutBlock* LayoutBlock::blockBeforeWithinSelectionRoot(LayoutSize& offset) const
{
    if (isSelectionRoot())
        return nullptr;

    const LayoutObject* object = this;
    LayoutObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!sibling->isLayoutBlock() || toLayoutBlock(sibling)->isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(toLayoutBlock(object)->logicalLeft(), toLayoutBlock(object)->logicalTop());
        object = object->parent();
    } while (!sibling && object && object->isLayoutBlock() && !toLayoutBlock(object)->isSelectionRoot());

    if (!sibling)
        return nullptr;

    LayoutBlock* beforeBlock = toLayoutBlock(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    LayoutObject* child = beforeBlock->lastChild();
    while (child && child->isLayoutBlock()) {
        beforeBlock = toLayoutBlock(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

void LayoutBlock::setSelectionState(SelectionState state)
{
    LayoutBox::setSelectionState(state);

    if (inlineBoxWrapper() && canUpdateSelectionOnRootLineBoxes())
        inlineBoxWrapper()->root().setHasSelectedChildren(state != SelectionNone);
}

TrackedLayoutBoxListHashSet* LayoutBlock::positionedObjectsInternal() const
{
    return gPositionedDescendantsMap ? gPositionedDescendantsMap->get(this) : nullptr;
}

void LayoutBlock::insertPositionedObject(LayoutBox* o)
{
    ASSERT(!isAnonymousBlock());
    ASSERT(o->containingBlock() == this);

    if (gPositionedContainerMap) {
        auto containerMapIt = gPositionedContainerMap->find(o);
        if (containerMapIt != gPositionedContainerMap->end()) {
            if (containerMapIt->value == this) {
                ASSERT(hasPositionedObjects() && positionedObjects()->contains(o));
                return;
            }
            removePositionedObject(o);
        }
    } else {
        gPositionedContainerMap = new TrackedContainerMap;
    }
    gPositionedContainerMap->set(o, this);

    if (!gPositionedDescendantsMap)
        gPositionedDescendantsMap = new TrackedDescendantsMap;
    TrackedLayoutBoxListHashSet* descendantSet = gPositionedDescendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedLayoutBoxListHashSet;
        gPositionedDescendantsMap->set(this, wrapUnique(descendantSet));
    }
    descendantSet->add(o);

    m_hasPositionedObjects = true;
}

void LayoutBlock::removePositionedObject(LayoutBox* o)
{
    if (!gPositionedContainerMap)
        return;

    LayoutBlock* container = gPositionedContainerMap->take(o);
    if (!container)
        return;

    TrackedLayoutBoxListHashSet* positionedDescendants = gPositionedDescendantsMap->get(container);
    ASSERT(positionedDescendants && positionedDescendants->contains(o));
    positionedDescendants->remove(o);
    if (positionedDescendants->isEmpty()) {
        gPositionedDescendantsMap->remove(container);
        container->m_hasPositionedObjects = false;
    }
}

void LayoutBlock::removePositionedObjects(LayoutBlock* o, ContainingBlockState containingBlockState)
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    Vector<LayoutBox*, 16> deadObjects;
    for (auto* positionedObject : *positionedDescendants) {
        if (!o || (positionedObject->isDescendantOf(o) && o != positionedObject)) {
            if (containingBlockState == NewContainingBlock) {
                positionedObject->setChildNeedsLayout(MarkOnlyThis);
                if (positionedObject->needsPreferredWidthsRecalculation())
                    positionedObject->setPreferredLogicalWidthsDirty(MarkOnlyThis);

                // The positioned object changing containing block may change paint invalidation container.
                // Invalidate it (including non-compositing descendants) on its original paint invalidation container.
                if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
                    // This valid because we need to invalidate based on the current status.
                    DisableCompositingQueryAsserts compositingDisabler;
                    if (!positionedObject->isPaintInvalidationContainer())
                        positionedObject->invalidatePaintIncludingNonCompositingDescendants();
                }
            }

            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            LayoutObject* p = positionedObject->parent();
            while (p && !p->isLayoutBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout();

            deadObjects.append(positionedObject);
        }
    }

    for (auto object : deadObjects) {
        ASSERT(gPositionedContainerMap->get(object) == this);
        positionedDescendants->remove(object);
        gPositionedContainerMap->remove(object);
    }
    if (positionedDescendants->isEmpty()) {
        gPositionedDescendantsMap->remove(this);
        m_hasPositionedObjects = false;
    }
}

void LayoutBlock::addPercentHeightDescendant(LayoutBox* descendant)
{
    if (descendant->percentHeightContainer()) {
        if (descendant->percentHeightContainer() == this) {
            ASSERT(hasPercentHeightDescendant(descendant));
            return;
        }
        descendant->removeFromPercentHeightContainer();
    }
    descendant->setPercentHeightContainer(this);

    if (!gPercentHeightDescendantsMap)
        gPercentHeightDescendantsMap = new TrackedDescendantsMap;
    TrackedLayoutBoxListHashSet* descendantSet = gPercentHeightDescendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedLayoutBoxListHashSet;
        gPercentHeightDescendantsMap->set(this, wrapUnique(descendantSet));
    }
    descendantSet->add(descendant);

    m_hasPercentHeightDescendants = true;
}

void LayoutBlock::removePercentHeightDescendant(LayoutBox* descendant)
{
    if (TrackedLayoutBoxListHashSet* descendants = percentHeightDescendants()) {
        descendants->remove(descendant);
        descendant->setPercentHeightContainer(nullptr);
        if (descendants->isEmpty()) {
            gPercentHeightDescendantsMap->remove(this);
            m_hasPercentHeightDescendants = false;
        }
    }
}

TrackedLayoutBoxListHashSet* LayoutBlock::percentHeightDescendantsInternal() const
{
    return gPercentHeightDescendantsMap ? gPercentHeightDescendantsMap->get(this) : nullptr;
}

void LayoutBlock::dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope& layoutScope)
{
    TrackedLayoutBoxListHashSet* descendants = percentHeightDescendants();
    if (!descendants)
        return;

    for (auto* box : *descendants) {
        ASSERT(box->isDescendantOf(this));
        while (box != this) {
            if (box->normalChildNeedsLayout())
                break;
            layoutScope.setChildNeedsLayout(box);
            box = box->containingBlock();
            ASSERT(box);
            if (!box)
                break;
        }
    }
}

LayoutUnit LayoutBlock::textIndentOffset() const
{
    LayoutUnit cw;
    if (style()->textIndent().hasPercent())
        cw = containingBlock()->availableLogicalWidth();
    return minimumValueForLength(style()->textIndent(), cw);
}

bool LayoutBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) const
{
    if (!scrollsOverflow())
        return false;

    return layer()->getScrollableArea()->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
}

bool LayoutBlock::hitTestOverflowControl(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation)
{
    if (visibleToHitTestRequest(result.hitTestRequest())
        && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), locationInContainer) == StopHitTesting)
            return true;
    }
    return false;
}

bool LayoutBlock::hitTestChildren(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    ASSERT(!childrenInline());
    LayoutPoint scrolledOffset(hasOverflowClip() ? accumulatedOffset - scrolledContentOffset() : accumulatedOffset);
    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    for (LayoutBox* child = lastChildBox(); child; child = child->previousSiblingBox()) {
        LayoutPoint childPoint = flipForWritingModeForChild(child, scrolledOffset);
        if (!child->hasSelfPaintingLayer() && !child->isFloating() && !child->isColumnSpanAll() && child->nodeAtPoint(result, locationInContainer, childPoint, childHitTest)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - accumulatedOffset)));
            return true;
        }
    }

    return false;
}

Position LayoutBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->getLineLayoutItem().nonPseudoNode())
        return Position::editingPositionOf(nonPseudoNode(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return Position::editingPositionOf(box->getLineLayoutItem().nonPseudoNode(), start ? box->getLineLayoutItem().caretMinOffset() : box->getLineLayoutItem().caretMaxOffset());

    InlineTextBox* textBox = toInlineTextBox(box);
    return Position::editingPositionOf(box->getLineLayoutItem().nonPseudoNode(), start ? textBox->start() : textBox->start() + textBox->len());
}

static inline bool isEditingBoundary(LayoutObject* ancestor, LineLayoutBox child)
{
    ASSERT(!ancestor || ancestor->nonPseudoNode());
    ASSERT(child && child.nonPseudoNode());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isLayoutView())
        || ancestor->nonPseudoNode()->hasEditableStyle() == child.nonPseudoNode()->hasEditableStyle();
}

// FIXME: This function should go on LayoutObject.
// Then all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
PositionWithAffinity LayoutBlock::positionForPointRespectingEditingBoundaries(LineLayoutBox child, const LayoutPoint& pointInParentCoordinates)
{
    LayoutPoint childLocation = child.location();
    if (child.isInFlowPositioned())
        childLocation += child.offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous layoutObject, we just recur normally
    Node* childNode = child.nonPseudoNode();
    if (!childNode)
        return child.positionForPoint(pointInChildCoordinates);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    LayoutObject* ancestor = this;
    while (ancestor && !ancestor->nonPseudoNode())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child.positionForPoint(pointInChildCoordinates);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = logicalWidthForChildSize(child.size()) / 2;
    LayoutUnit logicalLeft = isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createPositionWithAffinity(childNode->nodeIndex());
    return ancestor->createPositionWithAffinity(childNode->nodeIndex() + 1, TextAffinity::Upstream);
}

PositionWithAffinity LayoutBlock::positionForPointIfOutsideAtomicInlineLevel(const LayoutPoint& point)
{
    ASSERT(isAtomicInlineLevel());
    // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
    LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
    LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

    if (pointLogicalLeft < 0)
        return createPositionWithAffinity(caretMinOffset());
    if (pointLogicalLeft >= logicalWidth())
        return createPositionWithAffinity(caretMaxOffset());
    if (pointLogicalTop < 0)
        return createPositionWithAffinity(caretMinOffset());
    if (pointLogicalTop >= logicalHeight())
        return createPositionWithAffinity(caretMaxOffset());
    return PositionWithAffinity();
}

static inline bool isChildHitTestCandidate(LayoutBox* box)
{
    return box->size().height() && box->style()->visibility() == VISIBLE && !box->isFloatingOrOutOfFlowPositioned() && !box->isLayoutFlowThread();
}

PositionWithAffinity LayoutBlock::positionForPoint(const LayoutPoint& point)
{
    if (isTable())
        return LayoutBox::positionForPoint(point);

    if (isAtomicInlineLevel()) {
        PositionWithAffinity position = positionForPointIfOutsideAtomicInlineLevel(point);
        if (!position.isNull())
            return position;
    }

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    ASSERT(!childrenInline());

    LayoutBox* lastCandidateBox = lastChildBox();
    while (lastCandidateBox && !isChildHitTestCandidate(lastCandidateBox))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(LineLayoutBox(lastCandidateBox), pointInContents);

        for (LayoutBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(childBox))
                continue;
            LayoutUnit childLogicalBottom = logicalTopForChild(*childBox) + logicalHeightForChild(*childBox);
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (isChildHitTestCandidate(childBox) && (pointInLogicalContents.y() < childLogicalBottom
                || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom)))
                return positionForPointRespectingEditingBoundaries(LineLayoutBox(childBox), pointInContents);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return LayoutBox::positionForPoint(point);
}

void LayoutBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);

    if (hasOverflowClip())
        offset += LayoutSize(scrolledContentOffset());

    offset = flipForWritingMode(offset);
}

int LayoutBlock::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->getFontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

void LayoutBlock::scrollbarsChanged(bool horizontalScrollbarChanged, bool verticalScrollbarChanged)
{
    m_widthAvailableToChildrenChanged |= verticalScrollbarChanged;
    m_heightAvailableToChildrenChanged |= horizontalScrollbarChanged;
}

void LayoutBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Size-contained elements don't consider their contents for preferred sizing.
    if (style()->containsSize())
        return;

    if (childrenInline()) {
        // FIXME: Remove this const_cast.
        toLayoutBlockFlow(const_cast<LayoutBlock*>(this))->computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    } else {
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    if (isHTMLMarqueeElement(node()) && toHTMLMarqueeElement(node())->isHorizontal())
        minLogicalWidth = LayoutUnit();

    if (isTableCell()) {
        Length tableCellWidth = toLayoutTableCell(this)->styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(tableCellWidth.value())));
    }

    int scrollbarWidth = scrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void LayoutBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = LayoutUnit();
    m_maxPreferredLogicalWidth = LayoutUnit();

    // FIXME: The isFixed() calls here should probably be checking for isSpecified since you
    // should be able to use percentage, calc or viewport relative values for width.
    const ComputedStyle& styleToUse = styleRef();
    if (!isTableCell() && styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() >= 0
        && !(isDeprecatedFlexItem() && !styleToUse.logicalWidth().intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalWidth().value()));
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMinWidth().value())));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMinWidth().value())));
    }

    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMaxWidth().value())));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMaxWidth().value())));
    }

    // Table layout uses integers, ceil the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = LayoutUnit(m_minPreferredLogicalWidth.ceil());
        m_maxPreferredLogicalWidth = LayoutUnit(m_maxPreferredLogicalWidth.ceil());
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearPreferredLogicalWidthsDirty();
}

void LayoutBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const ComputedStyle& styleToUse = styleRef();
    bool nowrap = styleToUse.whiteSpace() == NOWRAP;

    LayoutObject* child = firstChild();
    LayoutBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth, floatRightWidth;
    while (child) {
        // Positioned children don't affect the min/max width. Spanners only affect the min/max
        // width of the multicol container, not the flow thread.
        if (child->isOutOfFlowPositioned() || child->isColumnSpanAll()) {
            child = child->nextSibling();
            continue;
        }

        RefPtr<ComputedStyle> childStyle = child->mutableStyle();
        if (child->isFloating() || (child->isBox() && toLayoutBox(child)->avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (childStyle->clear() & ClearLeft) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = LayoutUnit();
            }
            if (childStyle->clear() & ClearRight) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = LayoutUnit();
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle->marginStartUsing(&styleToUse);
        Length endMarginLength = childStyle->marginEndUsing(&styleToUse);
        LayoutUnit margin;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);

        LayoutUnit w = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = std::max(w, minLogicalWidth);

        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            maxLogicalWidth = std::max(w, maxLogicalWidth);

        w = childMaxPreferredLogicalWidth + margin;

        if (!child->isFloating()) {
            if (child->isBox() && toLayoutBox(child)->avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                bool ltr = containingBlock ? containingBlock->style()->isLeftToRightDirection() : styleToUse.isLeftToRightDirection();
                LayoutUnit marginLogicalLeft = ltr ? marginStart : marginEnd;
                LayoutUnit marginLogicalRight = ltr ? marginEnd : marginStart;
                LayoutUnit maxLeft = marginLogicalLeft > 0 ? std::max(floatLeftWidth, marginLogicalLeft) : floatLeftWidth + marginLogicalLeft;
                LayoutUnit maxRight = marginLogicalRight > 0 ? std::max(floatRightWidth, marginLogicalRight) : floatRightWidth + marginLogicalRight;
                w = childMaxPreferredLogicalWidth + maxLeft + maxRight;
                w = std::max(w, floatLeftWidth + floatRightWidth);
            } else {
                maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
            }
            floatLeftWidth = floatRightWidth = LayoutUnit();
        }

        if (child->isFloating()) {
            if (childStyle->floating() == LeftFloat)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        } else {
            maxLogicalWidth = std::max(w, maxLogicalWidth);
        }

        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    minLogicalWidth = minLogicalWidth.clampNegativeToZero();
    maxLogicalWidth = maxLogicalWidth.clampNegativeToZero();

    maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
}

void LayoutBlock::computeChildPreferredLogicalWidths(LayoutObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    if (child.isBox() && child.isHorizontalWritingMode() != isHorizontalWritingMode()) {
        // If the child is an orthogonal flow, child's height determines the width, but the height is not available until layout.
        // http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-shrink-to-fit
        if (!child.needsLayout()) {
            minPreferredLogicalWidth = maxPreferredLogicalWidth = toLayoutBox(child).logicalHeight();
            return;
        }
        minPreferredLogicalWidth = maxPreferredLogicalWidth = toLayoutBox(child).computeLogicalHeightWithoutLayout();
        return;
    }
    minPreferredLogicalWidth = child.minPreferredLogicalWidth();
    maxPreferredLogicalWidth = child.maxPreferredLogicalWidth();

    // For non-replaced blocks if the inline size is min|max-content or a definite size the min|max-content contribution
    // is that size plus border, padding and margin https://drafts.csswg.org/css-sizing/#block-intrinsic
    if (child.isLayoutBlock()) {
        const Length& computedInlineSize = child.styleRef().logicalWidth();
        if (computedInlineSize.isMaxContent())
            minPreferredLogicalWidth = maxPreferredLogicalWidth;
        else if (computedInlineSize.isMinContent())
            maxPreferredLogicalWidth = minPreferredLogicalWidth;
    }
}

bool LayoutBlock::hasLineIfEmpty() const
{
    if (!node())
        return false;

    if (node()->isRootEditableElement())
        return true;

    if (node()->isShadowRoot() && isHTMLInputElement(toShadowRoot(node())->host()))
        return true;

    return false;
}

LayoutUnit LayoutBlock::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isAtomicInlineLevel() && linePositionMode == PositionOnContainingLine)
        return LayoutBox::lineHeight(firstLine, direction, linePositionMode);

    const ComputedStyle& style = styleRef(firstLine && document().styleEngine().usesFirstLineRules());
    return LayoutUnit(style.computedLineHeight());
}

int LayoutBlock::beforeMarginInLineDirection(LineDirectionMode direction) const
{
    return direction == HorizontalLine ? marginTop() : marginRight();
}

int LayoutBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isInline() && linePositionMode == PositionOnContainingLine) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style()->hasAppearance() && !LayoutTheme::theme().isControlContainer(style()->appearance()))
            return LayoutTheme::theme().baselinePosition(this);

        int baselinePos = (isWritingModeRoot() && !isRubyRun()) ? -1 : inlineBlockBaseline(direction);

        if (isDeprecatedFlexibleBox()) {
            // Historically, we did this check for all baselines. But we can't
            // remove this code from deprecated flexbox, because it effectively
            // breaks -webkit-line-clamp, which is used in the wild -- we would
            // calculate the baseline as if -webkit-line-clamp wasn't used.
            // For simplicity, we use this for all uses of deprecated flexbox.
            LayoutUnit bottomOfContent = direction == HorizontalLine ? size().height() - borderBottom() - paddingBottom() - horizontalScrollbarHeight() : size().width() - borderLeft() - paddingLeft() - verticalScrollbarWidth();
            if (baselinePos > bottomOfContent)
                baselinePos = -1;
        }
        if (baselinePos != -1)
            return beforeMarginInLineDirection(direction) + baselinePos;

        return LayoutBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    }

    // If we're not replaced, we'll only get called with PositionOfInteriorLineBoxes.
    // Note that inline-block counts as replaced here.
    ASSERT(linePositionMode == PositionOfInteriorLineBoxes);

    const FontMetrics& fontMetrics = style(firstLine)->getFontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

LayoutUnit LayoutBlock::minLineHeightForReplacedObject(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

// TODO(mstensho): Figure out if all of this baseline code is needed here, or if it should be moved
// down to LayoutBlockFlow. LayoutDeprecatedFlexibleBox and LayoutGrid lack baseline calculation
// overrides, so the code is here just for them. Just walking the block children in logical order
// seems rather wrong for those two layout modes, though.

int LayoutBlock::firstLineBoxBaseline() const
{
    ASSERT(!childrenInline());
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    for (LayoutBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            int result = curr->firstLineBoxBaseline();
            if (result != -1)
                return curr->logicalTop() + result; // Translate to our coordinate space.
        }
    }
    return -1;
}

int LayoutBlock::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    ASSERT(!childrenInline());
    if ((!style()->isOverflowVisible() && !shouldIgnoreOverflowPropertyForInlineBlockBaseline()) || style()->containsSize()) {
        // We are not calling LayoutBox::baselinePosition here because the caller should add the margin-top/margin-right, not us.
        return lineDirection == HorizontalLine ? size().height() + marginBottom() : size().width() + marginLeft();
    }

    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    bool haveNormalFlowChild = false;
    for (LayoutBox* curr = lastChildBox(); curr; curr = curr->previousSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            haveNormalFlowChild = true;
            int result = curr->inlineBlockBaseline(lineDirection);
            if (result != -1)
                return curr->logicalTop() + result; // Translate to our coordinate space.
        }
    }
    if (!haveNormalFlowChild && hasLineIfEmpty()) {
        const FontMetrics& fontMetrics = firstLineStyle()->getFontMetrics();
        return fontMetrics.ascent()
            + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
            + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
    }
    return -1;
}

const LayoutBlock* LayoutBlock::enclosingFirstLineStyleBlock() const
{
    const LayoutBlock* firstLineBlock = this;
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(PseudoIdFirstLine);
        if (hasPseudo)
            break;
        LayoutObject* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isAtomicInlineLevel() || firstLineBlock->isFloatingOrOutOfFlowPositioned()
            || !parentBlock
            || !parentBlock->behavesLikeBlockContainer())
            break;
        ASSERT_WITH_SECURITY_IMPLICATION(parentBlock->isLayoutBlock());
        if (toLayoutBlock(parentBlock)->firstChild() != firstLineBlock)
            break;
        firstLineBlock = toLayoutBlock(parentBlock);
    }

    if (!hasPseudo)
        return nullptr;

    return firstLineBlock;
}

LayoutBlockFlow* LayoutBlock::nearestInnerBlockWithFirstLine()
{
    if (childrenInline())
        return toLayoutBlockFlow(this);
    for (LayoutObject* child = firstChild(); child && !child->isFloatingOrOutOfFlowPositioned() && child->isLayoutBlockFlow(); child = toLayoutBlock(child)->firstChild()) {
        if (child->childrenInline())
            return toLayoutBlockFlow(child);
    }
    return nullptr;
}

void LayoutBlock::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (Node* n = nodeForHitTest())
        result.setNodeAndPosition(n, point);
}

// An inline-block uses its inlineBox as the inlineBoxWrapper,
// so the firstChild() is nullptr if the only child is an empty inline-block.
inline bool LayoutBlock::isInlineBoxWrapperActuallyChild() const
{
    return isInlineBlockOrInlineTable() && !size().isEmpty() && node() && editingIgnoresContent(node());
}

static inline bool caretBrowsingEnabled(const LocalFrame* frame)
{
    Settings* settings = frame->settings();
    return settings && settings->caretBrowsingEnabled();
}

bool LayoutBlock::hasCursorCaret() const
{
    LocalFrame* frame = this->frame();
    return frame->selection().caretLayoutObject() == this && (frame->selection().hasEditableStyle() || caretBrowsingEnabled(frame));
}

bool LayoutBlock::hasDragCaret() const
{
    LocalFrame* frame = this->frame();
    DragCaretController& dragCaretController = frame->page()->dragCaretController();
    return dragCaretController.caretLayoutObject() == this && (dragCaretController.isContentEditable() || caretBrowsingEnabled(frame));
}

LayoutRect LayoutBlock::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // Do the normal calculation in most cases.
    if (firstChild() || isInlineBoxWrapperActuallyChild())
        return LayoutBox::localCaretRect(inlineBox, caretOffset, extraWidthToEndOfLine);

    LayoutRect caretRect = localCaretRectForEmptyElement(size().width(), textIndentOffset());

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = size().width() - caretRect.maxX();

    return caretRect;
}

void LayoutBlock::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot includeBlockOverflows) const
{
    if (!isAnonymous()) // For anonymous blocks, the children add outline rects.
        rects.append(LayoutRect(additionalOffset, size()));

    if (includeBlockOverflows == IncludeBlockVisualOverflow && !hasOverflowClip() && !hasControlClip()) {
        addOutlineRectsForNormalChildren(rects, additionalOffset, includeBlockOverflows);
        if (TrackedLayoutBoxListHashSet* positionedObjects = this->positionedObjects()) {
            for (auto* box : *positionedObjects)
                addOutlineRectsForDescendant(*box, rects, additionalOffset, includeBlockOverflows);
        }
    }
}

LayoutBox* LayoutBlock::createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const
{
    return createAnonymousWithParentAndDisplay(parent, style()->display());
}

LayoutUnit LayoutBlock::nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;

    return logicalOffset + pageRemainingLogicalHeightForOffset(logicalOffset, pageBoundaryRule);
}

void LayoutBlock::paginatedContentWasLaidOut(LayoutUnit logicalBottomOffsetAfterPagination)
{
    if (LayoutFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->contentWasLaidOut(offsetFromLogicalTopOfFirstPage() + logicalBottomOffsetAfterPagination);
}

LayoutUnit LayoutBlock::collapsedMarginBeforeForChild(const LayoutBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginBefore();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginAfter();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" sides of the child box.  We can just return the raw margin in this case.
    return marginBeforeForChild(child);
}

LayoutUnit LayoutBlock::collapsedMarginAfterForChild(const LayoutBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginAfter();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginBefore();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" side of the child box.  We can just return the raw margin in this case.
    return marginAfterForChild(child);
}

bool LayoutBlock::hasMarginBeforeQuirk(const LayoutBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool LayoutBlock::hasMarginAfterQuirk(const LayoutBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

const char* LayoutBlock::name() const
{
    ASSERT_NOT_REACHED();
    return "LayoutBlock";
}

LayoutBlock* LayoutBlock::createAnonymousWithParentAndDisplay(const LayoutObject* parent, EDisplay display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    EDisplay newDisplay;
    LayoutBlock* newBox = nullptr;
    if (display == FLEX || display == INLINE_FLEX) {
        newBox = LayoutFlexibleBox::createAnonymous(&parent->document());
        newDisplay = FLEX;
    } else {
        newBox = LayoutBlockFlow::createAnonymous(&parent->document());
        newDisplay = BLOCK;
    }

    RefPtr<ComputedStyle> newStyle = ComputedStyle::createAnonymousStyleWithDisplay(parent->styleRef(), newDisplay);
    parent->updateAnonymousChildStyle(*newBox, *newStyle);
    newBox->setStyle(newStyle.release());
    return newBox;
}

bool LayoutBlock::recalcNormalFlowChildOverflowIfNeeded(LayoutObject* layoutObject)
{
    if (layoutObject->isOutOfFlowPositioned() || !layoutObject->needsOverflowRecalcAfterStyleChange())
        return false;

    ASSERT(layoutObject->isLayoutBlock());
    return toLayoutBlock(layoutObject)->recalcOverflowAfterStyleChange();
}

bool LayoutBlock::recalcChildOverflowAfterStyleChange()
{
    ASSERT(childNeedsOverflowRecalcAfterStyleChange());
    clearChildNeedsOverflowRecalcAfterStyleChange();

    bool childrenOverflowChanged = false;

    if (childrenInline()) {
        ASSERT_WITH_SECURITY_IMPLICATION(isLayoutBlockFlow());
        childrenOverflowChanged = toLayoutBlockFlow(this)->recalcInlineChildrenOverflowAfterStyleChange();
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (recalcNormalFlowChildOverflowIfNeeded(box))
                childrenOverflowChanged = true;
        }
    }

    return recalcPositionedDescendantsOverflowAfterStyleChange() || childrenOverflowChanged;
}

bool LayoutBlock::recalcPositionedDescendantsOverflowAfterStyleChange()
{
    bool childrenOverflowChanged = false;

    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return childrenOverflowChanged;

    for (auto* box : *positionedDescendants) {
        if (!box->needsOverflowRecalcAfterStyleChange())
            continue;
        LayoutBlock* block = toLayoutBlock(box);
        if (!block->recalcOverflowAfterStyleChange() || box->style()->position() == FixedPosition)
            continue;

        childrenOverflowChanged = true;
    }
    return childrenOverflowChanged;
}

bool LayoutBlock::recalcOverflowAfterStyleChange()
{
    ASSERT(needsOverflowRecalcAfterStyleChange());

    bool childrenOverflowChanged = false;
    if (childNeedsOverflowRecalcAfterStyleChange())
        childrenOverflowChanged = recalcChildOverflowAfterStyleChange();

    if (!selfNeedsOverflowRecalcAfterStyleChange() && !childrenOverflowChanged)
        return false;

    clearSelfNeedsOverflowRecalcAfterStyleChange();
    // If the current block needs layout, overflow will be recalculated during
    // layout time anyway. We can safely exit here.
    if (needsLayout())
        return false;

    LayoutUnit oldClientAfterEdge = hasOverflowModel() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, true);

    if (hasOverflowClip())
        layer()->getScrollableArea()->updateAfterOverflowRecalc();

    return !hasOverflowClip();
}

// Called when a positioned object moves but doesn't necessarily change size.  A simplified layout is attempted
// that just updates the object's position. If the size does change, the object remains dirty.
bool LayoutBlock::tryLayoutDoingPositionedMovementOnly()
{
    LayoutUnit oldWidth = logicalWidth();
    LogicalExtentComputedValues computedValues;
    logicalExtentAfterUpdatingLogicalWidth(logicalTop(), computedValues);
    // If we shrink to fit our width may have changed, so we still need full layout.
    if (oldWidth != computedValues.m_extent)
        return false;
    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);

    LayoutUnit oldHeight = logicalHeight();
    LayoutUnit oldIntrinsicContentLogicalHeight = intrinsicContentLogicalHeight();

    setIntrinsicContentLogicalHeight(contentLogicalHeight());
    computeLogicalHeight(oldHeight, logicalTop(), computedValues);

    if (oldHeight != computedValues.m_extent && (hasPercentHeightDescendants() || isFlexibleBox())) {
        setIntrinsicContentLogicalHeight(oldIntrinsicContentLogicalHeight);
        return false;
    }

    setLogicalHeight(computedValues.m_extent);
    setLogicalTop(computedValues.m_position);
    setMarginBefore(computedValues.m_margins.m_before);
    setMarginAfter(computedValues.m_margins.m_after);

    return true;
}

#if ENABLE(ASSERT)
void LayoutBlock::checkPositionedObjectsNeedLayout()
{
    if (!gPositionedDescendantsMap)
        return;

    if (TrackedLayoutBoxListHashSet* positionedDescendantSet = positionedObjects()) {
        TrackedLayoutBoxListHashSet::const_iterator end = positionedDescendantSet->end();
        for (TrackedLayoutBoxListHashSet::const_iterator it = positionedDescendantSet->begin(); it != end; ++it) {
            LayoutBox* currBox = *it;
            ASSERT(!currBox->needsLayout());
        }
    }
}

#endif

} // namespace blink
