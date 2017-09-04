/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/layout/LayoutFlexibleBox.h"

#include "core/frame/UseCounter.h"
#include "core/layout/FlexibleBoxAlgorithm.h"
#include "core/layout/LayoutState.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "wtf/AutoReset.h"
#include "wtf/MathExtras.h"
#include <limits>

namespace blink {

static bool hasAspectRatio(const LayoutBox& child) {
  return child.isImage() || child.isCanvas() || child.isVideo();
}

struct LayoutFlexibleBox::LineContext {
  LineContext(LayoutUnit crossAxisOffset,
              LayoutUnit crossAxisExtent,
              LayoutUnit maxAscent,
              Vector<FlexItem>&& flexItems)
      : crossAxisOffset(crossAxisOffset),
        crossAxisExtent(crossAxisExtent),
        maxAscent(maxAscent),
        flexItems(flexItems) {}

  LayoutUnit crossAxisOffset;
  LayoutUnit crossAxisExtent;
  LayoutUnit maxAscent;
  Vector<FlexItem> flexItems;
};

LayoutFlexibleBox::LayoutFlexibleBox(Element* element)
    : LayoutBlock(element),
      m_orderIterator(this),
      m_numberOfInFlowChildrenOnFirstLine(-1),
      m_hasDefiniteHeight(SizeDefiniteness::Unknown),
      m_inLayout(false) {
  DCHECK(!childrenInline());
}

LayoutFlexibleBox::~LayoutFlexibleBox() {}

LayoutFlexibleBox* LayoutFlexibleBox::createAnonymous(Document* document) {
  LayoutFlexibleBox* layoutObject = new LayoutFlexibleBox(nullptr);
  layoutObject->setDocumentForAnonymous(document);
  return layoutObject;
}

void LayoutFlexibleBox::computeIntrinsicLogicalWidths(
    LayoutUnit& minLogicalWidth,
    LayoutUnit& maxLogicalWidth) const {
  // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
  // honoring it though until the flex shorthand stops setting it to 0. See
  // https://bugs.webkit.org/show_bug.cgi?id=116117 and
  // https://crbug.com/240765.
  float previousMaxContentFlexFraction = -1;
  for (LayoutBox* child = firstChildBox(); child;
       child = child->nextSiblingBox()) {
    if (child->isOutOfFlowPositioned())
      continue;

    LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);

    LayoutUnit minPreferredLogicalWidth;
    LayoutUnit maxPreferredLogicalWidth;
    computeChildPreferredLogicalWidths(*child, minPreferredLogicalWidth,
                                       maxPreferredLogicalWidth);
    DCHECK_GE(minPreferredLogicalWidth, LayoutUnit());
    DCHECK_GE(maxPreferredLogicalWidth, LayoutUnit());
    minPreferredLogicalWidth += margin;
    maxPreferredLogicalWidth += margin;
    if (!isColumnFlow()) {
      maxLogicalWidth += maxPreferredLogicalWidth;
      if (isMultiline()) {
        // For multiline, the min preferred width is if you put a break between
        // each item.
        minLogicalWidth = std::max(minLogicalWidth, minPreferredLogicalWidth);
      } else {
        minLogicalWidth += minPreferredLogicalWidth;
      }
    } else {
      minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
      maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
    }

    previousMaxContentFlexFraction = countIntrinsicSizeForAlgorithmChange(
        maxPreferredLogicalWidth, child, previousMaxContentFlexFraction);
  }

  maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  minLogicalWidth = std::max(LayoutUnit(), minLogicalWidth);
  maxLogicalWidth = std::max(LayoutUnit(), maxLogicalWidth);

  LayoutUnit scrollbarWidth(scrollbarLogicalWidth());
  maxLogicalWidth += scrollbarWidth;
  minLogicalWidth += scrollbarWidth;
}

float LayoutFlexibleBox::countIntrinsicSizeForAlgorithmChange(
    LayoutUnit maxPreferredLogicalWidth,
    LayoutBox* child,
    float previousMaxContentFlexFraction) const {
  // Determine whether the new version of the intrinsic size algorithm of the
  // flexbox spec would produce a different result than our above algorithm.
  // The algorithm produces a different result iff the max-content flex
  // fraction (as defined in the new algorithm) is not identical for each flex
  // item.
  if (isColumnFlow())
    return previousMaxContentFlexFraction;
  Length flexBasis = child->styleRef().flexBasis();
  float flexGrow = child->styleRef().flexGrow();
  // A flex-basis of auto will lead to a max-content flex fraction of zero, so
  // just like an inflexible item it would compute to a size of max-content, so
  // we ignore it here.
  if (flexBasis.isAuto() || flexGrow == 0)
    return previousMaxContentFlexFraction;
  flexGrow = std::max(1.0f, flexGrow);
  float maxContentFlexFraction = maxPreferredLogicalWidth.toFloat() / flexGrow;
  if (previousMaxContentFlexFraction != -1 &&
      maxContentFlexFraction != previousMaxContentFlexFraction)
    UseCounter::count(document(),
                      UseCounter::FlexboxIntrinsicSizeAlgorithmIsDifferent);
  return maxContentFlexFraction;
}

static int synthesizedBaselineFromContentBox(const LayoutBox& box,
                                             LineDirectionMode direction) {
  if (direction == HorizontalLine) {
    return (box.size().height() - box.borderBottom() - box.paddingBottom() -
            box.verticalScrollbarWidth())
        .toInt();
  }
  return (box.size().width() - box.borderLeft() - box.paddingLeft() -
          box.horizontalScrollbarHeight())
      .toInt();
}

int LayoutFlexibleBox::baselinePosition(FontBaseline,
                                        bool,
                                        LineDirectionMode direction,
                                        LinePositionMode mode) const {
  DCHECK_EQ(mode, PositionOnContainingLine);
  int baseline = firstLineBoxBaseline();
  if (baseline == -1)
    baseline = synthesizedBaselineFromContentBox(*this, direction);

  return beforeMarginInLineDirection(direction) + baseline;
}

static const StyleContentAlignmentData& contentAlignmentNormalBehavior() {
  // The justify-content property applies along the main axis, but since
  // flexing in the main axis is controlled by flex, stretch behaves as
  // flex-start (ignoring the specified fallback alignment, if any).
  // https://drafts.csswg.org/css-align/#distribution-flex
  static const StyleContentAlignmentData normalBehavior = {
      ContentPositionNormal, ContentDistributionStretch};
  return normalBehavior;
}

int LayoutFlexibleBox::firstLineBoxBaseline() const {
  if (isWritingModeRoot() || m_numberOfInFlowChildrenOnFirstLine <= 0)
    return -1;
  LayoutBox* baselineChild = nullptr;
  int childNumber = 0;
  for (LayoutBox* child = m_orderIterator.first(); child;
       child = m_orderIterator.next()) {
    if (child->isOutOfFlowPositioned())
      continue;
    if (alignmentForChild(*child) == ItemPositionBaseline &&
        !hasAutoMarginsInCrossAxis(*child)) {
      baselineChild = child;
      break;
    }
    if (!baselineChild)
      baselineChild = child;

    ++childNumber;
    if (childNumber == m_numberOfInFlowChildrenOnFirstLine)
      break;
  }

  if (!baselineChild)
    return -1;

  if (!isColumnFlow() && hasOrthogonalFlow(*baselineChild))
    return (crossAxisExtentForChild(*baselineChild) +
            baselineChild->logicalTop())
        .toInt();
  if (isColumnFlow() && !hasOrthogonalFlow(*baselineChild))
    return (mainAxisExtentForChild(*baselineChild) +
            baselineChild->logicalTop())
        .toInt();

  int baseline = baselineChild->firstLineBoxBaseline();
  if (baseline == -1) {
    // FIXME: We should pass |direction| into firstLineBoxBaseline and stop
    // bailing out if we're a writing mode root. This would also fix some
    // cases where the flexbox is orthogonal to its container.
    LineDirectionMode direction =
        isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
    return (synthesizedBaselineFromContentBox(*baselineChild, direction) +
            baselineChild->logicalTop())
        .toInt();
  }

  return (baseline + baselineChild->logicalTop()).toInt();
}

int LayoutFlexibleBox::inlineBlockBaseline(LineDirectionMode direction) const {
  int baseline = firstLineBoxBaseline();
  if (baseline != -1)
    return baseline;

  int marginAscent =
      (direction == HorizontalLine ? marginTop() : marginRight()).toInt();
  return synthesizedBaselineFromContentBox(*this, direction) + marginAscent;
}

IntSize LayoutFlexibleBox::originAdjustmentForScrollbars() const {
  IntSize size;
  int adjustmentWidth = verticalScrollbarWidth();
  int adjustmentHeight = horizontalScrollbarHeight();
  if (!adjustmentWidth && !adjustmentHeight)
    return size;

  EFlexDirection flexDirection = style()->flexDirection();
  TextDirection textDirection = style()->direction();
  WritingMode writingMode = style()->getWritingMode();

  if (flexDirection == FlowRow) {
    if (textDirection == RTL) {
      if (writingMode == TopToBottomWritingMode)
        size.expand(adjustmentWidth, 0);
      else
        size.expand(0, adjustmentHeight);
    }
    if (writingMode == RightToLeftWritingMode)
      size.expand(adjustmentWidth, 0);
  } else if (flexDirection == FlowRowReverse) {
    if (textDirection == LTR) {
      if (writingMode == TopToBottomWritingMode)
        size.expand(adjustmentWidth, 0);
      else
        size.expand(0, adjustmentHeight);
    }
    if (writingMode == RightToLeftWritingMode)
      size.expand(adjustmentWidth, 0);
  } else if (flexDirection == FlowColumn) {
    if (writingMode == RightToLeftWritingMode)
      size.expand(adjustmentWidth, 0);
  } else {
    if (writingMode == TopToBottomWritingMode)
      size.expand(0, adjustmentHeight);
    else if (writingMode == LeftToRightWritingMode)
      size.expand(adjustmentWidth, 0);
  }
  return size;
}

bool LayoutFlexibleBox::hasTopOverflow() const {
  EFlexDirection flexDirection = style()->flexDirection();
  if (isHorizontalWritingMode())
    return flexDirection == FlowColumnReverse;
  return flexDirection ==
         (style()->isLeftToRightDirection() ? FlowRowReverse : FlowRow);
}

bool LayoutFlexibleBox::hasLeftOverflow() const {
  EFlexDirection flexDirection = style()->flexDirection();
  if (isHorizontalWritingMode())
    return flexDirection ==
           (style()->isLeftToRightDirection() ? FlowRowReverse : FlowRow);
  return flexDirection == FlowColumnReverse;
}

void LayoutFlexibleBox::removeChild(LayoutObject* child) {
  LayoutBlock::removeChild(child);
  m_intrinsicSizeAlongMainAxis.remove(child);
}

// TODO (lajava): Is this function still needed ? Every time the flex
// container's align-items value changes we propagate the diff to its children
// (see ComputedStyle::stylePropagationDiff).
void LayoutFlexibleBox::styleDidChange(StyleDifference diff,
                                       const ComputedStyle* oldStyle) {
  LayoutBlock::styleDidChange(diff, oldStyle);

  if (oldStyle && oldStyle->alignItemsPosition() == ItemPositionStretch &&
      diff.needsFullLayout()) {
    // Flex items that were previously stretching need to be relayed out so we
    // can compute new available cross axis space. This is only necessary for
    // stretching since other alignment values don't change the size of the
    // box.
    for (LayoutBox* child = firstChildBox(); child;
         child = child->nextSiblingBox()) {
      ItemPosition previousAlignment =
          child->styleRef()
              .resolvedAlignSelf(selfAlignmentNormalBehavior(), oldStyle)
              .position();
      if (previousAlignment == ItemPositionStretch &&
          previousAlignment !=
              child->styleRef()
                  .resolvedAlignSelf(selfAlignmentNormalBehavior(), style())
                  .position())
        child->setChildNeedsLayout(MarkOnlyThis);
    }
  }
}

void LayoutFlexibleBox::layoutBlock(bool relayoutChildren) {
  DCHECK(needsLayout());

  if (!relayoutChildren && simplifiedLayout())
    return;

  m_relaidOutChildren.clear();
  WTF::AutoReset<bool> reset1(&m_inLayout, true);
  DCHECK_EQ(m_hasDefiniteHeight, SizeDefiniteness::Unknown);

  if (updateLogicalWidthAndColumnWidth())
    relayoutChildren = true;

  SubtreeLayoutScope layoutScope(*this);
  LayoutUnit previousHeight = logicalHeight();
  setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delayClampScope;

  {
    TextAutosizer::LayoutScope textAutosizerLayoutScope(this, &layoutScope);
    LayoutState state(*this);

    m_numberOfInFlowChildrenOnFirstLine = -1;

    prepareOrderIteratorAndMargins();

    layoutFlexItems(relayoutChildren, layoutScope);
    if (PaintLayerScrollableArea::PreventRelayoutScope::relayoutNeeded()) {
      PaintLayerScrollableArea::FreezeScrollbarsScope freezeScrollbarsScope;
      prepareOrderIteratorAndMargins();
      layoutFlexItems(true, layoutScope);
      PaintLayerScrollableArea::PreventRelayoutScope::resetRelayoutNeeded();
    }

    if (logicalHeight() != previousHeight)
      relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isDocumentElement());

    // FIXME: css3/flexbox/repaint-rtl-column.html seems to issue paint
    // invalidations for more overflow than it needs to.
    computeOverflow(clientLogicalBottomAfterRepositioning());
  }

  updateLayerTransformAfterLayout();

  // We have to reset this, because changes to our ancestors' style can affect
  // this value. Also, this needs to be before we call updateAfterLayout, as
  // that function may re-enter this one.
  m_hasDefiniteHeight = SizeDefiniteness::Unknown;

  // Update our scroll information if we're overflow:auto/scroll/hidden now
  // that we know if we overflow or not.
  updateAfterLayout();

  clearNeedsLayout();
}

void LayoutFlexibleBox::paintChildren(const PaintInfo& paintInfo,
                                      const LayoutPoint& paintOffset) const {
  BlockPainter::paintChildrenOfFlexibleBox(*this, paintInfo, paintOffset);
}

void LayoutFlexibleBox::repositionLogicalHeightDependentFlexItems(
    Vector<LineContext>& lineContexts) {
  LayoutUnit crossAxisStartEdge =
      lineContexts.isEmpty() ? LayoutUnit() : lineContexts[0].crossAxisOffset;
  alignFlexLines(lineContexts);

  alignChildren(lineContexts);

  if (style()->flexWrap() == FlexWrapReverse)
    flipForWrapReverse(lineContexts, crossAxisStartEdge);

  // direction:rtl + flex-direction:column means the cross-axis direction is
  // flipped.
  flipForRightToLeftColumn(lineContexts);
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::clientLogicalBottomAfterRepositioning() {
  LayoutUnit maxChildLogicalBottom;
  for (LayoutBox* child = firstChildBox(); child;
       child = child->nextSiblingBox()) {
    if (child->isOutOfFlowPositioned())
      continue;
    LayoutUnit childLogicalBottom = logicalTopForChild(*child) +
                                    logicalHeightForChild(*child) +
                                    marginAfterForChild(*child);
    maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
  }
  return std::max(clientLogicalBottom(),
                  maxChildLogicalBottom + paddingAfter());
}

bool LayoutFlexibleBox::hasOrthogonalFlow(const LayoutBox& child) const {
  return isHorizontalFlow() != child.isHorizontalWritingMode();
}

bool LayoutFlexibleBox::isColumnFlow() const {
  return style()->isColumnFlexDirection();
}

bool LayoutFlexibleBox::isHorizontalFlow() const {
  if (isHorizontalWritingMode())
    return !isColumnFlow();
  return isColumnFlow();
}

bool LayoutFlexibleBox::isLeftToRightFlow() const {
  if (isColumnFlow())
    return style()->getWritingMode() == TopToBottomWritingMode ||
           style()->getWritingMode() == LeftToRightWritingMode;
  return style()->isLeftToRightDirection() ^
         (style()->flexDirection() == FlowRowReverse);
}

bool LayoutFlexibleBox::isMultiline() const {
  return style()->flexWrap() != FlexNoWrap;
}

Length LayoutFlexibleBox::flexBasisForChild(const LayoutBox& child) const {
  Length flexLength = child.style()->flexBasis();
  if (flexLength.isAuto())
    flexLength =
        isHorizontalFlow() ? child.style()->width() : child.style()->height();
  return flexLength;
}

LayoutUnit LayoutFlexibleBox::crossAxisExtentForChild(
    const LayoutBox& child) const {
  return isHorizontalFlow() ? child.size().height() : child.size().width();
}

static inline LayoutUnit constrainedChildIntrinsicContentLogicalHeight(
    const LayoutBox& child,
    LayoutUnit childIntrinsicContentLogicalHeight) {
  // TODO(cbiesinger): scrollbar height?
  return child.constrainLogicalHeightByMinMax(
      childIntrinsicContentLogicalHeight +
          child.borderAndPaddingLogicalHeight(),
      childIntrinsicContentLogicalHeight);
}

LayoutUnit LayoutFlexibleBox::childIntrinsicLogicalHeight(
    const LayoutBox& child) const {
  // This should only be called if the logical height is the cross size
  DCHECK(!hasOrthogonalFlow(child));
  if (needToStretchChildLogicalHeight(child)) {
    LayoutUnit childIntrinsicContentLogicalHeight;
    if (!child.styleRef().containsSize())
      childIntrinsicContentLogicalHeight =
          child.intrinsicContentLogicalHeight();
    return constrainedChildIntrinsicContentLogicalHeight(
        child, childIntrinsicContentLogicalHeight);
  }
  return child.logicalHeight();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::childIntrinsicLogicalWidth(
    const LayoutBox& child) const {
  // This should only be called if the logical width is the cross size
  DCHECK(hasOrthogonalFlow(child));
  // If our height is auto, make sure that our returned height is unaffected by
  // earlier layouts by returning the max preferred logical width
  if (!crossAxisLengthIsDefinite(child, child.styleRef().logicalWidth()))
    return child.maxPreferredLogicalWidth();

  return child.logicalWidth();
}

LayoutUnit LayoutFlexibleBox::crossAxisIntrinsicExtentForChild(
    const LayoutBox& child) const {
  return hasOrthogonalFlow(child) ? childIntrinsicLogicalWidth(child)
                                  : childIntrinsicLogicalHeight(child);
}

LayoutUnit LayoutFlexibleBox::mainAxisExtentForChild(
    const LayoutBox& child) const {
  return isHorizontalFlow() ? child.size().width() : child.size().height();
}

LayoutUnit LayoutFlexibleBox::mainAxisContentExtentForChildIncludingScrollbar(
    const LayoutBox& child) const {
  return isHorizontalFlow()
             ? child.contentWidth() + child.verticalScrollbarWidth()
             : child.contentHeight() + child.horizontalScrollbarHeight();
}

LayoutUnit LayoutFlexibleBox::crossAxisExtent() const {
  return isHorizontalFlow() ? size().height() : size().width();
}

LayoutUnit LayoutFlexibleBox::mainAxisExtent() const {
  return isHorizontalFlow() ? size().width() : size().height();
}

LayoutUnit LayoutFlexibleBox::crossAxisContentExtent() const {
  return isHorizontalFlow() ? contentHeight() : contentWidth();
}

LayoutUnit LayoutFlexibleBox::mainAxisContentExtent(
    LayoutUnit contentLogicalHeight) {
  if (isColumnFlow()) {
    LogicalExtentComputedValues computedValues;
    LayoutUnit borderPaddingAndScrollbar =
        borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
    LayoutUnit borderBoxLogicalHeight =
        contentLogicalHeight + borderPaddingAndScrollbar;
    computeLogicalHeight(borderBoxLogicalHeight, logicalTop(), computedValues);
    if (computedValues.m_extent == LayoutUnit::max())
      return computedValues.m_extent;
    return std::max(LayoutUnit(),
                    computedValues.m_extent - borderPaddingAndScrollbar);
  }
  return contentLogicalWidth();
}

LayoutUnit LayoutFlexibleBox::computeMainAxisExtentForChild(
    const LayoutBox& child,
    SizeType sizeType,
    const Length& size) {
  // If we have a horizontal flow, that means the main size is the width.
  // That's the logical width for horizontal writing modes, and the logical
  // height in vertical writing modes. For a vertical flow, main size is the
  // height, so it's the inverse. So we need the logical width if we have a
  // horizontal flow and horizontal writing mode, or vertical flow and vertical
  // writing mode. Otherwise we need the logical height.
  if (isHorizontalFlow() != child.styleRef().isHorizontalWritingMode()) {
    // We don't have to check for "auto" here - computeContentLogicalHeight
    // will just return -1 for that case anyway. It's safe to access
    // scrollbarLogicalHeight here because ComputeNextFlexLine will have
    // already forced layout on the child. We previously layed out the child
    // if necessary (see ComputeNextFlexLine and the call to
    // childHasIntrinsicMainAxisSize) so we can be sure that the two height
    // calls here will return up-to-date data.
    return child.computeContentLogicalHeight(
               sizeType, size, child.intrinsicContentLogicalHeight()) +
           child.scrollbarLogicalHeight();
  }
  // computeLogicalWidth always re-computes the intrinsic widths. However, when
  // our logical width is auto, we can just use our cached value. So let's do
  // that here. (Compare code in LayoutBlock::computePreferredLogicalWidths)
  LayoutUnit borderAndPadding = child.borderAndPaddingLogicalWidth();
  if (child.styleRef().logicalWidth().isAuto() && !hasAspectRatio(child)) {
    if (size.type() == MinContent)
      return child.minPreferredLogicalWidth() - borderAndPadding;
    if (size.type() == MaxContent)
      return child.maxPreferredLogicalWidth() - borderAndPadding;
  }
  return child.computeLogicalWidthUsing(sizeType, size, contentLogicalWidth(),
                                        this) -
         borderAndPadding;
}

LayoutFlexibleBox::TransformedWritingMode
LayoutFlexibleBox::getTransformedWritingMode() const {
  WritingMode mode = style()->getWritingMode();
  if (!isColumnFlow()) {
    static_assert(
        static_cast<TransformedWritingMode>(TopToBottomWritingMode) ==
                TransformedWritingMode::TopToBottomWritingMode &&
            static_cast<TransformedWritingMode>(LeftToRightWritingMode) ==
                TransformedWritingMode::LeftToRightWritingMode &&
            static_cast<TransformedWritingMode>(RightToLeftWritingMode) ==
                TransformedWritingMode::RightToLeftWritingMode,
        "WritingMode and TransformedWritingMode must match values.");
    return static_cast<TransformedWritingMode>(mode);
  }

  switch (mode) {
    case TopToBottomWritingMode:
      return style()->isLeftToRightDirection()
                 ? TransformedWritingMode::LeftToRightWritingMode
                 : TransformedWritingMode::RightToLeftWritingMode;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
      return style()->isLeftToRightDirection()
                 ? TransformedWritingMode::TopToBottomWritingMode
                 : TransformedWritingMode::BottomToTopWritingMode;
  }
  NOTREACHED();
  return TransformedWritingMode::TopToBottomWritingMode;
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderStart() const {
  if (isHorizontalFlow())
    return LayoutUnit(isLeftToRightFlow() ? borderLeft() : borderRight());
  return LayoutUnit(isLeftToRightFlow() ? borderTop() : borderBottom());
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderEnd() const {
  if (isHorizontalFlow())
    return LayoutUnit(isLeftToRightFlow() ? borderRight() : borderLeft());
  return LayoutUnit(isLeftToRightFlow() ? borderBottom() : borderTop());
}

LayoutUnit LayoutFlexibleBox::flowAwareBorderBefore() const {
  switch (getTransformedWritingMode()) {
    case TransformedWritingMode::TopToBottomWritingMode:
      return LayoutUnit(borderTop());
    case TransformedWritingMode::BottomToTopWritingMode:
      return LayoutUnit(borderBottom());
    case TransformedWritingMode::LeftToRightWritingMode:
      return LayoutUnit(borderLeft());
    case TransformedWritingMode::RightToLeftWritingMode:
      return LayoutUnit(borderRight());
  }
  NOTREACHED();
  return LayoutUnit(borderTop());
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::flowAwareBorderAfter() const {
  switch (getTransformedWritingMode()) {
    case TransformedWritingMode::TopToBottomWritingMode:
      return LayoutUnit(borderBottom());
    case TransformedWritingMode::BottomToTopWritingMode:
      return LayoutUnit(borderTop());
    case TransformedWritingMode::LeftToRightWritingMode:
      return LayoutUnit(borderRight());
    case TransformedWritingMode::RightToLeftWritingMode:
      return LayoutUnit(borderLeft());
  }
  NOTREACHED();
  return LayoutUnit(borderTop());
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingStart() const {
  if (isHorizontalFlow())
    return isLeftToRightFlow() ? paddingLeft() : paddingRight();
  return isLeftToRightFlow() ? paddingTop() : paddingBottom();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingEnd() const {
  if (isHorizontalFlow())
    return isLeftToRightFlow() ? paddingRight() : paddingLeft();
  return isLeftToRightFlow() ? paddingBottom() : paddingTop();
}

LayoutUnit LayoutFlexibleBox::flowAwarePaddingBefore() const {
  switch (getTransformedWritingMode()) {
    case TransformedWritingMode::TopToBottomWritingMode:
      return paddingTop();
    case TransformedWritingMode::BottomToTopWritingMode:
      return paddingBottom();
    case TransformedWritingMode::LeftToRightWritingMode:
      return paddingLeft();
    case TransformedWritingMode::RightToLeftWritingMode:
      return paddingRight();
  }
  NOTREACHED();
  return paddingTop();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::flowAwarePaddingAfter() const {
  switch (getTransformedWritingMode()) {
    case TransformedWritingMode::TopToBottomWritingMode:
      return paddingBottom();
    case TransformedWritingMode::BottomToTopWritingMode:
      return paddingTop();
    case TransformedWritingMode::LeftToRightWritingMode:
      return paddingRight();
    case TransformedWritingMode::RightToLeftWritingMode:
      return paddingLeft();
  }
  NOTREACHED();
  return paddingTop();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::flowAwareMarginStartForChild(
    const LayoutBox& child) const {
  if (isHorizontalFlow())
    return isLeftToRightFlow() ? child.marginLeft() : child.marginRight();
  return isLeftToRightFlow() ? child.marginTop() : child.marginBottom();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::flowAwareMarginEndForChild(
    const LayoutBox& child) const {
  if (isHorizontalFlow())
    return isLeftToRightFlow() ? child.marginRight() : child.marginLeft();
  return isLeftToRightFlow() ? child.marginBottom() : child.marginTop();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::flowAwareMarginBeforeForChild(
    const LayoutBox& child) const {
  switch (getTransformedWritingMode()) {
    case TransformedWritingMode::TopToBottomWritingMode:
      return child.marginTop();
    case TransformedWritingMode::BottomToTopWritingMode:
      return child.marginBottom();
    case TransformedWritingMode::LeftToRightWritingMode:
      return child.marginLeft();
    case TransformedWritingMode::RightToLeftWritingMode:
      return child.marginRight();
  }
  NOTREACHED();
  return marginTop();
}

LayoutUnit LayoutFlexibleBox::crossAxisMarginExtentForChild(
    const LayoutBox& child) const {
  return isHorizontalFlow() ? child.marginHeight() : child.marginWidth();
}

LayoutUnit LayoutFlexibleBox::crossAxisScrollbarExtent() const {
  return LayoutUnit(isHorizontalFlow() ? horizontalScrollbarHeight()
                                       : verticalScrollbarWidth());
}

LayoutUnit LayoutFlexibleBox::crossAxisScrollbarExtentForChild(
    const LayoutBox& child) const {
  return LayoutUnit(isHorizontalFlow() ? child.horizontalScrollbarHeight()
                                       : child.verticalScrollbarWidth());
}

LayoutPoint LayoutFlexibleBox::flowAwareLocationForChild(
    const LayoutBox& child) const {
  return isHorizontalFlow() ? child.location()
                            : child.location().transposedPoint();
}

bool LayoutFlexibleBox::useChildAspectRatio(const LayoutBox& child) const {
  if (!hasAspectRatio(child))
    return false;
  if (child.intrinsicSize().height() == 0) {
    // We can't compute a ratio in this case.
    return false;
  }
  Length crossSize;
  if (isHorizontalFlow())
    crossSize = child.styleRef().height();
  else
    crossSize = child.styleRef().width();
  return crossAxisLengthIsDefinite(child, crossSize);
}

LayoutUnit LayoutFlexibleBox::computeMainSizeFromAspectRatioUsing(
    const LayoutBox& child,
    Length crossSizeLength) const {
  DCHECK(hasAspectRatio(child));
  DCHECK_NE(child.intrinsicSize().height(), 0);

  LayoutUnit crossSize;
  if (crossSizeLength.isFixed()) {
    crossSize = LayoutUnit(crossSizeLength.value());
  } else {
    DCHECK(crossSizeLength.isPercentOrCalc());
    crossSize = hasOrthogonalFlow(child)
                    ? adjustBorderBoxLogicalWidthForBoxSizing(
                          valueForLength(crossSizeLength, contentWidth()))
                    : child.computePercentageLogicalHeight(crossSizeLength);
  }

  const LayoutSize& childIntrinsicSize = child.intrinsicSize();
  double ratio = childIntrinsicSize.width().toFloat() /
                 childIntrinsicSize.height().toFloat();
  if (isHorizontalFlow())
    return LayoutUnit(crossSize * ratio);
  return LayoutUnit(crossSize / ratio);
}

void LayoutFlexibleBox::setFlowAwareLocationForChild(
    LayoutBox& child,
    const LayoutPoint& location) {
  if (isHorizontalFlow())
    child.setLocationAndUpdateOverflowControlsIfNeeded(location);
  else
    child.setLocationAndUpdateOverflowControlsIfNeeded(
        location.transposedPoint());
}

bool LayoutFlexibleBox::mainAxisLengthIsDefinite(
    const LayoutBox& child,
    const Length& flexBasis) const {
  if (flexBasis.isAuto())
    return false;
  if (flexBasis.isPercentOrCalc()) {
    if (!isColumnFlow() || m_hasDefiniteHeight == SizeDefiniteness::Definite)
      return true;
    if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
      return false;
    bool definite = child.computePercentageLogicalHeight(flexBasis) != -1;
    if (m_inLayout) {
      // We can reach this code even while we're not laying ourselves out, such
      // as from mainSizeForPercentageResolution.
      m_hasDefiniteHeight =
          definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
    }
    return definite;
  }
  return true;
}

bool LayoutFlexibleBox::crossAxisLengthIsDefinite(const LayoutBox& child,
                                                  const Length& length) const {
  if (length.isAuto())
    return false;
  if (length.isPercentOrCalc()) {
    if (hasOrthogonalFlow(child) ||
        m_hasDefiniteHeight == SizeDefiniteness::Definite)
      return true;
    if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
      return false;
    bool definite = child.computePercentageLogicalHeight(length) != -1;
    m_hasDefiniteHeight =
        definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
    return definite;
  }
  // TODO(cbiesinger): Eventually we should support other types of sizes here.
  // Requires updating computeMainSizeFromAspectRatioUsing.
  return length.isFixed();
}

void LayoutFlexibleBox::cacheChildMainSize(const LayoutBox& child) {
  DCHECK(!child.needsLayout());
  LayoutUnit mainSize;
  if (hasOrthogonalFlow(child)) {
    mainSize = child.logicalHeight();
  } else {
    // The max preferred logical width includes the intrinsic scrollbar logical
    // width, which is only set for overflow: scroll. To handle overflow: auto,
    // we have to take scrollbarLogicalWidth() into account, and then subtract
    // the intrinsic width again so as to not double-count overflow: scroll
    // scrollbars.
    mainSize = child.maxPreferredLogicalWidth() +
               child.scrollbarLogicalWidth() - child.scrollbarLogicalWidth();
  }
  m_intrinsicSizeAlongMainAxis.set(&child, mainSize);
  m_relaidOutChildren.add(&child);
}

void LayoutFlexibleBox::clearCachedMainSizeForChild(const LayoutBox& child) {
  m_intrinsicSizeAlongMainAxis.remove(&child);
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::computeInnerFlexBaseSizeForChild(
    LayoutBox& child,
    LayoutUnit mainAxisBorderAndPadding,
    ChildLayoutType childLayoutType) {
  child.clearOverrideSize();

  if (child.isImage() || child.isVideo() || child.isCanvas())
    UseCounter::count(document(), UseCounter::AspectRatioFlexItem);

  Length flexBasis = flexBasisForChild(child);
  if (mainAxisLengthIsDefinite(child, flexBasis))
    return std::max(LayoutUnit(), computeMainAxisExtentForChild(
                                      child, MainOrPreferredSize, flexBasis));

  if (child.styleRef().containsSize())
    return LayoutUnit();

  // The flex basis is indefinite (=auto), so we need to compute the actual
  // width of the child. For the logical width axis we just use the preferred
  // width; for the height we need to lay out the child.
  LayoutUnit mainAxisExtent;
  if (hasOrthogonalFlow(child)) {
    if (childLayoutType == NeverLayout)
      return LayoutUnit();

    updateBlockChildDirtyBitsBeforeLayout(childLayoutType == ForceLayout,
                                          child);
    if (child.needsLayout() || childLayoutType == ForceLayout ||
        !m_intrinsicSizeAlongMainAxis.contains(&child)) {
      child.forceChildLayout();
      cacheChildMainSize(child);
    }
    mainAxisExtent = m_intrinsicSizeAlongMainAxis.get(&child);
  } else {
    // We don't need to add scrollbarLogicalWidth here because the preferred
    // width includes the scrollbar, even for overflow: auto.
    mainAxisExtent = child.maxPreferredLogicalWidth();
  }
  DCHECK_GE(mainAxisExtent - mainAxisBorderAndPadding, LayoutUnit())
      << mainAxisExtent << " - " << mainAxisBorderAndPadding;
  return mainAxisExtent - mainAxisBorderAndPadding;
}

void LayoutFlexibleBox::layoutFlexItems(bool relayoutChildren,
                                        SubtreeLayoutScope& layoutScope) {
  Vector<LineContext> lineContexts;
  LayoutUnit sumFlexBaseSize;
  double totalFlexGrow;
  double totalFlexShrink;
  double totalWeightedFlexShrink;
  LayoutUnit sumHypotheticalMainSize;

  PaintLayerScrollableArea::PreventRelayoutScope preventRelayoutScope(
      layoutScope);


  // Set up our master list of flex items. All of the rest of the algorithm
  // should work off this list of a subset.
  // TODO(cbiesinger): That second part is not yet true.
  ChildLayoutType layoutType = relayoutChildren ? ForceLayout : LayoutIfNeeded;
  Vector<FlexItem> allItems;
  m_orderIterator.first();
  for (LayoutBox* child = m_orderIterator.currentChild(); child;
       child = m_orderIterator.next()) {

    if (child->isOutOfFlowPositioned()) {
      // Out-of-flow children are not flex items, so we skip them here.
      prepareChildForPositionedLayout(*child);
      continue;
    }

    allItems.append(constructFlexItem(*child, layoutType));
  }

  const LayoutUnit lineBreakLength = mainAxisContentExtent(LayoutUnit::max());
  FlexLayoutAlgorithm flexAlgorithm(style(), lineBreakLength, allItems);
  LayoutUnit crossAxisOffset =
      flowAwareBorderBefore() + flowAwarePaddingBefore();
  Vector<FlexItem> lineItems;
  size_t nextIndex = 0;
  while (flexAlgorithm.ComputeNextFlexLine(
      nextIndex, lineItems, sumFlexBaseSize, totalFlexGrow, totalFlexShrink,
      totalWeightedFlexShrink, sumHypotheticalMainSize)) {
    DCHECK_GE(lineItems.size(), 0ULL);
    LayoutUnit containerMainInnerSize =
        mainAxisContentExtent(sumHypotheticalMainSize);
    // availableFreeSpace is the initial amount of free space in this flexbox.
    // remainingFreeSpace starts out at the same value but as we place and lay
    // out flex items we subtract from it. Note that both values can be
    // negative.
    LayoutUnit remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
    FlexSign flexSign = (sumHypotheticalMainSize < containerMainInnerSize)
                            ? PositiveFlexibility
                            : NegativeFlexibility;
    freezeInflexibleItems(flexSign, lineItems, remainingFreeSpace,
                          totalFlexGrow, totalFlexShrink,
                          totalWeightedFlexShrink);
    // The initial free space gets calculated after freezing inflexible items.
    // https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 3
    const LayoutUnit initialFreeSpace = remainingFreeSpace;
    while (!resolveFlexibleLengths(flexSign, lineItems, initialFreeSpace,
                                   remainingFreeSpace, totalFlexGrow,
                                   totalFlexShrink, totalWeightedFlexShrink)) {
      DCHECK_GE(totalFlexGrow, 0);
      DCHECK_GE(totalWeightedFlexShrink, 0);
    }

    // Recalculate the remaining free space. The adjustment for flex factors
    // between 0..1 means we can't just use remainingFreeSpace here.
    remainingFreeSpace = containerMainInnerSize;
    for (size_t i = 0; i < lineItems.size(); ++i) {
      FlexItem& flexItem = lineItems[i];
      DCHECK(!flexItem.box->isOutOfFlowPositioned());
      remainingFreeSpace -= flexItem.flexedMarginBoxSize();
    }
    // This will std::move lineItems into a newly-created LineContext.
    layoutAndPlaceChildren(crossAxisOffset, lineItems, remainingFreeSpace,
                           relayoutChildren, layoutScope, lineContexts);
  }
  if (hasLineIfEmpty()) {
    // Even if ComputeNextFlexLine returns true, the flexbox might not have
    // a line because all our children might be out of flow positioned.
    // Instead of just checking if we have a line, make sure the flexbox
    // has at least a line's worth of height to cover this case.
    LayoutUnit minHeight = minimumLogicalHeightForEmptyLine();
    if (size().height() < minHeight)
      setLogicalHeight(minHeight);
  }

  updateLogicalHeight();
  repositionLogicalHeightDependentFlexItems(lineContexts);
}

LayoutUnit LayoutFlexibleBox::autoMarginOffsetInMainAxis(
    const Vector<FlexItem>& children,
    LayoutUnit& availableFreeSpace) {
  if (availableFreeSpace <= LayoutUnit())
    return LayoutUnit();

  int numberOfAutoMargins = 0;
  bool isHorizontal = isHorizontalFlow();
  for (size_t i = 0; i < children.size(); ++i) {
    LayoutBox* child = children[i].box;
    DCHECK(!child->isOutOfFlowPositioned());
    if (isHorizontal) {
      if (child->style()->marginLeft().isAuto())
        ++numberOfAutoMargins;
      if (child->style()->marginRight().isAuto())
        ++numberOfAutoMargins;
    } else {
      if (child->style()->marginTop().isAuto())
        ++numberOfAutoMargins;
      if (child->style()->marginBottom().isAuto())
        ++numberOfAutoMargins;
    }
  }
  if (!numberOfAutoMargins)
    return LayoutUnit();

  LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
  availableFreeSpace = LayoutUnit();
  return sizeOfAutoMargin;
}

void LayoutFlexibleBox::updateAutoMarginsInMainAxis(
    LayoutBox& child,
    LayoutUnit autoMarginOffset) {
  DCHECK_GE(autoMarginOffset, LayoutUnit());

  if (isHorizontalFlow()) {
    if (child.style()->marginLeft().isAuto())
      child.setMarginLeft(autoMarginOffset);
    if (child.style()->marginRight().isAuto())
      child.setMarginRight(autoMarginOffset);
  } else {
    if (child.style()->marginTop().isAuto())
      child.setMarginTop(autoMarginOffset);
    if (child.style()->marginBottom().isAuto())
      child.setMarginBottom(autoMarginOffset);
  }
}

bool LayoutFlexibleBox::hasAutoMarginsInCrossAxis(
    const LayoutBox& child) const {
  if (isHorizontalFlow())
    return child.style()->marginTop().isAuto() ||
           child.style()->marginBottom().isAuto();
  return child.style()->marginLeft().isAuto() ||
         child.style()->marginRight().isAuto();
}

LayoutUnit LayoutFlexibleBox::availableAlignmentSpaceForChild(
    LayoutUnit lineCrossAxisExtent,
    const LayoutBox& child) {
  DCHECK(!child.isOutOfFlowPositioned());
  LayoutUnit childCrossExtent =
      crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
  return lineCrossAxisExtent - childCrossExtent;
}

LayoutUnit LayoutFlexibleBox::availableAlignmentSpaceForChildBeforeStretching(
    LayoutUnit lineCrossAxisExtent,
    const LayoutBox& child) {
  DCHECK(!child.isOutOfFlowPositioned());
  LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) +
                                crossAxisIntrinsicExtentForChild(child);
  return lineCrossAxisExtent - childCrossExtent;
}

bool LayoutFlexibleBox::updateAutoMarginsInCrossAxis(
    LayoutBox& child,
    LayoutUnit availableAlignmentSpace) {
  DCHECK(!child.isOutOfFlowPositioned());
  DCHECK_GE(availableAlignmentSpace, LayoutUnit());

  bool isHorizontal = isHorizontalFlow();
  Length topOrLeft =
      isHorizontal ? child.style()->marginTop() : child.style()->marginLeft();
  Length bottomOrRight = isHorizontal ? child.style()->marginBottom()
                                      : child.style()->marginRight();
  if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
    adjustAlignmentForChild(child, availableAlignmentSpace / 2);
    if (isHorizontal) {
      child.setMarginTop(availableAlignmentSpace / 2);
      child.setMarginBottom(availableAlignmentSpace / 2);
    } else {
      child.setMarginLeft(availableAlignmentSpace / 2);
      child.setMarginRight(availableAlignmentSpace / 2);
    }
    return true;
  }
  bool shouldAdjustTopOrLeft = true;
  if (isColumnFlow() && !child.style()->isLeftToRightDirection()) {
    // For column flows, only make this adjustment if topOrLeft corresponds to
    // the "before" margin, so that flipForRightToLeftColumn will do the right
    // thing.
    shouldAdjustTopOrLeft = false;
  }
  if (!isColumnFlow() && child.style()->isFlippedBlocksWritingMode()) {
    // If we are a flipped writing mode, we need to adjust the opposite side.
    // This is only needed for row flows because this only affects the
    // block-direction axis.
    shouldAdjustTopOrLeft = false;
  }

  if (topOrLeft.isAuto()) {
    if (shouldAdjustTopOrLeft)
      adjustAlignmentForChild(child, availableAlignmentSpace);

    if (isHorizontal)
      child.setMarginTop(availableAlignmentSpace);
    else
      child.setMarginLeft(availableAlignmentSpace);
    return true;
  }
  if (bottomOrRight.isAuto()) {
    if (!shouldAdjustTopOrLeft)
      adjustAlignmentForChild(child, availableAlignmentSpace);

    if (isHorizontal)
      child.setMarginBottom(availableAlignmentSpace);
    else
      child.setMarginRight(availableAlignmentSpace);
    return true;
  }
  return false;
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::marginBoxAscentForChild(const LayoutBox& child) {
  LayoutUnit ascent(child.firstLineBoxBaseline());
  if (ascent == -1)
    ascent = crossAxisExtentForChild(child);
  return ascent + flowAwareMarginBeforeForChild(child);
}

LayoutUnit LayoutFlexibleBox::computeChildMarginValue(Length margin) {
  // When resolving the margins, we use the content size for resolving percent
  // and calc (for percents in calc expressions) margins. Fortunately, percent
  // margins are always computed with respect to the block's width, even for
  // margin-top and margin-bottom.
  LayoutUnit availableSize = contentLogicalWidth();
  return minimumValueForLength(margin, availableSize);
}

void LayoutFlexibleBox::prepareOrderIteratorAndMargins() {
  OrderIteratorPopulator populator(m_orderIterator);

  for (LayoutBox* child = firstChildBox(); child;
       child = child->nextSiblingBox()) {
    populator.collectChild(child);

    if (child->isOutOfFlowPositioned())
      continue;

    // Before running the flex algorithm, 'auto' has a margin of 0.
    // Also, if we're not auto sizing, we don't do a layout that computes the
    // start/end margins.
    if (isHorizontalFlow()) {
      child->setMarginLeft(
          computeChildMarginValue(child->style()->marginLeft()));
      child->setMarginRight(
          computeChildMarginValue(child->style()->marginRight()));
    } else {
      child->setMarginTop(computeChildMarginValue(child->style()->marginTop()));
      child->setMarginBottom(
          computeChildMarginValue(child->style()->marginBottom()));
    }
  }
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::adjustChildSizeForMinAndMax(
    const LayoutBox& child,
    LayoutUnit childSize) {
  Length max = isHorizontalFlow() ? child.style()->maxWidth()
                                  : child.style()->maxHeight();
  LayoutUnit maxExtent(-1);
  if (max.isSpecifiedOrIntrinsic()) {
    maxExtent = computeMainAxisExtentForChild(child, MaxSize, max);
    DCHECK_GE(maxExtent, LayoutUnit(-1));
    if (maxExtent != -1 && childSize > maxExtent)
      childSize = maxExtent;
  }

  Length min = isHorizontalFlow() ? child.style()->minWidth()
                                  : child.style()->minHeight();
  LayoutUnit minExtent;
  if (min.isSpecifiedOrIntrinsic()) {
    minExtent = computeMainAxisExtentForChild(child, MinSize, min);
    // computeMainAxisExtentForChild can return -1 when the child has a
    // percentage min size, but we have an indefinite size in that axis.
    minExtent = std::max(LayoutUnit(), minExtent);
  } else if (min.isAuto() && !child.styleRef().containsSize() &&
             mainAxisOverflowForChild(child) == OverflowVisible &&
             !(isColumnFlow() && child.isFlexibleBox())) {
    // TODO(cbiesinger): For now, we do not handle min-height: auto for nested
    // column flexboxes. We need to implement
    // https://drafts.csswg.org/css-flexbox/#intrinsic-sizes before that
    // produces reasonable results. Tracking bug: https://crbug.com/581553
    // css-flexbox section 4.5
    LayoutUnit contentSize =
        computeMainAxisExtentForChild(child, MinSize, Length(MinContent));
    DCHECK_GE(contentSize, LayoutUnit());
    if (hasAspectRatio(child) && child.intrinsicSize().height() > 0)
      contentSize =
          adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, contentSize);
    if (maxExtent != -1 && contentSize > maxExtent)
      contentSize = maxExtent;

    Length mainSize = isHorizontalFlow() ? child.styleRef().width()
                                         : child.styleRef().height();
    if (mainAxisLengthIsDefinite(child, mainSize)) {
      LayoutUnit resolvedMainSize =
          computeMainAxisExtentForChild(child, MainOrPreferredSize, mainSize);
      DCHECK_GE(resolvedMainSize, LayoutUnit());
      LayoutUnit specifiedSize = maxExtent != -1
                                     ? std::min(resolvedMainSize, maxExtent)
                                     : resolvedMainSize;

      minExtent = std::min(specifiedSize, contentSize);
    } else if (useChildAspectRatio(child)) {
      Length crossSizeLength = isHorizontalFlow() ? child.styleRef().height()
                                                  : child.styleRef().width();
      LayoutUnit transferredSize =
          computeMainSizeFromAspectRatioUsing(child, crossSizeLength);
      transferredSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(
          child, transferredSize);
      minExtent = std::min(transferredSize, contentSize);
    } else {
      minExtent = contentSize;
    }
  }
  DCHECK_GE(minExtent, LayoutUnit());
  return std::max(childSize, minExtent);
}

LayoutUnit LayoutFlexibleBox::crossSizeForPercentageResolution(
    const LayoutBox& child) {
  if (alignmentForChild(child) != ItemPositionStretch)
    return LayoutUnit(-1);

  // Here we implement https://drafts.csswg.org/css-flexbox/#algo-stretch
  if (hasOrthogonalFlow(child) && child.hasOverrideLogicalContentWidth())
    return child.overrideLogicalContentWidth();
  if (!hasOrthogonalFlow(child) && child.hasOverrideLogicalContentHeight())
    return child.overrideLogicalContentHeight();

  // We don't currently implement the optimization from
  // https://drafts.csswg.org/css-flexbox/#definite-sizes case 1. While that
  // could speed up a specialized case, it requires determining if we have a
  // definite size, which itself is not cheap. We can consider implementing it
  // at a later time. (The correctness is ensured by redoing layout in
  // applyStretchAlignmentToChild)
  return LayoutUnit(-1);
}

LayoutUnit LayoutFlexibleBox::mainSizeForPercentageResolution(
    const LayoutBox& child) {
  // This function implements section 9.8. Definite and Indefinite Sizes, case
  // 2) of the flexbox spec.
  // We need to check for the flexbox to have a definite main size, and for the
  // flex item to have a definite flex basis.
  const Length& flexBasis = flexBasisForChild(child);
  if (!mainAxisLengthIsDefinite(child, flexBasis))
    return LayoutUnit(-1);
  if (!flexBasis.isPercentOrCalc()) {
    // If flex basis had a percentage, our size is guaranteed to be definite or
    // the flex item's size could not be definite. Otherwise, we make up a
    // percentage to check whether we have a definite size.
    if (!mainAxisLengthIsDefinite(child, Length(0, Percent)))
      return LayoutUnit(-1);
  }

  if (hasOrthogonalFlow(child))
    return child.hasOverrideLogicalContentHeight()
               ? child.overrideLogicalContentHeight()
               : LayoutUnit(-1);
  return child.hasOverrideLogicalContentWidth()
             ? child.overrideLogicalContentWidth()
             : LayoutUnit(-1);
}

LayoutUnit LayoutFlexibleBox::childLogicalHeightForPercentageResolution(
    const LayoutBox& child) {
  if (!hasOrthogonalFlow(child))
    return crossSizeForPercentageResolution(child);
  return mainSizeForPercentageResolution(child);
}

LayoutUnit LayoutFlexibleBox::adjustChildSizeForAspectRatioCrossAxisMinAndMax(
    const LayoutBox& child,
    LayoutUnit childSize) {
  Length crossMin = isHorizontalFlow() ? child.style()->minHeight()
                                       : child.style()->minWidth();
  Length crossMax = isHorizontalFlow() ? child.style()->maxHeight()
                                       : child.style()->maxWidth();

  if (crossAxisLengthIsDefinite(child, crossMax)) {
    LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(child, crossMax);
    childSize = std::min(maxValue, childSize);
  }

  if (crossAxisLengthIsDefinite(child, crossMin)) {
    LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(child, crossMin);
    childSize = std::max(minValue, childSize);
  }

  return childSize;
}

DISABLE_CFI_PERF
FlexItem LayoutFlexibleBox::constructFlexItem(LayoutBox& child,
                                              ChildLayoutType layoutType) {
  // If this condition is true, then computeMainAxisExtentForChild will call
  // child.intrinsicContentLogicalHeight() and
  // child.scrollbarLogicalHeight(), so if the child has intrinsic
  // min/max/preferred size, run layout on it now to make sure its logical
  // height and scroll bars are up to date.
  if (layoutType != NeverLayout && childHasIntrinsicMainAxisSize(child) &&
      child.needsLayout()) {
    child.clearOverrideSize();
    child.forceChildLayout();
    cacheChildMainSize(child);
    layoutType = LayoutIfNeeded;
  }

  LayoutUnit borderAndPadding = isHorizontalFlow()
                                    ? child.borderAndPaddingWidth()
                                    : child.borderAndPaddingHeight();
  LayoutUnit childInnerFlexBaseSize =
      computeInnerFlexBaseSizeForChild(child, borderAndPadding, layoutType);
  LayoutUnit childMinMaxAppliedMainAxisExtent =
      adjustChildSizeForMinAndMax(child, childInnerFlexBaseSize);
  LayoutUnit margin =
      isHorizontalFlow() ? child.marginWidth() : child.marginHeight();
  return FlexItem(&child, childInnerFlexBaseSize,
                  childMinMaxAppliedMainAxisExtent, borderAndPadding, margin);
}

void LayoutFlexibleBox::freezeViolations(Vector<FlexItem*>& violations,
                                         LayoutUnit& availableFreeSpace,
                                         double& totalFlexGrow,
                                         double& totalFlexShrink,
                                         double& totalWeightedFlexShrink) {
  for (size_t i = 0; i < violations.size(); ++i) {
    DCHECK(!violations[i]->frozen) << i;
    LayoutBox* child = violations[i]->box;
    LayoutUnit childSize = violations[i]->flexedContentSize;
    availableFreeSpace -= childSize - violations[i]->flexBaseContentSize;
    totalFlexGrow -= child->style()->flexGrow();
    totalFlexShrink -= child->style()->flexShrink();
    totalWeightedFlexShrink -=
        child->style()->flexShrink() * violations[i]->flexBaseContentSize;
    // totalWeightedFlexShrink can be negative when we exceed the precision of
    // a double when we initially calcuate totalWeightedFlexShrink. We then
    // subtract each child's weighted flex shrink with full precision, now
    // leading to a negative result. See
    // css3/flexbox/large-flex-shrink-assert.html
    totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
    violations[i]->frozen = true;
  }
}

void LayoutFlexibleBox::freezeInflexibleItems(FlexSign flexSign,
                                              Vector<FlexItem>& children,
                                              LayoutUnit& remainingFreeSpace,
                                              double& totalFlexGrow,
                                              double& totalFlexShrink,
                                              double& totalWeightedFlexShrink) {
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
  // we freeze all items with a flex factor of 0 as well as those with a min/max
  // size violation.
  Vector<FlexItem*> newInflexibleItems;
  for (size_t i = 0; i < children.size(); ++i) {
    FlexItem& flexItem = children[i];
    LayoutBox* child = flexItem.box;
    DCHECK(!flexItem.box->isOutOfFlowPositioned());
    DCHECK(!flexItem.frozen) << i;
    float flexFactor = (flexSign == PositiveFlexibility)
                           ? child->style()->flexGrow()
                           : child->style()->flexShrink();
    if (flexFactor == 0 ||
        (flexSign == PositiveFlexibility &&
         flexItem.flexBaseContentSize > flexItem.hypotheticalMainContentSize) ||
        (flexSign == NegativeFlexibility &&
         flexItem.flexBaseContentSize < flexItem.hypotheticalMainContentSize)) {
      flexItem.flexedContentSize = flexItem.hypotheticalMainContentSize;
      newInflexibleItems.append(&flexItem);
    }
  }
  freezeViolations(newInflexibleItems, remainingFreeSpace, totalFlexGrow,
                   totalFlexShrink, totalWeightedFlexShrink);
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool LayoutFlexibleBox::resolveFlexibleLengths(
    FlexSign flexSign,
    Vector<FlexItem>& children,
    LayoutUnit initialFreeSpace,
    LayoutUnit& remainingFreeSpace,
    double& totalFlexGrow,
    double& totalFlexShrink,
    double& totalWeightedFlexShrink) {
  LayoutUnit totalViolation;
  LayoutUnit usedFreeSpace;
  Vector<FlexItem*> minViolations;
  Vector<FlexItem*> maxViolations;

  double sumFlexFactors =
      (flexSign == PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
  if (sumFlexFactors > 0 && sumFlexFactors < 1) {
    LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
    if (fractional.abs() < remainingFreeSpace.abs())
      remainingFreeSpace = fractional;
  }

  for (size_t i = 0; i < children.size(); ++i) {
    FlexItem& flexItem = children[i];
    LayoutBox* child = flexItem.box;

    // This check also covers out-of-flow children.
    if (flexItem.frozen)
      continue;

    LayoutUnit childSize = flexItem.flexBaseContentSize;
    double extraSpace = 0;
    if (remainingFreeSpace > 0 && totalFlexGrow > 0 &&
        flexSign == PositiveFlexibility && std::isfinite(totalFlexGrow)) {
      extraSpace =
          remainingFreeSpace * child->style()->flexGrow() / totalFlexGrow;
    } else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 &&
               flexSign == NegativeFlexibility &&
               std::isfinite(totalWeightedFlexShrink) &&
               child->style()->flexShrink()) {
      extraSpace = remainingFreeSpace * child->style()->flexShrink() *
                   flexItem.flexBaseContentSize / totalWeightedFlexShrink;
    }
    if (std::isfinite(extraSpace))
      childSize += LayoutUnit::fromFloatRound(extraSpace);

    LayoutUnit adjustedChildSize =
        adjustChildSizeForMinAndMax(*child, childSize);
    DCHECK_GE(adjustedChildSize, 0);
    flexItem.flexedContentSize = adjustedChildSize;
    usedFreeSpace += adjustedChildSize - flexItem.flexBaseContentSize;

    LayoutUnit violation = adjustedChildSize - childSize;
    if (violation > 0)
      minViolations.append(&flexItem);
    else if (violation < 0)
      maxViolations.append(&flexItem);
    totalViolation += violation;
  }

  if (totalViolation)
    freezeViolations(totalViolation < 0 ? maxViolations : minViolations,
                     remainingFreeSpace, totalFlexGrow, totalFlexShrink,
                     totalWeightedFlexShrink);
  else
    remainingFreeSpace -= usedFreeSpace;

  return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(
    LayoutUnit availableFreeSpace,
    ContentPosition justifyContent,
    ContentDistributionType justifyContentDistribution,
    unsigned numberOfChildren) {
  if (justifyContent == ContentPositionFlexEnd)
    return availableFreeSpace;
  if (justifyContent == ContentPositionCenter)
    return availableFreeSpace / 2;
  if (justifyContentDistribution == ContentDistributionSpaceAround) {
    if (availableFreeSpace > 0 && numberOfChildren)
      return availableFreeSpace / (2 * numberOfChildren);

    return availableFreeSpace / 2;
  }
  return LayoutUnit();
}

static LayoutUnit justifyContentSpaceBetweenChildren(
    LayoutUnit availableFreeSpace,
    ContentDistributionType justifyContentDistribution,
    unsigned numberOfChildren) {
  if (availableFreeSpace > 0 && numberOfChildren > 1) {
    if (justifyContentDistribution == ContentDistributionSpaceBetween)
      return availableFreeSpace / (numberOfChildren - 1);
    if (justifyContentDistribution == ContentDistributionSpaceAround)
      return availableFreeSpace / numberOfChildren;
  }
  return LayoutUnit();
}

static LayoutUnit alignmentOffset(LayoutUnit availableFreeSpace,
                                  ItemPosition position,
                                  LayoutUnit ascent,
                                  LayoutUnit maxAscent,
                                  bool isWrapReverse) {
  switch (position) {
    case ItemPositionAuto:
    case ItemPositionNormal:
      NOTREACHED();
      break;
    case ItemPositionStretch:
      // Actual stretching must be handled by the caller. Since wrap-reverse
      // flips cross start and cross end, stretch children should be aligned
      // with the cross end. This matters because applyStretchAlignment
      // doesn't always stretch or stretch fully (explicit cross size given, or
      // stretching constrained by max-height/max-width). For flex-start and
      // flex-end this is handled by alignmentForChild().
      if (isWrapReverse)
        return availableFreeSpace;
      break;
    case ItemPositionFlexStart:
      break;
    case ItemPositionFlexEnd:
      return availableFreeSpace;
    case ItemPositionCenter:
      return availableFreeSpace / 2;
    case ItemPositionBaseline:
      // FIXME: If we get here in columns, we want the use the descent, except
      // we currently can't get the ascent/descent of orthogonal children.
      // https://bugs.webkit.org/show_bug.cgi?id=98076
      return maxAscent - ascent;
    case ItemPositionLastBaseline:
    case ItemPositionSelfStart:
    case ItemPositionSelfEnd:
    case ItemPositionStart:
    case ItemPositionEnd:
    case ItemPositionLeft:
    case ItemPositionRight:
      // FIXME: Implement these (https://crbug.com/507690). The extended grammar
      // is not enabled by default so we shouldn't hit this codepath.
      // The new grammar is only used when Grid Layout feature is enabled.
      DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      break;
  }
  return LayoutUnit();
}

void LayoutFlexibleBox::setOverrideMainAxisContentSizeForChild(
    LayoutBox& child,
    LayoutUnit childPreferredSize) {
  if (hasOrthogonalFlow(child))
    child.setOverrideLogicalContentHeight(childPreferredSize);
  else
    child.setOverrideLogicalContentWidth(childPreferredSize);
}

LayoutUnit LayoutFlexibleBox::staticMainAxisPositionForPositionedChild(
    const LayoutBox& child) {
  const LayoutUnit availableSpace =
      mainAxisContentExtent(contentLogicalHeight()) -
      mainAxisExtentForChild(child);

  ContentPosition position = styleRef().resolvedJustifyContentPosition(
      contentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      styleRef().resolvedJustifyContentDistribution(
          contentAlignmentNormalBehavior());
  LayoutUnit offset =
      initialJustifyContentOffset(availableSpace, position, distribution, 1);
  if (styleRef().flexDirection() == FlowRowReverse ||
      styleRef().flexDirection() == FlowColumnReverse)
    offset = availableSpace - offset;
  return offset;
}

LayoutUnit LayoutFlexibleBox::staticCrossAxisPositionForPositionedChild(
    const LayoutBox& child) {
  LayoutUnit availableSpace =
      crossAxisContentExtent() - crossAxisExtentForChild(child);
  return alignmentOffset(availableSpace, alignmentForChild(child), LayoutUnit(),
                         LayoutUnit(),
                         styleRef().flexWrap() == FlexWrapReverse);
}

LayoutUnit LayoutFlexibleBox::staticInlinePositionForPositionedChild(
    const LayoutBox& child) {
  return startOffsetForContent() +
         (isColumnFlow() ? staticCrossAxisPositionForPositionedChild(child)
                         : staticMainAxisPositionForPositionedChild(child));
}

LayoutUnit LayoutFlexibleBox::staticBlockPositionForPositionedChild(
    const LayoutBox& child) {
  return borderAndPaddingBefore() +
         (isColumnFlow() ? staticMainAxisPositionForPositionedChild(child)
                         : staticCrossAxisPositionForPositionedChild(child));
}

bool LayoutFlexibleBox::setStaticPositionForPositionedLayout(LayoutBox& child) {
  bool positionChanged = false;
  PaintLayer* childLayer = child.layer();
  if (child.styleRef().hasStaticInlinePosition(
          styleRef().isHorizontalWritingMode())) {
    LayoutUnit inlinePosition = staticInlinePositionForPositionedChild(child);
    if (childLayer->staticInlinePosition() != inlinePosition) {
      childLayer->setStaticInlinePosition(inlinePosition);
      positionChanged = true;
    }
  }
  if (child.styleRef().hasStaticBlockPosition(
          styleRef().isHorizontalWritingMode())) {
    LayoutUnit blockPosition = staticBlockPositionForPositionedChild(child);
    if (childLayer->staticBlockPosition() != blockPosition) {
      childLayer->setStaticBlockPosition(blockPosition);
      positionChanged = true;
    }
  }
  return positionChanged;
}

void LayoutFlexibleBox::prepareChildForPositionedLayout(LayoutBox& child) {
  DCHECK(child.isOutOfFlowPositioned());
  child.containingBlock()->insertPositionedObject(&child);
  PaintLayer* childLayer = child.layer();
  LayoutUnit staticInlinePosition =
      flowAwareBorderStart() + flowAwarePaddingStart();
  if (childLayer->staticInlinePosition() != staticInlinePosition) {
    childLayer->setStaticInlinePosition(staticInlinePosition);
    if (child.style()->hasStaticInlinePosition(
            style()->isHorizontalWritingMode()))
      child.setChildNeedsLayout(MarkOnlyThis);
  }

  LayoutUnit staticBlockPosition =
      flowAwareBorderBefore() + flowAwarePaddingBefore();
  if (childLayer->staticBlockPosition() != staticBlockPosition) {
    childLayer->setStaticBlockPosition(staticBlockPosition);
    if (child.style()->hasStaticBlockPosition(
            style()->isHorizontalWritingMode()))
      child.setChildNeedsLayout(MarkOnlyThis);
  }
}

ItemPosition LayoutFlexibleBox::alignmentForChild(
    const LayoutBox& child) const {
  ItemPosition align =
      child.styleRef()
          .resolvedAlignSelf(selfAlignmentNormalBehavior(),
                             child.isAnonymous() ? style() : nullptr)
          .position();
  DCHECK(align != ItemPositionAuto && align != ItemPositionNormal);

  if (align == ItemPositionBaseline && hasOrthogonalFlow(child))
    align = ItemPositionFlexStart;

  if (style()->flexWrap() == FlexWrapReverse) {
    if (align == ItemPositionFlexStart)
      align = ItemPositionFlexEnd;
    else if (align == ItemPositionFlexEnd)
      align = ItemPositionFlexStart;
  }

  return align;
}

void LayoutFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(
    LayoutBox& child) {
  if (hasAutoMarginsInCrossAxis(child)) {
    child.updateLogicalHeight();
    if (isHorizontalFlow()) {
      if (child.style()->marginTop().isAuto())
        child.setMarginTop(LayoutUnit());
      if (child.style()->marginBottom().isAuto())
        child.setMarginBottom(LayoutUnit());
    } else {
      if (child.style()->marginLeft().isAuto())
        child.setMarginLeft(LayoutUnit());
      if (child.style()->marginRight().isAuto())
        child.setMarginRight(LayoutUnit());
    }
  }
}

bool LayoutFlexibleBox::needToStretchChildLogicalHeight(
    const LayoutBox& child) const {
  // This function is a little bit magical. It relies on the fact that blocks
  // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
  // an implicit width: 100%. So the child will automatically stretch if our
  // cross axis is the child's inline axis. That's the case if:
  // - We are horizontal and the child is in vertical writing mode
  // - We are vertical and the child is in horizontal writing mode
  // Otherwise, we need to stretch if the cross axis size is auto.
  if (alignmentForChild(child) != ItemPositionStretch)
    return false;

  if (isHorizontalFlow() != child.styleRef().isHorizontalWritingMode())
    return false;

  return child.styleRef().logicalHeight().isAuto();
}

bool LayoutFlexibleBox::childHasIntrinsicMainAxisSize(
    const LayoutBox& child) const {
  bool result = false;
  if (isHorizontalFlow() != child.styleRef().isHorizontalWritingMode()) {
    Length childFlexBasis = flexBasisForChild(child);
    Length childMinSize = isHorizontalFlow() ? child.style()->minWidth()
                                             : child.style()->minHeight();
    Length childMaxSize = isHorizontalFlow() ? child.style()->maxWidth()
                                             : child.style()->maxHeight();
    if (childFlexBasis.isIntrinsic() || childMinSize.isIntrinsicOrAuto() ||
        childMaxSize.isIntrinsic())
      result = true;
  }
  return result;
}

EOverflow LayoutFlexibleBox::mainAxisOverflowForChild(
    const LayoutBox& child) const {
  if (isHorizontalFlow())
    return child.styleRef().overflowX();
  return child.styleRef().overflowY();
}

EOverflow LayoutFlexibleBox::crossAxisOverflowForChild(
    const LayoutBox& child) const {
  if (isHorizontalFlow())
    return child.styleRef().overflowY();
  return child.styleRef().overflowX();
}

DISABLE_CFI_PERF
void LayoutFlexibleBox::layoutAndPlaceChildren(
    LayoutUnit& crossAxisOffset,
    Vector<FlexItem>& children,
    LayoutUnit availableFreeSpace,
    bool relayoutChildren,
    SubtreeLayoutScope& layoutScope,
    Vector<LineContext>& lineContexts) {
  ContentPosition position = styleRef().resolvedJustifyContentPosition(
      contentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      styleRef().resolvedJustifyContentDistribution(
          contentAlignmentNormalBehavior());

  LayoutUnit autoMarginOffset =
      autoMarginOffsetInMainAxis(children, availableFreeSpace);
  LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
  mainAxisOffset += initialJustifyContentOffset(availableFreeSpace, position,
                                                distribution, children.size());
  if (style()->flexDirection() == FlowRowReverse &&
      shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth()
                                         : horizontalScrollbarHeight();

  LayoutUnit totalMainExtent = mainAxisExtent();
  if (!shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    totalMainExtent -= isHorizontalFlow() ? verticalScrollbarWidth()
                                          : horizontalScrollbarHeight();
  LayoutUnit maxAscent, maxDescent;  // Used when align-items: baseline.
  LayoutUnit maxChildCrossAxisExtent;
  bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
  bool isPaginated = view()->layoutState()->isPaginated();
  for (size_t i = 0; i < children.size(); ++i) {
    const FlexItem& flexItem = children[i];
    LayoutBox* child = flexItem.box;

    DCHECK(!flexItem.box->isOutOfFlowPositioned());

    child->setMayNeedPaintInvalidation();

    setOverrideMainAxisContentSizeForChild(*child, flexItem.flexedContentSize);
    // The flexed content size and the override size include the scrollbar
    // width, so we need to compare to the size including the scrollbar.
    // TODO(cbiesinger): Should it include the scrollbar?
    if (flexItem.flexedContentSize !=
        mainAxisContentExtentForChildIncludingScrollbar(*child)) {
      child->setChildNeedsLayout(MarkOnlyThis);
    } else {
      // To avoid double applying margin changes in
      // updateAutoMarginsInCrossAxis, we reset the margins here.
      resetAutoMarginsAndLogicalTopInCrossAxis(*child);
    }
    // We may have already forced relayout for orthogonal flowing children in
    // computeInnerFlexBaseSizeForChild.
    bool forceChildRelayout =
        relayoutChildren && !m_relaidOutChildren.contains(child);
    if (child->isLayoutBlock() &&
        toLayoutBlock(*child).hasPercentHeightDescendants()) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      forceChildRelayout = true;
    }
    updateBlockChildDirtyBitsBeforeLayout(forceChildRelayout, *child);
    if (!child->needsLayout())
      markChildForPaginationRelayoutIfNeeded(*child, layoutScope);
    if (child->needsLayout())
      m_relaidOutChildren.add(child);
    child->layoutIfNeeded();

    updateAutoMarginsInMainAxis(*child, autoMarginOffset);

    LayoutUnit childCrossAxisMarginBoxExtent;
    if (alignmentForChild(*child) == ItemPositionBaseline &&
        !hasAutoMarginsInCrossAxis(*child)) {
      LayoutUnit ascent = marginBoxAscentForChild(*child);
      LayoutUnit descent = (crossAxisMarginExtentForChild(*child) +
                            crossAxisExtentForChild(*child)) -
                           ascent;

      maxAscent = std::max(maxAscent, ascent);
      maxDescent = std::max(maxDescent, descent);

      // TODO(cbiesinger): Take scrollbar into account
      childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
    } else {
      childCrossAxisMarginBoxExtent = crossAxisIntrinsicExtentForChild(*child) +
                                      crossAxisMarginExtentForChild(*child) +
                                      crossAxisScrollbarExtentForChild(*child);
    }
    if (!isColumnFlow())
      setLogicalHeight(std::max(
          logicalHeight(),
          crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() +
              childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
    maxChildCrossAxisExtent =
        std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

    mainAxisOffset += flowAwareMarginStartForChild(*child);

    LayoutUnit childMainExtent = mainAxisExtentForChild(*child);
    // In an RTL column situation, this will apply the margin-right/margin-end
    // on the left. This will be fixed later in flipForRightToLeftColumn.
    LayoutPoint childLocation(
        shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent
                           : mainAxisOffset,
        crossAxisOffset + flowAwareMarginBeforeForChild(*child));
    setFlowAwareLocationForChild(*child, childLocation);
    mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(*child);

    mainAxisOffset += justifyContentSpaceBetweenChildren(
        availableFreeSpace, distribution, children.size());

    if (isPaginated)
      updateFragmentationInfoForChild(*child);
  }

  if (isColumnFlow())
    setLogicalHeight(std::max(
        logicalHeight(), mainAxisOffset + flowAwareBorderEnd() +
                             flowAwarePaddingEnd() + scrollbarLogicalHeight()));

  if (style()->flexDirection() == FlowColumnReverse) {
    // We have to do an extra pass for column-reverse to reposition the flex
    // items since the start depends on the height of the flexbox, which we
    // only know after we've positioned all the flex items.
    updateLogicalHeight();
    layoutColumnReverse(children, crossAxisOffset, availableFreeSpace);
  }

  if (m_numberOfInFlowChildrenOnFirstLine == -1)
    m_numberOfInFlowChildrenOnFirstLine = children.size();
  lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent,
                                  maxAscent, std::move(children)));
  crossAxisOffset += maxChildCrossAxisExtent;
}

void LayoutFlexibleBox::layoutColumnReverse(const Vector<FlexItem>& children,
                                            LayoutUnit crossAxisOffset,
                                            LayoutUnit availableFreeSpace) {
  ContentPosition position = styleRef().resolvedJustifyContentPosition(
      contentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      styleRef().resolvedJustifyContentDistribution(
          contentAlignmentNormalBehavior());

  // This is similar to the logic in layoutAndPlaceChildren, except we place
  // the children starting from the end of the flexbox. We also don't need to
  // layout anything since we're just moving the children to a new position.
  LayoutUnit mainAxisOffset =
      logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
  mainAxisOffset -= initialJustifyContentOffset(availableFreeSpace, position,
                                                distribution, children.size());
  mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth()
                                       : horizontalScrollbarHeight();

  for (size_t i = 0; i < children.size(); ++i) {
    LayoutBox* child = children[i].box;

    DCHECK(!child->isOutOfFlowPositioned());

    mainAxisOffset -=
        mainAxisExtentForChild(*child) + flowAwareMarginEndForChild(*child);

    setFlowAwareLocationForChild(
        *child,
        LayoutPoint(mainAxisOffset,
                    crossAxisOffset + flowAwareMarginBeforeForChild(*child)));

    mainAxisOffset -= flowAwareMarginStartForChild(*child);

    mainAxisOffset -= justifyContentSpaceBetweenChildren(
        availableFreeSpace, distribution, children.size());
  }
}

static LayoutUnit initialAlignContentOffset(
    LayoutUnit availableFreeSpace,
    ContentPosition alignContent,
    ContentDistributionType alignContentDistribution,
    unsigned numberOfLines) {
  if (numberOfLines <= 1)
    return LayoutUnit();
  if (alignContent == ContentPositionFlexEnd)
    return availableFreeSpace;
  if (alignContent == ContentPositionCenter)
    return availableFreeSpace / 2;
  if (alignContentDistribution == ContentDistributionSpaceAround) {
    if (availableFreeSpace > 0 && numberOfLines)
      return availableFreeSpace / (2 * numberOfLines);
    if (availableFreeSpace < 0)
      return availableFreeSpace / 2;
  }
  return LayoutUnit();
}

static LayoutUnit alignContentSpaceBetweenChildren(
    LayoutUnit availableFreeSpace,
    ContentDistributionType alignContentDistribution,
    unsigned numberOfLines) {
  if (availableFreeSpace > 0 && numberOfLines > 1) {
    if (alignContentDistribution == ContentDistributionSpaceBetween)
      return availableFreeSpace / (numberOfLines - 1);
    if (alignContentDistribution == ContentDistributionSpaceAround ||
        alignContentDistribution == ContentDistributionStretch)
      return availableFreeSpace / numberOfLines;
  }
  return LayoutUnit();
}

void LayoutFlexibleBox::alignFlexLines(Vector<LineContext>& lineContexts) {
  ContentPosition position =
      styleRef().resolvedAlignContentPosition(contentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      styleRef().resolvedAlignContentDistribution(
          contentAlignmentNormalBehavior());

  // If we have a single line flexbox or a multiline line flexbox with only one
  // flex line, the line height is all the available space. For
  // flex-direction: row, this means we need to use the height, so we do this
  // after calling updateLogicalHeight.
  if (lineContexts.size() == 1) {
    lineContexts[0].crossAxisExtent = crossAxisContentExtent();
    return;
  }

  if (position == ContentPositionFlexStart)
    return;

  LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
  for (size_t i = 0; i < lineContexts.size(); ++i)
    availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;

  LayoutUnit lineOffset = initialAlignContentOffset(
      availableCrossAxisSpace, position, distribution, lineContexts.size());
  for (unsigned lineNumber = 0; lineNumber < lineContexts.size();
       ++lineNumber) {
    LineContext& lineContext = lineContexts[lineNumber];
    lineContext.crossAxisOffset += lineOffset;
    for (size_t childNumber = 0; childNumber < lineContext.flexItems.size();
         ++childNumber) {
      FlexItem& flexItem = lineContext.flexItems[childNumber];
      adjustAlignmentForChild(*flexItem.box, lineOffset);
    }

    if (distribution == ContentDistributionStretch &&
        availableCrossAxisSpace > 0)
      lineContexts[lineNumber].crossAxisExtent +=
          availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());

    lineOffset += alignContentSpaceBetweenChildren(
        availableCrossAxisSpace, distribution, lineContexts.size());
  }
}

void LayoutFlexibleBox::adjustAlignmentForChild(LayoutBox& child,
                                                LayoutUnit delta) {
  DCHECK(!child.isOutOfFlowPositioned());

  setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) +
                                          LayoutSize(LayoutUnit(), delta));
}

void LayoutFlexibleBox::alignChildren(const Vector<LineContext>& lineContexts) {
  // Keep track of the space between the baseline edge and the after edge of
  // the box for each line.
  Vector<LayoutUnit> minMarginAfterBaselines;

  for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
    const LineContext& lineContext = lineContexts[lineNumber];

    LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
    LayoutUnit lineCrossAxisExtent = lineContext.crossAxisExtent;
    LayoutUnit maxAscent = lineContext.maxAscent;

    for (size_t childNumber = 0; childNumber < lineContext.flexItems.size();
         ++childNumber) {
      const FlexItem& flexItem = lineContext.flexItems[childNumber];
      DCHECK(!flexItem.box->isOutOfFlowPositioned());

      if (updateAutoMarginsInCrossAxis(
              *flexItem.box,
              std::max(LayoutUnit(), availableAlignmentSpaceForChild(
                                         lineCrossAxisExtent, *flexItem.box))))
        continue;

      ItemPosition position = alignmentForChild(*flexItem.box);
      if (position == ItemPositionStretch)
        applyStretchAlignmentToChild(*flexItem.box, lineCrossAxisExtent);
      LayoutUnit availableSpace =
          availableAlignmentSpaceForChild(lineCrossAxisExtent, *flexItem.box);
      LayoutUnit offset = alignmentOffset(
          availableSpace, position, marginBoxAscentForChild(*flexItem.box),
          maxAscent, styleRef().flexWrap() == FlexWrapReverse);
      adjustAlignmentForChild(*flexItem.box, offset);
      if (position == ItemPositionBaseline &&
          styleRef().flexWrap() == FlexWrapReverse) {
        minMarginAfterBaseline = std::min(
            minMarginAfterBaseline, availableAlignmentSpaceForChild(
                                        lineCrossAxisExtent, *flexItem.box) -
                                        offset);
      }
    }
    minMarginAfterBaselines.append(minMarginAfterBaseline);
  }

  if (style()->flexWrap() != FlexWrapReverse)
    return;

  // wrap-reverse flips the cross axis start and end. For baseline alignment,
  // this means we need to align the after edge of baseline elements with the
  // after edge of the flex line.
  for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
    const LineContext& lineContext = lineContexts[lineNumber];
    LayoutUnit minMarginAfterBaseline = minMarginAfterBaselines[lineNumber];
    for (size_t childNumber = 0; childNumber < lineContext.flexItems.size();
         ++childNumber) {
      const FlexItem& flexItem = lineContext.flexItems[childNumber];
      if (alignmentForChild(*flexItem.box) == ItemPositionBaseline &&
          !hasAutoMarginsInCrossAxis(*flexItem.box) && minMarginAfterBaseline)
        adjustAlignmentForChild(*flexItem.box, minMarginAfterBaseline);
    }
  }
}

void LayoutFlexibleBox::applyStretchAlignmentToChild(
    LayoutBox& child,
    LayoutUnit lineCrossAxisExtent) {
  if (!hasOrthogonalFlow(child) && child.style()->logicalHeight().isAuto()) {
    LayoutUnit heightBeforeStretching = childIntrinsicLogicalHeight(child);
    LayoutUnit stretchedLogicalHeight =
        std::max(child.borderAndPaddingLogicalHeight(),
                 heightBeforeStretching +
                     availableAlignmentSpaceForChildBeforeStretching(
                         lineCrossAxisExtent, child));
    DCHECK(!child.needsLayout());
    LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(
        stretchedLogicalHeight,
        heightBeforeStretching - child.borderAndPaddingLogicalHeight());

    // FIXME: Can avoid laying out here in some cases. See
    // https://webkit.org/b/87905.
    bool childNeedsRelayout = desiredLogicalHeight != child.logicalHeight();
    if (child.isLayoutBlock() &&
        toLayoutBlock(child).hasPercentHeightDescendants() &&
        m_relaidOutChildren.contains(&child)) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      childNeedsRelayout = true;
    }
    if (childNeedsRelayout || !child.hasOverrideLogicalContentHeight())
      child.setOverrideLogicalContentHeight(
          desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
    if (childNeedsRelayout) {
      child.setLogicalHeight(LayoutUnit());
      // We cache the child's intrinsic content logical height to avoid it being
      // reset to the stretched height.
      // FIXME: This is fragile. LayoutBoxes should be smart enough to
      // determine their intrinsic content logical height correctly even when
      // there's an overrideHeight.
      LayoutUnit childIntrinsicContentLogicalHeight =
          child.intrinsicContentLogicalHeight();
      child.forceChildLayout();
      child.setIntrinsicContentLogicalHeight(
          childIntrinsicContentLogicalHeight);
    }
  } else if (hasOrthogonalFlow(child) &&
             child.style()->logicalWidth().isAuto()) {
    LayoutUnit childWidth =
        (lineCrossAxisExtent - crossAxisMarginExtentForChild(child))
            .clampNegativeToZero();
    childWidth = child.constrainLogicalWidthByMinMax(
        childWidth, crossAxisContentExtent(), this);

    if (childWidth != child.logicalWidth()) {
      child.setOverrideLogicalContentWidth(
          childWidth - child.borderAndPaddingLogicalWidth());
      child.forceChildLayout();
    }
  }
}

void LayoutFlexibleBox::flipForRightToLeftColumn(
    const Vector<LineContext>& lineContexts) {
  if (style()->isLeftToRightDirection() || !isColumnFlow())
    return;

  LayoutUnit crossExtent = crossAxisExtent();
  for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
    const LineContext& lineContext = lineContexts[lineNumber];
    for (size_t childNumber = 0; childNumber < lineContext.flexItems.size();
         ++childNumber) {
      const FlexItem& flexItem = lineContext.flexItems[childNumber];
      DCHECK(!flexItem.box->isOutOfFlowPositioned());

      LayoutPoint location = flowAwareLocationForChild(*flexItem.box);
      // For vertical flows, setFlowAwareLocationForChild will transpose x and
      // y,
      // so using the y axis for a column cross axis extent is correct.
      location.setY(crossExtent - crossAxisExtentForChild(*flexItem.box) -
                    location.y());
      if (!isHorizontalWritingMode())
        location.move(LayoutSize(0, -horizontalScrollbarHeight()));
      setFlowAwareLocationForChild(*flexItem.box, location);
    }
  }
}

void LayoutFlexibleBox::flipForWrapReverse(
    const Vector<LineContext>& lineContexts,
    LayoutUnit crossAxisStartEdge) {
  LayoutUnit contentExtent = crossAxisContentExtent();
  for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
    const LineContext& lineContext = lineContexts[lineNumber];
    for (size_t childNumber = 0; childNumber < lineContext.flexItems.size();
         ++childNumber) {
      const FlexItem& flexItem = lineContext.flexItems[childNumber];
      LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
      LayoutUnit originalOffset =
          lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
      LayoutUnit newOffset =
          contentExtent - originalOffset - lineCrossAxisExtent;
      adjustAlignmentForChild(*flexItem.box, newOffset - originalOffset);
    }
  }
}

}  // namespace blink
