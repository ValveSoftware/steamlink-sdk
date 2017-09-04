/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@gmail.com>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "core/paint/PaintLayerScrollableArea.h"

#include "core/css/PseudoStyleRequest.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBoxItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayerFragment.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"

namespace blink {

static LayoutRect localToAbsolute(LayoutBox& offset, LayoutRect rect) {
  return LayoutRect(
      offset.localToAbsoluteQuad(FloatQuad(FloatRect(rect)), UseTransforms)
          .boundingBox());
}

PaintLayerScrollableAreaRareData::PaintLayerScrollableAreaRareData() {}

const int ResizerControlExpandRatioForTouch = 2;

PaintLayerScrollableArea::PaintLayerScrollableArea(PaintLayer& layer)
    : m_layer(layer),
      m_nextTopmostScrollChild(0),
      m_topmostScrollChild(0),
      m_inResizeMode(false),
      m_scrollsOverflow(false),
      m_inOverflowRelayout(false),
      m_needsCompositedScrolling(false),
      m_rebuildHorizontalScrollbarLayer(false),
      m_rebuildVerticalScrollbarLayer(false),
      m_needsScrollOffsetClamp(false),
      m_needsRelayout(false),
      m_hadHorizontalScrollbarBeforeRelayout(false),
      m_hadVerticalScrollbarBeforeRelayout(false),
      m_scrollbarManager(*this),
      m_scrollCorner(nullptr),
      m_resizer(nullptr),
      m_scrollAnchor(this)
#if DCHECK_IS_ON()
      ,
      m_hasBeenDisposed(false)
#endif
{
  Node* node = box().node();
  if (node && node->isElementNode()) {
    // We save and restore only the scrollOffset as the other scroll values are
    // recalculated.
    Element* element = toElement(node);
    m_scrollOffset = element->savedLayerScrollOffset();
    if (!m_scrollOffset.isZero())
      scrollAnimator().setCurrentOffset(m_scrollOffset);
    element->setSavedLayerScrollOffset(ScrollOffset());
  }
  updateResizerAreaSet();
}

PaintLayerScrollableArea::~PaintLayerScrollableArea() {
#if DCHECK_IS_ON()
  DCHECK(m_hasBeenDisposed);
#endif
}

void PaintLayerScrollableArea::dispose() {
  if (inResizeMode() && !box().documentBeingDestroyed()) {
    if (LocalFrame* frame = box().frame())
      frame->eventHandler().resizeScrollableAreaDestroyed();
  }

  if (LocalFrame* frame = box().frame()) {
    if (FrameView* frameView = frame->view()) {
      frameView->removeScrollableArea(this);
      frameView->removeAnimatingScrollableArea(this);
    }
  }

  if (box().frame() && box().frame()->page()) {
    if (ScrollingCoordinator* scrollingCoordinator =
            box().frame()->page()->scrollingCoordinator())
      scrollingCoordinator->willDestroyScrollableArea(this);
  }

  if (!box().documentBeingDestroyed()) {
    Node* node = box().node();
    // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
    if (node && node->isElementNode())
      toElement(node)->setSavedLayerScrollOffset(m_scrollOffset);
  }

  if (LocalFrame* frame = box().frame()) {
    if (FrameView* frameView = frame->view())
      frameView->removeResizerArea(box());
  }

  if (RootScrollerController* controller =
          box().document().rootScrollerController())
    controller->didDisposePaintLayerScrollableArea(*this);

  m_scrollbarManager.dispose();

  if (m_scrollCorner)
    m_scrollCorner->destroy();
  if (m_resizer)
    m_resizer->destroy();

  clearScrollableArea();

  // Note: it is not safe to call ScrollAnchor::clear if the document is being
  // destroyed, because LayoutObjectChildList::removeChildNode skips the call to
  // willBeRemovedFromTree,
  // leaving the ScrollAnchor with a stale LayoutObject pointer.
  if (RuntimeEnabledFeatures::scrollAnchoringEnabled() &&
      !box().documentBeingDestroyed())
    m_scrollAnchor.clearSelf();

#if DCHECK_IS_ON()
  m_hasBeenDisposed = true;
#endif
}

DEFINE_TRACE(PaintLayerScrollableArea) {
  visitor->trace(m_scrollbarManager);
  visitor->trace(m_scrollAnchor);
  ScrollableArea::trace(visitor);
}

HostWindow* PaintLayerScrollableArea::getHostWindow() const {
  if (Page* page = box().frame()->page())
    return &page->chromeClient();
  return nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrolling() const {
  return layer()->hasCompositedLayerMapping()
             ? layer()->compositedLayerMapping()->scrollingContentsLayer()
             : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForHorizontalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return layer()->hasCompositedLayerMapping()
             ? layer()->compositedLayerMapping()->layerForHorizontalScrollbar()
             : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForVerticalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return layer()->hasCompositedLayerMapping()
             ? layer()->compositedLayerMapping()->layerForVerticalScrollbar()
             : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrollCorner() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return layer()->hasCompositedLayerMapping()
             ? layer()->compositedLayerMapping()->layerForScrollCorner()
             : 0;
}

bool PaintLayerScrollableArea::shouldUseIntegerScrollOffset() const {
  Frame* frame = box().frame();
  if (frame->settings() &&
      !frame->settings()->preferCompositingToLCDTextEnabled())
    return true;

  return ScrollableArea::shouldUseIntegerScrollOffset();
}

bool PaintLayerScrollableArea::isActive() const {
  Page* page = box().frame()->page();
  return page && page->focusController().isActive();
}

bool PaintLayerScrollableArea::isScrollCornerVisible() const {
  return !scrollCornerRect().isEmpty();
}

static int cornerStart(const LayoutBox& box,
                       int minX,
                       int maxX,
                       int thickness) {
  if (box.shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return minX + box.styleRef().borderLeftWidth();
  return maxX - thickness - box.styleRef().borderRightWidth();
}

static IntRect cornerRect(const LayoutBox& box,
                          const Scrollbar* horizontalScrollbar,
                          const Scrollbar* verticalScrollbar,
                          const IntRect& bounds) {
  int horizontalThickness;
  int verticalThickness;
  if (!verticalScrollbar && !horizontalScrollbar) {
    // FIXME: This isn't right. We need to know the thickness of custom
    // scrollbars even when they don't exist in order to set the resizer square
    // size properly.
    horizontalThickness = ScrollbarTheme::theme().scrollbarThickness();
    verticalThickness = horizontalThickness;
  } else if (verticalScrollbar && !horizontalScrollbar) {
    horizontalThickness = verticalScrollbar->scrollbarThickness();
    verticalThickness = horizontalThickness;
  } else if (horizontalScrollbar && !verticalScrollbar) {
    verticalThickness = horizontalScrollbar->scrollbarThickness();
    horizontalThickness = verticalThickness;
  } else {
    horizontalThickness = verticalScrollbar->scrollbarThickness();
    verticalThickness = horizontalScrollbar->scrollbarThickness();
  }
  return IntRect(
      cornerStart(box, bounds.x(), bounds.maxX(), horizontalThickness),
      bounds.maxY() - verticalThickness - box.styleRef().borderBottomWidth(),
      horizontalThickness, verticalThickness);
}

IntRect PaintLayerScrollableArea::scrollCornerRect() const {
  // We have a scrollbar corner when a scrollbar is visible and not filling the
  // entire length of the box.
  // This happens when:
  // (a) A resizer is present and at least one scrollbar is present
  // (b) Both scrollbars are present.
  bool hasHorizontalBar = horizontalScrollbar();
  bool hasVerticalBar = verticalScrollbar();
  bool hasResizer = box().style()->resize() != RESIZE_NONE;
  if ((hasHorizontalBar && hasVerticalBar) ||
      (hasResizer && (hasHorizontalBar || hasVerticalBar)))
    return cornerRect(box(), horizontalScrollbar(), verticalScrollbar(),
                      box().pixelSnappedBorderBoxRect());
  return IntRect();
}

IntRect PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(
    const Scrollbar& scrollbar,
    const IntRect& scrollbarRect) const {
  LayoutView* view = box().view();
  if (!view)
    return scrollbarRect;

  IntRect rect = scrollbarRect;
  rect.move(scrollbarOffset(scrollbar));

  return view->frameView()->convertFromLayoutItem(LayoutBoxItem(&box()), rect);
}

IntRect PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(
    const Scrollbar& scrollbar,
    const IntRect& parentRect) const {
  LayoutView* view = box().view();
  if (!view)
    return parentRect;

  IntRect rect =
      view->frameView()->convertToLayoutItem(LayoutBoxItem(&box()), parentRect);
  rect.move(-scrollbarOffset(scrollbar));
  return rect;
}

IntPoint PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(
    const Scrollbar& scrollbar,
    const IntPoint& scrollbarPoint) const {
  LayoutView* view = box().view();
  if (!view)
    return scrollbarPoint;

  IntPoint point = scrollbarPoint;
  point.move(scrollbarOffset(scrollbar));
  return view->frameView()->convertFromLayoutItem(LayoutBoxItem(&box()), point);
}

IntPoint PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(
    const Scrollbar& scrollbar,
    const IntPoint& parentPoint) const {
  LayoutView* view = box().view();
  if (!view)
    return parentPoint;

  IntPoint point = view->frameView()->convertToLayoutItem(LayoutBoxItem(&box()),
                                                          parentPoint);

  point.move(-scrollbarOffset(scrollbar));
  return point;
}

int PaintLayerScrollableArea::scrollSize(
    ScrollbarOrientation orientation) const {
  IntSize scrollDimensions =
      maximumScrollOffsetInt() - minimumScrollOffsetInt();
  return (orientation == HorizontalScrollbar) ? scrollDimensions.width()
                                              : scrollDimensions.height();
}

void PaintLayerScrollableArea::updateScrollOffset(const ScrollOffset& newOffset,
                                                  ScrollType scrollType) {
  if (scrollOffset() == newOffset)
    return;

  showOverlayScrollbars();
  ScrollOffset scrollDelta = scrollOffset() - newOffset;
  m_scrollOffset = newOffset;

  LocalFrame* frame = box().frame();
  DCHECK(frame);

  FrameView* frameView = box().frameView();

  TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data",
               InspectorScrollLayerEvent::data(&box()));

  // FIXME(420741): Resolve circular dependency between scroll offset and
  // compositing state, and remove this disabler.
  DisableCompositingQueryAsserts disabler;

  // Update the positions of our child layers (if needed as only fixed layers
  // should be impacted by a scroll).  We don't update compositing layers,
  // because we need to do a deep update from the compositing ancestor.
  if (!frameView->isInPerformLayout()) {
    // If we're in the middle of layout, we'll just update layers once layout
    // has finished.
    layer()->updateLayerPositionsAfterOverflowScroll(scrollDelta);
    // Update regions, scrolling may change the clip of a particular region.
    frameView->updateDocumentAnnotatedRegions();
    frameView->setNeedsUpdateWidgetGeometries();
    updateCompositingLayersAfterScroll();
  }

  const LayoutBoxModelObject& paintInvalidationContainer =
      box().containerForPaintInvalidation();
  // The caret rect needs to be invalidated after scrolling
  frame->selection().setCaretRectNeedsUpdate();

  FloatQuad quadForFakeMouseMoveEvent = FloatQuad(FloatRect(
      layer()->layoutObject()->previousVisualRectIncludingCompositedScrolling(
          paintInvalidationContainer)));

  quadForFakeMouseMoveEvent =
      paintInvalidationContainer.localToAbsoluteQuad(quadForFakeMouseMoveEvent);
  frame->eventHandler().dispatchFakeMouseMoveEventSoonInQuad(
      quadForFakeMouseMoveEvent);

  Page* page = frame->page();
  if (page)
    page->chromeClient().clearToolTip(*frame);

  bool requiresPaintInvalidation = true;

  if (box().view()->compositor()->inCompositingMode()) {
    bool onlyScrolledCompositedLayers =
        scrollsOverflow() && layer()->isAllScrollingContentComposited() &&
        box().style()->backgroundLayers().attachment() !=
            LocalBackgroundAttachment;

    if (usesCompositedScrolling() || onlyScrolledCompositedLayers)
      requiresPaintInvalidation = false;
  }

  // Only the root layer can overlap non-composited fixed-position elements.
  if (!requiresPaintInvalidation && layer()->isRootLayer() &&
      frameView->hasViewportConstrainedObjects()) {
    if (!frameView->invalidateViewportConstrainedObjects())
      requiresPaintInvalidation = true;
  }

  // Just schedule a full paint invalidation of our object.
  // FIXME: This invalidation will be unnecessary in slimming paint phase 2.
  if (requiresPaintInvalidation) {
    box().setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  }

  // Schedule the scroll DOM event.
  if (box().node())
    box().node()->document().enqueueScrollEventForNode(box().node());

  if (AXObjectCache* cache = box().document().existingAXObjectCache())
    cache->handleScrollPositionChanged(&box());
  box().view()->clearHitTestCache();

  // Inform the FrameLoader of the new scroll position, so it can be restored
  // when navigating back.
  if (layer()->isRootLayer()) {
    frameView->frame().loader().saveScrollState();
    frameView->didChangeScrollOffset();
  }

  if (scrollTypeClearsFragmentAnchor(scrollType))
    frameView->clearFragmentAnchor();

  // Clear the scroll anchor, unless it is the reason for this scroll.
  if (RuntimeEnabledFeatures::scrollAnchoringEnabled() &&
      scrollType != AnchoringScroll && scrollType != ClampingScroll)
    scrollAnchor()->clear();
}

IntSize PaintLayerScrollableArea::scrollOffsetInt() const {
  return flooredIntSize(m_scrollOffset);
}

ScrollOffset PaintLayerScrollableArea::scrollOffset() const {
  return m_scrollOffset;
}

IntSize PaintLayerScrollableArea::minimumScrollOffsetInt() const {
  return toIntSize(-scrollOrigin());
}

IntSize PaintLayerScrollableArea::maximumScrollOffsetInt() const {
  IntSize contentSize;
  IntSize visibleSize;
  if (box().hasOverflowClip()) {
    contentSize = contentsSize();
    visibleSize =
        pixelSnappedIntRect(box().overflowClipRect(box().location())).size();

    // TODO(skobes): We should really ASSERT that contentSize >= visibleSize
    // when we are not the root layer, but we can't because contentSize is
    // based on stale layout overflow data (http://crbug.com/576933).
    contentSize = contentSize.expandedTo(visibleSize);
  }
  if (box().isLayoutView())
    visibleSize += toLayoutView(box()).frameView()->browserControlsSize();
  return toIntSize(-scrollOrigin() + (contentSize - visibleSize));
}

IntRect PaintLayerScrollableArea::visibleContentRect(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  int verticalScrollbarWidth = 0;
  int horizontalScrollbarHeight = 0;
  if (scrollbarInclusion == IncludeScrollbars) {
    verticalScrollbarWidth =
        (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar())
            ? verticalScrollbar()->scrollbarThickness()
            : 0;
    horizontalScrollbarHeight =
        (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar())
            ? horizontalScrollbar()->scrollbarThickness()
            : 0;
  }

  // TODO(szager): Handle fractional scroll offsets correctly.
  return IntRect(
      flooredIntPoint(scrollPosition()),
      IntSize(max(0, layer()->size().width() - verticalScrollbarWidth),
              max(0, layer()->size().height() - horizontalScrollbarHeight)));
}

void PaintLayerScrollableArea::visibleSizeChanged() {
  showOverlayScrollbars();
}

int PaintLayerScrollableArea::visibleHeight() const {
  return layer()->size().height();
}

int PaintLayerScrollableArea::visibleWidth() const {
  return layer()->size().width();
}

IntSize PaintLayerScrollableArea::contentsSize() const {
  return IntSize(pixelSnappedScrollWidth(), pixelSnappedScrollHeight());
}

IntPoint PaintLayerScrollableArea::lastKnownMousePosition() const {
  return box().frame() ? box().frame()->eventHandler().lastKnownMousePosition()
                       : IntPoint();
}

bool PaintLayerScrollableArea::scrollAnimatorEnabled() const {
  if (Settings* settings = box().frame()->settings())
    return settings->scrollAnimatorEnabled();
  return false;
}

bool PaintLayerScrollableArea::shouldSuspendScrollAnimations() const {
  LayoutView* view = box().view();
  if (!view)
    return true;
  return view->frameView()->shouldSuspendScrollAnimations();
}

void PaintLayerScrollableArea::scrollbarVisibilityChanged() {
  if (LayoutView* view = box().view())
    return view->clearHitTestCache();
}

bool PaintLayerScrollableArea::scrollbarsCanBeActive() const {
  LayoutView* view = box().view();
  if (!view)
    return false;
  return view->frameView()->scrollbarsCanBeActive();
}

IntRect PaintLayerScrollableArea::scrollableAreaBoundingBox() const {
  return box().absoluteBoundingBoxRect();
}

void PaintLayerScrollableArea::registerForAnimation() {
  if (LocalFrame* frame = box().frame()) {
    if (FrameView* frameView = frame->view())
      frameView->addAnimatingScrollableArea(this);
  }
}

void PaintLayerScrollableArea::deregisterForAnimation() {
  if (LocalFrame* frame = box().frame()) {
    if (FrameView* frameView = frame->view())
      frameView->removeAnimatingScrollableArea(this);
  }
}

bool PaintLayerScrollableArea::userInputScrollable(
    ScrollbarOrientation orientation) const {
  if (box().isIntrinsicallyScrollable(orientation))
    return true;

  EOverflow overflowStyle = (orientation == HorizontalScrollbar)
                                ? box().style()->overflowX()
                                : box().style()->overflowY();
  return (overflowStyle == OverflowScroll || overflowStyle == OverflowAuto ||
          overflowStyle == OverflowOverlay);
}

bool PaintLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const {
  return box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int PaintLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const {
  int length = (orientation == HorizontalScrollbar)
                   ? box().pixelSnappedClientWidth()
                   : box().pixelSnappedClientHeight();
  int minPageStep = static_cast<float>(length) *
                    ScrollableArea::minFractionToStepWhenPaging();
  int pageStep =
      max(minPageStep, length - ScrollableArea::maxOverlapBetweenPages());

  return max(pageStep, 1);
}

LayoutBox& PaintLayerScrollableArea::box() const {
  return *m_layer.layoutBox();
}

PaintLayer* PaintLayerScrollableArea::layer() const {
  return &m_layer;
}

LayoutUnit PaintLayerScrollableArea::scrollWidth() const {
  return m_overflowRect.width();
}

LayoutUnit PaintLayerScrollableArea::scrollHeight() const {
  return m_overflowRect.height();
}

int PaintLayerScrollableArea::pixelSnappedScrollWidth() const {
  return snapSizeToPixel(scrollWidth(),
                         box().clientLeft() + box().location().x());
}

int PaintLayerScrollableArea::pixelSnappedScrollHeight() const {
  return snapSizeToPixel(scrollHeight(),
                         box().clientTop() + box().location().y());
}

void PaintLayerScrollableArea::updateScrollOrigin() {
  // This should do nothing prior to first layout; the if-clause will catch
  // that.
  if (overflowRect().isEmpty())
    return;
  LayoutPoint scrollableOverflow =
      m_overflowRect.location() -
      LayoutSize(box().borderLeft(), box().borderTop());
  setScrollOrigin(flooredIntPoint(-scrollableOverflow) +
                  box().originAdjustmentForScrollbars());
}

void PaintLayerScrollableArea::updateScrollDimensions() {
  if (m_overflowRect.size() != box().layoutOverflowRect().size())
    contentsResized();
  m_overflowRect = box().layoutOverflowRect();
  box().flipForWritingMode(m_overflowRect);
  updateScrollOrigin();
}

void PaintLayerScrollableArea::setScrollOffsetUnconditionally(
    const ScrollOffset& offset,
    ScrollType scrollType) {
  cancelScrollAnimation();
  scrollOffsetChanged(offset, scrollType);
}

void PaintLayerScrollableArea::updateAfterLayout() {
  DCHECK(box().hasOverflowClip());

  bool relayoutIsPrevented = PreventRelayoutScope::relayoutIsPrevented();
  bool scrollbarsAreFrozen =
      m_inOverflowRelayout || FreezeScrollbarsScope::scrollbarsAreFrozen();

  if (needsScrollbarReconstruction()) {
    setHasHorizontalScrollbar(false);
    setHasVerticalScrollbar(false);
  }

  updateScrollDimensions();

  bool hadHorizontalScrollbar = hasHorizontalScrollbar();
  bool hadVerticalScrollbar = hasVerticalScrollbar();

  bool needsHorizontalScrollbar;
  bool needsVerticalScrollbar;
  computeScrollbarExistence(needsHorizontalScrollbar, needsVerticalScrollbar);

  bool horizontalScrollbarShouldChange =
      needsHorizontalScrollbar != hadHorizontalScrollbar;
  bool verticalScrollbarShouldChange =
      needsVerticalScrollbar != hadVerticalScrollbar;

  bool scrollbarsWillChange =
      !scrollbarsAreFrozen &&
      (horizontalScrollbarShouldChange || verticalScrollbarShouldChange);
  if (scrollbarsWillChange) {
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    if (hasScrollbar())
      updateScrollCornerStyle();

    layer()->updateSelfPaintingLayer();

    // Force an update since we know the scrollbars have changed things.
    if (box().document().hasAnnotatedRegions())
      box().document().setAnnotatedRegionsDirty(true);

    // Our proprietary overflow: overlay value doesn't trigger a layout.
    if ((horizontalScrollbarShouldChange &&
         box().style()->overflowX() != OverflowOverlay) ||
        (verticalScrollbarShouldChange &&
         box().style()->overflowY() != OverflowOverlay)) {
      if ((verticalScrollbarShouldChange && box().isHorizontalWritingMode()) ||
          (horizontalScrollbarShouldChange &&
           !box().isHorizontalWritingMode())) {
        box().setPreferredLogicalWidthsDirty();
      }
      if (relayoutIsPrevented) {
        // We're not doing re-layout right now, but we still want to
        // add the scrollbar to the logical width now, to facilitate parent
        // layout.
        box().updateLogicalWidth();
        PreventRelayoutScope::setBoxNeedsLayout(*this, hadHorizontalScrollbar,
                                                hadVerticalScrollbar);
      } else {
        m_inOverflowRelayout = true;
        SubtreeLayoutScope layoutScope(box());
        layoutScope.setNeedsLayout(&box(),
                                   LayoutInvalidationReason::ScrollbarChanged);
        if (box().isLayoutBlock()) {
          LayoutBlock& block = toLayoutBlock(box());
          block.scrollbarsChanged(horizontalScrollbarShouldChange,
                                  verticalScrollbarShouldChange);
          block.layoutBlock(true);
        } else {
          box().layout();
        }
        m_inOverflowRelayout = false;
        m_scrollbarManager.destroyDetachedScrollbars();
      }
      LayoutObject* parent = box().parent();
      if (parent && parent->isFlexibleBox())
        toLayoutFlexibleBox(parent)->clearCachedMainSizeForChild(box());
    }
  }

  {
    // Hits in
    // compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
    DisableCompositingQueryAsserts disabler;

    // overflow:scroll should just enable/disable.
    if (box().style()->overflowX() == OverflowScroll && horizontalScrollbar())
      horizontalScrollbar()->setEnabled(hasHorizontalOverflow());
    if (box().style()->overflowY() == OverflowScroll && verticalScrollbar())
      verticalScrollbar()->setEnabled(hasVerticalOverflow());

    // Set up the range (and page step/line step).
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
      int clientWidth = box().pixelSnappedClientWidth();
      horizontalScrollbar->setProportion(clientWidth,
                                         overflowRect().width().toInt());
    }
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
      int clientHeight = box().pixelSnappedClientHeight();
      verticalScrollbar->setProportion(clientHeight,
                                       overflowRect().height().toInt());
    }
  }

  if (!scrollbarsAreFrozen && hasOverlayScrollbars()) {
    if (!scrollSize(HorizontalScrollbar))
      setHasHorizontalScrollbar(false);
    if (!scrollSize(VerticalScrollbar))
      setHasVerticalScrollbar(false);
  }

  clampScrollOffsetsAfterLayout();

  if (!scrollbarsAreFrozen) {
    updateScrollableAreaSet(hasScrollableHorizontalOverflow() ||
                            hasScrollableVerticalOverflow());
  }

  DisableCompositingQueryAsserts disabler;
  positionOverflowControls();
}

void PaintLayerScrollableArea::clampScrollOffsetsAfterLayout() {
  // If a vertical scrollbar was removed, the min/max scroll offsets may have
  // changed, so the scroll offsets needs to be clamped.  If the scroll offset
  // did not change, but the scroll origin *did* change, we still need to notify
  // the scrollbars to update their dimensions.

  if (DelayScrollOffsetClampScope::clampingIsDelayed()) {
    DelayScrollOffsetClampScope::setNeedsClamp(this);
    return;
  }

  if (scrollOriginChanged())
    setScrollOffsetUnconditionally(clampScrollOffset(scrollOffset()));
  else
    ScrollableArea::setScrollOffset(scrollOffset(), ClampingScroll);

  setNeedsScrollOffsetClamp(false);
  resetScrollOriginChanged();
  m_scrollbarManager.destroyDetachedScrollbars();
}

void PaintLayerScrollableArea::didChangeGlobalRootScroller() {
  // On Android, where the VisualViewport supplies scrollbars, we need to
  // remove the PLSA's scrollbars. In general, this would be problematic as
  // that can cause layout but this should only ever apply with overlay
  // scrollbars.
  if (!box().frame()->settings() ||
      !box().frame()->settings()->viewportEnabled())
    return;

  bool needsHorizontalScrollbar;
  bool needsVerticalScrollbar;
  computeScrollbarExistence(needsHorizontalScrollbar, needsVerticalScrollbar);
  setHasHorizontalScrollbar(needsHorizontalScrollbar);
  setHasVerticalScrollbar(needsVerticalScrollbar);
}

bool PaintLayerScrollableArea::shouldPerformScrollAnchoring() const {
  return RuntimeEnabledFeatures::scrollAnchoringEnabled() &&
         m_scrollAnchor.hasScroller() &&
         layoutBox()->style()->overflowAnchor() != AnchorNone &&
         !box().document().finishingOrIsPrinting();
}

FloatQuad PaintLayerScrollableArea::localToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* localObject,
    MapCoordinatesFlags flags) const {
  LayoutBox* box = layoutBox();
  if (!box)
    return quad;
  DCHECK(localObject);
  return localObject->localToAncestorQuad(quad, box, flags);
}

ScrollBehavior PaintLayerScrollableArea::scrollBehaviorStyle() const {
  return box().style()->getScrollBehavior();
}

bool PaintLayerScrollableArea::hasHorizontalOverflow() const {
  // TODO(szager): Make the algorithm for adding/subtracting overflow:auto
  // scrollbars memoryless (crbug.com/625300).  This clientWidth hack will
  // prevent the spurious horizontal scrollbar, but it can cause a converse
  // problem: it can leave a sliver of horizontal overflow hidden behind the
  // vertical scrollbar without creating a horizontal scrollbar.  This
  // converse problem seems to happen much less frequently in practice, so we
  // bias the logic towards preventing unwanted horizontal scrollbars, which
  // are more common and annoying.
  int clientWidth = box().pixelSnappedClientWidth();
  if (needsRelayout() && !hadVerticalScrollbarBeforeRelayout())
    clientWidth += verticalScrollbarWidth();
  return pixelSnappedScrollWidth() > clientWidth;
}

bool PaintLayerScrollableArea::hasVerticalOverflow() const {
  return pixelSnappedScrollHeight() > box().pixelSnappedClientHeight();
}

bool PaintLayerScrollableArea::hasScrollableHorizontalOverflow() const {
  return hasHorizontalOverflow() && box().scrollsOverflowX();
}

bool PaintLayerScrollableArea::hasScrollableVerticalOverflow() const {
  return hasVerticalOverflow() && box().scrollsOverflowY();
}

// This function returns true if the given box requires overflow scrollbars (as
// opposed to the 'viewport' scrollbars managed by the PaintLayerCompositor).
// FIXME: we should use the same scrolling machinery for both the viewport and
// overflow. Currently, we need to avoid producing scrollbars here if they'll be
// handled externally in the RLC.
static bool canHaveOverflowScrollbars(const LayoutBox& box) {
  return (RuntimeEnabledFeatures::rootLayerScrollingEnabled() ||
          !box.isLayoutView()) &&
         box.document().viewportDefiningElement() != box.node();
}

void PaintLayerScrollableArea::updateAfterStyleChange(
    const ComputedStyle* oldStyle) {
  // Don't do this on first style recalc, before layout has ever happened.
  if (!overflowRect().size().isZero()) {
    updateScrollableAreaSet(hasScrollableHorizontalOverflow() ||
                            hasScrollableVerticalOverflow());
  }

  // Whenever background changes on the scrollable element, the scroll bar
  // overlay style might need to be changed to have contrast against the
  // background.
  // Skip the need scrollbar check, because we dont know do we need a scrollbar
  // when this method get called.
  Color oldBackground;
  if (oldStyle) {
    oldBackground = oldStyle->visitedDependentColor(CSSPropertyBackgroundColor);
  }
  Color newBackground =
      box().style()->visitedDependentColor(CSSPropertyBackgroundColor);

  if (newBackground != oldBackground) {
    recalculateScrollbarOverlayColorTheme(newBackground);
  }

  bool needsHorizontalScrollbar;
  bool needsVerticalScrollbar;
  // We add auto scrollbars only during layout to prevent spurious activations.
  computeScrollbarExistence(needsHorizontalScrollbar, needsVerticalScrollbar,
                            ForbidAddingAutoBars);

  // Avoid some unnecessary computation if there were and will be no scrollbars.
  if (!hasScrollbar() && !needsHorizontalScrollbar && !needsVerticalScrollbar)
    return;

  setHasHorizontalScrollbar(needsHorizontalScrollbar);
  setHasVerticalScrollbar(needsVerticalScrollbar);

  // With overflow: scroll, scrollbars are always visible but may be disabled.
  // When switching to another value, we need to re-enable them (see bug 11985).
  if (hasHorizontalScrollbar() && oldStyle &&
      oldStyle->overflowX() == OverflowScroll &&
      box().style()->overflowX() != OverflowScroll) {
    horizontalScrollbar()->setEnabled(true);
  }

  if (hasVerticalScrollbar() && oldStyle &&
      oldStyle->overflowY() == OverflowScroll &&
      box().style()->overflowY() != OverflowScroll) {
    verticalScrollbar()->setEnabled(true);
  }

  // FIXME: Need to detect a swap from custom to native scrollbars (and vice
  // versa).
  if (horizontalScrollbar())
    horizontalScrollbar()->styleChanged();
  if (verticalScrollbar())
    verticalScrollbar()->styleChanged();

  updateScrollCornerStyle();
  updateResizerAreaSet();
  updateResizerStyle();
}

bool PaintLayerScrollableArea::updateAfterCompositingChange() {
  layer()->updateScrollingStateAfterCompositingChange();
  const bool layersChanged = m_topmostScrollChild != m_nextTopmostScrollChild;
  m_topmostScrollChild = m_nextTopmostScrollChild;
  m_nextTopmostScrollChild = nullptr;
  return layersChanged;
}

void PaintLayerScrollableArea::updateAfterOverflowRecalc() {
  updateScrollDimensions();
  if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
    int clientWidth = box().pixelSnappedClientWidth();
    horizontalScrollbar->setProportion(clientWidth,
                                       overflowRect().width().toInt());
  }
  if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
    int clientHeight = box().pixelSnappedClientHeight();
    verticalScrollbar->setProportion(clientHeight,
                                     overflowRect().height().toInt());
  }

  bool needsHorizontalScrollbar;
  bool needsVerticalScrollbar;
  computeScrollbarExistence(needsHorizontalScrollbar, needsVerticalScrollbar);

  bool horizontalScrollbarShouldChange =
      needsHorizontalScrollbar != hasHorizontalScrollbar();
  bool verticalScrollbarShouldChange =
      needsVerticalScrollbar != hasVerticalScrollbar();

  if ((box().hasAutoHorizontalScrollbar() && horizontalScrollbarShouldChange) ||
      (box().hasAutoVerticalScrollbar() && verticalScrollbarShouldChange)) {
    box().setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::Unknown);
  }
}

IntRect PaintLayerScrollableArea::rectForHorizontalScrollbar(
    const IntRect& borderBoxRect) const {
  if (!hasHorizontalScrollbar())
    return IntRect();

  const IntRect& scrollCorner = scrollCornerRect();

  return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
                 borderBoxRect.maxY() - box().borderBottom() -
                     horizontalScrollbar()->scrollbarThickness(),
                 borderBoxRect.width() -
                     (box().borderLeft() + box().borderRight()) -
                     scrollCorner.width(),
                 horizontalScrollbar()->scrollbarThickness());
}

IntRect PaintLayerScrollableArea::rectForVerticalScrollbar(
    const IntRect& borderBoxRect) const {
  if (!hasVerticalScrollbar())
    return IntRect();

  const IntRect& scrollCorner = scrollCornerRect();

  return IntRect(
      verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
      borderBoxRect.y() + box().borderTop(),
      verticalScrollbar()->scrollbarThickness(),
      borderBoxRect.height() - (box().borderTop() + box().borderBottom()) -
          scrollCorner.height());
}

int PaintLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const {
  if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return minX + box().borderLeft();
  return maxX - box().borderRight() - verticalScrollbar()->scrollbarThickness();
}

int PaintLayerScrollableArea::horizontalScrollbarStart(int minX) const {
  int x = minX + box().borderLeft();
  if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    x += hasVerticalScrollbar()
             ? verticalScrollbar()->scrollbarThickness()
             : resizerCornerRect(box().pixelSnappedBorderBoxRect(),
                                 ResizerForPointer)
                   .width();
  return x;
}

IntSize PaintLayerScrollableArea::scrollbarOffset(
    const Scrollbar& scrollbar) const {
  if (&scrollbar == verticalScrollbar())
    return IntSize(verticalScrollbarStart(0, box().size().width().toInt()),
                   box().borderTop());

  if (&scrollbar == horizontalScrollbar())
    return IntSize(
        horizontalScrollbarStart(0),
        (box().size().height() - box().borderBottom() - scrollbar.height())
            .toInt());

  ASSERT_NOT_REACHED();
  return IntSize();
}

static inline const LayoutObject& layoutObjectForScrollbar(
    const LayoutObject& layoutObject) {
  if (Node* node = layoutObject.node()) {
    if (layoutObject.isLayoutView()) {
      Document& doc = node->document();
      if (Settings* settings = doc.settings()) {
        if (!settings->allowCustomScrollbarInMainFrame() &&
            layoutObject.frame() && layoutObject.frame()->isMainFrame())
          return layoutObject;
      }

      // Try the <body> element first as a scrollbar source.
      Element* body = doc.body();
      if (body && body->layoutObject() &&
          body->layoutObject()->style()->hasPseudoStyle(PseudoIdScrollbar))
        return *body->layoutObject();

      // If the <body> didn't have a custom style, then the root element might.
      Element* docElement = doc.documentElement();
      if (docElement && docElement->layoutObject() &&
          docElement->layoutObject()->style()->hasPseudoStyle(
              PseudoIdScrollbar))
        return *docElement->layoutObject();
    }

    if (layoutObject.styleRef().hasPseudoStyle(PseudoIdScrollbar))
      return layoutObject;

    if (ShadowRoot* shadowRoot = node->containingShadowRoot()) {
      if (shadowRoot->type() == ShadowRootType::UserAgent)
        return *shadowRoot->host().layoutObject();
    }
  }

  return layoutObject;
}

bool PaintLayerScrollableArea::needsScrollbarReconstruction() const {
  const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
  bool shouldUseCustom =
      actualLayoutObject.isBox() &&
      actualLayoutObject.styleRef().hasPseudoStyle(PseudoIdScrollbar);
  bool hasAnyScrollbar = hasScrollbar();
  bool hasCustom =
      (hasHorizontalScrollbar() &&
       horizontalScrollbar()->isCustomScrollbar()) ||
      (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar());
  bool didCustomScrollbarOwnerChanged = false;

  if (hasHorizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) {
    if (actualLayoutObject !=
        toLayoutScrollbar(horizontalScrollbar())->owningLayoutObject())
      didCustomScrollbarOwnerChanged = true;
  }

  if (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar()) {
    if (actualLayoutObject !=
        toLayoutScrollbar(verticalScrollbar())->owningLayoutObject())
      didCustomScrollbarOwnerChanged = true;
  }

  return hasAnyScrollbar &&
         ((shouldUseCustom != hasCustom) ||
          (shouldUseCustom && didCustomScrollbarOwnerChanged));
}

void PaintLayerScrollableArea::computeScrollbarExistence(
    bool& needsHorizontalScrollbar,
    bool& needsVerticalScrollbar,
    ComputeScrollbarExistenceOption option) const {
  // Scrollbars may be hidden or provided by visual viewport or frame instead.
  DCHECK(box().frame()->settings());
  if (visualViewportSuppliesScrollbars() || !canHaveOverflowScrollbars(box()) ||
      box().frame()->settings()->hideScrollbars()) {
    needsHorizontalScrollbar = false;
    needsVerticalScrollbar = false;
    return;
  }

  needsHorizontalScrollbar = box().scrollsOverflowX();
  needsVerticalScrollbar = box().scrollsOverflowY();

  // Don't add auto scrollbars if the box contents aren't visible.
  if (box().hasAutoHorizontalScrollbar()) {
    if (option == ForbidAddingAutoBars)
      needsHorizontalScrollbar &= hasHorizontalScrollbar();
    needsHorizontalScrollbar &= box().isRooted() &&
                                this->hasHorizontalOverflow() &&
                                box().pixelSnappedClientHeight();
  }

  if (box().hasAutoVerticalScrollbar()) {
    if (option == ForbidAddingAutoBars)
      needsVerticalScrollbar &= hasVerticalScrollbar();
    needsVerticalScrollbar &= box().isRooted() && this->hasVerticalOverflow() &&
                              box().pixelSnappedClientWidth();
  }

  // Look for the scrollbarModes and reset the needs Horizontal & vertical
  // Scrollbar values based on scrollbarModes, as during force style change
  // StyleResolver::styleForDocument returns documentStyle with no overflow
  // values, due to which we are destroying the scrollbars that were already
  // present.
  if (box().isLayoutView()) {
    if (LocalFrame* frame = box().frame()) {
      if (FrameView* frameView = frame->view()) {
        ScrollbarMode hMode;
        ScrollbarMode vMode;
        frameView->calculateScrollbarModes(hMode, vMode);
        if (hMode == ScrollbarAlwaysOn)
          needsHorizontalScrollbar = true;
        if (vMode == ScrollbarAlwaysOn)
          needsVerticalScrollbar = true;
      }
    }
  }
}

void PaintLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar) {
  if (FreezeScrollbarsScope::scrollbarsAreFrozen())
    return;

  if (hasScrollbar == hasHorizontalScrollbar())
    return;

  setScrollbarNeedsPaintInvalidation(HorizontalScrollbar);

  m_scrollbarManager.setHasHorizontalScrollbar(hasScrollbar);

  updateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (hasHorizontalScrollbar())
    horizontalScrollbar()->styleChanged();
  if (hasVerticalScrollbar())
    verticalScrollbar()->styleChanged();

  setScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (box().document().hasAnnotatedRegions())
    box().document().setAnnotatedRegionsDirty(true);
}

void PaintLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar) {
  if (FreezeScrollbarsScope::scrollbarsAreFrozen())
    return;

  if (hasScrollbar == hasVerticalScrollbar())
    return;

  setScrollbarNeedsPaintInvalidation(VerticalScrollbar);

  m_scrollbarManager.setHasVerticalScrollbar(hasScrollbar);

  updateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (hasHorizontalScrollbar())
    horizontalScrollbar()->styleChanged();
  if (hasVerticalScrollbar())
    verticalScrollbar()->styleChanged();

  setScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (box().document().hasAnnotatedRegions())
    box().document().setAnnotatedRegionsDirty(true);
}

int PaintLayerScrollableArea::verticalScrollbarWidth(
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const {
  if (!hasVerticalScrollbar())
    return 0;
  if (verticalScrollbar()->isOverlayScrollbar() &&
      (overlayScrollbarClipBehavior == IgnoreOverlayScrollbarSize ||
       !verticalScrollbar()->shouldParticipateInHitTesting()))
    return 0;
  return verticalScrollbar()->scrollbarThickness();
}

int PaintLayerScrollableArea::horizontalScrollbarHeight(
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const {
  if (!hasHorizontalScrollbar())
    return 0;
  if (horizontalScrollbar()->isOverlayScrollbar() &&
      (overlayScrollbarClipBehavior == IgnoreOverlayScrollbarSize ||
       !horizontalScrollbar()->shouldParticipateInHitTesting()))
    return 0;
  return horizontalScrollbar()->scrollbarThickness();
}

void PaintLayerScrollableArea::positionOverflowControls() {
  if (!hasScrollbar() && !box().canResize())
    return;

  const IntRect borderBox = box().pixelSnappedBorderBoxRect();
  if (Scrollbar* verticalScrollbar = this->verticalScrollbar())
    verticalScrollbar->setFrameRect(rectForVerticalScrollbar(borderBox));

  if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar())
    horizontalScrollbar->setFrameRect(rectForHorizontalScrollbar(borderBox));

  const IntRect& scrollCorner = scrollCornerRect();
  if (m_scrollCorner)
    m_scrollCorner->setFrameRect(LayoutRect(scrollCorner));

  if (m_resizer)
    m_resizer->setFrameRect(
        LayoutRect(resizerCornerRect(borderBox, ResizerForPointer)));

  // FIXME, this should eventually be removed, once we are certain that
  // composited controls get correctly positioned on a compositor update. For
  // now, conservatively leaving this unchanged.
  if (layer()->hasCompositedLayerMapping())
    layer()->compositedLayerMapping()->positionOverflowControlsLayers();
}

void PaintLayerScrollableArea::updateScrollCornerStyle() {
  if (!m_scrollCorner && !hasScrollbar())
    return;
  if (!m_scrollCorner && hasOverlayScrollbars())
    return;

  const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
  RefPtr<ComputedStyle> corner =
      box().hasOverflowClip()
          ? actualLayoutObject.getUncachedPseudoStyle(
                PseudoStyleRequest(PseudoIdScrollbarCorner),
                actualLayoutObject.style())
          : PassRefPtr<ComputedStyle>(nullptr);
  if (corner) {
    if (!m_scrollCorner) {
      m_scrollCorner =
          LayoutScrollbarPart::createAnonymous(&box().document(), this);
      m_scrollCorner->setDangerousOneWayParent(&box());
    }
    m_scrollCorner->setStyleWithWritingModeOfParent(corner.release());
  } else if (m_scrollCorner) {
    m_scrollCorner->destroy();
    m_scrollCorner = nullptr;
  }
}

bool PaintLayerScrollableArea::hitTestOverflowControls(
    HitTestResult& result,
    const IntPoint& localPoint) {
  if (!hasScrollbar() && !box().canResize())
    return false;

  IntRect resizeControlRect;
  if (box().style()->resize() != RESIZE_NONE) {
    resizeControlRect =
        resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    if (resizeControlRect.contains(localPoint))
      return true;
  }

  int resizeControlSize = max(resizeControlRect.height(), 0);
  if (hasVerticalScrollbar() &&
      verticalScrollbar()->shouldParticipateInHitTesting()) {
    LayoutRect vBarRect(verticalScrollbarStart(0, box().size().width().toInt()),
                        box().borderTop(),
                        verticalScrollbar()->scrollbarThickness(),
                        box().size().height().toInt() -
                            (box().borderTop() + box().borderBottom()) -
                            (hasHorizontalScrollbar()
                                 ? horizontalScrollbar()->scrollbarThickness()
                                 : resizeControlSize));
    if (vBarRect.contains(localPoint)) {
      result.setScrollbar(verticalScrollbar());
      return true;
    }
  }

  resizeControlSize = max(resizeControlRect.width(), 0);
  if (hasHorizontalScrollbar() &&
      horizontalScrollbar()->shouldParticipateInHitTesting()) {
    // TODO(crbug.com/638981): Are the conversions to int intentional?
    LayoutRect hBarRect(
        horizontalScrollbarStart(0),
        (box().size().height() - box().borderBottom() -
         horizontalScrollbar()->scrollbarThickness())
            .toInt(),
        (box().size().width() - (box().borderLeft() + box().borderRight()) -
         (hasVerticalScrollbar() ? verticalScrollbar()->scrollbarThickness()
                                 : resizeControlSize))
            .toInt(),
        horizontalScrollbar()->scrollbarThickness());
    if (hBarRect.contains(localPoint)) {
      result.setScrollbar(horizontalScrollbar());
      return true;
    }
  }

  // FIXME: We should hit test the m_scrollCorner and pass it back through the
  // result.

  return false;
}

IntRect PaintLayerScrollableArea::resizerCornerRect(
    const IntRect& bounds,
    ResizerHitTestType resizerHitTestType) const {
  if (box().style()->resize() == RESIZE_NONE)
    return IntRect();
  IntRect corner =
      cornerRect(box(), horizontalScrollbar(), verticalScrollbar(), bounds);

  if (resizerHitTestType == ResizerForTouch) {
    // We make the resizer virtually larger for touch hit testing. With the
    // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
    // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
    // then expand the rect by new_w/h = w/h * k.
    int expandRatio = ResizerControlExpandRatioForTouch - 1;
    corner.move(-corner.width() * expandRatio, -corner.height() * expandRatio);
    corner.expand(corner.width() * expandRatio, corner.height() * expandRatio);
  }

  return corner;
}

IntRect PaintLayerScrollableArea::scrollCornerAndResizerRect() const {
  IntRect scrollCornerAndResizer = scrollCornerRect();
  if (scrollCornerAndResizer.isEmpty())
    scrollCornerAndResizer =
        resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
  return scrollCornerAndResizer;
}

bool PaintLayerScrollableArea::isPointInResizeControl(
    const IntPoint& absolutePoint,
    ResizerHitTestType resizerHitTestType) const {
  if (!box().canResize())
    return false;

  IntPoint localPoint =
      roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
  IntRect localBounds(0, 0, box().pixelSnappedWidth(),
                      box().pixelSnappedHeight());
  return resizerCornerRect(localBounds, resizerHitTestType)
      .contains(localPoint);
}

bool PaintLayerScrollableArea::hitTestResizerInFragments(
    const PaintLayerFragments& layerFragments,
    const HitTestLocation& hitTestLocation) const {
  if (!box().canResize())
    return false;

  if (layerFragments.isEmpty())
    return false;

  for (int i = layerFragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layerFragments.at(i);
    if (fragment.backgroundRect.intersects(hitTestLocation) &&
        resizerCornerRect(pixelSnappedIntRect(fragment.layerBounds),
                          ResizerForPointer)
            .contains(hitTestLocation.roundedPoint()))
      return true;
  }

  return false;
}

void PaintLayerScrollableArea::updateResizerAreaSet() {
  LocalFrame* frame = box().frame();
  if (!frame)
    return;
  FrameView* frameView = frame->view();
  if (!frameView)
    return;
  if (box().canResize())
    frameView->addResizerArea(box());
  else
    frameView->removeResizerArea(box());
}

void PaintLayerScrollableArea::updateResizerStyle() {
  if (!m_resizer && !box().canResize())
    return;

  const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
  RefPtr<ComputedStyle> resizer =
      box().hasOverflowClip()
          ? actualLayoutObject.getUncachedPseudoStyle(
                PseudoStyleRequest(PseudoIdResizer), actualLayoutObject.style())
          : PassRefPtr<ComputedStyle>(nullptr);
  if (resizer) {
    if (!m_resizer) {
      m_resizer = LayoutScrollbarPart::createAnonymous(&box().document(), this);
      m_resizer->setDangerousOneWayParent(&box());
    }
    m_resizer->setStyleWithWritingModeOfParent(resizer.release());
  } else if (m_resizer) {
    m_resizer->destroy();
    m_resizer = nullptr;
  }
}

void PaintLayerScrollableArea::invalidateAllStickyConstraints() {
  if (PaintLayerScrollableAreaRareData* d = rareData()) {
    for (PaintLayer* stickyLayer : d->m_stickyConstraintsMap.keys()) {
      if (stickyLayer->layoutObject()->style()->position() == StickyPosition)
        stickyLayer->setNeedsCompositingInputsUpdate();
    }
    d->m_stickyConstraintsMap.clear();
  }
}

void PaintLayerScrollableArea::invalidateStickyConstraintsFor(
    PaintLayer* layer,
    bool needsCompositingUpdate) {
  if (PaintLayerScrollableAreaRareData* d = rareData()) {
    d->m_stickyConstraintsMap.remove(layer);
    if (needsCompositingUpdate &&
        layer->layoutObject()->style()->position() == StickyPosition)
      layer->setNeedsCompositingInputsUpdate();
  }
}

IntSize PaintLayerScrollableArea::offsetFromResizeCorner(
    const IntPoint& absolutePoint) const {
  // Currently the resize corner is either the bottom right corner or the bottom
  // left corner.
  // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be
  // the case?
  IntSize elementSize = layer()->size();
  if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    elementSize.setWidth(0);
  IntPoint resizerPoint = IntPoint(elementSize);
  IntPoint localPoint =
      roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
  return localPoint - resizerPoint;
}

void PaintLayerScrollableArea::resize(const PlatformEvent& evt,
                                      const LayoutSize& oldOffset) {
  // FIXME: This should be possible on generated content but is not right now.
  if (!inResizeMode() || !box().canResize() || !box().node())
    return;

  DCHECK(box().node()->isElementNode());
  Element* element = toElement(box().node());

  Document& document = element->document();

  IntPoint pos;
  const PlatformGestureEvent* gevt = 0;

  switch (evt.type()) {
    case PlatformEvent::MouseMoved:
      if (!document.frame()->eventHandler().mousePressed())
        return;
      pos = static_cast<const PlatformMouseEvent*>(&evt)->position();
      break;
    case PlatformEvent::GestureScrollUpdate:
      pos = static_cast<const PlatformGestureEvent*>(&evt)->position();
      gevt = static_cast<const PlatformGestureEvent*>(&evt);
      pos = gevt->position();
      pos.move(gevt->deltaX(), gevt->deltaY());
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  float zoomFactor = box().style()->effectiveZoom();

  IntSize newOffset =
      offsetFromResizeCorner(document.view()->rootFrameToContents(pos));
  newOffset.setWidth(newOffset.width() / zoomFactor);
  newOffset.setHeight(newOffset.height() / zoomFactor);

  LayoutSize currentSize = box().size();
  currentSize.scale(1 / zoomFactor);
  LayoutSize minimumSize =
      element->minimumSizeForResizing().shrunkTo(currentSize);
  element->setMinimumSizeForResizing(minimumSize);

  LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor,
                                            oldOffset.height() / zoomFactor);
  if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    newOffset.setWidth(-newOffset.width());
    adjustedOldOffset.setWidth(-adjustedOldOffset.width());
  }

  LayoutSize difference(
      (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) -
      currentSize);

  bool isBoxSizingBorder = box().style()->boxSizing() == BoxSizingBorderBox;

  EResize resize = box().style()->resize();
  if (resize != RESIZE_VERTICAL && difference.width()) {
    if (element->isFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->setInlineStyleProperty(CSSPropertyMarginLeft,
                                      box().marginLeft() / zoomFactor,
                                      CSSPrimitiveValue::UnitType::Pixels);
      element->setInlineStyleProperty(CSSPropertyMarginRight,
                                      box().marginRight() / zoomFactor,
                                      CSSPrimitiveValue::UnitType::Pixels);
    }
    LayoutUnit baseWidth =
        box().size().width() -
        (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingWidth());
    baseWidth = LayoutUnit(baseWidth / zoomFactor);
    element->setInlineStyleProperty(CSSPropertyWidth,
                                    roundToInt(baseWidth + difference.width()),
                                    CSSPrimitiveValue::UnitType::Pixels);
  }

  if (resize != RESIZE_HORIZONTAL && difference.height()) {
    if (element->isFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->setInlineStyleProperty(CSSPropertyMarginTop,
                                      box().marginTop() / zoomFactor,
                                      CSSPrimitiveValue::UnitType::Pixels);
      element->setInlineStyleProperty(CSSPropertyMarginBottom,
                                      box().marginBottom() / zoomFactor,
                                      CSSPrimitiveValue::UnitType::Pixels);
    }
    LayoutUnit baseHeight =
        box().size().height() -
        (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingHeight());
    baseHeight = LayoutUnit(baseHeight / zoomFactor);
    element->setInlineStyleProperty(
        CSSPropertyHeight, roundToInt(baseHeight + difference.height()),
        CSSPrimitiveValue::UnitType::Pixels);
  }

  document.updateStyleAndLayout();

  // FIXME (Radar 4118564): We should also autoscroll the window as necessary to
  // keep the point under the cursor in view.
}

LayoutRect PaintLayerScrollableArea::scrollIntoView(
    const LayoutRect& rect,
    const ScrollAlignment& alignX,
    const ScrollAlignment& alignY,
    ScrollType scrollType) {
  LayoutRect localExposeRect(
      box()
          .absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms)
          .boundingBox());
  localExposeRect.move(-box().borderLeft(), -box().borderTop());
  LayoutRect layerBounds(LayoutPoint(),
                         LayoutSize(box().clientWidth(), box().clientHeight()));
  LayoutRect r = ScrollAlignment::getRectToExpose(layerBounds, localExposeRect,
                                                  alignX, alignY);

  ScrollOffset oldScrollOffset = scrollOffset();
  ScrollOffset newScrollOffset(clampScrollOffset(roundedIntSize(
      toScrollOffset(FloatPoint(r.location()) + oldScrollOffset))));
  setScrollOffset(newScrollOffset, scrollType, ScrollBehaviorInstant);
  ScrollOffset scrollOffsetDifference = scrollOffset() - oldScrollOffset;
  localExposeRect.move(-LayoutSize(scrollOffsetDifference));

  LayoutRect intersect =
      localToAbsolute(box(), intersection(layerBounds, localExposeRect));
  if (intersect.isEmpty() && !layerBounds.isEmpty() &&
      !localExposeRect.isEmpty()) {
    return localToAbsolute(box(), localExposeRect);
  }
  return intersect;
}

void PaintLayerScrollableArea::updateScrollableAreaSet(bool hasOverflow) {
  LocalFrame* frame = box().frame();
  if (!frame)
    return;

  FrameView* frameView = frame->view();
  if (!frameView)
    return;

  // FIXME: Does this need to be fixed later for OOPI?
  bool isVisibleToHitTest = box().style()->visibleToHitTesting();
  if (HTMLFrameOwnerElement* owner = frame->deprecatedLocalOwner()) {
    isVisibleToHitTest &= owner->layoutObject() &&
                          owner->layoutObject()->style()->visibleToHitTesting();
  }

  bool didScrollOverflow = m_scrollsOverflow;

  m_scrollsOverflow = hasOverflow && isVisibleToHitTest;
  if (didScrollOverflow == scrollsOverflow())
    return;

  if (m_scrollsOverflow) {
    DCHECK(canHaveOverflowScrollbars(box()));
    frameView->addScrollableArea(this);
  } else {
    frameView->removeScrollableArea(this);
  }
}

void PaintLayerScrollableArea::updateCompositingLayersAfterScroll() {
  PaintLayerCompositor* compositor = box().view()->compositor();
  if (compositor->inCompositingMode()) {
    if (usesCompositedScrolling()) {
      DCHECK(layer()->hasCompositedLayerMapping());
      layer()->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(
          GraphicsLayerUpdateSubtree);
      compositor->setNeedsCompositingUpdate(
          CompositingUpdateAfterGeometryChange);
    } else {
      layer()->setNeedsCompositingInputsUpdate();
    }
  }
}

bool PaintLayerScrollableArea::usesCompositedScrolling() const {
  // See https://codereview.chromium.org/176633003/ for the tests that fail
  // without this disabler.
  DisableCompositingQueryAsserts disabler;
  return layer()->hasCompositedLayerMapping() &&
         layer()->compositedLayerMapping()->scrollingLayer();
}

bool PaintLayerScrollableArea::shouldScrollOnMainThread() const {
  if (LocalFrame* frame = box().frame()) {
    if (Page* page = frame->page()) {
      if (page->scrollingCoordinator()
              ->shouldUpdateScrollLayerPositionOnMainThread())
        return true;
    }
  }
  return ScrollableArea::shouldScrollOnMainThread();
}

static bool layerNeedsCompositedScrolling(
    PaintLayerScrollableArea::LCDTextMode mode,
    const PaintLayer* layer) {
  if (!layer->scrollsOverflow())
    return false;

  Node* node = layer->enclosingNode();
  if (node && node->isElementNode() &&
      (toElement(node)->compositorMutableProperties() &
       (CompositorMutableProperty::kScrollTop |
        CompositorMutableProperty::kScrollLeft)))
    return true;

  // TODO(flackr): Allow integer transforms as long as all of the ancestor
  // transforms are also integer.
  bool backgroundSupportsLCDText =
      RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled() &&
      layer->layoutObject()->style()->isStackingContext() &&
      layer->canPaintBackgroundOntoScrollingContentsLayer() &&
      layer->backgroundIsKnownToBeOpaqueInRect(
          toLayoutBox(layer->layoutObject())->paddingBoxRect()) &&
      !layer->compositesWithTransform() && !layer->compositesWithOpacity();
  if (mode == PaintLayerScrollableArea::ConsiderLCDText &&
      !layer->compositor()->preferCompositingToLCDTextEnabled() &&
      !backgroundSupportsLCDText)
    return false;

  // TODO(schenney) Tests fail if we do not also exclude
  // layer->layoutObject()->style()->hasBorderDecoration() (missing background
  // behind dashed borders). Resolve this case, or not, and update this check
  // with the results.
  return !(layer->size().isEmpty() || layer->hasDescendantWithClipPath() ||
           layer->hasAncestorWithClipPath() ||
           layer->layoutObject()->style()->hasBorderRadius());
}

void PaintLayerScrollableArea::updateNeedsCompositedScrolling(
    LCDTextMode mode) {
  const bool needsCompositedScrolling =
      layerNeedsCompositedScrolling(mode, layer());
  if (static_cast<bool>(m_needsCompositedScrolling) !=
      needsCompositedScrolling) {
    m_needsCompositedScrolling = needsCompositedScrolling;
    layer()->didUpdateNeedsCompositedScrolling();
  }
}

void PaintLayerScrollableArea::setTopmostScrollChild(PaintLayer* scrollChild) {
  // We only want to track the topmost scroll child for scrollable areas with
  // overlay scrollbars.
  if (!hasOverlayScrollbars())
    return;
  m_nextTopmostScrollChild = scrollChild;
}

bool PaintLayerScrollableArea::visualViewportSuppliesScrollbars() const {
  LocalFrame* frame = box().frame();
  if (!frame || !frame->settings())
    return false;

  // On desktop, we always use the layout viewport's scrollbars.
  if (!frame->settings()->viewportEnabled())
    return false;

  const TopDocumentRootScrollerController& controller =
      layoutBox()->document().frameHost()->globalRootScrollerController();

  if (!controller.globalRootScroller())
    return false;

  return RootScrollerUtil::scrollableAreaFor(
             *controller.globalRootScroller()) == this;
}

Widget* PaintLayerScrollableArea::getWidget() {
  return box().frame()->view();
}

void PaintLayerScrollableArea::resetRebuildScrollbarLayerFlags() {
  m_rebuildHorizontalScrollbarLayer = false;
  m_rebuildVerticalScrollbarLayer = false;
}

CompositorAnimationTimeline*
PaintLayerScrollableArea::compositorAnimationTimeline() const {
  if (LocalFrame* frame = box().frame()) {
    if (Page* page = frame->page())
      return page->scrollingCoordinator()
                 ? page->scrollingCoordinator()->compositorAnimationTimeline()
                 : nullptr;
  }
  return nullptr;
}

PaintLayerScrollableArea*
PaintLayerScrollableArea::ScrollbarManager::scrollableArea() {
  return toPaintLayerScrollableArea(m_scrollableArea.get());
}

void PaintLayerScrollableArea::ScrollbarManager::destroyDetachedScrollbars() {
  DCHECK(!m_hBarIsAttached || m_hBar);
  DCHECK(!m_vBarIsAttached || m_vBar);
  if (m_hBar && !m_hBarIsAttached)
    destroyScrollbar(HorizontalScrollbar);
  if (m_vBar && !m_vBarIsAttached)
    destroyScrollbar(VerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::setHasHorizontalScrollbar(
    bool hasScrollbar) {
  if (hasScrollbar) {
    // This doesn't hit in any tests, but since the equivalent code in
    // setHasVerticalScrollbar does, presumably this code does as well.
    DisableCompositingQueryAsserts disabler;
    if (!m_hBar) {
      m_hBar = createScrollbar(HorizontalScrollbar);
      m_hBarIsAttached = 1;
      if (!m_hBar->isCustomScrollbar())
        scrollableArea()->didAddScrollbar(*m_hBar, HorizontalScrollbar);
    } else {
      m_hBarIsAttached = 1;
    }
  } else {
    m_hBarIsAttached = 0;
    if (!DelayScrollOffsetClampScope::clampingIsDelayed())
      destroyScrollbar(HorizontalScrollbar);
  }
}

void PaintLayerScrollableArea::ScrollbarManager::setHasVerticalScrollbar(
    bool hasScrollbar) {
  if (hasScrollbar) {
    DisableCompositingQueryAsserts disabler;
    if (!m_vBar) {
      m_vBar = createScrollbar(VerticalScrollbar);
      m_vBarIsAttached = 1;
      if (!m_vBar->isCustomScrollbar())
        scrollableArea()->didAddScrollbar(*m_vBar, VerticalScrollbar);
    } else {
      m_vBarIsAttached = 1;
    }
  } else {
    m_vBarIsAttached = 0;
    if (!DelayScrollOffsetClampScope::clampingIsDelayed())
      destroyScrollbar(VerticalScrollbar);
  }
}

Scrollbar* PaintLayerScrollableArea::ScrollbarManager::createScrollbar(
    ScrollbarOrientation orientation) {
  DCHECK(orientation == HorizontalScrollbar ? !m_hBarIsAttached
                                            : !m_vBarIsAttached);
  Scrollbar* scrollbar = nullptr;
  const LayoutObject& actualLayoutObject =
      layoutObjectForScrollbar(scrollableArea()->box());
  bool hasCustomScrollbarStyle =
      actualLayoutObject.isBox() &&
      actualLayoutObject.styleRef().hasPseudoStyle(PseudoIdScrollbar);
  if (hasCustomScrollbarStyle) {
    scrollbar = LayoutScrollbar::createCustomScrollbar(
        scrollableArea(), orientation, actualLayoutObject.node());
  } else {
    ScrollbarControlSize scrollbarSize = RegularScrollbar;
    if (actualLayoutObject.styleRef().hasAppearance())
      scrollbarSize = LayoutTheme::theme().scrollbarControlSizeForPart(
          actualLayoutObject.styleRef().appearance());
    scrollbar = Scrollbar::create(
        scrollableArea(), orientation, scrollbarSize,
        &scrollableArea()->box().frame()->page()->chromeClient());
  }
  scrollableArea()->box().document().view()->addChild(scrollbar);
  return scrollbar;
}

void PaintLayerScrollableArea::ScrollbarManager::destroyScrollbar(
    ScrollbarOrientation orientation) {
  Member<Scrollbar>& scrollbar =
      orientation == HorizontalScrollbar ? m_hBar : m_vBar;
  DCHECK(orientation == HorizontalScrollbar ? !m_hBarIsAttached
                                            : !m_vBarIsAttached);
  if (!scrollbar)
    return;

  scrollableArea()->setScrollbarNeedsPaintInvalidation(orientation);
  if (orientation == HorizontalScrollbar)
    scrollableArea()->m_rebuildHorizontalScrollbarLayer = true;
  else
    scrollableArea()->m_rebuildVerticalScrollbarLayer = true;

  if (!scrollbar->isCustomScrollbar())
    scrollableArea()->willRemoveScrollbar(*scrollbar, orientation);

  toFrameView(scrollbar->parent())->removeChild(scrollbar.get());
  scrollbar->disconnectFromScrollableArea();
  scrollbar = nullptr;
}

uint64_t PaintLayerScrollableArea::id() const {
  return DOMNodeIds::idForNode(box().node());
}

int PaintLayerScrollableArea::PreventRelayoutScope::s_count = 0;
SubtreeLayoutScope*
    PaintLayerScrollableArea::PreventRelayoutScope::s_layoutScope = nullptr;
bool PaintLayerScrollableArea::PreventRelayoutScope::s_relayoutNeeded = false;
PersistentHeapVector<Member<PaintLayerScrollableArea>>*
    PaintLayerScrollableArea::PreventRelayoutScope::s_needsRelayout = nullptr;

PaintLayerScrollableArea::PreventRelayoutScope::PreventRelayoutScope(
    SubtreeLayoutScope& layoutScope) {
  if (!s_count) {
    DCHECK(!s_layoutScope);
    DCHECK(!s_needsRelayout || s_needsRelayout->isEmpty());
    s_layoutScope = &layoutScope;
  }
  s_count++;
}

PaintLayerScrollableArea::PreventRelayoutScope::~PreventRelayoutScope() {
  if (--s_count == 0) {
    if (s_relayoutNeeded) {
      for (auto scrollableArea : *s_needsRelayout) {
        DCHECK(scrollableArea->needsRelayout());
        LayoutBox& box = scrollableArea->box();
        s_layoutScope->setNeedsLayout(
            &box, LayoutInvalidationReason::ScrollbarChanged);
        if (box.isLayoutBlock()) {
          bool horizontalScrollbarChanged =
              scrollableArea->hasHorizontalScrollbar() !=
              scrollableArea->hadHorizontalScrollbarBeforeRelayout();
          bool verticalScrollbarChanged =
              scrollableArea->hasVerticalScrollbar() !=
              scrollableArea->hadVerticalScrollbarBeforeRelayout();
          if (horizontalScrollbarChanged || verticalScrollbarChanged)
            toLayoutBlock(box).scrollbarsChanged(horizontalScrollbarChanged,
                                                 verticalScrollbarChanged);
        }
        scrollableArea->setNeedsRelayout(false);
      }

      s_needsRelayout->clear();
    }
    s_layoutScope = nullptr;
  }
}

void PaintLayerScrollableArea::PreventRelayoutScope::setBoxNeedsLayout(
    PaintLayerScrollableArea& scrollableArea,
    bool hadHorizontalScrollbar,
    bool hadVerticalScrollbar) {
  DCHECK(s_count);
  DCHECK(s_layoutScope);
  if (scrollableArea.needsRelayout())
    return;
  scrollableArea.setNeedsRelayout(true);
  scrollableArea.setHadHorizontalScrollbarBeforeRelayout(
      hadHorizontalScrollbar);
  scrollableArea.setHadVerticalScrollbarBeforeRelayout(hadVerticalScrollbar);

  s_relayoutNeeded = true;
  if (!s_needsRelayout)
    s_needsRelayout =
        new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
  s_needsRelayout->append(&scrollableArea);
}

void PaintLayerScrollableArea::PreventRelayoutScope::resetRelayoutNeeded() {
  DCHECK_EQ(s_count, 0);
  DCHECK(!s_needsRelayout || s_needsRelayout->isEmpty());
  s_relayoutNeeded = false;
}

int PaintLayerScrollableArea::FreezeScrollbarsScope::s_count = 0;

int PaintLayerScrollableArea::DelayScrollOffsetClampScope::s_count = 0;
PersistentHeapVector<Member<PaintLayerScrollableArea>>*
    PaintLayerScrollableArea::DelayScrollOffsetClampScope::s_needsClamp =
        nullptr;

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    DelayScrollOffsetClampScope() {
  if (!s_needsClamp)
    s_needsClamp = new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
  DCHECK(s_count > 0 || s_needsClamp->isEmpty());
  s_count++;
}

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    ~DelayScrollOffsetClampScope() {
  if (--s_count == 0)
    DelayScrollOffsetClampScope::clampScrollableAreas();
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::setNeedsClamp(
    PaintLayerScrollableArea* scrollableArea) {
  if (!scrollableArea->needsScrollOffsetClamp()) {
    scrollableArea->setNeedsScrollOffsetClamp(true);
    s_needsClamp->append(scrollableArea);
  }
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    clampScrollableAreas() {
  for (auto& scrollableArea : *s_needsClamp)
    scrollableArea->clampScrollOffsetsAfterLayout();
  delete s_needsClamp;
  s_needsClamp = nullptr;
}

}  // namespace blink
