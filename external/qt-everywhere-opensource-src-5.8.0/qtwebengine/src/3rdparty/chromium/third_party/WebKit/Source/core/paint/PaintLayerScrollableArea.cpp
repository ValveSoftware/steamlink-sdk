/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
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

PaintLayerScrollableAreaRareData::PaintLayerScrollableAreaRareData()
{
}

const int ResizerControlExpandRatioForTouch = 2;

PaintLayerScrollableArea::PaintLayerScrollableArea(PaintLayer& layer)
    : m_layer(layer)
    , m_nextTopmostScrollChild(0)
    , m_topmostScrollChild(0)
    , m_inResizeMode(false)
    , m_scrollsOverflow(false)
    , m_inOverflowRelayout(false)
    , m_needsCompositedScrolling(false)
    , m_rebuildHorizontalScrollbarLayer(false)
    , m_rebuildVerticalScrollbarLayer(false)
    , m_needsScrollPositionClamp(false)
    , m_needsRelayout(false)
    , m_hadHorizontalScrollbarBeforeRelayout(false)
    , m_hadVerticalScrollbarBeforeRelayout(false)
    , m_scrollbarManager(*this)
    , m_scrollCorner(nullptr)
    , m_resizer(nullptr)
    , m_scrollAnchor(this)
#if ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
{
    Node* node = box().node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator().setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }
    updateResizerAreaSet();
}

PaintLayerScrollableArea::~PaintLayerScrollableArea()
{
    ASSERT(m_hasBeenDisposed);
}

void PaintLayerScrollableArea::dispose()
{
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
        if (ScrollingCoordinator* scrollingCoordinator = box().frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (!box().documentBeingDestroyed()) {
        Node* node = box().node();
        // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
        if (node && node->isElementNode())
            toElement(node)->setSavedLayerScrollOffset(flooredIntSize(m_scrollOffset));
    }

    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeResizerArea(box());
    }

    m_scrollbarManager.dispose();

    if (m_scrollCorner)
        m_scrollCorner->destroy();
    if (m_resizer)
        m_resizer->destroy();

    clearScrollAnimators();

    // Note: it is not safe to call ScrollAnchor::clear if the document is being destroyed,
    // because LayoutObjectChildList::removeChildNode skips the call to willBeRemovedFromTree,
    // leaving the ScrollAnchor with a stale LayoutObject pointer.
    if (RuntimeEnabledFeatures::scrollAnchoringEnabled() && !box().documentBeingDestroyed())
        m_scrollAnchor.clear();

#if ENABLE(ASSERT)
    m_hasBeenDisposed = true;
#endif
}

DEFINE_TRACE(PaintLayerScrollableArea)
{
    visitor->trace(m_scrollbarManager);
    visitor->trace(m_scrollAnchor);
    ScrollableArea::trace(visitor);
}

HostWindow* PaintLayerScrollableArea::getHostWindow() const
{
    if (Page* page = box().frame()->page())
        return &page->chromeClient();
    return nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrolling() const
{
    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->scrollingContentsLayer() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForHorizontalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForHorizontalScrollbar() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForVerticalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForVerticalScrollbar() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrollCorner() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForScrollCorner() : 0;
}

bool PaintLayerScrollableArea::shouldUseIntegerScrollOffset() const
{
    Frame* frame = box().frame();
    if (frame->settings() && !frame->settings()->preferCompositingToLCDTextEnabled())
        return true;

    return ScrollableArea::shouldUseIntegerScrollOffset();
}

bool PaintLayerScrollableArea::isActive() const
{
    Page* page = box().frame()->page();
    return page && page->focusController().isActive();
}

bool PaintLayerScrollableArea::isScrollCornerVisible() const
{
    return !scrollCornerRect().isEmpty();
}

static int cornerStart(const LayoutBox& box, int minX, int maxX, int thickness)
{
    if (box.shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box.styleRef().borderLeftWidth();
    return maxX - thickness - box.styleRef().borderRightWidth();
}

static IntRect cornerRect(const LayoutBox& box, const Scrollbar* horizontalScrollbar, const Scrollbar* verticalScrollbar, const IntRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!verticalScrollbar && !horizontalScrollbar) {
        // FIXME: This isn't right. We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
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
    return IntRect(cornerStart(box, bounds.x(), bounds.maxX(), horizontalThickness),
        bounds.maxY() - verticalThickness - box.styleRef().borderBottomWidth(),
        horizontalThickness, verticalThickness);
}

IntRect PaintLayerScrollableArea::scrollCornerRect() const
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = horizontalScrollbar();
    bool hasVerticalBar = verticalScrollbar();
    bool hasResizer = box().style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(box(), horizontalScrollbar(), verticalScrollbar(), box().pixelSnappedBorderBoxRect());
    return IntRect();
}

IntRect PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    LayoutView* view = box().view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromLayoutObject(box(), rect);
}

IntRect PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    LayoutView* view = box().view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToLayoutObject(box(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    LayoutView* view = box().view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromLayoutObject(box(), point);
}

IntPoint PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    LayoutView* view = box().view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToLayoutObject(box(), parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

int PaintLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

void PaintLayerScrollableArea::setScrollOffset(const DoublePoint& newScrollOffset, ScrollType scrollType)
{
    if (scrollOffset() == toDoubleSize(newScrollOffset))
        return;

    DoubleSize scrollDelta = scrollOffset() - toDoubleSize(newScrollOffset);
    m_scrollOffset = toDoubleSize(newScrollOffset);

    LocalFrame* frame = box().frame();
    ASSERT(frame);

    FrameView* frameView = box().frameView();

    TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data", InspectorScrollLayerEvent::data(&box()));

    // FIXME(420741): Resolve circular dependency between scroll offset and
    // compositing state, and remove this disabler.
    DisableCompositingQueryAsserts disabler;

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    if (!frameView->isInPerformLayout()) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        layer()->updateLayerPositionsAfterOverflowScroll(scrollDelta);
        // Update regions, scrolling may change the clip of a particular region.
        frameView->updateDocumentAnnotatedRegions();
        frameView->setNeedsUpdateWidgetGeometries();
        updateCompositingLayersAfterScroll();
    }

    const LayoutBoxModelObject& paintInvalidationContainer = box().containerForPaintInvalidation();
    // The caret rect needs to be invalidated after scrolling
    frame->selection().setCaretRectNeedsUpdate();

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(FloatRect(layer()->layoutObject()->previousPaintInvalidationRectIncludingCompositedScrolling(paintInvalidationContainer)));

    quadForFakeMouseMoveEvent = paintInvalidationContainer.localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame->eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresPaintInvalidation = true;

    if (box().view()->compositor()->inCompositingMode()) {
        bool onlyScrolledCompositedLayers = scrollsOverflow()
            && layer()->isAllScrollingContentComposited()
            && box().style()->backgroundLayers().attachment() != LocalBackgroundAttachment;

        if (usesCompositedScrolling() || onlyScrolledCompositedLayers)
            requiresPaintInvalidation = false;
    }

    // Only the root layer can overlap non-composited fixed-position elements.
    if (!requiresPaintInvalidation && layer()->isRootLayer() && frameView->hasViewportConstrainedObjects()) {
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

    // Inform the FrameLoader of the new scroll position, so it can be restored when navigating back.
    if (layer()->isRootLayer()) {
        frameView->frame().loader().saveScrollState();
        frame->loader().client()->didChangeScrollOffset();
    }

    // All scrolls clear the fragment anchor.
    frameView->clearFragmentAnchor();

    // Clear the scroll anchor, unless it is the reason for this scroll.
    if (RuntimeEnabledFeatures::scrollAnchoringEnabled() && scrollType != AnchoringScroll)
        scrollAnchor().clear();
}

IntPoint PaintLayerScrollableArea::scrollPosition() const
{
    return IntPoint(flooredIntSize(m_scrollOffset));
}

DoublePoint PaintLayerScrollableArea::scrollPositionDouble() const
{
    return DoublePoint(m_scrollOffset);
}

IntPoint PaintLayerScrollableArea::minimumScrollPosition() const
{
    return -scrollOrigin();
}

IntPoint PaintLayerScrollableArea::maximumScrollPosition() const
{
    IntSize contentSize;
    IntSize visibleSize;
    if (box().hasOverflowClip()) {
        contentSize = IntSize(pixelSnappedScrollWidth(), pixelSnappedScrollHeight());
        visibleSize = pixelSnappedIntRect(box().overflowClipRect(box().location())).size();

        // TODO(skobes): We should really ASSERT that contentSize >= visibleSize
        // when we are not the root layer, but we can't because contentSize is
        // based on stale layout overflow data (http://crbug.com/576933).
        contentSize = contentSize.expandedTo(visibleSize);
    }
    return -scrollOrigin() + (contentSize - visibleSize);
}

IntRect PaintLayerScrollableArea::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;
    if (scrollbarInclusion == IncludeScrollbars) {
        verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0;
        horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar()) ? horizontalScrollbar()->height() : 0;
    }

    return IntRect(IntPoint(scrollXOffset(), scrollYOffset()),
        IntSize(max(0, layer()->size().width() - verticalScrollbarWidth), max(0, layer()->size().height() - horizontalScrollbarHeight)));
}

int PaintLayerScrollableArea::visibleHeight() const
{
    return layer()->size().height();
}

int PaintLayerScrollableArea::visibleWidth() const
{
    return layer()->size().width();
}

IntSize PaintLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntPoint PaintLayerScrollableArea::lastKnownMousePosition() const
{
    return box().frame() ? box().frame()->eventHandler().lastKnownMousePosition() : IntPoint();
}

bool PaintLayerScrollableArea::scrollAnimatorEnabled() const
{
    if (Settings* settings = box().frame()->settings())
        return settings->scrollAnimatorEnabled();
    return false;
}

bool PaintLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    LayoutView* view = box().view();
    if (!view)
        return true;
    return view->frameView()->shouldSuspendScrollAnimations();
}

void PaintLayerScrollableArea::scrollbarVisibilityChanged()
{
    if (LayoutView* view = box().view())
        return view->clearHitTestCache();
}

bool PaintLayerScrollableArea::scrollbarsCanBeActive() const
{
    LayoutView* view = box().view();
    if (!view)
        return false;
    return view->frameView()->scrollbarsCanBeActive();
}

IntRect PaintLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return box().absoluteBoundingBoxRect();
}

void PaintLayerScrollableArea::registerForAnimation()
{
    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->addAnimatingScrollableArea(this);
    }
}

void PaintLayerScrollableArea::deregisterForAnimation()
{
    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeAnimatingScrollableArea(this);
    }
}

bool PaintLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    if (box().isIntrinsicallyScrollable(orientation))
        return true;

    EOverflow overflowStyle = (orientation == HorizontalScrollbar) ?
        box().style()->overflowX() : box().style()->overflowY();
    return (overflowStyle == OverflowScroll || overflowStyle == OverflowAuto || overflowStyle == OverflowOverlay);
}

bool PaintLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int PaintLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    int length = (orientation == HorizontalScrollbar) ?
        box().pixelSnappedClientWidth() : box().pixelSnappedClientHeight();
    int minPageStep = static_cast<float>(length) * ScrollableArea::minFractionToStepWhenPaging();
    int pageStep = max(minPageStep, length - ScrollableArea::maxOverlapBetweenPages());

    return max(pageStep, 1);
}

LayoutBox& PaintLayerScrollableArea::box() const
{
    return *m_layer.layoutBox();
}

PaintLayer* PaintLayerScrollableArea::layer() const
{
    return &m_layer;
}

LayoutUnit PaintLayerScrollableArea::scrollWidth() const
{
    return m_overflowRect.width();
}

LayoutUnit PaintLayerScrollableArea::scrollHeight() const
{
    return m_overflowRect.height();
}

int PaintLayerScrollableArea::pixelSnappedScrollWidth() const
{
    return snapSizeToPixel(scrollWidth(), box().clientLeft() + box().location().x());
}

int PaintLayerScrollableArea::pixelSnappedScrollHeight() const
{
    return snapSizeToPixel(scrollHeight(), box().clientTop() + box().location().y());
}

void PaintLayerScrollableArea::updateScrollOrigin()
{
    // This should do nothing prior to first layout; the if-clause will catch that.
    if (overflowRect().isEmpty())
        return;
    LayoutPoint scrollableOverflow = m_overflowRect.location() - LayoutSize(box().borderLeft(), box().borderTop());
    setScrollOrigin(flooredIntPoint(-scrollableOverflow) + box().originAdjustmentForScrollbars());
}

void PaintLayerScrollableArea::updateScrollDimensions()
{
    m_overflowRect = box().layoutOverflowRect();
    box().flipForWritingMode(m_overflowRect);
    updateScrollOrigin();
}

void PaintLayerScrollableArea::scrollToPosition(const DoublePoint& scrollPosition, ScrollOffsetClamping clamp, ScrollBehavior scrollBehavior, ScrollType scrollType)
{
    DoublePoint newScrollPosition = clamp == ScrollOffsetClamped ? clampScrollPosition(scrollPosition) : scrollPosition;
    if (newScrollPosition != scrollPositionDouble())
        ScrollableArea::setScrollPosition(newScrollPosition, scrollType, scrollBehavior);
}

void PaintLayerScrollableArea::updateAfterLayout()
{
    ASSERT(box().hasOverflowClip());

    bool relayoutIsPrevented = PreventRelayoutScope::relayoutIsPrevented();
    bool scrollbarsAreFrozen = m_inOverflowRelayout || FreezeScrollbarsScope::scrollbarsAreFrozen();

    if (needsScrollbarReconstruction()) {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    updateScrollDimensions();

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    // Don't add auto scrollbars if the box contents aren't visible.
    bool shouldHaveAutoHorizontalScrollbar = hasHorizontalOverflow && box().pixelSnappedClientHeight();
    bool shouldHaveAutoVerticalScrollbar = hasVerticalOverflow && box().pixelSnappedClientWidth();

    {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // overflow:scroll should just enable/disable.
        if (box().style()->overflowX() == OverflowScroll && horizontalScrollbar())
            horizontalScrollbar()->setEnabled(hasHorizontalOverflow);
        if (box().style()->overflowY() == OverflowScroll && verticalScrollbar())
            verticalScrollbar()->setEnabled(hasVerticalOverflow);
    }

    // We need to layout again if scrollbars are added or removed by overflow:auto,
    // or by changing between native and custom.
    bool horizontalScrollbarShouldChange = (box().hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != shouldHaveAutoHorizontalScrollbar))
        || (box().style()->overflowX() == OverflowScroll && !horizontalScrollbar());
    bool verticalScrollbarShouldChange = (box().hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != shouldHaveAutoVerticalScrollbar))
        || (box().style()->overflowY() == OverflowScroll && !verticalScrollbar());
    bool scrollbarsWillChange = !scrollbarsAreFrozen && !visualViewportSuppliesScrollbars()
        && (horizontalScrollbarShouldChange || verticalScrollbarShouldChange);

    if (scrollbarsWillChange) {
        bool hadHorizontalScrollbar = hasHorizontalScrollbar();
        bool hadVerticalScrollbar = hasVerticalScrollbar();
        if (box().hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(shouldHaveAutoHorizontalScrollbar);
        else if (box().style()->overflowX() == OverflowScroll)
            setHasHorizontalScrollbar(true);
        if (box().hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(shouldHaveAutoVerticalScrollbar);
        else if (box().style()->overflowY() == OverflowScroll)
            setHasVerticalScrollbar(true);

        if (hasScrollbar())
            updateScrollCornerStyle();

        layer()->updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (box().document().hasAnnotatedRegions())
            box().document().setAnnotatedRegionsDirty(true);

        // Our proprietary overflow: overlay value doesn't trigger a layout.
        if ((horizontalScrollbarShouldChange && box().style()->overflowX() != OverflowOverlay)
            || (verticalScrollbarShouldChange && box().style()->overflowY() != OverflowOverlay)) {
            if ((verticalScrollbarShouldChange && box().isHorizontalWritingMode())
                || (horizontalScrollbarShouldChange && !box().isHorizontalWritingMode())) {
                box().setPreferredLogicalWidthsDirty();
            }
            if (relayoutIsPrevented) {
                // We're not doing re-layout right now, but we still want to
                // add the scrollbar to the logical width now, to facilitate parent layout.
                box().updateLogicalWidth();
                PreventRelayoutScope::setBoxNeedsLayout(*this, hadHorizontalScrollbar, hadVerticalScrollbar);
            } else {
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(box());
                layoutScope.setNeedsLayout(&box(), LayoutInvalidationReason::ScrollbarChanged);
                if (box().isLayoutBlock()) {
                    LayoutBlock& block = toLayoutBlock(box());
                    block.scrollbarsChanged(horizontalScrollbarShouldChange, verticalScrollbarShouldChange);
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
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // Set up the range (and page step/line step).
        if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
            int clientWidth = box().pixelSnappedClientWidth();
            horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
        }
        if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
            int clientHeight = box().pixelSnappedClientHeight();
            verticalScrollbar->setProportion(clientHeight, overflowRect().height());
        }
    }

    if (!scrollbarsAreFrozen && hasOverlayScrollbars()) {
        if (!scrollSize(HorizontalScrollbar))
            setHasHorizontalScrollbar(false);
        if (!scrollSize(VerticalScrollbar))
            setHasVerticalScrollbar(false);
    }

    clampScrollPositionsAfterLayout();

    if (!scrollbarsAreFrozen) {
        bool hasOverflow = hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow();
        updateScrollableAreaSet(hasOverflow);
    }

    DisableCompositingQueryAsserts disabler;
    positionOverflowControls();
}


void PaintLayerScrollableArea::clampScrollPositionsAfterLayout()
{
    // If a vertical scrollbar was removed, the min/max scroll positions may have changed,
    // so the scroll positions needs to be clamped.  If the scroll position did not change,
    // but the scroll origin *did* change, we still need to notify the scrollbars to
    // update their dimensions.

    if (DelayScrollPositionClampScope::clampingIsDelayed()) {
        DelayScrollPositionClampScope::setNeedsClamp(this);
        return;
    }

    DoublePoint clampedScrollPosition = clampScrollPosition(scrollPositionDouble());
    if (clampedScrollPosition != scrollPositionDouble())
        ScrollableArea::setScrollPosition(clampedScrollPosition, ProgrammaticScroll);
    else if (scrollOriginChanged())
        scrollPositionChanged(clampedScrollPosition, ProgrammaticScroll);

    setNeedsScrollPositionClamp(false);
    resetScrollOriginChanged();
    m_scrollbarManager.destroyDetachedScrollbars();
}

ScrollBehavior PaintLayerScrollableArea::scrollBehaviorStyle() const
{
    return box().style()->getScrollBehavior();
}

bool PaintLayerScrollableArea::hasHorizontalOverflow() const
{
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

bool PaintLayerScrollableArea::hasVerticalOverflow() const
{
    return pixelSnappedScrollHeight() > box().pixelSnappedClientHeight();
}

bool PaintLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && box().scrollsOverflowX();
}

bool PaintLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && box().scrollsOverflowY();
}

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OverflowScroll;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OverflowAuto || overflow == OverflowOverlay;
}

// This function returns true if the given box requires overflow scrollbars (as
// opposed to the 'viewport' scrollbars managed by the PaintLayerCompositor).
// FIXME: we should use the same scrolling machinery for both the viewport and
// overflow. Currently, we need to avoid producing scrollbars here if they'll be
// handled externally in the RLC.
static bool canHaveOverflowScrollbars(const LayoutBox& box)
{
    bool rootLayerScrolls = box.document().settings() && box.document().settings()->rootLayerScrolls();
    return (rootLayerScrolls || !box.isLayoutView()) && box.document().viewportDefiningElement() != box.node();
}

void PaintLayerScrollableArea::updateAfterStyleChange(const ComputedStyle* oldStyle)
{
    // Don't do this on first style recalc, before layout has ever happened.
    if (!overflowRect().size().isZero())
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    if (!canHaveOverflowScrollbars(box()))
        return;

    // Avoid drawing two sets of scrollbars when one is provided by the visual viewport.
    if (visualViewportSuppliesScrollbars()) {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
        return;
    }

    EOverflow overflowX = box().style()->overflowX();
    EOverflow overflowY = box().style()->overflowY();

    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);

    // Look for the scrollbarModes and reset the needs Horizontal & vertical Scrollbar values based on scrollbarModes, as during force style change
    // StyleResolver::styleForDocument returns documentStyle with  no overflow values, due to which we are destorying the scrollbars that was
    // already present.
    if (box().isLayoutView()) {
        if (LocalFrame* frame = box().frame()) {
            if (FrameView* frameView = frame->view()) {
                ScrollbarMode hMode;
                ScrollbarMode vMode;
                frameView->calculateScrollbarModes(hMode, vMode);
                if (hMode == ScrollbarAlwaysOn && !needsHorizontalScrollbar)
                    needsHorizontalScrollbar = true;
                if (vMode == ScrollbarAlwaysOn && !needsVerticalScrollbar)
                    needsVerticalScrollbar = true;
            }
        }
    }

    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OverflowScroll && overflowX != OverflowScroll) {
        ASSERT(hasHorizontalScrollbar());
        horizontalScrollbar()->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OverflowScroll && overflowY != OverflowScroll) {
        ASSERT(hasVerticalScrollbar());
        verticalScrollbar()->setEnabled(true);
    }

    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (horizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (verticalScrollbar())
        verticalScrollbar()->styleChanged();

    updateScrollCornerStyle();
    updateResizerAreaSet();
    updateResizerStyle();

    // Whenever background changes on the scrollable element, the scroll bar
    // overlay style might need to be changed to have contrast against the
    // background.
    Color oldBackground;
    if (oldStyle) {
        oldBackground = oldStyle->visitedDependentColor(CSSPropertyBackgroundColor);
    }
    Color newBackground = box().style()->visitedDependentColor(CSSPropertyBackgroundColor);

    if (newBackground != oldBackground) {
        recalculateScrollbarOverlayStyle(newBackground);
    }
}

bool PaintLayerScrollableArea::updateAfterCompositingChange()
{
    layer()->updateScrollingStateAfterCompositingChange();
    const bool layersChanged = m_topmostScrollChild != m_nextTopmostScrollChild;
    m_topmostScrollChild = m_nextTopmostScrollChild;
    m_nextTopmostScrollChild = nullptr;
    return layersChanged;
}

void PaintLayerScrollableArea::updateAfterOverflowRecalc()
{
    updateScrollDimensions();
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        int clientWidth = box().pixelSnappedClientWidth();
        horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
    }
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        int clientHeight = box().pixelSnappedClientHeight();
        verticalScrollbar->setProportion(clientHeight, overflowRect().height());
    }

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();
    bool autoHorizontalScrollbarChanged = box().hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollbarChanged = box().hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);
    if (autoHorizontalScrollbarChanged || autoVerticalScrollbarChanged)
        box().setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::Unknown);
}

IntRect PaintLayerScrollableArea::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!hasHorizontalScrollbar())
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box().borderBottom() - horizontalScrollbar()->height(),
        borderBoxRect.width() - (box().borderLeft() + box().borderRight()) - scrollCorner.width(),
        horizontalScrollbar()->height());
}

IntRect PaintLayerScrollableArea::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!hasVerticalScrollbar())
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box().borderTop(),
        verticalScrollbar()->width(),
        borderBoxRect.height() - (box().borderTop() + box().borderBottom()) - scrollCorner.height());
}

int PaintLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const
{
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box().borderLeft();
    return maxX - box().borderRight() - verticalScrollbar()->width();
}

int PaintLayerScrollableArea::horizontalScrollbarStart(int minX) const
{
    int x = minX + box().borderLeft();
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += hasVerticalScrollbar() ? verticalScrollbar()->width() : resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize PaintLayerScrollableArea::scrollbarOffset(const Scrollbar& scrollbar) const
{
    if (&scrollbar == verticalScrollbar())
        return IntSize(verticalScrollbarStart(0, box().size().width()), box().borderTop());

    if (&scrollbar == horizontalScrollbar())
        return IntSize(horizontalScrollbarStart(0), box().size().height() - box().borderBottom() - scrollbar.height());

    ASSERT_NOT_REACHED();
    return IntSize();
}

static inline const LayoutObject& layoutObjectForScrollbar(const LayoutObject& layoutObject)
{
    if (Node* node = layoutObject.node()) {
        if (layoutObject.isLayoutView()) {
            Document& doc = node->document();
            if (Settings* settings = doc.settings()) {
                if (!settings->allowCustomScrollbarInMainFrame() && layoutObject.frame() && layoutObject.frame()->isMainFrame())
                    return layoutObject;
            }

            // Try the <body> element first as a scrollbar source.
            Element* body = doc.body();
            if (body && body->layoutObject() && body->layoutObject()->style()->hasPseudoStyle(PseudoIdScrollbar))
                return *body->layoutObject();

            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc.documentElement();
            if (docElement && docElement->layoutObject() && docElement->layoutObject()->style()->hasPseudoStyle(PseudoIdScrollbar))
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

bool PaintLayerScrollableArea::needsScrollbarReconstruction() const
{
    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    bool shouldUseCustom = actualLayoutObject.isBox() && actualLayoutObject.styleRef().hasPseudoStyle(PseudoIdScrollbar);
    bool hasAnyScrollbar = hasScrollbar();
    bool hasCustom = (hasHorizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) || (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar());
    bool didCustomScrollbarOwnerChanged = false;

    if (hasHorizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) {
        if (actualLayoutObject != toLayoutScrollbar(horizontalScrollbar())->owningLayoutObject())
            didCustomScrollbarOwnerChanged = true;
    }

    if (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar()) {
        if (actualLayoutObject != toLayoutScrollbar(verticalScrollbar())->owningLayoutObject())
            didCustomScrollbarOwnerChanged = true;
    }

    return hasAnyScrollbar && ((shouldUseCustom != hasCustom) || (shouldUseCustom && didCustomScrollbarOwnerChanged));
}

void PaintLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (FreezeScrollbarsScope::scrollbarsAreFrozen())
        return;

    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    setScrollbarNeedsPaintInvalidation(HorizontalScrollbar);

    m_scrollbarManager.setHasHorizontalScrollbar(hasScrollbar);

    updateScrollOrigin();

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (hasHorizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (hasVerticalScrollbar())
        verticalScrollbar()->styleChanged();

    setScrollCornerNeedsPaintInvalidation();

    // Force an update since we know the scrollbars have changed things.
    if (box().document().hasAnnotatedRegions())
        box().document().setAnnotatedRegionsDirty(true);
}

void PaintLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (FreezeScrollbarsScope::scrollbarsAreFrozen())
        return;

    if (hasScrollbar == hasVerticalScrollbar())
        return;

    setScrollbarNeedsPaintInvalidation(VerticalScrollbar);

    m_scrollbarManager.setHasVerticalScrollbar(hasScrollbar);

    updateScrollOrigin();

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (hasHorizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (hasVerticalScrollbar())
        verticalScrollbar()->styleChanged();

    setScrollCornerNeedsPaintInvalidation();

    // Force an update since we know the scrollbars have changed things.
    if (box().document().hasAnnotatedRegions())
        box().document().setAnnotatedRegionsDirty(true);
}

int PaintLayerScrollableArea::verticalScrollbarWidth(OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const
{
    if (!hasVerticalScrollbar())
        return 0;
    if (verticalScrollbar()->isOverlayScrollbar() && (overlayScrollbarClipBehavior == IgnoreOverlayScrollbarSize || !verticalScrollbar()->shouldParticipateInHitTesting()))
        return 0;
    return verticalScrollbar()->width();
}

int PaintLayerScrollableArea::horizontalScrollbarHeight(OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const
{
    if (!hasHorizontalScrollbar())
        return 0;
    if (horizontalScrollbar()->isOverlayScrollbar() && (overlayScrollbarClipBehavior == IgnoreOverlayScrollbarSize || !horizontalScrollbar()->shouldParticipateInHitTesting()))
        return 0;
    return horizontalScrollbar()->height();
}

void PaintLayerScrollableArea::positionOverflowControls()
{
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
        m_resizer->setFrameRect(LayoutRect(resizerCornerRect(borderBox, ResizerForPointer)));

    // FIXME, this should eventually be removed, once we are certain that composited
    // controls get correctly positioned on a compositor update. For now, conservatively
    // leaving this unchanged.
    if (layer()->hasCompositedLayerMapping())
        layer()->compositedLayerMapping()->positionOverflowControlsLayers();
}

void PaintLayerScrollableArea::updateScrollCornerStyle()
{
    if (!m_scrollCorner && !hasScrollbar())
        return;
    if (!m_scrollCorner && hasOverlayScrollbars())
        return;

    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    RefPtr<ComputedStyle> corner = box().hasOverflowClip() ? actualLayoutObject.getUncachedPseudoStyle(PseudoStyleRequest(PseudoIdScrollbarCorner), actualLayoutObject.style()) : PassRefPtr<ComputedStyle>(nullptr);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = LayoutScrollbarPart::createAnonymous(&box().document(), this);
            m_scrollCorner->setDangerousOneWayParent(&box());
        }
        m_scrollCorner->setStyleWithWritingModeOfParent(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = nullptr;
    }
}

bool PaintLayerScrollableArea::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!hasScrollbar() && !box().canResize())
        return false;

    IntRect resizeControlRect;
    if (box().style()->resize() != RESIZE_NONE) {
        resizeControlRect = resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (hasVerticalScrollbar() && verticalScrollbar()->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box().size().width()),
            LayoutUnit(box().borderTop()),
            verticalScrollbar()->width(),
            box().size().height() - (box().borderTop() + box().borderBottom()) - (hasHorizontalScrollbar() ? horizontalScrollbar()->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(verticalScrollbar());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (hasHorizontalScrollbar() && horizontalScrollbar()->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(LayoutUnit()),
            box().size().height() - box().borderBottom() - horizontalScrollbar()->height(),
            box().size().width() - (box().borderLeft() + box().borderRight()) - (hasVerticalScrollbar() ? verticalScrollbar()->width() : resizeControlSize),
            horizontalScrollbar()->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(horizontalScrollbar());
            return true;
        }
    }

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    return false;
}

IntRect PaintLayerScrollableArea::resizerCornerRect(const IntRect& bounds, ResizerHitTestType resizerHitTestType) const
{
    if (box().style()->resize() == RESIZE_NONE)
        return IntRect();
    IntRect corner = cornerRect(box(), horizontalScrollbar(), verticalScrollbar(), bounds);

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

IntRect PaintLayerScrollableArea::scrollCornerAndResizerRect() const
{
    IntRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    return scrollCornerAndResizer;
}

bool PaintLayerScrollableArea::isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType resizerHitTestType) const
{
    if (!box().canResize())
        return false;

    IntPoint localPoint = roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
    IntRect localBounds(0, 0, box().pixelSnappedWidth(), box().pixelSnappedHeight());
    return resizerCornerRect(localBounds, resizerHitTestType).contains(localPoint);
}

bool PaintLayerScrollableArea::hitTestResizerInFragments(const PaintLayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (!box().canResize())
        return false;

    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const PaintLayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(pixelSnappedIntRect(fragment.layerBounds), ResizerForPointer).contains(hitTestLocation.roundedPoint()))
            return true;
    }

    return false;
}

void PaintLayerScrollableArea::updateResizerAreaSet()
{
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

void PaintLayerScrollableArea::updateResizerStyle()
{
    if (!m_resizer && !box().canResize())
        return;

    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    RefPtr<ComputedStyle> resizer = box().hasOverflowClip() ? actualLayoutObject.getUncachedPseudoStyle(PseudoStyleRequest(PseudoIdResizer), actualLayoutObject.style()) : PassRefPtr<ComputedStyle>(nullptr);
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

void PaintLayerScrollableArea::invalidateAllStickyConstraints()
{
    if (PaintLayerScrollableAreaRareData* d = rareData()) {
        for (PaintLayer* stickyLayer : d->m_stickyConstraintsMap.keys()) {
            if (stickyLayer->layoutObject()->style()->position() == StickyPosition)
                stickyLayer->setNeedsCompositingInputsUpdate();
        }
        d->m_stickyConstraintsMap.clear();
    }
}

void PaintLayerScrollableArea::invalidateStickyConstraintsFor(PaintLayer* layer, bool needsCompositingUpdate)
{
    if (PaintLayerScrollableAreaRareData* d = rareData()) {
        d->m_stickyConstraintsMap.remove(layer);
        if (needsCompositingUpdate && layer->layoutObject()->style()->position() == StickyPosition)
            layer->setNeedsCompositingInputsUpdate();
    }
}

IntSize PaintLayerScrollableArea::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = layer()->size();
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
    return localPoint - resizerPoint;
}

void PaintLayerScrollableArea::resize(const PlatformEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !box().canResize() || !box().node())
        return;

    ASSERT(box().node()->isElementNode());
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

    IntSize newOffset = offsetFromResizeCorner(document.view()->rootFrameToContents(pos));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);

    LayoutSize currentSize = box().size();
    currentSize.scale(1 / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);

    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }

    LayoutSize difference((currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize);

    bool isBoxSizingBorder = box().style()->boxSizing() == BoxSizingBorderBox;

    EResize resize = box().style()->resize();
    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginLeft, box().marginLeft() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
            element->setInlineStyleProperty(CSSPropertyMarginRight, box().marginRight() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
        }
        LayoutUnit baseWidth = box().size().width() - (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingWidth());
        baseWidth = LayoutUnit(baseWidth / zoomFactor);
        element->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::UnitType::Pixels);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginTop, box().marginTop() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
            element->setInlineStyleProperty(CSSPropertyMarginBottom, box().marginBottom() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
        }
        LayoutUnit baseHeight = box().size().height() - (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingHeight());
        baseHeight = LayoutUnit(baseHeight / zoomFactor);
        element->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::UnitType::Pixels);
    }

    document.updateStyleAndLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

LayoutRect PaintLayerScrollableArea::scrollIntoView(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType scrollType)
{
    LayoutRect localExposeRect(box().absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms).boundingBox());
    localExposeRect.move(-box().borderLeft(), -box().borderTop());
    LayoutRect layerBounds(LayoutPoint(), LayoutSize(box().clientWidth(), box().clientHeight()));
    LayoutRect r = ScrollAlignment::getRectToExpose(layerBounds, localExposeRect, alignX, alignY);

    DoublePoint clampedScrollPosition = clampScrollPosition(scrollPositionDouble() + roundedIntSize(r.location()));
    if (clampedScrollPosition == scrollPositionDouble())
        return rect;

    DoubleSize oldScrollOffset = adjustedScrollOffset();
    scrollToPosition(clampedScrollPosition, ScrollOffsetUnclamped, ScrollBehaviorInstant, scrollType);
    DoubleSize scrollOffsetDifference = adjustedScrollOffset() - oldScrollOffset;
    localExposeRect.move(-LayoutSize(scrollOffsetDifference));
    return LayoutRect(box().localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
}

void PaintLayerScrollableArea::updateScrollableAreaSet(bool hasOverflow)
{
    LocalFrame* frame = box().frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    // FIXME: Does this need to be fixed later for OOPI?
    bool isVisibleToHitTest = box().visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frame->deprecatedLocalOwner())
        isVisibleToHitTest &= owner->layoutObject() && owner->layoutObject()->visibleToHitTesting();

    bool didScrollOverflow = m_scrollsOverflow;

    m_scrollsOverflow = hasOverflow && isVisibleToHitTest;
    if (didScrollOverflow == scrollsOverflow())
        return;

    if (m_scrollsOverflow) {
        ASSERT(canHaveOverflowScrollbars(box()));
        frameView->addScrollableArea(this);
    } else {
        frameView->removeScrollableArea(this);
    }
}

void PaintLayerScrollableArea::updateCompositingLayersAfterScroll()
{
    PaintLayerCompositor* compositor = box().view()->compositor();
    if (compositor->inCompositingMode()) {
        if (usesCompositedScrolling()) {
            ASSERT(layer()->hasCompositedLayerMapping());
            layer()->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
            compositor->setNeedsCompositingUpdate(CompositingUpdateAfterGeometryChange);
        } else {
            layer()->setNeedsCompositingInputsUpdate();
        }
    }
}

bool PaintLayerScrollableArea::usesCompositedScrolling() const
{
    // See https://codereview.chromium.org/176633003/ for the tests that fail without this disabler.
    DisableCompositingQueryAsserts disabler;
    return layer()->hasCompositedLayerMapping() && layer()->compositedLayerMapping()->scrollingLayer();
}

bool PaintLayerScrollableArea::shouldScrollOnMainThread() const
{
    if (LocalFrame* frame = box().frame()) {
        if (Page* page = frame->page()) {
            if (page->scrollingCoordinator()->shouldUpdateScrollLayerPositionOnMainThread())
                return true;
        }
    }
    return ScrollableArea::shouldScrollOnMainThread();
}

static bool layerNeedsCompositedScrolling(PaintLayerScrollableArea::LCDTextMode mode, const PaintLayer* layer)
{
    if (!layer->scrollsOverflow())
        return false;

    Node* node = layer->enclosingNode();
    if (node && node->isElementNode() && (toElement(node)->compositorMutableProperties() & (CompositorMutableProperty::kScrollTop | CompositorMutableProperty::kScrollLeft)))
        return true;

    if (mode == PaintLayerScrollableArea::ConsiderLCDText && !layer->compositor()->preferCompositingToLCDTextEnabled())
        return false;

    return !layer->size().isEmpty()
        && !layer->hasDescendantWithClipPath()
        && !layer->hasAncestorWithClipPath()
        && !layer->layoutObject()->style()->hasBorderRadius();
}

void PaintLayerScrollableArea::updateNeedsCompositedScrolling(LCDTextMode mode)
{
    const bool needsCompositedScrolling = layerNeedsCompositedScrolling(mode, layer());
    if (static_cast<bool>(m_needsCompositedScrolling) != needsCompositedScrolling) {
        m_needsCompositedScrolling = needsCompositedScrolling;
        layer()->didUpdateNeedsCompositedScrolling();
    }
}

void PaintLayerScrollableArea::setTopmostScrollChild(PaintLayer* scrollChild)
{
    // We only want to track the topmost scroll child for scrollable areas with
    // overlay scrollbars.
    if (!hasOverlayScrollbars())
        return;
    m_nextTopmostScrollChild = scrollChild;
}

bool PaintLayerScrollableArea::visualViewportSuppliesScrollbars() const
{
    if (!layer()->isRootLayer())
        return false;

    LocalFrame* frame = box().frame();
    if (!frame || !frame->isMainFrame() || !frame->settings())
        return false;

    return frame->settings()->viewportEnabled();
}

Widget* PaintLayerScrollableArea::getWidget()
{
    return box().frame()->view();
}

void PaintLayerScrollableArea::resetRebuildScrollbarLayerFlags()
{
    m_rebuildHorizontalScrollbarLayer = false;
    m_rebuildVerticalScrollbarLayer = false;
}

CompositorAnimationTimeline* PaintLayerScrollableArea::compositorAnimationTimeline() const
{
    if (LocalFrame* frame = box().frame()) {
        if (Page* page = frame->page())
            return page->scrollingCoordinator() ? page->scrollingCoordinator()->compositorAnimationTimeline() : nullptr;
    }
    return nullptr;
}

PaintLayerScrollableArea::ScrollbarManager::ScrollbarManager(PaintLayerScrollableArea& scrollableArea)
    : m_scrollableArea(&scrollableArea)
    , m_hBarIsAttached(0)
    , m_vBarIsAttached(0)
{
}

void PaintLayerScrollableArea::ScrollbarManager::dispose()
{
    m_hBarIsAttached = m_vBarIsAttached = 0;
    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::destroyDetachedScrollbars()
{
    ASSERT(!m_hBarIsAttached || m_hBar);
    ASSERT(!m_vBarIsAttached || m_vBar);
    if (m_hBar && !m_hBarIsAttached)
        destroyScrollbar(HorizontalScrollbar);
    if (m_vBar && !m_vBarIsAttached)
        destroyScrollbar(VerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar) {
        // This doesn't hit in any tests, but since the equivalent code in setHasVerticalScrollbar
        // does, presumably this code does as well.
        DisableCompositingQueryAsserts disabler;
        if (!m_hBar) {
            m_hBar = createScrollbar(HorizontalScrollbar);
            m_hBarIsAttached = 1;
            if (!m_hBar->isCustomScrollbar())
                m_scrollableArea->didAddScrollbar(*m_hBar, HorizontalScrollbar);
        } else {
            m_hBarIsAttached = 1;
        }
    } else {
        m_hBarIsAttached = 0;
        if (!DelayScrollPositionClampScope::clampingIsDelayed())
            destroyScrollbar(HorizontalScrollbar);
    }
}

void PaintLayerScrollableArea::ScrollbarManager::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar) {
        DisableCompositingQueryAsserts disabler;
        if (!m_vBar) {
            m_vBar = createScrollbar(VerticalScrollbar);
            m_vBarIsAttached = 1;
            if (!m_vBar->isCustomScrollbar())
                m_scrollableArea->didAddScrollbar(*m_vBar, VerticalScrollbar);
        } else {
            m_vBarIsAttached = 1;
        }
    } else {
        m_vBarIsAttached = 0;
        if (!DelayScrollPositionClampScope::clampingIsDelayed())
            destroyScrollbar(VerticalScrollbar);
    }
}

Scrollbar* PaintLayerScrollableArea::ScrollbarManager::createScrollbar(ScrollbarOrientation orientation)
{
    ASSERT(orientation == HorizontalScrollbar ? !m_hBarIsAttached : !m_vBarIsAttached);
    Scrollbar* scrollbar = nullptr;
    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(m_scrollableArea->box());
    bool hasCustomScrollbarStyle = actualLayoutObject.isBox() && actualLayoutObject.styleRef().hasPseudoStyle(PseudoIdScrollbar);
    if (hasCustomScrollbarStyle) {
        scrollbar = LayoutScrollbar::createCustomScrollbar(m_scrollableArea.get(), orientation, actualLayoutObject.node());
    } else {
        ScrollbarControlSize scrollbarSize = RegularScrollbar;
        if (actualLayoutObject.styleRef().hasAppearance())
            scrollbarSize = LayoutTheme::theme().scrollbarControlSizeForPart(actualLayoutObject.styleRef().appearance());
        scrollbar = Scrollbar::create(m_scrollableArea.get(), orientation, scrollbarSize, &m_scrollableArea->box().frame()->page()->chromeClient());
    }
    m_scrollableArea->box().document().view()->addChild(scrollbar);
    return scrollbar;
}

void PaintLayerScrollableArea::ScrollbarManager::destroyScrollbar(ScrollbarOrientation orientation)
{
    Member<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    ASSERT(orientation == HorizontalScrollbar ? !m_hBarIsAttached: !m_vBarIsAttached);
    if (!scrollbar)
        return;

    m_scrollableArea->setScrollbarNeedsPaintInvalidation(orientation);
    if (orientation == HorizontalScrollbar)
        m_scrollableArea->m_rebuildHorizontalScrollbarLayer = true;
    else
        m_scrollableArea->m_rebuildVerticalScrollbarLayer = true;

    if (!scrollbar->isCustomScrollbar())
        m_scrollableArea->willRemoveScrollbar(*scrollbar, orientation);

    toFrameView(scrollbar->parent())->removeChild(scrollbar.get());
    scrollbar->disconnectFromScrollableArea();
    scrollbar = nullptr;
}

uint64_t PaintLayerScrollableArea::id() const
{
    return DOMNodeIds::idForNode(box().node());
}

DEFINE_TRACE(PaintLayerScrollableArea::ScrollbarManager)
{
    visitor->trace(m_scrollableArea);
    visitor->trace(m_hBar);
    visitor->trace(m_vBar);
}

int PaintLayerScrollableArea::PreventRelayoutScope::s_count = 0;
SubtreeLayoutScope* PaintLayerScrollableArea::PreventRelayoutScope::s_layoutScope = nullptr;
bool PaintLayerScrollableArea::PreventRelayoutScope::s_relayoutNeeded = false;
PersistentHeapVector<Member<PaintLayerScrollableArea>>* PaintLayerScrollableArea::PreventRelayoutScope::s_needsRelayout = nullptr;

PaintLayerScrollableArea::PreventRelayoutScope::PreventRelayoutScope(SubtreeLayoutScope& layoutScope)
{
    if (!s_count) {
        DCHECK(!s_layoutScope);
        DCHECK(!s_needsRelayout || s_needsRelayout->isEmpty());
        s_layoutScope = &layoutScope;
    }
    s_count++;
}

PaintLayerScrollableArea::PreventRelayoutScope::~PreventRelayoutScope()
{
    if (--s_count == 0) {
        if (s_relayoutNeeded) {
            for (auto scrollableArea: *s_needsRelayout) {
                DCHECK(scrollableArea->needsRelayout());
                LayoutBox& box = scrollableArea->box();
                s_layoutScope->setNeedsLayout(&box, LayoutInvalidationReason::ScrollbarChanged);
                if (box.isLayoutBlock()) {
                    bool horizontalScrollbarChanged = scrollableArea->hasHorizontalScrollbar() != scrollableArea->hadHorizontalScrollbarBeforeRelayout();
                    bool verticalScrollbarChanged = scrollableArea->hasVerticalScrollbar() != scrollableArea->hadVerticalScrollbarBeforeRelayout();
                    if (horizontalScrollbarChanged || verticalScrollbarChanged)
                        toLayoutBlock(box).scrollbarsChanged(horizontalScrollbarChanged, verticalScrollbarChanged);
                }
                scrollableArea->setNeedsRelayout(false);
            }

            s_needsRelayout->clear();
        }
        s_layoutScope = nullptr;
    }
}

void PaintLayerScrollableArea::PreventRelayoutScope::setBoxNeedsLayout(PaintLayerScrollableArea& scrollableArea, bool hadHorizontalScrollbar, bool hadVerticalScrollbar)
{
    DCHECK(s_count);
    DCHECK(s_layoutScope);
    if (scrollableArea.needsRelayout())
        return;
    scrollableArea.setNeedsRelayout(true);
    scrollableArea.setHadHorizontalScrollbarBeforeRelayout(hadHorizontalScrollbar);
    scrollableArea.setHadVerticalScrollbarBeforeRelayout(hadVerticalScrollbar);

    s_relayoutNeeded = true;
    if (!s_needsRelayout)
        s_needsRelayout = new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
    s_needsRelayout->append(&scrollableArea);
}

void PaintLayerScrollableArea::PreventRelayoutScope::resetRelayoutNeeded()
{
    DCHECK_EQ(s_count, 0);
    DCHECK(!s_needsRelayout || s_needsRelayout->isEmpty());
    s_relayoutNeeded = false;
}

int PaintLayerScrollableArea::FreezeScrollbarsScope::s_count = 0;

int PaintLayerScrollableArea::DelayScrollPositionClampScope::s_count = 0;
PersistentHeapVector<Member<PaintLayerScrollableArea>>* PaintLayerScrollableArea::DelayScrollPositionClampScope::s_needsClamp = nullptr;

PaintLayerScrollableArea::DelayScrollPositionClampScope::DelayScrollPositionClampScope()
{
    if (!s_needsClamp)
        s_needsClamp = new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
    DCHECK(s_count > 0 || s_needsClamp->isEmpty());
    s_count++;
}

PaintLayerScrollableArea::DelayScrollPositionClampScope::~DelayScrollPositionClampScope()
{
    if (--s_count == 0)
        DelayScrollPositionClampScope::clampScrollableAreas();
}

void PaintLayerScrollableArea::DelayScrollPositionClampScope::setNeedsClamp(PaintLayerScrollableArea* scrollableArea)
{
    if (!scrollableArea->needsScrollPositionClamp()) {
        scrollableArea->setNeedsScrollPositionClamp(true);
        s_needsClamp->append(scrollableArea);
    }
}

void PaintLayerScrollableArea::DelayScrollPositionClampScope::clampScrollableAreas()
{
    for (auto& scrollableArea : *s_needsClamp)
        scrollableArea->clampScrollPositionsAfterLayout();
    delete s_needsClamp;
    s_needsClamp = nullptr;
}

} // namespace blink
