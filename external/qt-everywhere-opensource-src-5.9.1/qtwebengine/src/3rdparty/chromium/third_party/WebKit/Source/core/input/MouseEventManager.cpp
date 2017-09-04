// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/MouseEventManager.h"

#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionController.h"
#include "core/events/DragEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/input/KeyboardEventManager.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/AutoscrollController.h"
#include "core/page/DragController.h"
#include "core/page/DragState.h"
#include "core/page/FocusController.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/geometry/FloatQuad.h"

namespace blink {

namespace {

PlatformMouseEvent mouseEventWithRegion(Node* node,
                                        const PlatformMouseEvent& mouseEvent) {
  if (!node->isElementNode())
    return mouseEvent;

  Element* element = toElement(node);
  if (!element->isInCanvasSubtree())
    return mouseEvent;

  HTMLCanvasElement* canvas =
      Traversal<HTMLCanvasElement>::firstAncestorOrSelf(*element);
  // In this case, the event target is canvas and mouse rerouting doesn't
  // happen.
  if (canvas == element)
    return mouseEvent;
  String region = canvas->getIdFromControl(element);
  PlatformMouseEvent newMouseEvent = mouseEvent;
  newMouseEvent.setRegion(region);
  return newMouseEvent;
}

// The amount of time to wait before sending a fake mouse event triggered
// during a scroll.
const double kFakeMouseMoveInterval = 0.1;

// TODO(crbug.com/653490): Read these values from the OS.
#if OS(MACOSX)
const int kDragThresholdX = 3;
const int kDragThresholdY = 3;
const double kTextDragDelay = 0.15;
#else
const int kDragThresholdX = 4;
const int kDragThresholdY = 4;
const double kTextDragDelay = 0.0;
#endif

}  // namespace

enum class DragInitiator { Mouse, Touch };

MouseEventManager::MouseEventManager(LocalFrame* frame,
                                     ScrollManager* scrollManager)
    : m_frame(frame),
      m_scrollManager(scrollManager),
      m_fakeMouseMoveEventTimer(
          this,
          &MouseEventManager::fakeMouseMoveEventTimerFired) {
  clear();
}

void MouseEventManager::clear() {
  m_nodeUnderMouse = nullptr;
  m_mousePressNode = nullptr;
  m_mouseDownMayStartAutoscroll = false;
  m_mouseDownMayStartDrag = false;
  m_capturesDragging = false;
  m_isMousePositionUnknown = true;
  m_lastKnownMousePosition = IntPoint();
  m_lastKnownMouseGlobalPosition = IntPoint();
  m_mousePressed = false;
  m_clickCount = 0;
  m_clickNode = nullptr;
  m_mouseDownPos = IntPoint();
  m_mouseDownTimestamp = 0;
  m_mouseDown = PlatformMouseEvent();
  m_svgPan = false;
  m_dragStartPos = LayoutPoint();
  m_fakeMouseMoveEventTimer.stop();
}

MouseEventManager::~MouseEventManager() = default;

DEFINE_TRACE(MouseEventManager) {
  visitor->trace(m_frame);
  visitor->trace(m_scrollManager);
  visitor->trace(m_nodeUnderMouse);
  visitor->trace(m_mousePressNode);
  visitor->trace(m_clickNode);
  SynchronousMutationObserver::trace(visitor);
}

MouseEventManager::MouseEventBoundaryEventDispatcher::
    MouseEventBoundaryEventDispatcher(
        MouseEventManager* mouseEventManager,
        const PlatformMouseEvent* platformMouseEvent,
        EventTarget* exitedTarget)
    : m_mouseEventManager(mouseEventManager),
      m_platformMouseEvent(platformMouseEvent),
      m_exitedTarget(exitedTarget) {}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchOut(
    EventTarget* target,
    EventTarget* relatedTarget) {
  dispatch(
      target, relatedTarget, EventTypeNames::mouseout,
      mouseEventWithRegion(m_exitedTarget->toNode(), *m_platformMouseEvent),
      false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchOver(
    EventTarget* target,
    EventTarget* relatedTarget) {
  dispatch(target, relatedTarget, EventTypeNames::mouseover,
           *m_platformMouseEvent, false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchLeave(
    EventTarget* target,
    EventTarget* relatedTarget,
    bool checkForListener) {
  dispatch(
      target, relatedTarget, EventTypeNames::mouseleave,
      mouseEventWithRegion(m_exitedTarget->toNode(), *m_platformMouseEvent),
      checkForListener);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchEnter(
    EventTarget* target,
    EventTarget* relatedTarget,
    bool checkForListener) {
  dispatch(target, relatedTarget, EventTypeNames::mouseenter,
           *m_platformMouseEvent, checkForListener);
}

AtomicString
MouseEventManager::MouseEventBoundaryEventDispatcher::getLeaveEvent() {
  return EventTypeNames::mouseleave;
}

AtomicString
MouseEventManager::MouseEventBoundaryEventDispatcher::getEnterEvent() {
  return EventTypeNames::mouseenter;
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatch(
    EventTarget* target,
    EventTarget* relatedTarget,
    const AtomicString& type,
    const PlatformMouseEvent& platformMouseEvent,
    bool checkForListener) {
  m_mouseEventManager->dispatchMouseEvent(target, type, platformMouseEvent,
                                          relatedTarget, checkForListener);
}

void MouseEventManager::sendBoundaryEvents(
    EventTarget* exitedTarget,
    EventTarget* enteredTarget,
    const PlatformMouseEvent& mousePlatformEvent) {
  MouseEventBoundaryEventDispatcher boundaryEventDispatcher(
      this, &mousePlatformEvent, exitedTarget);
  boundaryEventDispatcher.sendBoundaryEvents(exitedTarget, enteredTarget);
}

WebInputEventResult MouseEventManager::dispatchMouseEvent(
    EventTarget* target,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent,
    EventTarget* relatedTarget,
    bool checkForListener) {
  if (target && target->toNode() &&
      (!checkForListener || target->hasEventListeners(mouseEventType))) {
    Node* targetNode = target->toNode();
    int clickCount = 0;
    if (mouseEventType == EventTypeNames::mouseup ||
        mouseEventType == EventTypeNames::mousedown ||
        mouseEventType == EventTypeNames::click ||
        mouseEventType == EventTypeNames::auxclick ||
        mouseEventType == EventTypeNames::dblclick) {
      clickCount = m_clickCount;
    }
    MouseEvent* event = MouseEvent::create(
        mouseEventType, targetNode->document().domWindow(), mouseEvent,
        clickCount, relatedTarget ? relatedTarget->toNode() : nullptr);
    DispatchEventResult dispatchResult = target->dispatchEvent(event);
    return EventHandlingUtil::toWebInputEventResult(dispatchResult);
  }
  return WebInputEventResult::NotHandled;
}

WebInputEventResult MouseEventManager::setMousePositionAndDispatchMouseEvent(
    Node* targetNode,
    const AtomicString& eventType,
    const PlatformMouseEvent& platformMouseEvent) {
  // If the target node is a text node, dispatch on the parent node.
  if (targetNode && targetNode->isTextNode())
    targetNode = FlatTreeTraversal::parent(*targetNode);

  setNodeUnderMouse(targetNode, platformMouseEvent);

  return dispatchMouseEvent(m_nodeUnderMouse, eventType, platformMouseEvent,
                            nullptr);
}

WebInputEventResult MouseEventManager::dispatchMouseClickIfNeeded(
    const MouseEventWithHitTestResults& mev) {
  // We only prevent click event when the click may cause contextmenu to popup.
  // However, we always send auxclick.
  bool contextMenuEvent = !RuntimeEnabledFeatures::auxclickEnabled() &&
                          mev.event().pointerProperties().button ==
                              WebPointerProperties::Button::Right;
#if OS(MACOSX)
  // FIXME: The Mac port achieves the same behavior by checking whether the
  // context menu is currently open in WebPage::mouseEvent(). Consider merging
  // the implementations.
  if (mev.event().pointerProperties().button ==
          WebPointerProperties::Button::Left &&
      mev.event().getModifiers() & PlatformEvent::CtrlKey)
    contextMenuEvent = true;
#endif

  WebInputEventResult clickEventResult = WebInputEventResult::NotHandled;
  const bool shouldDispatchClickEvent =
      m_clickCount > 0 && !contextMenuEvent && mev.innerNode() && m_clickNode &&
      mev.innerNode()->canParticipateInFlatTree() &&
      m_clickNode->canParticipateInFlatTree() &&
      !(m_frame->eventHandler().selectionController().hasExtendedSelection() &&
        isLinkSelection(mev));
  if (shouldDispatchClickEvent) {
    Node* clickTargetNode = nullptr;
    // Updates distribution because a 'mouseup' event listener can make the
    // tree dirty at dispatchMouseEvent() invocation above.
    // Unless distribution is updated, commonAncestor would hit ASSERT.
    if (m_clickNode == mev.innerNode()) {
      clickTargetNode = m_clickNode;
      clickTargetNode->updateDistribution();
    } else if (m_clickNode->document() == mev.innerNode()->document()) {
      m_clickNode->updateDistribution();
      mev.innerNode()->updateDistribution();
      clickTargetNode = mev.innerNode()->commonAncestor(
          *m_clickNode, EventHandlingUtil::parentForClickEvent);
    }
    if (clickTargetNode) {
      clickEventResult = dispatchMouseEvent(
          clickTargetNode, !RuntimeEnabledFeatures::auxclickEnabled() ||
                                   (mev.event().pointerProperties().button ==
                                    WebPointerProperties::Button::Left)
                               ? EventTypeNames::click
                               : EventTypeNames::auxclick,
          mev.event(), nullptr);
    }
  }
  return clickEventResult;
}

void MouseEventManager::fakeMouseMoveEventTimerFired(TimerBase* timer) {
  TRACE_EVENT0("input", "MouseEventManager::fakeMouseMoveEventTimerFired");
  DCHECK(timer == &m_fakeMouseMoveEventTimer);
  DCHECK(!m_mousePressed);

  Settings* settings = m_frame->settings();
  if (settings && !settings->deviceSupportsMouse())
    return;

  FrameView* view = m_frame->view();
  if (!view)
    return;

  if (!m_frame->page() || !m_frame->page()->focusController().isActive())
    return;

  // Don't dispatch a synthetic mouse move event if the mouse cursor is not
  // visible to the user.
  if (!m_frame->page()->isCursorVisible())
    return;

  PlatformMouseEvent fakeMouseMoveEvent(
      m_lastKnownMousePosition, m_lastKnownMouseGlobalPosition,
      WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 0,
      static_cast<PlatformEvent::Modifiers>(
          KeyboardEventManager::getCurrentModifierState()),
      PlatformMouseEvent::RealOrIndistinguishable,
      monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
  m_frame->eventHandler().handleMouseMoveEvent(fakeMouseMoveEvent);
}

void MouseEventManager::cancelFakeMouseMoveEvent() {
  m_fakeMouseMoveEventTimer.stop();
}

void MouseEventManager::setNodeUnderMouse(
    Node* target,
    const PlatformMouseEvent& platformMouseEvent) {
  Node* lastNodeUnderMouse = m_nodeUnderMouse;
  m_nodeUnderMouse = target;

  PaintLayer* layerForLastNode =
      EventHandlingUtil::layerForNode(lastNodeUnderMouse);
  PaintLayer* layerForNodeUnderMouse =
      EventHandlingUtil::layerForNode(m_nodeUnderMouse.get());
  Page* page = m_frame->page();

  if (lastNodeUnderMouse &&
      (!m_nodeUnderMouse ||
       m_nodeUnderMouse->document() != m_frame->document())) {
    // The mouse has moved between frames.
    if (LocalFrame* frame = lastNodeUnderMouse->document().frame()) {
      if (FrameView* frameView = frame->view())
        frameView->mouseExitedContentArea();
    }
  } else if (page && (layerForLastNode &&
                      (!layerForNodeUnderMouse ||
                       layerForNodeUnderMouse != layerForLastNode))) {
    // The mouse has moved between layers.
    if (ScrollableArea* scrollableAreaForLastNode =
            EventHandlingUtil::associatedScrollableArea(layerForLastNode))
      scrollableAreaForLastNode->mouseExitedContentArea();
  }

  if (m_nodeUnderMouse &&
      (!lastNodeUnderMouse ||
       lastNodeUnderMouse->document() != m_frame->document())) {
    // The mouse has moved between frames.
    if (LocalFrame* frame = m_nodeUnderMouse->document().frame()) {
      if (FrameView* frameView = frame->view())
        frameView->mouseEnteredContentArea();
    }
  } else if (page && (layerForNodeUnderMouse &&
                      (!layerForLastNode ||
                       layerForNodeUnderMouse != layerForLastNode))) {
    // The mouse has moved between layers.
    if (ScrollableArea* scrollableAreaForNodeUnderMouse =
            EventHandlingUtil::associatedScrollableArea(layerForNodeUnderMouse))
      scrollableAreaForNodeUnderMouse->mouseEnteredContentArea();
  }

  if (lastNodeUnderMouse &&
      lastNodeUnderMouse->document() != m_frame->document()) {
    lastNodeUnderMouse = nullptr;
  }

  sendBoundaryEvents(lastNodeUnderMouse, m_nodeUnderMouse, platformMouseEvent);
}

void MouseEventManager::nodeChildrenWillBeRemoved(ContainerNode& container) {
  if (container == m_clickNode)
    return;
  if (!container.isShadowIncludingInclusiveAncestorOf(m_clickNode.get()))
    return;
  m_clickNode = nullptr;
}

void MouseEventManager::nodeWillBeRemoved(Node& nodeToBeRemoved) {
  if (nodeToBeRemoved.isShadowIncludingInclusiveAncestorOf(m_clickNode.get())) {
    // We don't dispatch click events if the mousedown node is removed
    // before a mouseup event. It is compatible with IE and Firefox.
    m_clickNode = nullptr;
  }
}

Node* MouseEventManager::getNodeUnderMouse() {
  return m_nodeUnderMouse;
}

WebInputEventResult MouseEventManager::handleMouseFocus(
    const HitTestResult& hitTestResult,
    InputDeviceCapabilities* sourceCapabilities) {
  // If clicking on a frame scrollbar, do not mess up with content focus.
  if (hitTestResult.scrollbar() && !m_frame->contentLayoutItem().isNull()) {
    if (hitTestResult.scrollbar()->getScrollableArea() ==
        m_frame->contentLayoutItem().getScrollableArea())
      return WebInputEventResult::NotHandled;
  }

  // The layout needs to be up to date to determine if an element is focusable.
  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  Element* element = nullptr;
  if (m_nodeUnderMouse) {
    element = m_nodeUnderMouse->isElementNode()
                  ? toElement(m_nodeUnderMouse)
                  : m_nodeUnderMouse->parentOrShadowHostElement();
  }
  for (; element; element = element->parentOrShadowHostElement()) {
    if (element->isFocusable() && element->isFocusedElementInDocument())
      return WebInputEventResult::NotHandled;
    if (element->isMouseFocusable())
      break;
  }
  DCHECK(!element || element->isMouseFocusable());

  // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus
  // a node on mouse down if it's selected and inside a focused node. It will
  // be focused if the user does a mouseup over it, however, because the
  // mouseup will set a selection inside it, which will call
  // FrameSelection::setFocusedNodeIfNeeded.
  if (element && m_frame->selection().isRange()) {
    // TODO(yosin) We should not create |Range| object for calling
    // |isNodeFullyContained()|.
    if (createRange(
            m_frame->selection().selection().toNormalizedEphemeralRange())
            ->isNodeFullyContained(*element) &&
        element->isDescendantOf(m_frame->document()->focusedElement()))
      return WebInputEventResult::NotHandled;
  }

  // Only change the focus when clicking scrollbars if it can transfered to a
  // mouse focusable node.
  if (!element && hitTestResult.scrollbar())
    return WebInputEventResult::HandledSystem;

  if (Page* page = m_frame->page()) {
    // If focus shift is blocked, we eat the event. Note we should never
    // clear swallowEvent if the page already set it (e.g., by canceling
    // default behavior).
    if (element) {
      if (slideFocusOnShadowHostIfNecessary(*element))
        return WebInputEventResult::HandledSystem;
      if (!page->focusController().setFocusedElement(
              element, m_frame,
              FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeMouse,
                          sourceCapabilities)))
        return WebInputEventResult::HandledSystem;
    } else {
      // We call setFocusedElement even with !element in order to blur
      // current focus element when a link is clicked; this is expected by
      // some sites that rely on onChange handlers running from form
      // fields before the button click is processed.
      if (!page->focusController().setFocusedElement(
              nullptr, m_frame,
              FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone,
                          sourceCapabilities)))
        return WebInputEventResult::HandledSystem;
    }
  }

  return WebInputEventResult::NotHandled;
}

bool MouseEventManager::slideFocusOnShadowHostIfNecessary(
    const Element& element) {
  if (element.authorShadowRoot() &&
      element.authorShadowRoot()->delegatesFocus()) {
    Document* doc = m_frame->document();
    if (element.isShadowIncludingInclusiveAncestorOf(doc->focusedElement())) {
      // If the inner element is already focused, do nothing.
      return true;
    }

    // If the host has a focusable inner element, focus it. Otherwise, the host
    // takes focus.
    Page* page = m_frame->page();
    DCHECK(page);
    Element* found =
        page->focusController().findFocusableElementInShadowHost(element);
    if (found && element.isShadowIncludingInclusiveAncestorOf(found)) {
      // Use WebFocusTypeForward instead of WebFocusTypeMouse here to mean the
      // focus has slided.
      found->focus(FocusParams(SelectionBehaviorOnFocus::Reset,
                               WebFocusTypeForward, nullptr));
      return true;
    }
  }
  return false;
}

void MouseEventManager::handleMousePressEventUpdateStates(
    const PlatformMouseEvent& mouseEvent) {
  cancelFakeMouseMoveEvent();
  m_mousePressed = true;
  m_capturesDragging = true;
  setLastKnownMousePosition(mouseEvent);
  m_mouseDownMayStartDrag = false;
  m_mouseDownMayStartAutoscroll = false;
  m_mouseDownTimestamp = mouseEvent.timestamp();

  if (FrameView* view = m_frame->view()) {
    m_mouseDownPos = view->rootFrameToContents(mouseEvent.position());
  } else {
    invalidateClick();
  }
}

bool MouseEventManager::isMousePositionUnknown() {
  return m_isMousePositionUnknown;
}

IntPoint MouseEventManager::lastKnownMousePosition() {
  return m_lastKnownMousePosition;
}

void MouseEventManager::setLastKnownMousePosition(
    const PlatformMouseEvent& event) {
  m_isMousePositionUnknown = false;
  m_lastKnownMousePosition = event.position();
  m_lastKnownMouseGlobalPosition = event.globalPosition();
}

void MouseEventManager::dispatchFakeMouseMoveEventSoon() {
  if (m_mousePressed)
    return;

  if (m_isMousePositionUnknown)
    return;

  Settings* settings = m_frame->settings();
  if (settings && !settings->deviceSupportsMouse())
    return;

  // Reschedule the timer, to prevent dispatching mouse move events
  // during a scroll. This avoids a potential source of scroll jank.
  m_fakeMouseMoveEventTimer.startOneShot(kFakeMouseMoveInterval,
                                         BLINK_FROM_HERE);
}

void MouseEventManager::dispatchFakeMouseMoveEventSoonInQuad(
    const FloatQuad& quad) {
  FrameView* view = m_frame->view();
  if (!view)
    return;

  if (!quad.containsPoint(view->rootFrameToContents(m_lastKnownMousePosition)))
    return;

  dispatchFakeMouseMoveEventSoon();
}

WebInputEventResult MouseEventManager::handleMousePressEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "MouseEventManager::handleMousePressEvent");

  // Reset drag state.
  dragState().m_dragSrc = nullptr;

  cancelFakeMouseMoveEvent();

  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  if (FrameView* frameView = m_frame->view()) {
    if (frameView->isPointInScrollbarCorner(event.event().position()))
      return WebInputEventResult::NotHandled;
  }

  bool singleClick = event.event().clickCount() <= 1;

  m_mouseDownMayStartDrag =
      singleClick && !isLinkSelection(event) && !isExtendingSelection(event);

  m_frame->eventHandler().selectionController().handleMousePressEvent(event);

  m_mouseDown = event.event();

  if (m_frame->document()->isSVGDocument() &&
      m_frame->document()->accessSVGExtensions().zoomAndPanEnabled()) {
    if (event.event().shiftKey() && singleClick) {
      m_svgPan = true;
      m_frame->document()->accessSVGExtensions().startPan(
          m_frame->view()->rootFrameToContents(event.event().position()));
      return WebInputEventResult::HandledSystem;
    }
  }

  // We don't do this at the start of mouse down handling,
  // because we don't want to do it until we know we didn't hit a widget.
  if (singleClick)
    focusDocumentView();

  Node* innerNode = event.innerNode();

  m_mousePressNode = innerNode;
  m_frame->document()->setSequentialFocusNavigationStartingPoint(innerNode);
  m_dragStartPos = event.event().position();

  bool swallowEvent = false;
  m_mousePressed = true;

  if (event.event().clickCount() == 2) {
    swallowEvent = m_frame->eventHandler()
                       .selectionController()
                       .handleMousePressEventDoubleClick(event);
  } else if (event.event().clickCount() >= 3) {
    swallowEvent = m_frame->eventHandler()
                       .selectionController()
                       .handleMousePressEventTripleClick(event);
  } else {
    swallowEvent = m_frame->eventHandler()
                       .selectionController()
                       .handleMousePressEventSingleClick(event);
  }

  m_mouseDownMayStartAutoscroll =
      m_frame->eventHandler().selectionController().mouseDownMayStartSelect() ||
      (m_mousePressNode && m_mousePressNode->layoutBox() &&
       m_mousePressNode->layoutBox()->canBeProgramaticallyScrolled());

  return swallowEvent ? WebInputEventResult::HandledSystem
                      : WebInputEventResult::NotHandled;
}

WebInputEventResult MouseEventManager::handleMouseReleaseEvent(
    const MouseEventWithHitTestResults& event) {
  AutoscrollController* controller = m_scrollManager->autoscrollController();
  if (controller && controller->autoscrollInProgress())
    m_scrollManager->stopAutoscroll();

  return m_frame->eventHandler().selectionController().handleMouseReleaseEvent(
             event, m_dragStartPos)
             ? WebInputEventResult::HandledSystem
             : WebInputEventResult::NotHandled;
}

void MouseEventManager::updateSelectionForMouseDrag() {
  m_frame->eventHandler().selectionController().updateSelectionForMouseDrag(
      m_mousePressNode, m_dragStartPos, m_lastKnownMousePosition);
}

bool MouseEventManager::handleDragDropIfPossible(
    const GestureEventWithHitTestResults& targetedEvent) {
  if (m_frame->settings() && m_frame->settings()->touchDragDropEnabled() &&
      m_frame->view()) {
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    IntPoint adjustedPoint = gestureEvent.position();
    unsigned modifiers = gestureEvent.getModifiers();

    // TODO(mustaq): Suppressing long-tap MouseEvents could break
    // drag-drop. Will do separately because of the risk. crbug.com/606938.
    PlatformMouseEvent mouseDownEvent(
        adjustedPoint, gestureEvent.globalPosition(),
        WebPointerProperties::Button::Left, PlatformEvent::MousePressed, 1,
        static_cast<PlatformEvent::Modifiers>(modifiers |
                                              PlatformEvent::LeftButtonDown),
        PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(),
        WebPointerProperties::PointerType::Mouse);
    m_mouseDown = mouseDownEvent;

    PlatformMouseEvent mouseDragEvent(
        adjustedPoint, gestureEvent.globalPosition(),
        WebPointerProperties::Button::Left, PlatformEvent::MouseMoved, 1,
        static_cast<PlatformEvent::Modifiers>(modifiers |
                                              PlatformEvent::LeftButtonDown),
        PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(),
        WebPointerProperties::PointerType::Mouse);
    HitTestRequest request(HitTestRequest::ReadOnly);
    MouseEventWithHitTestResults mev =
        EventHandlingUtil::performMouseEventHitTest(m_frame, request,
                                                    mouseDragEvent);
    m_mouseDownMayStartDrag = true;
    dragState().m_dragSrc = nullptr;
    m_mouseDownPos =
        m_frame->view()->rootFrameToContents(mouseDragEvent.position());
    return handleDrag(mev, DragInitiator::Touch);
  }
  return false;
}

void MouseEventManager::focusDocumentView() {
  Page* page = m_frame->page();
  if (!page)
    return;
  page->focusController().focusDocumentView(m_frame);
}

WebInputEventResult MouseEventManager::handleMouseDraggedEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "MouseEventManager::handleMouseDraggedEvent");

  // While resetting m_mousePressed here may seem out of place, it turns out
  // to be needed to handle some bugs^Wfeatures in Blink mouse event handling:
  // 1. Certain elements, such as <embed>, capture mouse events. They do not
  //    bubble back up. One way for a <embed> to start capturing mouse events
  //    is on a mouse press. The problem is the <embed> node only starts
  //    capturing mouse events *after* m_mousePressed for the containing frame
  //    has already been set to true. As a result, the frame's EventHandler
  //    never sees the mouse release event, which is supposed to reset
  //    m_mousePressed... so m_mousePressed ends up remaining true until the
  //    event handler finally gets another mouse released event. Oops.
  // 2. Dragging doesn't start until after a mouse press event, but a drag
  //    that ends as a result of a mouse release does not send a mouse release
  //    event. As a result, m_mousePressed also ends up remaining true until
  //    the next mouse release event seen by the EventHandler.
  if (event.event().pointerProperties().button !=
      WebPointerProperties::Button::Left)
    m_mousePressed = false;

  if (!m_mousePressed)
    return WebInputEventResult::NotHandled;

  if (handleDrag(event, DragInitiator::Mouse))
    return WebInputEventResult::HandledSystem;

  Node* targetNode = event.innerNode();
  if (!targetNode)
    return WebInputEventResult::NotHandled;

  LayoutObject* layoutObject = targetNode->layoutObject();
  if (!layoutObject) {
    Node* parent = FlatTreeTraversal::parent(*targetNode);
    if (!parent)
      return WebInputEventResult::NotHandled;

    layoutObject = parent->layoutObject();
    if (!layoutObject || !layoutObject->isListBox())
      return WebInputEventResult::NotHandled;
  }

  m_mouseDownMayStartDrag = false;

  if (m_mouseDownMayStartAutoscroll &&
      !m_scrollManager->middleClickAutoscrollInProgress()) {
    if (AutoscrollController* controller =
            m_scrollManager->autoscrollController()) {
      // Avoid updating the lifecycle unless it's possible to autoscroll.
      layoutObject->frameView()->updateAllLifecyclePhasesExceptPaint();

      // The lifecycle update above may have invalidated the previous layout.
      layoutObject = targetNode->layoutObject();
      if (layoutObject) {
        controller->startAutoscrollForSelection(layoutObject);
        m_mouseDownMayStartAutoscroll = false;
      }
    }
  }

  m_frame->eventHandler().selectionController().handleMouseDraggedEvent(
      event, m_mouseDownPos, m_dragStartPos, m_mousePressNode.get(),
      m_lastKnownMousePosition);
  return WebInputEventResult::HandledSystem;
}

bool MouseEventManager::handleDrag(const MouseEventWithHitTestResults& event,
                                   DragInitiator initiator) {
  DCHECK(event.event().type() == PlatformEvent::MouseMoved);
  // Callers must protect the reference to FrameView, since this function may
  // dispatch DOM events, causing page/FrameView to go away.
  DCHECK(m_frame);
  DCHECK(m_frame->view());
  if (!m_frame->page())
    return false;

  if (m_mouseDownMayStartDrag) {
    HitTestRequest request(HitTestRequest::ReadOnly);
    HitTestResult result(request, m_mouseDownPos);
    m_frame->contentLayoutItem().hitTest(result);
    Node* node = result.innerNode();
    if (node) {
      DragController::SelectionDragPolicy selectionDragPolicy =
          event.event().timestamp() - m_mouseDownTimestamp < kTextDragDelay
              ? DragController::DelayedSelectionDragResolution
              : DragController::ImmediateSelectionDragResolution;
      dragState().m_dragSrc = m_frame->page()->dragController().draggableNode(
          m_frame, node, m_mouseDownPos, selectionDragPolicy,
          dragState().m_dragType);
    } else {
      dragState().m_dragSrc = nullptr;
    }

    if (!dragState().m_dragSrc)
      m_mouseDownMayStartDrag = false;  // no element is draggable
  }

  if (!m_mouseDownMayStartDrag) {
    return initiator == DragInitiator::Mouse &&
           !m_frame->eventHandler()
                .selectionController()
                .mouseDownMayStartSelect() &&
           !m_mouseDownMayStartAutoscroll;
  }

  // We are starting a text/image/url drag, so the cursor should be an arrow
  // FIXME <rdar://7577595>: Custom cursors aren't supported during drag and
  // drop (default to pointer).
  m_frame->view()->setCursor(pointerCursor());

  if (initiator == DragInitiator::Mouse &&
      !dragThresholdExceeded(event.event().position()))
    return true;

  // Once we're past the drag threshold, we don't want to treat this gesture as
  // a click.
  invalidateClick();

  if (!tryStartDrag(event)) {
    // Something failed to start the drag, clean up.
    clearDragDataTransfer();
    dragState().m_dragSrc = nullptr;
  }

  m_mouseDownMayStartDrag = false;
  // Whether or not the drag actually started, no more default handling (like
  // selection).
  return true;
}

DataTransfer* MouseEventManager::createDraggingDataTransfer() const {
  return DataTransfer::create(DataTransfer::DragAndDrop, DataTransferWritable,
                              DataObject::create());
}

bool MouseEventManager::tryStartDrag(
    const MouseEventWithHitTestResults& event) {
  // The DataTransfer would only be non-empty if we missed a dragEnd.
  // Clear it anyway, just to make sure it gets numbified.
  clearDragDataTransfer();

  dragState().m_dragDataTransfer = createDraggingDataTransfer();

  DragController& dragController = m_frame->page()->dragController();
  if (!dragController.populateDragDataTransfer(m_frame, dragState(),
                                               m_mouseDownPos))
    return false;

  // If dispatching dragstart brings about another mouse down -- one way
  // this will happen is if a DevTools user breaks within a dragstart
  // handler and then clicks on the suspended page -- the drag state is
  // reset. Hence, need to check if this particular drag operation can
  // continue even if dispatchEvent() indicates no (direct) cancellation.
  // Do that by checking if m_dragSrc is still set.
  m_mouseDownMayStartDrag =
      dispatchDragSrcEvent(EventTypeNames::dragstart, m_mouseDown) ==
          WebInputEventResult::NotHandled &&
      !m_frame->selection().isInPasswordField() && dragState().m_dragSrc;

  // Invalidate clipboard here against anymore pasteboard writing for security.
  // The drag image can still be changed as we drag, but not the pasteboard
  // data.
  dragState().m_dragDataTransfer->setAccessPolicy(DataTransferImageWritable);

  if (m_mouseDownMayStartDrag) {
    // Dispatching the event could cause Page to go away. Make sure it's still
    // valid before trying to use DragController.
    if (m_frame->page() &&
        dragController.startDrag(m_frame, dragState(), event.event(),
                                 m_mouseDownPos))
      return true;
    // Drag was canned at the last minute - we owe m_dragSrc a DRAGEND event
    dispatchDragSrcEvent(EventTypeNames::dragend, event.event());
  }

  return false;
}

// Returns if we should continue "default processing", i.e., whether
// eventhandler canceled.
WebInputEventResult MouseEventManager::dispatchDragSrcEvent(
    const AtomicString& eventType,
    const PlatformMouseEvent& event) {
  return dispatchDragEvent(eventType, dragState().m_dragSrc.get(), event,
                           dragState().m_dragDataTransfer.get());
}

WebInputEventResult MouseEventManager::dispatchDragEvent(
    const AtomicString& eventType,
    Node* dragTarget,
    const PlatformMouseEvent& event,
    DataTransfer* dataTransfer) {
  FrameView* view = m_frame->view();

  // FIXME: We might want to dispatch a dragleave even if the view is gone.
  if (!view)
    return WebInputEventResult::NotHandled;

  const bool cancelable = eventType != EventTypeNames::dragleave &&
                          eventType != EventTypeNames::dragend;

  DragEvent* me = DragEvent::create(
      eventType, true, cancelable, m_frame->document()->domWindow(), 0,
      event.globalPosition().x(), event.globalPosition().y(),
      event.position().x(), event.position().y(), event.movementDelta().x(),
      event.movementDelta().y(), event.getModifiers(), 0,
      MouseEvent::platformModifiersToButtons(event.getModifiers()), nullptr,
      event.timestamp(), dataTransfer, event.getSyntheticEventType());

  return EventHandlingUtil::toWebInputEventResult(
      dragTarget->dispatchEvent(me));
}

void MouseEventManager::clearDragDataTransfer() {
  if (dragState().m_dragDataTransfer) {
    dragState().m_dragDataTransfer->clearDragImage();
    dragState().m_dragDataTransfer->setAccessPolicy(DataTransferNumb);
  }
}

void MouseEventManager::dragSourceEndedAt(const PlatformMouseEvent& event,
                                          DragOperation operation) {
  if (dragState().m_dragSrc) {
    dragState().m_dragDataTransfer->setDestinationOperation(operation);
    // For now we don't care if event handler cancels default behavior, since
    // there is none.
    dispatchDragSrcEvent(EventTypeNames::dragend, event);
  }
  clearDragDataTransfer();
  dragState().m_dragSrc = nullptr;
  // In case the drag was ended due to an escape key press we need to ensure
  // that consecutive mousemove events don't reinitiate the drag and drop.
  m_mouseDownMayStartDrag = false;
}

DragState& MouseEventManager::dragState() {
  DEFINE_STATIC_LOCAL(DragState, state, (new DragState));
  return state;
}

bool MouseEventManager::dragThresholdExceeded(
    const IntPoint& dragLocationInRootFrame) const {
  FrameView* view = m_frame->view();
  if (!view)
    return false;
  IntPoint dragLocation = view->rootFrameToContents(dragLocationInRootFrame);
  IntSize delta = dragLocation - m_mouseDownPos;

  // WebKit's drag thresholds depend on the type of object being dragged. If we
  // want to revive that behavior, we can multiply the threshold constants with
  // a number based on dragState().m_dragType.

  return abs(delta.width()) >= kDragThresholdX ||
         abs(delta.height()) >= kDragThresholdY;
}

void MouseEventManager::clearDragHeuristicState() {
  // Used to prevent mouseMoveEvent from initiating a drag before
  // the mouse is pressed again.
  m_mousePressed = false;
  m_capturesDragging = false;
  m_mouseDownMayStartDrag = false;
  m_mouseDownMayStartAutoscroll = false;
}

bool MouseEventManager::handleSvgPanIfNeeded(bool isReleaseEvent) {
  if (!m_svgPan)
    return false;
  m_svgPan = !isReleaseEvent;
  m_frame->document()->accessSVGExtensions().updatePan(
      m_frame->view()->rootFrameToContents(m_lastKnownMousePosition));
  return true;
}

void MouseEventManager::invalidateClick() {
  m_clickCount = 0;
  m_clickNode = nullptr;
}

bool MouseEventManager::mousePressed() {
  return m_mousePressed;
}

void MouseEventManager::setMousePressed(bool mousePressed) {
  m_mousePressed = mousePressed;
}

bool MouseEventManager::capturesDragging() const {
  return m_capturesDragging;
}

void MouseEventManager::setCapturesDragging(bool capturesDragging) {
  m_capturesDragging = capturesDragging;
}

Node* MouseEventManager::mousePressNode() {
  return m_mousePressNode;
}

void MouseEventManager::setMousePressNode(Node* node) {
  m_mousePressNode = node;
}

void MouseEventManager::setClickNode(Node* node) {
  setContext(node ? node->ownerDocument() : nullptr);
  m_clickNode = node;
}

void MouseEventManager::setClickCount(int clickCount) {
  m_clickCount = clickCount;
}

bool MouseEventManager::mouseDownMayStartDrag() {
  return m_mouseDownMayStartDrag;
}

}  // namespace blink
