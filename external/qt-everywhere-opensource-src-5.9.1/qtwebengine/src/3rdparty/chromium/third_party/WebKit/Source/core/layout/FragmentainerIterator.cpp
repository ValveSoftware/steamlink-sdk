// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/FragmentainerIterator.h"

#include "core/layout/LayoutMultiColumnSet.h"

namespace blink {

FragmentainerIterator::FragmentainerIterator(
    const LayoutFlowThread& flowThread,
    const LayoutRect& physicalBoundingBoxInFlowThread,
    const LayoutRect& clipRectInMulticolContainer)
    : m_flowThread(flowThread),
      m_clipRectInMulticolContainer(clipRectInMulticolContainer),
      m_currentFragmentainerGroupIndex(0) {
  // Put the bounds into flow thread-local coordinates by flipping it first.
  // This is how rectangles typically are represented in layout, i.e. with the
  // block direction coordinate flipped, if writing mode is vertical-rl.
  LayoutRect boundsInFlowThread = physicalBoundingBoxInFlowThread;
  m_flowThread.flipForWritingMode(boundsInFlowThread);

  if (m_flowThread.isHorizontalWritingMode()) {
    m_logicalTopInFlowThread = boundsInFlowThread.y();
    m_logicalBottomInFlowThread = boundsInFlowThread.maxY();
  } else {
    m_logicalTopInFlowThread = boundsInFlowThread.x();
    m_logicalBottomInFlowThread = boundsInFlowThread.maxX();
  }

  // Jump to the first interesting column set.
  m_currentColumnSet = flowThread.columnSetAtBlockOffset(
      m_logicalTopInFlowThread, LayoutBox::AssociateWithLatterPage);
  if (!m_currentColumnSet ||
      m_currentColumnSet->logicalTopInFlowThread() >=
          m_logicalBottomInFlowThread) {
    setAtEnd();
    return;
  }
  // Then find the first interesting fragmentainer group.
  m_currentFragmentainerGroupIndex =
      m_currentColumnSet->fragmentainerGroupIndexAtFlowThreadOffset(
          m_logicalTopInFlowThread, LayoutBox::AssociateWithLatterPage);

  // Now find the first and last fragmentainer we're interested in. We'll also
  // clip against the clip rect here. In case the clip rect doesn't intersect
  // with any of the fragmentainers, we have to move on to the next
  // fragmentainer group, and see if we find something there.
  if (!setFragmentainersOfInterest()) {
    moveToNextFragmentainerGroup();
    if (atEnd())
      return;
  }
}

void FragmentainerIterator::advance() {
  DCHECK(!atEnd());

  if (m_currentFragmentainerIndex < m_endFragmentainerIndex) {
    m_currentFragmentainerIndex++;
  } else {
    // That was the last fragmentainer to visit in this fragmentainer group.
    // Advance to the next group.
    moveToNextFragmentainerGroup();
    if (atEnd())
      return;
  }
}

LayoutSize FragmentainerIterator::paginationOffset() const {
  DCHECK(!atEnd());
  const MultiColumnFragmentainerGroup& group = currentGroup();
  LayoutUnit fragmentainerLogicalTopInFlowThread =
      group.logicalTopInFlowThread() +
      m_currentFragmentainerIndex * group.logicalHeight();
  return group.flowThreadTranslationAtOffset(
      fragmentainerLogicalTopInFlowThread, LayoutBox::AssociateWithLatterPage,
      CoordinateSpaceConversion::Visual);
}

LayoutRect FragmentainerIterator::fragmentainerInFlowThread() const {
  DCHECK(!atEnd());
  LayoutRect fragmentainerInFlowThread =
      currentGroup().flowThreadPortionRectAt(m_currentFragmentainerIndex);
  m_flowThread.flipForWritingMode(fragmentainerInFlowThread);
  return fragmentainerInFlowThread;
}

LayoutRect FragmentainerIterator::clipRectInFlowThread() const {
  DCHECK(!atEnd());
  LayoutRect clipRect = currentGroup().flowThreadPortionOverflowRectAt(
      m_currentFragmentainerIndex);
  m_flowThread.flipForWritingMode(clipRect);
  return clipRect;
}

const MultiColumnFragmentainerGroup& FragmentainerIterator::currentGroup()
    const {
  DCHECK(!atEnd());
  return m_currentColumnSet
      ->fragmentainerGroups()[m_currentFragmentainerGroupIndex];
}

void FragmentainerIterator::moveToNextFragmentainerGroup() {
  do {
    m_currentFragmentainerGroupIndex++;
    if (m_currentFragmentainerGroupIndex >=
        m_currentColumnSet->fragmentainerGroups().size()) {
      // That was the last fragmentainer group in this set. Advance to the next.
      m_currentColumnSet = m_currentColumnSet->nextSiblingMultiColumnSet();
      m_currentFragmentainerGroupIndex = 0;
      if (!m_currentColumnSet ||
          m_currentColumnSet->logicalTopInFlowThread() >=
              m_logicalBottomInFlowThread) {
        setAtEnd();
        return;  // No more sets or next set out of range. We're done.
      }
    }
    if (currentGroup().logicalTopInFlowThread() >=
        m_logicalBottomInFlowThread) {
      // This fragmentainer group doesn't intersect with the range we're
      // interested in. We're done.
      setAtEnd();
      return;
    }
  } while (!setFragmentainersOfInterest());
}

bool FragmentainerIterator::setFragmentainersOfInterest() {
  const MultiColumnFragmentainerGroup& group = currentGroup();

  // Figure out the start and end fragmentainers for the block range we're
  // interested in. We might not have to walk the entire fragmentainer group.
  group.columnIntervalForBlockRangeInFlowThread(
      m_logicalTopInFlowThread, m_logicalBottomInFlowThread,
      m_currentFragmentainerIndex, m_endFragmentainerIndex);

  if (hasClipRect()) {
    // Now intersect with the fragmentainers that actually intersect with the
    // visual clip rect, to narrow it down even further. The clip rect needs to
    // be relative to the current fragmentainer group.
    LayoutRect clipRect = m_clipRectInMulticolContainer;
    LayoutSize offset = group.flowThreadTranslationAtOffset(
        group.logicalTopInFlowThread(), LayoutBox::AssociateWithFormerPage,
        CoordinateSpaceConversion::Visual);
    clipRect.move(-offset);
    unsigned firstFragmentainerInClipRect, lastFragmentainerInClipRect;
    group.columnIntervalForVisualRect(clipRect, firstFragmentainerInClipRect,
                                      lastFragmentainerInClipRect);
    // If the two fragmentainer intervals are disjoint, there's nothing of
    // interest in this fragmentainer group.
    if (firstFragmentainerInClipRect > m_endFragmentainerIndex ||
        lastFragmentainerInClipRect < m_currentFragmentainerIndex)
      return false;
    if (m_currentFragmentainerIndex < firstFragmentainerInClipRect)
      m_currentFragmentainerIndex = firstFragmentainerInClipRect;
    if (m_endFragmentainerIndex > lastFragmentainerInClipRect)
      m_endFragmentainerIndex = lastFragmentainerInClipRect;
  }
  DCHECK(m_endFragmentainerIndex >= m_currentFragmentainerIndex);
  return true;
}

}  // namespace blink
