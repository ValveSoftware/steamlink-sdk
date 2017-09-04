// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ColumnBalancer.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/api/LineLayoutBlockFlow.h"

namespace blink {

ColumnBalancer::ColumnBalancer(const LayoutMultiColumnSet& columnSet,
                               LayoutUnit logicalTopInFlowThread,
                               LayoutUnit logicalBottomInFlowThread)
    : m_columnSet(columnSet),
      m_logicalTopInFlowThread(logicalTopInFlowThread),
      m_logicalBottomInFlowThread(logicalBottomInFlowThread) {
  DCHECK_GE(columnSet.usedColumnCount(), 1U);
}

void ColumnBalancer::traverse() {
  traverseSubtree(*columnSet().flowThread());
  ASSERT(!flowThreadOffset());
}

void ColumnBalancer::traverseSubtree(const LayoutBox& box) {
  if (box.childrenInline() && box.isLayoutBlockFlow()) {
    // Look for breaks between lines.
    traverseLines(toLayoutBlockFlow(box));
  }

  // Look for breaks between and inside block-level children. Even if this is a
  // block flow with inline children, there may be interesting floats to examine
  // here.
  traverseChildren(box);
}

void ColumnBalancer::traverseLines(const LayoutBlockFlow& blockFlow) {
  for (const RootInlineBox* line = blockFlow.firstRootBox(); line;
       line = line->nextRootBox()) {
    LayoutUnit lineTopInFlowThread =
        m_flowThreadOffset + line->lineTopWithLeading();
    if (lineTopInFlowThread < logicalTopInFlowThread())
      continue;
    if (lineTopInFlowThread >= logicalBottomInFlowThread())
      break;
    examineLine(*line);
  }
}

void ColumnBalancer::traverseChildren(const LayoutObject& object) {
  // The break-after value from the previous in-flow block-level object to be
  // joined with the break-before value of the next in-flow block-level sibling.
  EBreak previousBreakAfterValue = BreakAuto;

  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->isBox()) {
      // Keep traversing inside inlines. There may be floats there.
      if (child->isLayoutInline())
        traverseChildren(*child);
      continue;
    }

    const LayoutBox& childBox = toLayoutBox(*child);

    LayoutUnit borderEdgeOffset;
    LayoutUnit logicalTop = childBox.logicalTop();
    LayoutUnit logicalHeight = childBox.logicalHeightWithVisibleOverflow();
    // Floats' margins don't collapse with column boundaries, and we don't want
    // to break inside them, or separate them from the float's border box. Set
    // the offset to the margin-before edge (rather than border-before edge),
    // and include the block direction margins in the child height.
    if (childBox.isFloating()) {
      LayoutUnit marginBefore = childBox.marginBefore(object.style());
      LayoutUnit marginAfter = childBox.marginAfter(object.style());
      logicalHeight =
          std::max(logicalHeight, childBox.logicalHeight() + marginAfter);
      logicalTop -= marginBefore;
      logicalHeight += marginBefore;

      // As soon as we want to process content inside this child, though, we
      // need to get to its border-before edge.
      borderEdgeOffset = marginBefore;
    }

    if (m_flowThreadOffset + logicalTop + logicalHeight <=
        logicalTopInFlowThread()) {
      // This child is fully above the flow thread portion we're examining.
      continue;
    }
    if (m_flowThreadOffset + logicalTop >= logicalBottomInFlowThread()) {
      // This child is fully below the flow thread portion we're examining. We
      // cannot just stop here, though, thanks to negative margins.
      // So keep looking.
      continue;
    }
    if (childBox.isOutOfFlowPositioned() || childBox.isColumnSpanAll())
      continue;

    // Tables are wicked. Both table rows and table cells are relative to their
    // table section.
    LayoutUnit offsetForThisChild =
        childBox.isTableRow() ? LayoutUnit() : logicalTop;

    m_flowThreadOffset += offsetForThisChild;

    examineBoxAfterEntering(childBox, logicalHeight, previousBreakAfterValue);
    // Unless the child is unsplittable, or if the child establishes an inner
    // multicol container, we descend into its subtree for further examination.
    if (childBox.getPaginationBreakability() != LayoutBox::ForbidBreaks &&
        (!childBox.isLayoutBlockFlow() ||
         !toLayoutBlockFlow(childBox).multiColumnFlowThread())) {
      // We need to get to the border edge before processing content inside
      // this child. If the child is floated, we're currently at the margin
      // edge.
      m_flowThreadOffset += borderEdgeOffset;
      traverseSubtree(childBox);
      m_flowThreadOffset -= borderEdgeOffset;
    }
    previousBreakAfterValue = childBox.breakAfter();
    examineBoxBeforeLeaving(childBox, logicalHeight);

    m_flowThreadOffset -= offsetForThisChild;
  }
}

InitialColumnHeightFinder::InitialColumnHeightFinder(
    const LayoutMultiColumnSet& columnSet,
    LayoutUnit logicalTopInFlowThread,
    LayoutUnit logicalBottomInFlowThread)
    : ColumnBalancer(columnSet,
                     logicalTopInFlowThread,
                     logicalBottomInFlowThread) {
  m_shortestStruts.resize(columnSet.usedColumnCount());
  for (auto& strut : m_shortestStruts)
    strut = LayoutUnit::max();
  traverse();
  // We have now found each explicit / forced break, and their location. Now we
  // need to figure out how many additional implicit / soft breaks we need and
  // guess where they will occur, in order
  // to provide an initial column height.
  distributeImplicitBreaks();
}

LayoutUnit InitialColumnHeightFinder::initialMinimalBalancedHeight() const {
  LayoutUnit rowLogicalTop;
  if (m_contentRuns.size() > columnSet().usedColumnCount()) {
    // We have not inserted additional fragmentainer groups yet (because we
    // aren't able to calculate their constraints yet), but we already know for
    // sure that there'll be more than one of them, due to the number of forced
    // breaks in a nested multicol container. We will now attempt to take all
    // the imaginary rows into account and calculate a minimal balanced logical
    // height for everything.
    unsigned stride = columnSet().usedColumnCount();
    LayoutUnit rowStartOffset = logicalTopInFlowThread();
    for (unsigned i = 0; i < firstContentRunIndexInLastRow(); i += stride) {
      LayoutUnit rowEndOffset = m_contentRuns[i + stride - 1].breakOffset();
      float rowHeight = float(rowEndOffset - rowStartOffset) / float(stride);
      rowLogicalTop += LayoutUnit::fromFloatCeil(rowHeight);
      rowStartOffset = rowEndOffset;
    }
  }
  unsigned index = contentRunIndexWithTallestColumns();
  LayoutUnit startOffset = index > 0 ? m_contentRuns[index - 1].breakOffset()
                                     : logicalTopInFlowThread();
  LayoutUnit height = m_contentRuns[index].columnLogicalHeight(startOffset);
  return rowLogicalTop + std::max(height, m_tallestUnbreakableLogicalHeight);
}

void InitialColumnHeightFinder::examineBoxAfterEntering(
    const LayoutBox& box,
    LayoutUnit childLogicalHeight,
    EBreak previousBreakAfterValue) {
  if (isLogicalTopWithinBounds(flowThreadOffset() - box.paginationStrut())) {
    if (box.needsForcedBreakBefore(previousBreakAfterValue)) {
      addContentRun(flowThreadOffset());
    } else {
      if (isFirstAfterBreak(flowThreadOffset())) {
        // This box is first after a soft break.
        recordStrutBeforeOffset(flowThreadOffset(), box.paginationStrut());
      }
    }
  }

  if (box.getPaginationBreakability() != LayoutBox::AllowAnyBreaks) {
    m_tallestUnbreakableLogicalHeight =
        std::max(m_tallestUnbreakableLogicalHeight, childLogicalHeight);
    return;
  }
  // Need to examine inner multicol containers to find their tallest unbreakable
  // piece of content.
  if (!box.isLayoutBlockFlow())
    return;
  LayoutMultiColumnFlowThread* innerFlowThread =
      toLayoutBlockFlow(box).multiColumnFlowThread();
  if (!innerFlowThread || innerFlowThread->isLayoutPagedFlowThread())
    return;
  LayoutUnit offsetInInnerFlowThread =
      flowThreadOffset() -
      innerFlowThread->blockOffsetInEnclosingFragmentationContext();
  LayoutUnit innerUnbreakableHeight =
      innerFlowThread->tallestUnbreakableLogicalHeight(offsetInInnerFlowThread);
  m_tallestUnbreakableLogicalHeight =
      std::max(m_tallestUnbreakableLogicalHeight, innerUnbreakableHeight);
}

void InitialColumnHeightFinder::examineBoxBeforeLeaving(
    const LayoutBox& box,
    LayoutUnit childLogicalHeight) {}

static inline LayoutUnit columnLogicalHeightRequirementForLine(
    const ComputedStyle& style,
    const RootInlineBox& lastLine) {
  // We may require a certain minimum number of lines per page in order to
  // satisfy orphans and widows, and that may affect the minimum page height.
  unsigned minimumLineCount =
      std::max<unsigned>(style.orphans(), style.widows());
  const RootInlineBox* firstLine = &lastLine;
  for (unsigned i = 1; i < minimumLineCount && firstLine->prevRootBox(); i++)
    firstLine = firstLine->prevRootBox();
  return lastLine.lineBottomWithLeading() - firstLine->lineTopWithLeading();
}

void InitialColumnHeightFinder::examineLine(const RootInlineBox& line) {
  LayoutUnit lineTop = line.lineTopWithLeading();
  LayoutUnit lineTopInFlowThread = flowThreadOffset() + lineTop;
  LayoutUnit minimumLogialHeight =
      columnLogicalHeightRequirementForLine(line.block().styleRef(), line);
  m_tallestUnbreakableLogicalHeight =
      std::max(m_tallestUnbreakableLogicalHeight, minimumLogialHeight);
  ASSERT(
      isFirstAfterBreak(lineTopInFlowThread) || !line.paginationStrut() ||
      !isLogicalTopWithinBounds(lineTopInFlowThread - line.paginationStrut()));
  if (isFirstAfterBreak(lineTopInFlowThread))
    recordStrutBeforeOffset(lineTopInFlowThread, line.paginationStrut());
}

void InitialColumnHeightFinder::recordStrutBeforeOffset(
    LayoutUnit offsetInFlowThread,
    LayoutUnit strut) {
  unsigned columnCount = columnSet().usedColumnCount();
  ASSERT(m_shortestStruts.size() == columnCount);
  unsigned index = groupAtOffset(offsetInFlowThread)
                       .columnIndexAtOffset(offsetInFlowThread - strut,
                                            LayoutBox::AssociateWithLatterPage);
  if (index >= columnCount)
    return;
  m_shortestStruts[index] = std::min(m_shortestStruts[index], strut);
}

LayoutUnit InitialColumnHeightFinder::spaceUsedByStrutsAt(
    LayoutUnit offsetInFlowThread) const {
  unsigned stopBeforeColumn =
      groupAtOffset(offsetInFlowThread)
          .columnIndexAtOffset(offsetInFlowThread,
                               LayoutBox::AssociateWithLatterPage) +
      1;
  stopBeforeColumn = std::min(stopBeforeColumn, columnSet().usedColumnCount());
  ASSERT(stopBeforeColumn <= m_shortestStruts.size());
  LayoutUnit totalStrutSpace;
  for (unsigned i = 0; i < stopBeforeColumn; i++) {
    if (m_shortestStruts[i] != LayoutUnit::max())
      totalStrutSpace += m_shortestStruts[i];
  }
  return totalStrutSpace;
}

void InitialColumnHeightFinder::addContentRun(
    LayoutUnit endOffsetInFlowThread) {
  endOffsetInFlowThread -= spaceUsedByStrutsAt(endOffsetInFlowThread);
  if (!m_contentRuns.isEmpty() &&
      endOffsetInFlowThread <= m_contentRuns.last().breakOffset())
    return;
  // Append another item as long as we haven't exceeded used column count. What
  // ends up in the overflow area shouldn't affect column balancing. However, if
  // we're in a nested fragmentation context, we may still need to record all
  // runs, since there'll be no overflow area in the inline direction then, but
  // rather additional rows of columns in multiple outer fragmentainers.
  if (m_contentRuns.size() >= columnSet().usedColumnCount()) {
    const auto* flowThread = columnSet().multiColumnFlowThread();
    if (!flowThread->enclosingFragmentationContext() ||
        columnSet().newFragmentainerGroupsAllowed())
      return;
  }
  m_contentRuns.append(ContentRun(endOffsetInFlowThread));
}

unsigned InitialColumnHeightFinder::contentRunIndexWithTallestColumns() const {
  unsigned indexWithLargestHeight = 0;
  LayoutUnit largestHeight;
  LayoutUnit previousOffset = logicalTopInFlowThread();
  size_t runCount = m_contentRuns.size();
  ASSERT(runCount);
  for (size_t i = firstContentRunIndexInLastRow(); i < runCount; i++) {
    const ContentRun& run = m_contentRuns[i];
    LayoutUnit height = run.columnLogicalHeight(previousOffset);
    if (largestHeight < height) {
      largestHeight = height;
      indexWithLargestHeight = i;
    }
    previousOffset = run.breakOffset();
  }
  return indexWithLargestHeight;
}

void InitialColumnHeightFinder::distributeImplicitBreaks() {
  // Insert a final content run to encompass all content. This will include
  // overflow if we're at the end of the multicol container.
  addContentRun(logicalBottomInFlowThread());
  unsigned columnCount = m_contentRuns.size();

  // If there is room for more breaks (to reach the used value of column-count),
  // imagine that we insert implicit breaks at suitable locations. At any given
  // time, the content run with the currently tallest columns will get another
  // implicit break "inserted", which will increase its column count by one and
  // shrink its columns' height. Repeat until we have the desired total number
  // of breaks. The largest column height among the runs will then be the
  // initial column height for the balancer to use.
  if (columnCount > columnSet().usedColumnCount()) {
    // If we exceed used column-count (which we are allowed to do if we're at
    // the initial balancing pass for a multicol that lives inside another
    // to-be-balanced outer multicol container), we only care about content that
    // could end up in the last row. We need to pad up the number of columns, so
    // that all rows will contain as many columns as used column-count dictates.
    columnCount %= columnSet().usedColumnCount();
    // If there are just enough explicit breaks to fill all rows with the right
    // amount of columns, we won't be needing any implicit breaks.
    if (!columnCount)
      return;
  }
  while (columnCount < columnSet().usedColumnCount()) {
    unsigned index = contentRunIndexWithTallestColumns();
    m_contentRuns[index].assumeAnotherImplicitBreak();
    columnCount++;
  }
}

MinimumSpaceShortageFinder::MinimumSpaceShortageFinder(
    const LayoutMultiColumnSet& columnSet,
    LayoutUnit logicalTopInFlowThread,
    LayoutUnit logicalBottomInFlowThread)
    : ColumnBalancer(columnSet,
                     logicalTopInFlowThread,
                     logicalBottomInFlowThread),
      m_minimumSpaceShortage(LayoutUnit::max()),
      m_pendingStrut(LayoutUnit::min()),
      m_forcedBreaksCount(0) {
  traverse();
}

void MinimumSpaceShortageFinder::examineBoxAfterEntering(
    const LayoutBox& box,
    LayoutUnit childLogicalHeight,
    EBreak previousBreakAfterValue) {
  LayoutBox::PaginationBreakability breakability =
      box.getPaginationBreakability();

  // Look for breaks before the child box.
  if (isLogicalTopWithinBounds(flowThreadOffset() - box.paginationStrut())) {
    if (box.needsForcedBreakBefore(previousBreakAfterValue)) {
      m_forcedBreaksCount++;
    } else {
      if (isFirstAfterBreak(flowThreadOffset())) {
        // This box is first after a soft break.
        LayoutUnit strut = box.paginationStrut();
        // Figure out how much more space we would need to prevent it from being
        // pushed to the next column.
        recordSpaceShortage(childLogicalHeight - strut);
        if (breakability != LayoutBox::ForbidBreaks &&
            m_pendingStrut == LayoutUnit::min()) {
          // We now want to look for the first piece of unbreakable content
          // (e.g. a line or a block-displayed image) inside this block. That
          // ought to be a good candidate for minimum space shortage; a much
          // better one than reporting space shortage for the entire block
          // (which we'll also do (further down), in case we couldn't find
          // anything more suitable).
          m_pendingStrut = strut;
        }
      }
    }
  }

  if (breakability != LayoutBox::ForbidBreaks) {
    // See if this breakable box crosses column boundaries.
    LayoutUnit bottomInFlowThread = flowThreadOffset() + childLogicalHeight;
    const MultiColumnFragmentainerGroup& group =
        groupAtOffset(flowThreadOffset());
    if (isFirstAfterBreak(flowThreadOffset()) ||
        group.columnLogicalTopForOffset(flowThreadOffset()) !=
            group.columnLogicalTopForOffset(bottomInFlowThread)) {
      // If the child crosses a column boundary, record space shortage, in case
      // nothing inside it has already done so. The column balancer needs to
      // know by how much it has to stretch the columns to make more content
      // fit. If no breaks are reported (but do occur), the balancer will have
      // no clue. Only measure the space after the last column boundary, in case
      // it crosses more than one.
      LayoutUnit spaceUsedInLastColumn =
          bottomInFlowThread -
          group.columnLogicalTopForOffset(bottomInFlowThread);
      recordSpaceShortage(spaceUsedInLastColumn);
    }
  }

  // If this is an inner multicol container, look for space shortage inside it.
  if (!box.isLayoutBlockFlow())
    return;
  LayoutMultiColumnFlowThread* flowThread =
      toLayoutBlockFlow(box).multiColumnFlowThread();
  if (!flowThread || flowThread->isLayoutPagedFlowThread())
    return;
  for (const LayoutMultiColumnSet* columnSet =
           flowThread->firstMultiColumnSet();
       columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
    // Establish an inner shortage finder for this column set in the inner
    // multicol container. We need to let it walk through all fragmentainer
    // groups in one go, or we'd miss the column boundaries between each
    // fragmentainer group. We need to record space shortage there too.
    MinimumSpaceShortageFinder innerFinder(
        *columnSet, columnSet->logicalTopInFlowThread(),
        columnSet->logicalBottomInFlowThread());
    recordSpaceShortage(innerFinder.minimumSpaceShortage());
  }
}

void MinimumSpaceShortageFinder::examineBoxBeforeLeaving(
    const LayoutBox& box,
    LayoutUnit childLogicalHeight) {
  if (m_pendingStrut == LayoutUnit::min() ||
      box.getPaginationBreakability() != LayoutBox::ForbidBreaks)
    return;

  // The previous break was before a breakable block. Here's the first piece of
  // unbreakable content after / inside that block. We want to record the
  // distance from the top of the column to the bottom of this box as space
  // shortage.
  LayoutUnit logicalOffsetFromCurrentColumn =
      offsetFromColumnLogicalTop(flowThreadOffset());
  recordSpaceShortage(logicalOffsetFromCurrentColumn + childLogicalHeight -
                      m_pendingStrut);
  m_pendingStrut = LayoutUnit::min();
}

void MinimumSpaceShortageFinder::examineLine(const RootInlineBox& line) {
  LayoutUnit lineTop = line.lineTopWithLeading();
  LayoutUnit lineTopInFlowThread = flowThreadOffset() + lineTop;
  LayoutUnit lineHeight = line.lineBottomWithLeading() - lineTop;
  if (m_pendingStrut != LayoutUnit::min()) {
    // The previous break was before a breakable block. Here's the first line
    // after / inside that block. We want to record the distance from the top of
    // the column to the bottom of this box as space shortage.
    LayoutUnit logicalOffsetFromCurrentColumn =
        offsetFromColumnLogicalTop(lineTopInFlowThread);
    recordSpaceShortage(logicalOffsetFromCurrentColumn + lineHeight -
                        m_pendingStrut);
    m_pendingStrut = LayoutUnit::min();
    return;
  }
  ASSERT(
      isFirstAfterBreak(lineTopInFlowThread) || !line.paginationStrut() ||
      !isLogicalTopWithinBounds(lineTopInFlowThread - line.paginationStrut()));
  if (isFirstAfterBreak(lineTopInFlowThread))
    recordSpaceShortage(lineHeight - line.paginationStrut());

  // Even if the line box itself fits fine inside a column, some content may
  // overflow the line box bottom (due to restrictive line-height, for
  // instance). We should check if some portion of said overflow ends up in the
  // next column. That counts as space shortage.
  const MultiColumnFragmentainerGroup& group =
      groupAtOffset(lineTopInFlowThread);
  LayoutUnit lineBottomWithOverflow =
      lineTopInFlowThread + line.lineBottom() - lineTop;
  if (group.columnLogicalTopForOffset(lineTopInFlowThread) !=
      group.columnLogicalTopForOffset(lineBottomWithOverflow)) {
    LayoutUnit shortage =
        lineBottomWithOverflow -
        group.columnLogicalTopForOffset(lineBottomWithOverflow);
    recordSpaceShortage(shortage);
  }
}

}  // namespace blink
