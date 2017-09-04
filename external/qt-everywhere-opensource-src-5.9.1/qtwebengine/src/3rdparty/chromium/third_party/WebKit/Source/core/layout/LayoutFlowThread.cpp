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

#include "core/layout/LayoutFlowThread.h"

#include "core/layout/FragmentainerIterator.h"
#include "core/layout/LayoutMultiColumnSet.h"

namespace blink {

LayoutFlowThread::LayoutFlowThread()
    : LayoutBlockFlow(nullptr),
      m_columnSetsInvalidated(false),
      m_pageLogicalSizeChanged(false) {}

LayoutFlowThread* LayoutFlowThread::locateFlowThreadContainingBlockOf(
    const LayoutObject& descendant) {
  ASSERT(descendant.isInsideFlowThread());
  LayoutObject* curr = const_cast<LayoutObject*>(&descendant);
  while (curr) {
    if (curr->isSVG() && !curr->isSVGRoot())
      return nullptr;
    if (curr->isLayoutFlowThread())
      return toLayoutFlowThread(curr);
    LayoutObject* container = curr->container();
    curr = curr->parent();
    while (curr != container) {
      if (curr->isLayoutFlowThread()) {
        // The nearest ancestor flow thread isn't in our containing block chain.
        // Then we aren't really part of any flow thread, and we should stop
        // looking. This happens when there are out-of-flow objects or column
        // spanners.
        return nullptr;
      }
      curr = curr->parent();
    }
  }
  return nullptr;
}

void LayoutFlowThread::removeColumnSetFromThread(
    LayoutMultiColumnSet* columnSet) {
  ASSERT(columnSet);
  m_multiColumnSetList.remove(columnSet);
  invalidateColumnSets();
  // Clear the interval tree right away, instead of leaving it around with dead
  // objects. Not that anyone _should_ try to access the interval tree when the
  // column sets are marked as invalid, but this is actually possible if other
  // parts of the engine has bugs that cause us to not lay out everything that
  // was marked for layout, so that LayoutObject::assertLaidOut() (and a LOT
  // of other assertions) fails.
  m_multiColumnSetIntervalTree.clear();
}

void LayoutFlowThread::invalidateColumnSets() {
  if (m_columnSetsInvalidated) {
    ASSERT(selfNeedsLayout());
    return;
  }

  setNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReason::ColumnsChanged);

  m_columnSetsInvalidated = true;
}

void LayoutFlowThread::validateColumnSets() {
  m_columnSetsInvalidated = false;
  // Called to get the maximum logical width for the columnSet.
  updateLogicalWidth();
  generateColumnSetIntervalTree();
}

bool LayoutFlowThread::mapToVisualRectInAncestorSpace(
    const LayoutBoxModelObject* ancestor,
    LayoutRect& rect,
    VisualRectFlags visualRectFlags) const {
  // A flow thread should never be an invalidation container.
  DCHECK(ancestor != this);
  rect = fragmentsBoundingBox(rect);
  return LayoutBlockFlow::mapToVisualRectInAncestorSpace(ancestor, rect,
                                                         visualRectFlags);
}

void LayoutFlowThread::layout() {
  m_pageLogicalSizeChanged = m_columnSetsInvalidated && everHadLayout();
  LayoutBlockFlow::layout();
  m_pageLogicalSizeChanged = false;
}

void LayoutFlowThread::computeLogicalHeight(
    LayoutUnit,
    LayoutUnit logicalTop,
    LogicalExtentComputedValues& computedValues) const {
  computedValues.m_position = logicalTop;
  computedValues.m_extent = LayoutUnit();

  for (LayoutMultiColumnSetList::const_iterator iter =
           m_multiColumnSetList.begin();
       iter != m_multiColumnSetList.end(); ++iter) {
    LayoutMultiColumnSet* columnSet = *iter;
    computedValues.m_extent += columnSet->logicalHeightInFlowThread();
  }
}

void LayoutFlowThread::absoluteQuadsForDescendant(const LayoutBox& descendant,
                                                  Vector<FloatQuad>& quads) {
  LayoutPoint offsetFromFlowThread;
  for (const LayoutObject* object = &descendant; object != this;) {
    const LayoutObject* container = object->container();
    offsetFromFlowThread += object->offsetFromContainer(container);
    object = container;
  }
  LayoutRect boundingRectInFlowThread(offsetFromFlowThread,
                                      descendant.frameRect().size());
  // Set up a fragments relative to the descendant, in the flow thread
  // coordinate space, and convert each of them, individually, to absolute
  // coordinates.
  for (FragmentainerIterator iterator(*this, boundingRectInFlowThread);
       !iterator.atEnd(); iterator.advance()) {
    LayoutRect fragment = boundingRectInFlowThread;
    // We use inclusiveIntersect() because intersect() would reset the
    // coordinates for zero-height objects.
    fragment.inclusiveIntersect(iterator.fragmentainerInFlowThread());
    fragment.moveBy(-offsetFromFlowThread);
    quads.append(descendant.localToAbsoluteQuad(FloatRect(fragment)));
  }
}

bool LayoutFlowThread::nodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& locationInContainer,
                                   const LayoutPoint& accumulatedOffset,
                                   HitTestAction hitTestAction) {
  if (hitTestAction == HitTestBlockBackground)
    return false;
  return LayoutBlockFlow::nodeAtPoint(result, locationInContainer,
                                      accumulatedOffset, hitTestAction);
}

LayoutUnit LayoutFlowThread::pageLogicalHeightForOffset(LayoutUnit offset) {
  LayoutMultiColumnSet* columnSet =
      columnSetAtBlockOffset(offset, AssociateWithLatterPage);
  if (!columnSet)
    return LayoutUnit();

  return columnSet->pageLogicalHeightForOffset(offset);
}

LayoutUnit LayoutFlowThread::pageRemainingLogicalHeightForOffset(
    LayoutUnit offset,
    PageBoundaryRule pageBoundaryRule) {
  LayoutMultiColumnSet* columnSet =
      columnSetAtBlockOffset(offset, pageBoundaryRule);
  if (!columnSet)
    return LayoutUnit();

  return columnSet->pageRemainingLogicalHeightForOffset(offset,
                                                        pageBoundaryRule);
}

void LayoutFlowThread::generateColumnSetIntervalTree() {
  // FIXME: Optimize not to clear the interval all the time. This implies
  // manually managing the tree nodes lifecycle.
  m_multiColumnSetIntervalTree.clear();
  m_multiColumnSetIntervalTree.initIfNeeded();
  for (auto columnSet : m_multiColumnSetList)
    m_multiColumnSetIntervalTree.add(MultiColumnSetIntervalTree::createInterval(
        columnSet->logicalTopInFlowThread(),
        columnSet->logicalBottomInFlowThread(), columnSet));
}

LayoutUnit LayoutFlowThread::nextLogicalTopForUnbreakableContent(
    LayoutUnit flowThreadOffset,
    LayoutUnit contentLogicalHeight) const {
  LayoutMultiColumnSet* columnSet =
      columnSetAtBlockOffset(flowThreadOffset, AssociateWithLatterPage);
  if (!columnSet)
    return flowThreadOffset;
  return columnSet->nextLogicalTopForUnbreakableContent(flowThreadOffset,
                                                        contentLogicalHeight);
}

LayoutRect LayoutFlowThread::fragmentsBoundingBox(
    const LayoutRect& layerBoundingBox) const {
  ASSERT(!RuntimeEnabledFeatures::slimmingPaintV2Enabled() ||
         !m_columnSetsInvalidated);

  LayoutRect result;
  for (auto* columnSet : m_multiColumnSetList)
    result.unite(columnSet->fragmentsBoundingBox(layerBoundingBox));

  return result;
}

void LayoutFlowThread::flowThreadToContainingCoordinateSpace(
    LayoutUnit& blockPosition,
    LayoutUnit& inlinePosition) const {
  LayoutPoint position(inlinePosition, blockPosition);
  // First we have to make |position| physical, because that's what offsetLeft()
  // expects and returns.
  if (!isHorizontalWritingMode())
    position = position.transposedPoint();
  position = flipForWritingMode(position);

  position.move(columnOffset(position));

  // Make |position| logical again, and read out the values.
  position = flipForWritingMode(position);
  if (!isHorizontalWritingMode())
    position = position.transposedPoint();
  blockPosition = position.y();
  inlinePosition = position.x();
}

void LayoutFlowThread::MultiColumnSetSearchAdapter::collectIfNeeded(
    const MultiColumnSetInterval& interval) {
  if (m_result)
    return;
  if (interval.low() <= m_offset && interval.high() > m_offset)
    m_result = interval.data();
}

}  // namespace blink
