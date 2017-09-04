// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LayoutBoxItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/Histogram.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

ScrollAnchor::ScrollAnchor()
    : m_anchorObject(nullptr),
      m_corner(Corner::TopLeft),
      m_scrollAnchorDisablingStyleChanged(false),
      m_queued(false) {}

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller) : ScrollAnchor() {
  setScroller(scroller);
}

ScrollAnchor::~ScrollAnchor() {}

void ScrollAnchor::setScroller(ScrollableArea* scroller) {
  DCHECK(m_scroller != scroller);
  DCHECK(scroller);
  DCHECK(scroller->isRootFrameViewport() || scroller->isFrameView() ||
         scroller->isPaintLayerScrollableArea());
  m_scroller = scroller;
  clearSelf();
}

// TODO(pilgrim): Replace all instances of scrollerLayoutBox with
// scrollerLayoutBoxItem, https://crbug.com/499321
static LayoutBox* scrollerLayoutBox(const ScrollableArea* scroller) {
  LayoutBox* box = scroller->layoutBox();
  DCHECK(box);
  return box;
}

static LayoutBoxItem scrollerLayoutBoxItem(const ScrollableArea* scroller) {
  return LayoutBoxItem(scrollerLayoutBox(scroller));
}

static Corner cornerToAnchor(const ScrollableArea* scroller) {
  const ComputedStyle* style = scrollerLayoutBox(scroller)->style();
  if (style->isFlippedBlocksWritingMode() || !style->isLeftToRightDirection())
    return Corner::TopRight;
  return Corner::TopLeft;
}

static LayoutPoint cornerPointOfRect(LayoutRect rect, Corner whichCorner) {
  switch (whichCorner) {
    case Corner::TopLeft:
      return rect.minXMinYCorner();
    case Corner::TopRight:
      return rect.maxXMinYCorner();
  }
  ASSERT_NOT_REACHED();
  return LayoutPoint();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static LayoutRect relativeBounds(const LayoutObject* layoutObject,
                                 const ScrollableArea* scroller) {
  LayoutRect localBounds;
  if (layoutObject->isBox()) {
    localBounds = toLayoutBox(layoutObject)->borderBoxRect();
    if (!layoutObject->hasOverflowClip()) {
      // borderBoxRect doesn't include overflow content and floats.
      LayoutUnit maxHeight =
          std::max(localBounds.height(),
                   toLayoutBox(layoutObject)->layoutOverflowRect().height());
      if (layoutObject->isLayoutBlockFlow() &&
          toLayoutBlockFlow(layoutObject)->containsFloats()) {
        // Note that lowestFloatLogicalBottom doesn't include floating
        // grandchildren.
        maxHeight = std::max(
            maxHeight,
            toLayoutBlockFlow(layoutObject)->lowestFloatLogicalBottom());
      }
      localBounds.setHeight(maxHeight);
    }
  } else if (layoutObject->isText()) {
    // TODO(skobes): Use first and last InlineTextBox only?
    for (InlineTextBox* box = toLayoutText(layoutObject)->firstTextBox(); box;
         box = box->nextTextBox())
      localBounds.unite(box->calculateBoundaries());
  } else {
    // Only LayoutBox and LayoutText are supported.
    ASSERT_NOT_REACHED();
  }

  LayoutRect relativeBounds = LayoutRect(
      scroller->localToVisibleContentQuad(FloatRect(localBounds), layoutObject)
          .boundingBox());

  return relativeBounds;
}

static LayoutPoint computeRelativeOffset(const LayoutObject* layoutObject,
                                         const ScrollableArea* scroller,
                                         Corner corner) {
  return cornerPointOfRect(relativeBounds(layoutObject, scroller), corner);
}

static bool candidateMayMoveWithScroller(const LayoutObject* candidate,
                                         const ScrollableArea* scroller) {
  if (const ComputedStyle* style = candidate->style()) {
    if (style->hasViewportConstrainedPosition())
      return false;
  }

  bool skippedByContainerLookup = false;
  candidate->container(scrollerLayoutBox(scroller), &skippedByContainerLookup);
  return !skippedByContainerLookup;
}

ScrollAnchor::ExamineResult ScrollAnchor::examine(
    const LayoutObject* candidate) const {
  if (candidate->isLayoutInline())
    return ExamineResult(Continue);

  // Anonymous blocks are not in the DOM tree and it may be hard for
  // developers to reason about the anchor node.
  if (candidate->isAnonymous())
    return ExamineResult(Continue);

  if (!candidate->isText() && !candidate->isBox())
    return ExamineResult(Skip);

  if (!candidateMayMoveWithScroller(candidate, m_scroller))
    return ExamineResult(Skip);

  if (candidate->style()->overflowAnchor() == AnchorNone)
    return ExamineResult(Skip);

  LayoutRect candidateRect = relativeBounds(candidate, m_scroller);
  LayoutRect visibleRect =
      scrollerLayoutBoxItem(m_scroller).overflowClipRect(LayoutPoint());

  bool occupiesSpace = candidateRect.width() > 0 && candidateRect.height() > 0;
  if (occupiesSpace && visibleRect.intersects(candidateRect)) {
    return ExamineResult(
        visibleRect.contains(candidateRect) ? Return : Constrain,
        cornerToAnchor(m_scroller));
  } else {
    return ExamineResult(Skip);
  }
}

void ScrollAnchor::findAnchor() {
  TRACE_EVENT0("blink", "ScrollAnchor::findAnchor");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Layout.ScrollAnchor.TimeToFindAnchor");

  LayoutObject* stayWithin = scrollerLayoutBox(m_scroller);
  LayoutObject* candidate = stayWithin->nextInPreOrder(stayWithin);
  while (candidate) {
    ExamineResult result = examine(candidate);
    if (result.viable) {
      m_anchorObject = candidate;
      m_corner = result.corner;
    }
    switch (result.status) {
      case Skip:
        candidate = candidate->nextInPreOrderAfterChildren(stayWithin);
        break;
      case Constrain:
        stayWithin = candidate;
      // fall through
      case Continue:
        candidate = candidate->nextInPreOrder(stayWithin);
        break;
      case Return:
        return;
    }
  }
}

bool ScrollAnchor::computeScrollAnchorDisablingStyleChanged() {
  LayoutObject* current = anchorObject();
  if (!current)
    return false;

  LayoutObject* scrollerBox = scrollerLayoutBox(m_scroller);
  while (true) {
    DCHECK(current);
    if (current->scrollAnchorDisablingStyleChanged())
      return true;
    if (current == scrollerBox)
      return false;
    current = current->parent();
  }
}

void ScrollAnchor::notifyBeforeLayout() {
  if (m_queued) {
    m_scrollAnchorDisablingStyleChanged |=
        computeScrollAnchorDisablingStyleChanged();
    return;
  }
  DCHECK(m_scroller);
  ScrollOffset scrollOffset = m_scroller->scrollOffset();
  float blockDirectionScrollOffset =
      scrollerLayoutBox(m_scroller)->isHorizontalWritingMode()
          ? scrollOffset.height()
          : scrollOffset.width();
  if (blockDirectionScrollOffset == 0) {
    clearSelf();
    return;
  }

  if (!m_anchorObject) {
    findAnchor();
    if (!m_anchorObject)
      return;

    m_anchorObject->setIsScrollAnchorObject();
    m_savedRelativeOffset =
        computeRelativeOffset(m_anchorObject, m_scroller, m_corner);
  }

  m_scrollAnchorDisablingStyleChanged =
      computeScrollAnchorDisablingStyleChanged();

  FrameView* frameView = scrollerLayoutBox(m_scroller)->frameView();
  ScrollableArea* owningScroller =
      m_scroller->isRootFrameViewport()
          ? &toRootFrameViewport(m_scroller)->layoutViewport()
          : m_scroller.get();
  frameView->enqueueScrollAnchoringAdjustment(owningScroller);
  m_queued = true;
}

IntSize ScrollAnchor::computeAdjustment() const {
  // The anchor node can report fractional positions, but it is DIP-snapped when
  // painting (crbug.com/610805), so we must round the offsets to determine the
  // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
  // anchor node may round differently from the snapping of the scroll position.
  // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
  // should scroll by 1px instead of 0.2px.) This is true regardless of whether
  // the ScrollableArea actually uses fractional scroll positions.
  IntSize delta = roundedIntSize(computeRelativeOffset(m_anchorObject,
                                                       m_scroller, m_corner)) -
                  roundedIntSize(m_savedRelativeOffset);

  // Only adjust on the block layout axis.
  if (scrollerLayoutBox(m_scroller)->isHorizontalWritingMode())
    delta.setWidth(0);
  else
    delta.setHeight(0);
  return delta;
}

void ScrollAnchor::adjust() {
  if (!m_queued)
    return;
  m_queued = false;
  DCHECK(m_scroller);
  if (!m_anchorObject)
    return;
  IntSize adjustment = computeAdjustment();
  if (adjustment.isZero())
    return;

  if (m_scrollAnchorDisablingStyleChanged) {
    // Note that we only clear if the adjustment would have been non-zero.
    // This minimizes redundant calls to findAnchor.
    // TODO(skobes): add UMA metric for this.
    clearSelf();

    DEFINE_STATIC_LOCAL(EnumerationHistogram, suppressedBySanaclapHistogram,
                        ("Layout.ScrollAnchor.SuppressedBySanaclap", 2));
    suppressedBySanaclapHistogram.count(1);

    return;
  }

  m_scroller->setScrollOffset(
      m_scroller->scrollOffset() + FloatSize(adjustment), AnchoringScroll);

  // Update UMA metric.
  DEFINE_STATIC_LOCAL(EnumerationHistogram, adjustedOffsetHistogram,
                      ("Layout.ScrollAnchor.AdjustedScrollOffset", 2));
  adjustedOffsetHistogram.count(1);
  UseCounter::count(scrollerLayoutBox(m_scroller)->document(),
                    UseCounter::ScrollAnchored);
}

void ScrollAnchor::clearSelf() {
  LayoutObject* anchorObject = m_anchorObject;
  m_anchorObject = nullptr;

  if (anchorObject)
    anchorObject->maybeClearIsScrollAnchorObject();
}

void ScrollAnchor::clear() {
  LayoutObject* layoutObject =
      m_anchorObject ? m_anchorObject : scrollerLayoutBox(m_scroller);
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layoutObject->parent())
    layer = parent->enclosingLayer();

  // Walk up the layer tree to clear any scroll anchors.
  while (layer) {
    if (PaintLayerScrollableArea* scrollableArea = layer->getScrollableArea()) {
      ScrollAnchor* anchor = scrollableArea->scrollAnchor();
      DCHECK(anchor);
      anchor->clearSelf();
    }
    layer = layer->parent();
  }

  if (FrameView* view = layoutObject->frameView()) {
    ScrollAnchor* anchor = view->scrollAnchor();
    DCHECK(anchor);
    anchor->clearSelf();
  }
}

bool ScrollAnchor::refersTo(const LayoutObject* layoutObject) const {
  return m_anchorObject == layoutObject;
}

void ScrollAnchor::notifyRemoved(LayoutObject* layoutObject) {
  if (m_anchorObject == layoutObject)
    clearSelf();
}

}  // namespace blink
