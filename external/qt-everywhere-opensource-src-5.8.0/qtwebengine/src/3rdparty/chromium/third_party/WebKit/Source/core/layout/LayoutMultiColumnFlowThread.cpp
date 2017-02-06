/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutMultiColumnFlowThread.h"

#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutView.h"
#include "core/layout/MultiColumnFragmentainerGroup.h"
#include "core/layout/ViewFragmentationContext.h"

namespace blink {

LayoutMultiColumnFlowThread::LayoutMultiColumnFlowThread()
    : m_lastSetWorkedOn(nullptr)
    , m_columnCount(1)
    , m_columnHeightsChanged(false)
    , m_progressionIsInline(true)
    , m_isBeingEvacuated(false)
{
    setIsInsideFlowThread(true);
}

LayoutMultiColumnFlowThread::~LayoutMultiColumnFlowThread()
{
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::createAnonymous(Document& document, const ComputedStyle& parentStyle)
{
    LayoutMultiColumnFlowThread* layoutObject = new LayoutMultiColumnFlowThread();
    layoutObject->setDocumentForAnonymous(&document);
    layoutObject->setStyle(ComputedStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return layoutObject;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::firstMultiColumnSet() const
{
    for (LayoutObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isLayoutMultiColumnSet())
            return toLayoutMultiColumnSet(sibling);
    }
    return nullptr;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::lastMultiColumnSet() const
{
    for (LayoutObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isLayoutMultiColumnSet())
            return toLayoutMultiColumnSet(sibling);
    }
    return nullptr;
}

static inline bool isMultiColumnContainer(const LayoutObject& object)
{
    if (!object.isLayoutBlockFlow())
        return false;
    return toLayoutBlockFlow(object).multiColumnFlowThread();
}

// Return true if there's nothing that prevents the specified object from being in the ancestor
// chain between some column spanner and its containing multicol container. A column spanner needs
// the multicol container to be its containing block, so that the spanner is able to escape the flow
// thread. (Everything contained by the flow thread is split into columns, but this is precisely
// what shouldn't be done to a spanner, since it's supposed to span all columns.)
//
// We require that the parent of the spanner participate in the block formatting context established
// by the multicol container (i.e. that there are no BFCs or other formatting contexts
// in-between). We also require that there be no transforms, since transforms insist on being in the
// containing block chain for everything inside it, which conflicts with a spanners's need to have
// the multicol container as its direct containing block. We may also not put spanners inside
// objects that don't support fragmentation.
static inline bool canContainSpannerInParentFragmentationContext(const LayoutObject& object)
{
    if (!object.isLayoutBlockFlow())
        return false;
    const LayoutBlockFlow& blockFlow = toLayoutBlockFlow(object);
    return !blockFlow.createsNewFormattingContext()
        && !blockFlow.hasTransformRelatedProperty()
        && blockFlow.getPaginationBreakability() != LayoutBox::ForbidBreaks
        && !isMultiColumnContainer(blockFlow);
}

static inline bool hasAnyColumnSpanners(const LayoutMultiColumnFlowThread& flowThread)
{
    LayoutBox* firstBox = flowThread.firstMultiColumnBox();
    return firstBox && (firstBox != flowThread.lastMultiColumnBox() || firstBox->isLayoutMultiColumnSpannerPlaceholder());
}

// Find the next layout object that has the multicol container in its containing block chain, skipping nested multicol containers.
static LayoutObject* nextInPreOrderAfterChildrenSkippingOutOfFlow(LayoutMultiColumnFlowThread* flowThread, LayoutObject* descendant)
{
    ASSERT(descendant->isDescendantOf(flowThread));
    LayoutObject* object = descendant->nextInPreOrderAfterChildren(flowThread);
    while (object) {
        // Walk through the siblings and find the first one which is either in-flow or has this
        // flow thread as its containing block flow thread.
        if (!object->isOutOfFlowPositioned())
            break;
        if (object->containingBlock()->flowThreadContainingBlock() == flowThread) {
            // This out-of-flow object is still part of the flow thread, because its containing
            // block (probably relatively positioned) is part of the flow thread.
            break;
        }
        object = object->nextInPreOrderAfterChildren(flowThread);
    }
    if (!object)
        return nullptr;
#if ENABLE(ASSERT)
    // Make sure that we didn't stumble into an inner multicol container.
    for (LayoutObject* walker = object->parent(); walker && walker != flowThread; walker = walker->parent())
        ASSERT(!isMultiColumnContainer(*walker));
#endif
    return object;
}

// Find the previous layout object that has the multicol container in its containing block chain, skipping nested multicol containers.
static LayoutObject* previousInPreOrderSkippingOutOfFlow(LayoutMultiColumnFlowThread* flowThread, LayoutObject* descendant)
{
    ASSERT(descendant->isDescendantOf(flowThread));
    LayoutObject* object = descendant->previousInPreOrder(flowThread);
    while (object && object != flowThread) {
        if (object->isColumnSpanAll()) {
            LayoutMultiColumnFlowThread* placeholderFlowThread = toLayoutBox(object)->spannerPlaceholder()->flowThread();
            if (placeholderFlowThread == flowThread)
                break;
            // We're inside an inner multicol container. We have no business there. Continue on the outside.
            object = placeholderFlowThread->parent();
            ASSERT(object->isDescendantOf(flowThread));
            continue;
        }
        if (object->flowThreadContainingBlock() == flowThread) {
            LayoutObject* ancestor;
            for (ancestor = object->parent(); ; ancestor = ancestor->parent()) {
                if (ancestor == flowThread)
                    return object;
                if (isMultiColumnContainer(*ancestor)) {
                    // We're inside an inner multicol container. We have no business there.
                    break;
                }
            }
            object = ancestor;
            ASSERT(ancestor->isDescendantOf(flowThread));
            continue; // Continue on the outside of the inner flow thread.
        }
        // We're inside something that's out-of-flow. Keep looking upwards and backwards in the tree.
        object = object->previousInPreOrder(flowThread);
    }
    if (!object || object == flowThread)
        return nullptr;
#if ENABLE(ASSERT)
    // Make sure that we didn't stumble into an inner multicol container.
    for (LayoutObject* walker = object->parent(); walker && walker != flowThread; walker = walker->parent())
        ASSERT(!isMultiColumnContainer(*walker));
#endif
    return object;
}

static LayoutObject* firstLayoutObjectInSet(LayoutMultiColumnSet* multicolSet)
{
    LayoutBox* sibling = multicolSet->previousSiblingMultiColumnBox();
    if (!sibling)
        return multicolSet->flowThread()->firstChild();
    // Adjacent column content sets should not occur. We would have no way of figuring out what each
    // of them contains then.
    ASSERT(sibling->isLayoutMultiColumnSpannerPlaceholder());
    LayoutBox* spanner = toLayoutMultiColumnSpannerPlaceholder(sibling)->layoutObjectInFlowThread();
    return nextInPreOrderAfterChildrenSkippingOutOfFlow(multicolSet->multiColumnFlowThread(), spanner);
}

static LayoutObject* lastLayoutObjectInSet(LayoutMultiColumnSet* multicolSet)
{
    LayoutBox* sibling = multicolSet->nextSiblingMultiColumnBox();
    if (!sibling)
        return nullptr; // By right we should return lastLeafChild() here, but the caller doesn't care, so just return 0.
    // Adjacent column content sets should not occur. We would have no way of figuring out what each
    // of them contains then.
    ASSERT(sibling->isLayoutMultiColumnSpannerPlaceholder());
    LayoutBox* spanner = toLayoutMultiColumnSpannerPlaceholder(sibling)->layoutObjectInFlowThread();
    return previousInPreOrderSkippingOutOfFlow(multicolSet->multiColumnFlowThread(), spanner);
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::mapDescendantToColumnSet(LayoutObject* layoutObject) const
{
    ASSERT(!containingColumnSpannerPlaceholder(layoutObject)); // should not be used for spanners or content inside them.
    ASSERT(layoutObject != this);
    ASSERT(layoutObject->isDescendantOf(this));
    ASSERT(layoutObject->containingBlock()->isDescendantOf(this)); // Out-of-flow objects don't belong in column sets.
    ASSERT(layoutObject->flowThreadContainingBlock() == this);
    ASSERT(!layoutObject->isLayoutMultiColumnSet());
    ASSERT(!layoutObject->isLayoutMultiColumnSpannerPlaceholder());
    LayoutMultiColumnSet* multicolSet = firstMultiColumnSet();
    if (!multicolSet)
        return nullptr;
    if (!multicolSet->nextSiblingMultiColumnSet())
        return multicolSet;

    // This is potentially SLOW! But luckily very uncommon. You would have to dynamically insert a
    // spanner into the middle of column contents to need this.
    for (; multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        LayoutObject* firstLayoutObject = firstLayoutObjectInSet(multicolSet);
        LayoutObject* lastLayoutObject = lastLayoutObjectInSet(multicolSet);
        ASSERT(firstLayoutObject);

        for (LayoutObject* walker = firstLayoutObject; walker; walker = walker->nextInPreOrder(this)) {
            if (walker == layoutObject)
                return multicolSet;
            if (walker == lastLayoutObject)
                break;
        }
    }

    return nullptr;
}

LayoutMultiColumnSpannerPlaceholder* LayoutMultiColumnFlowThread::containingColumnSpannerPlaceholder(const LayoutObject* descendant) const
{
    ASSERT(descendant->isDescendantOf(this));

    if (!hasAnyColumnSpanners(*this))
        return nullptr;

    // We have spanners. See if the layoutObject in question is one or inside of one then.
    for (const LayoutObject* ancestor = descendant; ancestor && ancestor != this; ancestor = ancestor->parent()) {
        if (LayoutMultiColumnSpannerPlaceholder* placeholder = ancestor->spannerPlaceholder())
            return placeholder;
    }
    return nullptr;
}

void LayoutMultiColumnFlowThread::populate()
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    ASSERT(!nextSibling());
    // Reparent children preceding the flow thread into the flow thread. It's multicol content
    // now. At this point there's obviously nothing after the flow thread, but layoutObjects (column
    // sets and spanners) will be inserted there as we insert elements into the flow thread.
    multicolContainer->removeFloatingObjectsFromDescendants();
    multicolContainer->moveChildrenTo(this, multicolContainer->firstChild(), this, true);
}

void LayoutMultiColumnFlowThread::evacuateAndDestroy()
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    m_isBeingEvacuated = true;

    // Remove all sets and spanners.
    while (LayoutBox* columnBox = firstMultiColumnBox()) {
        ASSERT(columnBox->isAnonymous());
        columnBox->destroy();
    }

    ASSERT(!previousSibling());
    ASSERT(!nextSibling());

    // Finally we can promote all flow thread's children. Before we move them to the flow thread's
    // container, we need to unregister the flow thread, so that they aren't just re-added again to
    // the flow thread that we're trying to empty.
    multicolContainer->resetMultiColumnFlowThread();
    moveAllChildrenTo(multicolContainer, true);

    // We used to manually nuke the line box tree here, but that should happen automatically when
    // moving children around (the code above).
    ASSERT(!firstLineBox());

    destroy();
}

LayoutUnit LayoutMultiColumnFlowThread::maxColumnLogicalHeight() const
{
    if (m_columnHeightAvailable) {
        // If height is non-auto, it's already constrained against max-height as well.
        // Just return it.
        return m_columnHeightAvailable;
    }
    const LayoutBlockFlow* multicolBlock = multiColumnBlockFlow();
    Length logicalMaxHeight = multicolBlock->style()->logicalMaxHeight();
    if (!logicalMaxHeight.isMaxSizeNone()) {
        LayoutUnit resolvedLogicalMaxHeight = multicolBlock->computeContentLogicalHeight(MaxSize, logicalMaxHeight, LayoutUnit(-1));
        if (resolvedLogicalMaxHeight != -1)
            return resolvedLogicalMaxHeight;
    }
    return LayoutUnit::max();
}

LayoutUnit LayoutMultiColumnFlowThread::tallestUnbreakableLogicalHeight(LayoutUnit offsetInFlowThread) const
{
    if (LayoutMultiColumnSet* multicolSet = columnSetAtBlockOffset(offsetInFlowThread))
        return multicolSet->tallestUnbreakableLogicalHeight();
    return LayoutUnit();
}

LayoutSize LayoutMultiColumnFlowThread::columnOffset(const LayoutPoint& point) const
{
    return flowThreadTranslationAtPoint(point, CoordinateSpaceConversion::Containing);
}

bool LayoutMultiColumnFlowThread::needsNewWidth() const
{
    LayoutUnit newWidth;
    unsigned dummyColumnCount; // We only care if used column-width changes.
    calculateColumnCountAndWidth(newWidth, dummyColumnCount);
    return newWidth != logicalWidth();
}

bool LayoutMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (LayoutMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->isPageLogicalHeightKnown();
    return false;
}

LayoutSize LayoutMultiColumnFlowThread::flowThreadTranslationAtOffset(LayoutUnit offsetInFlowThread, CoordinateSpaceConversion mode) const
{
    if (!hasValidColumnSetInfo())
        return LayoutSize(0, 0);
    LayoutMultiColumnSet* columnSet = columnSetAtBlockOffset(offsetInFlowThread);
    if (!columnSet)
        return LayoutSize(0, 0);
    return columnSet->flowThreadTranslationAtOffset(offsetInFlowThread, mode);
}

LayoutSize LayoutMultiColumnFlowThread::flowThreadTranslationAtPoint(const LayoutPoint& flowThreadPoint, CoordinateSpaceConversion mode) const
{
    LayoutPoint flippedPoint = flipForWritingMode(flowThreadPoint);
    LayoutUnit blockOffset = isHorizontalWritingMode() ? flippedPoint.y() : flippedPoint.x();
    return flowThreadTranslationAtOffset(blockOffset, mode);
}

LayoutPoint LayoutMultiColumnFlowThread::flowThreadPointToVisualPoint(const LayoutPoint& flowThreadPoint) const
{
    return flowThreadPoint + flowThreadTranslationAtPoint(flowThreadPoint, CoordinateSpaceConversion::Visual);
}

LayoutPoint LayoutMultiColumnFlowThread::visualPointToFlowThreadPoint(const LayoutPoint& visualPoint) const
{
    LayoutUnit blockOffset = isHorizontalWritingMode() ? visualPoint.y() : visualPoint.x();
    const LayoutMultiColumnSet* columnSet = nullptr;
    for (const LayoutMultiColumnSet* candidate = firstMultiColumnSet(); candidate; candidate = candidate->nextSiblingMultiColumnSet()) {
        columnSet = candidate;
        if (candidate->logicalBottom() > blockOffset)
            break;
    }
    return columnSet ? columnSet->visualPointToFlowThreadPoint(toLayoutPoint(visualPoint + location() - columnSet->location())) : visualPoint;
}

int LayoutMultiColumnFlowThread::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    LayoutUnit baselineInFlowThread = LayoutUnit(LayoutFlowThread::inlineBlockBaseline(lineDirection));
    LayoutMultiColumnSet* columnSet = columnSetAtBlockOffset(baselineInFlowThread);
    if (!columnSet)
        return baselineInFlowThread.toInt();
    return (baselineInFlowThread - columnSet->pageLogicalTopForOffset(baselineInFlowThread)).ceil();
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::columnSetAtBlockOffset(LayoutUnit offset) const
{
    if (LayoutMultiColumnSet* columnSet = m_lastSetWorkedOn) {
        // Layout in progress. We are calculating the set heights as we speak, so the column set range
        // information is not up to date.
        while (columnSet->logicalTopInFlowThread() > offset) {
            // Sometimes we have to use a previous set. This happens when we're working with a block
            // that contains a spanner (so that there's a column set both before and after the
            // spanner, and both sets contain said block).
            LayoutMultiColumnSet* previousSet = columnSet->previousSiblingMultiColumnSet();
            if (!previousSet)
                break;
            columnSet = previousSet;
        }
        return columnSet;
    }

    ASSERT(!m_columnSetsInvalidated);
    if (m_multiColumnSetList.isEmpty())
        return nullptr;
    if (offset <= 0)
        return m_multiColumnSetList.first();

    MultiColumnSetSearchAdapter adapter(offset);
    m_multiColumnSetIntervalTree.allOverlapsWithAdapter<MultiColumnSetSearchAdapter>(adapter);

    // If no set was found, the offset is in the flow thread overflow.
    if (!adapter.result() && !m_multiColumnSetList.isEmpty())
        return m_multiColumnSetList.last();
    return adapter.result();
}

void LayoutMultiColumnFlowThread::layoutColumns(SubtreeLayoutScope& layoutScope)
{
    // Since we ended up here, it means that the multicol container (our parent) needed
    // layout. Since contents of the multicol container are diverted to the flow thread, the flow
    // thread needs layout as well.
    layoutScope.setChildNeedsLayout(this);

    if (FragmentationContext* enclosingFragmentationContext = this->enclosingFragmentationContext()) {
        m_blockOffsetInEnclosingFragmentationContext = multiColumnBlockFlow()->offsetFromLogicalTopOfFirstPage();
        m_blockOffsetInEnclosingFragmentationContext += multiColumnBlockFlow()->borderAndPaddingBefore();

        if (LayoutMultiColumnFlowThread* enclosingFlowThread = enclosingFragmentationContext->associatedFlowThread()) {
            if (LayoutMultiColumnSet* firstSet = firstMultiColumnSet()) {
                // Before we can start to lay out the contents of this multicol container, we need
                // to make sure that all ancestor multicol containers have established a row to hold
                // the first column contents of this container (this multicol container may start at
                // the beginning of a new outer row). Without sufficient rows in all ancestor
                // multicol containers, we may use the wrong column height.
                LayoutUnit offset = m_blockOffsetInEnclosingFragmentationContext + firstSet->logicalTopFromMulticolContentEdge();
                enclosingFlowThread->appendNewFragmentainerGroupIfNeeded(offset, AssociateWithLatterPage);
            }
        }
    }

    for (LayoutBox* columnBox = firstMultiColumnBox(); columnBox; columnBox = columnBox->nextSiblingMultiColumnBox()) {
        if (!columnBox->isLayoutMultiColumnSet()) {
            ASSERT(columnBox->isLayoutMultiColumnSpannerPlaceholder()); // no other type is expected.
            continue;
        }
        LayoutMultiColumnSet* columnSet = toLayoutMultiColumnSet(columnBox);
        layoutScope.setChildNeedsLayout(columnSet);
        if (!m_columnHeightsChanged) {
            // This is the initial layout pass. We need to reset the column height, because contents
            // typically have changed.
            columnSet->resetColumnHeight();
        }
        // Since column sets are regular block flow objects, and their position is changed in
        // regular block layout code (with no means for the multicol code to notice unless we add
        // hooks there), store the previous position now. If it changes in the imminent layout
        // pass, we may have to rebalance its columns.
        columnSet->storeOldPosition();
    }

    m_columnHeightsChanged = false;
    invalidateColumnSets();
    layout();
    validateColumnSets();
}

void LayoutMultiColumnFlowThread::columnRuleStyleDidChange()
{
    for (LayoutMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->setShouldDoFullPaintInvalidation(PaintInvalidationStyleChange);
}

bool LayoutMultiColumnFlowThread::removeSpannerPlaceholderIfNoLongerValid(LayoutBox* spannerObjectInFlowThread)
{
    ASSERT(spannerObjectInFlowThread->spannerPlaceholder());
    if (descendantIsValidColumnSpanner(spannerObjectInFlowThread))
        return false; // Still a valid spanner.

    // No longer a valid spanner. Get rid of the placeholder.
    destroySpannerPlaceholder(spannerObjectInFlowThread->spannerPlaceholder());
    ASSERT(!spannerObjectInFlowThread->spannerPlaceholder());

    // We may have a new containing block, since we're no longer a spanner. Mark it for relayout.
    spannerObjectInFlowThread->containingBlock()->setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::ColumnsChanged);

    // Now generate a column set for this ex-spanner, if needed and none is there for us already.
    flowThreadDescendantWasInserted(spannerObjectInFlowThread);

    return true;
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::enclosingFlowThread() const
{
    if (isLayoutPagedFlowThread()) {
        // Paged overflow containers should never be fragmented by enclosing fragmentation
        // contexts. They are to be treated as unbreakable content.
        return nullptr;
    }
    if (multiColumnBlockFlow()->isInsideFlowThread())
        return toLayoutMultiColumnFlowThread(locateFlowThreadContainingBlockOf(*multiColumnBlockFlow()));
    return nullptr;
}

FragmentationContext* LayoutMultiColumnFlowThread::enclosingFragmentationContext() const
{
    if (LayoutMultiColumnFlowThread* enclosingFlowThread = this->enclosingFlowThread())
        return enclosingFlowThread;
    return view()->fragmentationContext();
}

void LayoutMultiColumnFlowThread::appendNewFragmentainerGroupIfNeeded(LayoutUnit offsetInFlowThread, PageBoundaryRule pageBoundaryRule)
{
    if (!isPageLogicalHeightKnown()) {
        // If we have no clue about the height of the multicol container, bail. This situation
        // occurs initially when an auto-height multicol container is nested inside another
        // auto-height multicol container. We need at least an estimated height of the outer
        // multicol container before we can check what an inner fragmentainer group has room for.
        // Its height is indefinite for now.
        return;
    }
    // TODO(mstensho): If pageBoundaryRule is AssociateWithFormerPage, offsetInFlowThread is an
    // endpoint-exclusive offset, i.e. the offset just after the bottom of some object. So, ideally,
    // columnSetAtBlockOffset() should be informed about this (i.e. take a PageBoundaryRule
    // argument). This is not the only place with this issue; see also
    // pageRemainingLogicalHeightForOffset().
    LayoutMultiColumnSet* columnSet = columnSetAtBlockOffset(offsetInFlowThread);
    if (columnSet->isInitialHeightCalculated()) {
        // We only insert additional fragmentainer groups in the initial layout pass. We only want
        // to balance columns in the last fragmentainer group (if we need to balance at all), so we
        // want that last fragmentainer group to be the same one in all layout passes that follow.
        return;
    }

    if (!columnSet->hasFragmentainerGroupForColumnAt(offsetInFlowThread, pageBoundaryRule)) {
        FragmentationContext* enclosingFragmentationContext = this->enclosingFragmentationContext();
        if (!enclosingFragmentationContext)
            return; // Not nested. We'll never need more rows than the one we already have then.
        ASSERT(!isLayoutPagedFlowThread());

        // We have run out of columns here, so we need to add at least one more row to hold more
        // columns.
        LayoutMultiColumnFlowThread* enclosingFlowThread = enclosingFragmentationContext->associatedFlowThread();
        do {
            if (enclosingFlowThread) {
                // When we add a new row here, it implicitly means that we're inserting another
                // column in our enclosing multicol container. That in turn may mean that we've run
                // out of columns there too. Need to insert additional rows in ancestral multicol
                // containers before doing it in the descendants, in order to get the height
                // constraints right down there.
                const MultiColumnFragmentainerGroup& lastRow = columnSet->lastFragmentainerGroup();
                // The top offset where where the new fragmentainer group will start in this column
                // set, converted to the coordinate space of the enclosing multicol container.
                LayoutUnit logicalOffsetInOuter = lastRow.blockOffsetInEnclosingFragmentationContext() + lastRow.logicalHeight();
                enclosingFlowThread->appendNewFragmentainerGroupIfNeeded(logicalOffsetInOuter, AssociateWithLatterPage);
            }

            const MultiColumnFragmentainerGroup& newRow = columnSet->appendNewFragmentainerGroup();
            // Zero-height rows should really not occur here, but if it does anyway, break, so that
            // we don't get stuck in an infinite loop.
            ASSERT(newRow.logicalHeight() > 0);
            if (newRow.logicalHeight() <= 0)
                break;
        } while (!columnSet->hasFragmentainerGroupForColumnAt(offsetInFlowThread, pageBoundaryRule));
    }
}

bool LayoutMultiColumnFlowThread::isFragmentainerLogicalHeightKnown()
{
    return isPageLogicalHeightKnown();
}

LayoutUnit LayoutMultiColumnFlowThread::fragmentainerLogicalHeightAt(LayoutUnit blockOffset)
{
    return pageLogicalHeightForOffset(blockOffset);
}

LayoutUnit LayoutMultiColumnFlowThread::remainingLogicalHeightAt(LayoutUnit blockOffset)
{
    return pageRemainingLogicalHeightForOffset(blockOffset, AssociateWithLatterPage);
}

void LayoutMultiColumnFlowThread::calculateColumnCountAndWidth(LayoutUnit& width, unsigned& count) const
{
    LayoutBlock* columnBlock = multiColumnBlockFlow();
    const ComputedStyle* columnStyle = columnBlock->style();
    LayoutUnit availableWidth = columnBlock->contentLogicalWidth();
    LayoutUnit columnGap = LayoutUnit(columnBlock->columnGap());
    LayoutUnit computedColumnWidth = max(LayoutUnit(1), LayoutUnit(columnStyle->columnWidth()));
    unsigned computedColumnCount = max<int>(1, columnStyle->columnCount());

    ASSERT(!columnStyle->hasAutoColumnCount() || !columnStyle->hasAutoColumnWidth());
    if (columnStyle->hasAutoColumnWidth() && !columnStyle->hasAutoColumnCount()) {
        count = computedColumnCount;
        width = ((availableWidth - ((count - 1) * columnGap)) / count).clampNegativeToZero();
    } else if (!columnStyle->hasAutoColumnWidth() && columnStyle->hasAutoColumnCount()) {
        count = std::max(LayoutUnit(1), (availableWidth + columnGap) / (computedColumnWidth + columnGap));
        width = ((availableWidth + columnGap) / count) - columnGap;
    } else {
        count = std::max(std::min(LayoutUnit(computedColumnCount), (availableWidth + columnGap) / (computedColumnWidth + columnGap)), LayoutUnit(1));
        width = ((availableWidth + columnGap) / count) - columnGap;
    }
}

void LayoutMultiColumnFlowThread::createAndInsertMultiColumnSet(LayoutBox* insertBefore)
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    LayoutMultiColumnSet* newSet = LayoutMultiColumnSet::createAnonymous(*this, multicolContainer->styleRef());
    multicolContainer->LayoutBlock::addChild(newSet, insertBefore);
    invalidateColumnSets();

    // We cannot handle immediate column set siblings (and there's no need for it, either).
    // There has to be at least one spanner separating them.
    ASSERT(!newSet->previousSiblingMultiColumnBox() || !newSet->previousSiblingMultiColumnBox()->isLayoutMultiColumnSet());
    ASSERT(!newSet->nextSiblingMultiColumnBox() || !newSet->nextSiblingMultiColumnBox()->isLayoutMultiColumnSet());
}

void LayoutMultiColumnFlowThread::createAndInsertSpannerPlaceholder(LayoutBox* spannerObjectInFlowThread, LayoutObject* insertedBeforeInFlowThread)
{
    LayoutBox* insertBeforeColumnBox = nullptr;
    LayoutMultiColumnSet* setToSplit = nullptr;
    if (insertedBeforeInFlowThread) {
        // The spanner is inserted before something. Figure out what this entails. If the
        // next object is a spanner too, it means that we can simply insert a new spanner
        // placeholder in front of its placeholder.
        insertBeforeColumnBox = insertedBeforeInFlowThread->spannerPlaceholder();
        if (!insertBeforeColumnBox) {
            // The next object isn't a spanner; it's regular column content. Examine what
            // comes right before us in the flow thread, then.
            LayoutObject* previousLayoutObject = previousInPreOrderSkippingOutOfFlow(this, spannerObjectInFlowThread);
            if (!previousLayoutObject || previousLayoutObject == this) {
                // The spanner is inserted as the first child of the multicol container,
                // which means that we simply insert a new spanner placeholder at the
                // beginning.
                insertBeforeColumnBox = firstMultiColumnBox();
            } else if (LayoutMultiColumnSpannerPlaceholder* previousPlaceholder = containingColumnSpannerPlaceholder(previousLayoutObject)) {
                // Before us is another spanner. We belong right after it then.
                insertBeforeColumnBox = previousPlaceholder->nextSiblingMultiColumnBox();
            } else {
                // We're inside regular column content with both feet. Find out which column
                // set this is. It needs to be split it into two sets, so that we can insert
                // a new spanner placeholder between them.
                setToSplit = mapDescendantToColumnSet(previousLayoutObject);
                ASSERT(setToSplit == mapDescendantToColumnSet(insertedBeforeInFlowThread));
                insertBeforeColumnBox = setToSplit->nextSiblingMultiColumnBox();
                // We've found out which set that needs to be split. Now proceed to
                // inserting the spanner placeholder, and then insert a second column set.
            }
        }
        ASSERT(setToSplit || insertBeforeColumnBox);
    }

    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    LayoutMultiColumnSpannerPlaceholder* newPlaceholder = LayoutMultiColumnSpannerPlaceholder::createAnonymous(multicolContainer->styleRef(), *spannerObjectInFlowThread);
    ASSERT(!insertBeforeColumnBox || insertBeforeColumnBox->parent() == multicolContainer);
    multicolContainer->LayoutBlock::addChild(newPlaceholder, insertBeforeColumnBox);
    spannerObjectInFlowThread->setSpannerPlaceholder(*newPlaceholder);

    if (setToSplit)
        createAndInsertMultiColumnSet(insertBeforeColumnBox);
}

void LayoutMultiColumnFlowThread::destroySpannerPlaceholder(LayoutMultiColumnSpannerPlaceholder* placeholder)
{
    if (LayoutBox* nextColumnBox = placeholder->nextSiblingMultiColumnBox()) {
        LayoutBox* previousColumnBox = placeholder->previousSiblingMultiColumnBox();
        if (nextColumnBox && nextColumnBox->isLayoutMultiColumnSet()
            && previousColumnBox && previousColumnBox->isLayoutMultiColumnSet()) {
            // Need to merge two column sets.
            nextColumnBox->destroy();
            invalidateColumnSets();
        }
    }
    placeholder->destroy();
}

bool LayoutMultiColumnFlowThread::descendantIsValidColumnSpanner(LayoutObject* descendant) const
{
    // This method needs to behave correctly in the following situations:
    // - When the descendant doesn't have a spanner placeholder but should have one (return true)
    // - When the descendant doesn't have a spanner placeholder and still should not have one (return false)
    // - When the descendant has a spanner placeholder but should no longer have one (return false)
    // - When the descendant has a spanner placeholder and should still have one (return true)

    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant->isDescendantOf(this));

    // The spec says that column-span only applies to in-flow block-level elements.
    if (descendant->style()->getColumnSpan() != ColumnSpanAll || !descendant->isBox() || descendant->isInline() || descendant->isFloatingOrOutOfFlowPositioned())
        return false;

    if (!descendant->containingBlock()->isLayoutBlockFlow()) {
        // Needs to be in a block-flow container, and not e.g. a table.
        return false;
    }

    // This looks like a spanner, but if we're inside something unbreakable or something that
    // establishes a new formatting context, it's not to be treated as one.
    for (LayoutBox* ancestor = toLayoutBox(descendant)->parentBox(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor->isLayoutFlowThread()) {
            ASSERT(ancestor == this);
            return true;
        }
        if (!canContainSpannerInParentFragmentationContext(*ancestor))
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void LayoutMultiColumnFlowThread::addColumnSetToThread(LayoutMultiColumnSet* columnSet)
{
    if (LayoutMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        LayoutMultiColumnSetList::iterator it = m_multiColumnSetList.find(nextSet);
        ASSERT(it != m_multiColumnSetList.end());
        m_multiColumnSetList.insertBefore(it, columnSet);
    } else {
        m_multiColumnSetList.add(columnSet);
    }
}

void LayoutMultiColumnFlowThread::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (LayoutMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachFromFlowThread();
    multiColumnBlockFlow()->resetMultiColumnFlowThread();
    LayoutFlowThread::willBeRemovedFromTree();
}

void LayoutMultiColumnFlowThread::skipColumnSpanner(LayoutBox* layoutObject, LayoutUnit logicalTopInFlowThread)
{
    ASSERT(layoutObject->isColumnSpanAll());
    LayoutMultiColumnSpannerPlaceholder* placeholder = layoutObject->spannerPlaceholder();
    LayoutBox* previousColumnBox = placeholder->previousSiblingMultiColumnBox();
    if (previousColumnBox && previousColumnBox->isLayoutMultiColumnSet()) {
        LayoutMultiColumnSet* columnSet = toLayoutMultiColumnSet(previousColumnBox);
        if (logicalTopInFlowThread < columnSet->logicalTopInFlowThread())
            logicalTopInFlowThread = columnSet->logicalTopInFlowThread(); // Negative margins may cause this.
        columnSet->endFlow(logicalTopInFlowThread);
    }
    LayoutBox* nextColumnBox = placeholder->nextSiblingMultiColumnBox();
    if (nextColumnBox && nextColumnBox->isLayoutMultiColumnSet()) {
        LayoutMultiColumnSet* nextSet = toLayoutMultiColumnSet(nextColumnBox);
        m_lastSetWorkedOn = nextSet;
        nextSet->beginFlow(logicalTopInFlowThread);
    }

    // We'll lay out of spanners after flow thread layout has finished (during layout of the spanner
    // placeholders). There may be containing blocks for out-of-flow positioned descendants of the
    // spanner in the flow thread, so that out-of-flow objects inside the spanner will be laid out
    // as part of flow thread layout (even if the spanner itself won't). We need to add such
    // out-of-flow positioned objects to their containing blocks now, or they'll never get laid
    // out. Since it's non-trivial to determine if we need this, and where such out-of-flow objects
    // might be, just go through the whole subtree.
    for (LayoutObject* descendant = layoutObject->slowFirstChild(); descendant; descendant = descendant->nextInPreOrder()) {
        if (descendant->isBox() && descendant->isOutOfFlowPositioned())
            descendant->containingBlock()->insertPositionedObject(toLayoutBox(descendant));
    }
}

// When processing layout objects to remove or when processing layout objects that have just been
// inserted, certain types of objects should be skipped.
static bool shouldSkipInsertedOrRemovedChild(LayoutMultiColumnFlowThread* flowThread, const LayoutObject& child)
{
    if (child.isSVG() && !child.isSVGRoot()) {
        // Don't descend into SVG objects. What's in there is of no interest, and there might even
        // be a foreignObject there with column-span:all, which doesn't apply to us.
        return true;
    }
    if (child.isLayoutFlowThread()) {
        // Found an inner flow thread. We need to skip it and its descendants.
        return true;
    }
    if (child.isLayoutMultiColumnSet() || child.isLayoutMultiColumnSpannerPlaceholder()) {
        // Column sets and spanner placeholders in a child multicol context don't affect the parent
        // flow thread.
        return true;
    }
    if (child.isOutOfFlowPositioned() && child.containingBlock()->flowThreadContainingBlock() != flowThread) {
        // Out-of-flow with its containing block on the outside of the multicol container.
        return true;
    }
    return false;
}

void LayoutMultiColumnFlowThread::flowThreadDescendantWasInserted(LayoutObject* descendant)
{
    ASSERT(!m_isBeingEvacuated);
    // This method ensures that the list of column sets and spanner placeholders reflects the
    // multicol content after having inserted a descendant (or descendant subtree). See the header
    // file for more information. Go through the subtree that was just inserted and create column
    // sets (needed by regular column content) and spanner placeholders (one needed by each spanner)
    // where needed.
    if (shouldSkipInsertedOrRemovedChild(this, *descendant))
        return;
    LayoutObject* objectAfterSubtree = nextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant);
    LayoutObject* next;
    for (LayoutObject* layoutObject = descendant; layoutObject; layoutObject = next) {
        if (layoutObject != descendant && shouldSkipInsertedOrRemovedChild(this, *layoutObject)) {
            next = layoutObject->nextInPreOrderAfterChildren(descendant);
            continue;
        }
        next = layoutObject->nextInPreOrder(descendant);
        if (containingColumnSpannerPlaceholder(layoutObject))
            continue; // Inside a column spanner. Nothing to do, then.
        if (descendantIsValidColumnSpanner(layoutObject)) {
            // This layoutObject is a spanner, so it needs to establish a spanner placeholder.
            createAndInsertSpannerPlaceholder(toLayoutBox(layoutObject), objectAfterSubtree);
            continue;
        }
        // This layoutObject is regular column content (i.e. not a spanner). Create a set if necessary.
        if (objectAfterSubtree) {
            if (LayoutMultiColumnSpannerPlaceholder* placeholder = objectAfterSubtree->spannerPlaceholder()) {
                // If inserted right before a spanner, we need to make sure that there's a set for us there.
                LayoutBox* previous = placeholder->previousSiblingMultiColumnBox();
                if (!previous || !previous->isLayoutMultiColumnSet())
                    createAndInsertMultiColumnSet(placeholder);
            } else {
                // Otherwise, since |objectAfterSubtree| isn't a spanner, it has to mean that there's
                // already a set for that content. We can use it for this layoutObject too.
                ASSERT(mapDescendantToColumnSet(objectAfterSubtree));
                ASSERT(mapDescendantToColumnSet(layoutObject) == mapDescendantToColumnSet(objectAfterSubtree));
            }
        } else {
            // Inserting at the end. Then we just need to make sure that there's a column set at the end.
            LayoutBox* lastColumnBox = lastMultiColumnBox();
            if (!lastColumnBox || !lastColumnBox->isLayoutMultiColumnSet())
                createAndInsertMultiColumnSet();
        }
    }
}

void LayoutMultiColumnFlowThread::flowThreadDescendantWillBeRemoved(LayoutObject* descendant)
{
    // This method ensures that the list of column sets and spanner placeholders reflects the
    // multicol content that we'll be left with after removal of a descendant (or descendant
    // subtree). See the header file for more information. Removing content may mean that we need to
    // remove column sets and/or spanner placeholders.
    if (m_isBeingEvacuated)
        return;
    if (shouldSkipInsertedOrRemovedChild(this, *descendant))
        return;
    bool hadContainingPlaceholder = containingColumnSpannerPlaceholder(descendant);
    bool processedSomething = false;
    LayoutObject* next;
    // Remove spanner placeholders that are no longer needed, and merge column sets around them.
    for (LayoutObject* layoutObject = descendant; layoutObject; layoutObject = next) {
        if (layoutObject != descendant && shouldSkipInsertedOrRemovedChild(this, *layoutObject)) {
            next = layoutObject->nextInPreOrderAfterChildren(descendant);
            continue;
        }
        processedSomething = true;
        LayoutMultiColumnSpannerPlaceholder* placeholder = layoutObject->spannerPlaceholder();
        if (!placeholder) {
            next = layoutObject->nextInPreOrder(descendant);
            continue;
        }
        next = layoutObject->nextInPreOrderAfterChildren(descendant); // It's a spanner. Its children are of no interest to us.
        destroySpannerPlaceholder(placeholder);
    }
    if (hadContainingPlaceholder || !processedSomething)
        return; // No column content will be removed, so we can stop here.

    // Column content will be removed. Does this mean that we should destroy a column set?
    LayoutMultiColumnSpannerPlaceholder* adjacentPreviousSpannerPlaceholder = nullptr;
    LayoutObject* previousLayoutObject = previousInPreOrderSkippingOutOfFlow(this, descendant);
    if (previousLayoutObject && previousLayoutObject != this) {
        adjacentPreviousSpannerPlaceholder = containingColumnSpannerPlaceholder(previousLayoutObject);
        if (!adjacentPreviousSpannerPlaceholder)
            return; // Preceded by column content. Set still needed.
    }
    LayoutMultiColumnSpannerPlaceholder* adjacentNextSpannerPlaceholder = nullptr;
    LayoutObject* nextLayoutObject = nextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant);
    if (nextLayoutObject) {
        adjacentNextSpannerPlaceholder = containingColumnSpannerPlaceholder(nextLayoutObject);
        if (!adjacentNextSpannerPlaceholder)
            return; // Followed by column content. Set still needed.
    }
    // We have now determined that, with the removal of |descendant|, we should remove a column
    // set. Locate it and remove it. Do it without involving mapDescendantToColumnSet(), as that
    // might be very slow. Deduce the right set from the spanner placeholders that we've already
    // found.
    LayoutMultiColumnSet* columnSetToRemove;
    if (adjacentNextSpannerPlaceholder) {
        columnSetToRemove = toLayoutMultiColumnSet(adjacentNextSpannerPlaceholder->previousSiblingMultiColumnBox());
        ASSERT(!adjacentPreviousSpannerPlaceholder || columnSetToRemove == adjacentPreviousSpannerPlaceholder->nextSiblingMultiColumnBox());
    } else if (adjacentPreviousSpannerPlaceholder) {
        columnSetToRemove = toLayoutMultiColumnSet(adjacentPreviousSpannerPlaceholder->nextSiblingMultiColumnBox());
    } else {
        // If there were no adjacent spanners, it has to mean that there's only one column set,
        // since it's only spanners that may cause creation of multiple sets.
        columnSetToRemove = firstMultiColumnSet();
        ASSERT(columnSetToRemove);
        ASSERT(!columnSetToRemove->nextSiblingMultiColumnSet());
    }
    ASSERT(columnSetToRemove);
    columnSetToRemove->destroy();
}

static inline bool needsToReinsertIntoFlowThread(const ComputedStyle& oldStyle, const ComputedStyle& newStyle)
{
    // If we've become (or are about to become) a container for absolutely positioned descendants,
    // or if we're no longer going to be one, we need to re-evaluate the need for column
    // sets. There may be out-of-flow descendants further down that become part of the flow thread,
    // or cease to be part of the flow thread, because of this change.
    if (oldStyle.hasTransformRelatedProperty() != newStyle.hasTransformRelatedProperty())
        return true;
    return (oldStyle.hasInFlowPosition() && newStyle.position() == StaticPosition)
        || (newStyle.hasInFlowPosition() && oldStyle.position() == StaticPosition);
}

static inline bool needsToRemoveFromFlowThread(const ComputedStyle& oldStyle, const ComputedStyle& newStyle)
{
    // If an in-flow descendant goes out-of-flow, we may have to remove column sets and spanner placeholders.
    return (newStyle.hasOutOfFlowPosition() && !oldStyle.hasOutOfFlowPosition()) || needsToReinsertIntoFlowThread(oldStyle, newStyle);
}

static inline bool needsToInsertIntoFlowThread(const ComputedStyle& oldStyle, const ComputedStyle& newStyle)
{
    // If an out-of-flow descendant goes in-flow, we may have to insert column sets and spanner placeholders.
    return (!newStyle.hasOutOfFlowPosition() && oldStyle.hasOutOfFlowPosition()) || needsToReinsertIntoFlowThread(oldStyle, newStyle);
}

void LayoutMultiColumnFlowThread::flowThreadDescendantStyleWillChange(LayoutBox* descendant, StyleDifference diff, const ComputedStyle& newStyle)
{
    if (needsToRemoveFromFlowThread(descendant->styleRef(), newStyle))
        flowThreadDescendantWillBeRemoved(descendant);
}

void LayoutMultiColumnFlowThread::flowThreadDescendantStyleDidChange(LayoutBox* descendant, StyleDifference diff, const ComputedStyle& oldStyle)
{
    if (needsToInsertIntoFlowThread(oldStyle, descendant->styleRef())) {
        flowThreadDescendantWasInserted(descendant);
        return;
    }
    if (descendantIsValidColumnSpanner(descendant)) {
        // We went from being regular column content to becoming a spanner.
        ASSERT(!descendant->spannerPlaceholder());

        // First remove this as regular column content. Note that this will walk the entire subtree
        // of |descendant|. There might be spanners there (which won't be spanners anymore, since
        // we're not allowed to nest spanners), whose placeholders must die.
        flowThreadDescendantWillBeRemoved(descendant);

        createAndInsertSpannerPlaceholder(descendant, nextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant));
    }
}

void LayoutMultiColumnFlowThread::computePreferredLogicalWidths()
{
    LayoutFlowThread::computePreferredLogicalWidths();

    // The min/max intrinsic widths calculated really tell how much space elements need when
    // laid out inside the columns. In order to eventually end up with the desired column width,
    // we need to convert them to values pertaining to the multicol container.
    const LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    const ComputedStyle* multicolStyle = multicolContainer->style();
    int columnCount = multicolStyle->hasAutoColumnCount() ? 1 : multicolStyle->columnCount();
    LayoutUnit columnWidth;
    LayoutUnit gapExtra = LayoutUnit((columnCount - 1) * multicolContainer->columnGap());
    if (multicolStyle->hasAutoColumnWidth()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth * columnCount + gapExtra;
    } else {
        columnWidth = LayoutUnit(multicolStyle->columnWidth());
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, columnWidth);
    }
    // Note that if column-count is auto here, we should resolve it to calculate the maximum
    // intrinsic width, instead of pretending that it's 1. The only way to do that is by performing
    // a layout pass, but this is not an appropriate time or place for layout. The good news is that
    // if height is unconstrained and there are no explicit breaks, the resolved column-count really
    // should be 1.
    m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, columnWidth) * columnCount + gapExtra;
}

void LayoutMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

void LayoutMultiColumnFlowThread::updateLogicalWidth()
{
    LayoutUnit columnWidth;
    calculateColumnCountAndWidth(columnWidth, m_columnCount);
    setLogicalWidth(columnWidth);
}

void LayoutMultiColumnFlowThread::layout()
{
    ASSERT(!m_lastSetWorkedOn);
    m_lastSetWorkedOn = firstMultiColumnSet();
    if (m_lastSetWorkedOn)
        m_lastSetWorkedOn->beginFlow(LayoutUnit());
    LayoutFlowThread::layout();
    if (LayoutMultiColumnSet* lastSet = lastMultiColumnSet()) {
        ASSERT(lastSet == m_lastSetWorkedOn);
        if (!lastSet->nextSiblingMultiColumnBox()) {
            // Include trailing overflow in the last column set. The idea is that we will generate
            // additional columns and pages to hold that overflow, since people do write bad content
            // like <body style="height:0px"> in multi-column layouts.
            // TODO(mstensho): Once we support nested multicol, adding in overflow here may result
            // in the need for creating additional rows, since there may not be enough space
            // remaining in the currently last row.
            LayoutRect layoutRect = layoutOverflowRect();
            LayoutUnit logicalBottomInFlowThread = isHorizontalWritingMode() ? layoutRect.maxY() : layoutRect.maxX();
            ASSERT(logicalBottomInFlowThread >= logicalHeight());
            lastSet->endFlow(logicalBottomInFlowThread);
        }
    }
    m_lastSetWorkedOn = nullptr;
}

void LayoutMultiColumnFlowThread::contentWasLaidOut(LayoutUnit logicalBottomInFlowThreadAfterPagination)
{
    // Check if we need another fragmentainer group. If we've run out of columns in the last
    // fragmentainer group (column row), we need to insert another fragmentainer group to hold more
    // columns.

    // First figure out if there's any chance that we're nested at all. If we can be sure that
    // we're not, bail early. This code is run very often, and since locating a containing flow
    // thread has some cost (depending on tree depth), avoid calling
    // enclosingFragmentationContext() right away. This test may give some false positives (hence
    // the "mayBe"), if we're in an out-of-flow subtree and have an outer multicol container that
    // doesn't affect us, but that's okay. We'll discover that further down the road when trying to
    // locate our enclosing flow thread for real.
    bool mayBeNested = multiColumnBlockFlow()->isInsideFlowThread() || view()->fragmentationContext();
    if (!mayBeNested)
        return;
    appendNewFragmentainerGroupIfNeeded(logicalBottomInFlowThreadAfterPagination, AssociateWithFormerPage);
}

bool LayoutMultiColumnFlowThread::canSkipLayout(const LayoutBox& root) const
{
    // Objects containing spanners is all we need to worry about, so if there are no spanners at all
    // in this multicol container, we can just return the good news right away.
    if (!hasAnyColumnSpanners(*this))
        return true;

    LayoutObject* next;
    for (const LayoutObject* object = &root; object; object = next) {
        if (object->isColumnSpanAll()) {
            // A spanner potentially ends one fragmentainer group and begins a new one, and thus
            // determines the flow thread portion bottom and top of adjacent fragmentainer
            // groups. It's just too hard to guess these values without laying out.
            return false;
        }
        if (canContainSpannerInParentFragmentationContext(*object))
            next = object->nextInPreOrder(&root);
        else
            next = object->nextInPreOrderAfterChildren(&root);
    }
    return true;
}

} // namespace blink
