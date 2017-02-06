// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/ScrollManager.h"

#include "core/dom/DOMNodeIds.h"
#include "core/events/GestureEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutPart.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "platform/PlatformGestureEvent.h"
#include "wtf/PtrUtil.h"
#include <memory>


namespace blink {

ScrollManager::ScrollManager(LocalFrame* frame)
: m_frame(frame)
{
    clear();
}

ScrollManager::~ScrollManager()
{
}

void ScrollManager::clear()
{
    m_lastGestureScrollOverWidget = false;
    m_scrollbarHandlingScrollGesture = nullptr;
    m_resizeScrollableArea = nullptr;
    m_offsetFromResizeCorner = LayoutSize();
    clearGestureScrollState();
}

void ScrollManager::clearGestureScrollState()
{
    m_scrollGestureHandlingNode = nullptr;
    m_previousGestureScrolledNode = nullptr;
    m_deltaConsumedForScrollSequence = false;
    m_currentScrollChain.clear();

    if (FrameHost* host = frameHost()) {
        bool resetX = true;
        bool resetY = true;
        host->overscrollController().resetAccumulated(resetX, resetY);
    }
}

void ScrollManager::stopAutoscroll()
{
    if (AutoscrollController* controller = autoscrollController())
        controller->stopAutoscroll();
}

bool ScrollManager::panScrollInProgress() const
{
    return autoscrollController() && autoscrollController()->panScrollInProgress();
}

AutoscrollController* ScrollManager::autoscrollController() const
{
    if (Page* page = m_frame->page())
        return &page->autoscrollController();
    return nullptr;
}

void ScrollManager::recomputeScrollChain(const Node& startNode,
    std::deque<int>& scrollChain)
{
    scrollChain.clear();

    DCHECK(startNode.layoutObject());
    LayoutBox* curBox = startNode.layoutObject()->enclosingBox();

    // Scrolling propagates along the containing block chain and ends at the
    // RootScroller element. The RootScroller element will have a custom
    // applyScroll callback that scrolls the frame or element.
    while (curBox) {
        Node* curNode = curBox->node();
        Element* curElement = nullptr;

        // FIXME: this should reject more elements, as part of crbug.com/410974.
        if (curNode && curNode->isElementNode()) {
            curElement = toElement(curNode);
        } else if (curNode && curNode->isDocumentNode()) {
            // In normal circumastances, the documentElement will be the root
            // scroller but the documentElement itself isn't a containing block,
            // that'll be the document node rather than the element.
            curElement = m_frame->document()->documentElement();
            DCHECK(!curElement || isEffectiveRootScroller(*curElement));
        }

        if (curElement) {
            scrollChain.push_front(DOMNodeIds::idForNode(curElement));
            if (isEffectiveRootScroller(*curElement))
                break;
        }

        curBox = curBox->containingBlock();
    }
}

bool ScrollManager::logicalScroll(ScrollDirection direction, ScrollGranularity granularity, Node* startNode, Node* mousePressNode)
{
    Node* node = startNode;

    if (!node)
        node = m_frame->document()->focusedElement();

    if (!node)
        node = mousePressNode;

    if ((!node || !node->layoutObject()) && m_frame->view() && !m_frame->view()->layoutViewItem().isNull())
        node = m_frame->view()->layoutViewItem().node();

    if (!node)
        return false;

    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutBox* curBox = node->layoutObject()->enclosingBox();
    while (curBox) {
        ScrollDirectionPhysical physicalDirection = toPhysicalDirection(
            direction, curBox->isHorizontalWritingMode(), curBox->style()->isFlippedBlocksWritingMode());

        ScrollResult result = curBox->scroll(granularity, toScrollDelta(physicalDirection, 1));

        if (result.didScroll()) {
            setFrameWasScrolledByUser();
            return true;
        }

        curBox = curBox->containingBlock();
    }

    return false;
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool ScrollManager::bubblingScroll(ScrollDirection direction, ScrollGranularity granularity, Node* startingNode, Node* mousePressNode)
{
    // The layout needs to be up to date to determine if we can scroll. We may be
    // here because of an onLoad event, in which case the final layout hasn't been performed yet.
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();
    // FIXME: enable scroll customization in this case. See crbug.com/410974.
    if (logicalScroll(direction, granularity, startingNode, mousePressNode))
        return true;

    Frame* parentFrame = m_frame->tree().parent();
    if (!parentFrame || !parentFrame->isLocalFrame())
        return false;
    // FIXME: Broken for OOPI.
    return toLocalFrame(parentFrame)->eventHandler().bubblingScroll(direction, granularity, m_frame->deprecatedLocalOwner());
}

void ScrollManager::setFrameWasScrolledByUser()
{
    if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
        documentLoader->initialScrollState().wasScrolledByUser = true;
}

void ScrollManager::customizedScroll(const Node& startNode, ScrollState& scrollState)
{
    if (scrollState.fullyConsumed())
        return;

    if (scrollState.deltaX() || scrollState.deltaY())
        m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    if (m_currentScrollChain.empty())
        recomputeScrollChain(startNode, m_currentScrollChain);
    scrollState.setScrollChain(m_currentScrollChain);

    scrollState.distributeToScrollChainDescendant();
}

WebInputEventResult ScrollManager::handleGestureScrollBegin(const PlatformGestureEvent& gestureEvent)
{
    Document* document = m_frame->document();

    if (document->layoutViewItem().isNull())
        return WebInputEventResult::NotHandled;

    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;

    // If there's no layoutObject on the node, send the event to the nearest ancestor with a layoutObject.
    // Needed for <option> and <optgroup> elements so we can touch scroll <select>s
    while (m_scrollGestureHandlingNode && !m_scrollGestureHandlingNode->layoutObject())
        m_scrollGestureHandlingNode = m_scrollGestureHandlingNode->parentOrShadowHostNode();

    if (!m_scrollGestureHandlingNode)
        m_scrollGestureHandlingNode = m_frame->document()->documentElement();

    if (!m_scrollGestureHandlingNode)
        return WebInputEventResult::NotHandled;

    passScrollGestureEventToWidget(gestureEvent, m_scrollGestureHandlingNode->layoutObject());

    m_currentScrollChain.clear();
    std::unique_ptr<ScrollStateData> scrollStateData = wrapUnique(new ScrollStateData());
    scrollStateData->position_x = gestureEvent.position().x();
    scrollStateData->position_y = gestureEvent.position().y();
    scrollStateData->is_beginning = true;
    scrollStateData->from_user_input = true;
    scrollStateData->is_direct_manipulation = gestureEvent.source() == PlatformGestureSourceTouchscreen;
    scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
    ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
    customizedScroll(*m_scrollGestureHandlingNode.get(), *scrollState);
    return WebInputEventResult::HandledSystem;
}

WebInputEventResult ScrollManager::handleGestureScrollUpdate(const PlatformGestureEvent& gestureEvent)
{
    DCHECK_EQ(gestureEvent.type(), PlatformEvent::GestureScrollUpdate);

    // Negate the deltas since the gesture event stores finger movement and
    // scrolling occurs in the direction opposite the finger's movement
    // direction. e.g. Finger moving up has negative event delta but causes the
    // page to scroll down causing positive scroll delta.
    FloatSize delta(-gestureEvent.deltaX(), -gestureEvent.deltaY());
    FloatSize velocity(-gestureEvent.velocityX(), -gestureEvent.velocityY());
    FloatPoint position(gestureEvent.position());

    if (delta.isZero())
        return WebInputEventResult::NotHandled;

    Node* node = m_scrollGestureHandlingNode.get();

    if (!node)
        return WebInputEventResult::NotHandled;

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return WebInputEventResult::NotHandled;

    // Try to send the event to the correct view.
    WebInputEventResult result = passScrollGestureEventToWidget(gestureEvent, layoutObject);
    if (result != WebInputEventResult::NotHandled) {
        // FIXME: we should allow simultaneous scrolling of nested
        // iframes along perpendicular axes. See crbug.com/466991.
        m_deltaConsumedForScrollSequence = true;
        return result;
    }

    std::unique_ptr<ScrollStateData> scrollStateData = wrapUnique(new ScrollStateData());
    scrollStateData->delta_x = delta.width();
    scrollStateData->delta_y = delta.height();
    scrollStateData->delta_granularity = static_cast<double>(gestureEvent.deltaUnits());
    scrollStateData->velocity_x = velocity.width();
    scrollStateData->velocity_y = velocity.height();
    scrollStateData->position_x = position.x();
    scrollStateData->position_y = position.y();
    scrollStateData->should_propagate = !gestureEvent.preventPropagation();
    scrollStateData->is_in_inertial_phase = gestureEvent.inertialPhase() == ScrollInertialPhaseMomentum;
    scrollStateData->is_direct_manipulation = gestureEvent.source() == PlatformGestureSourceTouchscreen;
    scrollStateData->from_user_input = true;
    scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
    ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
    if (m_previousGestureScrolledNode) {
        // The ScrollState needs to know what the current
        // native scrolling element is, so that for an
        // inertial scroll that shouldn't propagate, only the
        // currently scrolling element responds.
        DCHECK(m_previousGestureScrolledNode->isElementNode());
        scrollState->setCurrentNativeScrollingElement(toElement(m_previousGestureScrolledNode.get()));
    }
    customizedScroll(*node, *scrollState);
    m_previousGestureScrolledNode = scrollState->currentNativeScrollingElement();
    m_deltaConsumedForScrollSequence = scrollState->deltaConsumedForScrollSequence();

    bool didScrollX = scrollState->deltaX() != delta.width();
    bool didScrollY = scrollState->deltaY() != delta.height();

    if ((!m_previousGestureScrolledNode || !isEffectiveRootScroller(*m_previousGestureScrolledNode)) && frameHost())
        frameHost()->overscrollController().resetAccumulated(didScrollX, didScrollY);

    if (didScrollX || didScrollY) {
        setFrameWasScrolledByUser();
        return WebInputEventResult::HandledSystem;
    }

    return WebInputEventResult::NotHandled;
}

WebInputEventResult ScrollManager::handleGestureScrollEnd(const PlatformGestureEvent& gestureEvent)
{
    Node* node = m_scrollGestureHandlingNode;

    if (node) {
        passScrollGestureEventToWidget(gestureEvent, node->layoutObject());
        std::unique_ptr<ScrollStateData> scrollStateData = wrapUnique(new ScrollStateData());
        scrollStateData->is_ending = true;
        scrollStateData->is_in_inertial_phase = gestureEvent.inertialPhase() == ScrollInertialPhaseMomentum;
        scrollStateData->from_user_input = true;
        scrollStateData->is_direct_manipulation = gestureEvent.source() == PlatformGestureSourceTouchscreen;
        scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
        ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
        customizedScroll(*node, *scrollState);
    }

    clearGestureScrollState();
    return WebInputEventResult::NotHandled;
}

FrameHost* ScrollManager::frameHost() const
{
    if (!m_frame->page())
        return nullptr;

    return &m_frame->page()->frameHost();
}

WebInputEventResult ScrollManager::passScrollGestureEventToWidget(const PlatformGestureEvent& gestureEvent, LayoutObject* layoutObject)
{
    DCHECK(gestureEvent.isScrollEvent());

    if (!m_lastGestureScrollOverWidget || !layoutObject || !layoutObject->isLayoutPart())
        return WebInputEventResult::NotHandled;

    Widget* widget = toLayoutPart(layoutObject)->widget();

    if (!widget || !widget->isFrameView())
        return WebInputEventResult::NotHandled;

    return toFrameView(widget)->frame().eventHandler().handleGestureScrollEvent(gestureEvent);
}

bool ScrollManager::isEffectiveRootScroller(const Node& node) const
{
    // The root scroller is the one Element on the page designated to perform
    // "viewport actions" like top controls movement and overscroll glow.
    if (!m_frame->document())
        return false;

    if (!node.isElementNode())
        return false;

    return node.isSameNode(m_frame->document()->effectiveRootScroller());
}


WebInputEventResult ScrollManager::handleGestureScrollEvent(const PlatformGestureEvent& gestureEvent)
{
    Node* eventTarget = nullptr;
    Scrollbar* scrollbar = nullptr;
    if (gestureEvent.type() != PlatformEvent::GestureScrollBegin) {
        scrollbar = m_scrollbarHandlingScrollGesture.get();
        eventTarget = m_scrollGestureHandlingNode.get();
    }

    if (!eventTarget) {
        Document* document = m_frame->document();
        if (document->layoutViewItem().isNull())
            return WebInputEventResult::NotHandled;

        FrameView* view = m_frame->view();
        LayoutPoint viewPoint = view->rootFrameToContents(gestureEvent.position());
        HitTestRequest request(HitTestRequest::ReadOnly);
        HitTestResult result(request, viewPoint);
        document->layoutViewItem().hitTest(result);

        eventTarget = result.innerNode();

        m_lastGestureScrollOverWidget = result.isOverWidget();
        m_scrollGestureHandlingNode = eventTarget;
        m_previousGestureScrolledNode = nullptr;
        m_deltaConsumedForScrollSequence = false;

        if (!scrollbar)
            scrollbar = result.scrollbar();
    }

    if (scrollbar) {
        bool shouldUpdateCapture = false;
        if (scrollbar->gestureEvent(gestureEvent, &shouldUpdateCapture)) {
            if (shouldUpdateCapture)
                m_scrollbarHandlingScrollGesture = scrollbar;
            return WebInputEventResult::HandledSuppressed;
        }
        m_scrollbarHandlingScrollGesture = nullptr;
    }

    if (eventTarget) {
        if (handleScrollGestureOnResizer(eventTarget, gestureEvent))
            return WebInputEventResult::HandledSuppressed;

        GestureEvent* gestureDomEvent = GestureEvent::create(eventTarget->document().domWindow(), gestureEvent);
        if (gestureDomEvent) {
            DispatchEventResult gestureDomEventResult = eventTarget->dispatchEvent(gestureDomEvent);
            if (gestureDomEventResult != DispatchEventResult::NotCanceled) {
                DCHECK(gestureDomEventResult != DispatchEventResult::CanceledByEventHandler);
                return EventHandler::toWebInputEventResult(gestureDomEventResult);
            }
        }
    }

    switch (gestureEvent.type()) {
    case PlatformEvent::GestureScrollBegin:
        return handleGestureScrollBegin(gestureEvent);
    case PlatformEvent::GestureScrollUpdate:
        return handleGestureScrollUpdate(gestureEvent);
    case PlatformEvent::GestureScrollEnd:
        return handleGestureScrollEnd(gestureEvent);
    case PlatformEvent::GestureFlingStart:
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
        return WebInputEventResult::NotHandled;
    default:
        NOTREACHED();
        return WebInputEventResult::NotHandled;
    }
}

bool ScrollManager::isScrollbarHandlingGestures() const
{
    return m_scrollbarHandlingScrollGesture.get();
}

bool ScrollManager::handleScrollGestureOnResizer(Node* eventTarget, const PlatformGestureEvent& gestureEvent)
{
    if (gestureEvent.type() == PlatformEvent::GestureScrollBegin) {
        PaintLayer* layer = eventTarget->layoutObject() ? eventTarget->layoutObject()->enclosingLayer() : nullptr;
        IntPoint p = m_frame->view()->rootFrameToContents(gestureEvent.position());
        if (layer && layer->getScrollableArea() && layer->getScrollableArea()->isPointInResizeControl(p, ResizerForTouch)) {
            m_resizeScrollableArea = layer->getScrollableArea();
            m_resizeScrollableArea->setInResizeMode(true);
            m_offsetFromResizeCorner = LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
            return true;
        }
    } else if (gestureEvent.type() == PlatformEvent::GestureScrollUpdate) {
        if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
            m_resizeScrollableArea->resize(gestureEvent, m_offsetFromResizeCorner);
            return true;
        }
    } else if (gestureEvent.type() == PlatformEvent::GestureScrollEnd) {
        if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
            m_resizeScrollableArea->setInResizeMode(false);
            m_resizeScrollableArea = nullptr;
            return false;
        }
    }

    return false;
}

bool ScrollManager::inResizeMode() const
{
    return m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode();
}

void ScrollManager::resize(const PlatformEvent& evt)
{
    m_resizeScrollableArea->resize(evt, m_offsetFromResizeCorner);
}

void ScrollManager::clearResizeScrollableArea(bool shouldNotBeNull)
{
    if (shouldNotBeNull)
        DCHECK(m_resizeScrollableArea);

    if (m_resizeScrollableArea)
        m_resizeScrollableArea->setInResizeMode(false);
    m_resizeScrollableArea = nullptr;
}

void ScrollManager::setResizeScrollableArea(PaintLayer* layer, IntPoint p)
{
    m_resizeScrollableArea = layer->getScrollableArea();
    m_resizeScrollableArea->setInResizeMode(true);
    m_offsetFromResizeCorner = LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
}

bool ScrollManager::canHandleGestureEvent(const GestureEventWithHitTestResults& targetedEvent)
{
    Scrollbar* scrollbar = targetedEvent.hitTestResult().scrollbar();

    if (scrollbar) {
        bool shouldUpdateCapture = false;
        if (scrollbar->gestureEvent(targetedEvent.event(), &shouldUpdateCapture)) {
            if (shouldUpdateCapture)
                m_scrollbarHandlingScrollGesture = scrollbar;
            return true;
        }
    }
    return false;
}

DEFINE_TRACE(ScrollManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_scrollGestureHandlingNode);
    visitor->trace(m_previousGestureScrolledNode);
    visitor->trace(m_scrollbarHandlingScrollGesture);
    visitor->trace(m_resizeScrollableArea);
}

} // namespace blink
