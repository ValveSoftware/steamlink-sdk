// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/PointerEventManager.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/MouseEventManager.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/PlatformTouchEvent.h"

namespace blink {

namespace {

size_t toPointerTypeIndex(WebPointerProperties::PointerType t) {
  return static_cast<size_t>(t);
}

const AtomicString& pointerEventNameForTouchPointState(
    PlatformTouchPoint::TouchState state) {
  switch (state) {
    case PlatformTouchPoint::TouchReleased:
      return EventTypeNames::pointerup;
    case PlatformTouchPoint::TouchCancelled:
      return EventTypeNames::pointercancel;
    case PlatformTouchPoint::TouchPressed:
      return EventTypeNames::pointerdown;
    case PlatformTouchPoint::TouchMoved:
      return EventTypeNames::pointermove;
    case PlatformTouchPoint::TouchStationary:
    // Fall through to default
    default:
      NOTREACHED();
      return emptyAtom;
  }
}

bool isInDocument(EventTarget* n) {
  return n && n->toNode() && n->toNode()->isConnected();
}

}  // namespace

PointerEventManager::PointerEventManager(LocalFrame* frame,
                                         MouseEventManager* mouseEventManager)
    : m_frame(frame),
      m_touchEventManager(new TouchEventManager(frame)),
      m_mouseEventManager(mouseEventManager) {
  clear();
}

void PointerEventManager::clear() {
  for (auto& entry : m_preventMouseEventForPointerType)
    entry = false;
  m_touchEventManager->clear();
  m_inCanceledStateForPointerTypeTouch = false;
  m_pointerEventFactory.clear();
  m_touchIdsForCanceledPointerdowns.clear();
  m_nodeUnderPointer.clear();
  m_pointerCaptureTarget.clear();
  m_pendingPointerCaptureTarget.clear();
}

DEFINE_TRACE(PointerEventManager) {
  visitor->trace(m_frame);
  visitor->trace(m_nodeUnderPointer);
  visitor->trace(m_pointerCaptureTarget);
  visitor->trace(m_pendingPointerCaptureTarget);
  visitor->trace(m_touchEventManager);
  visitor->trace(m_mouseEventManager);
}

PointerEventManager::PointerEventBoundaryEventDispatcher::
    PointerEventBoundaryEventDispatcher(
        PointerEventManager* pointerEventManager,
        PointerEvent* pointerEvent)
    : m_pointerEventManager(pointerEventManager),
      m_pointerEvent(pointerEvent) {}

void PointerEventManager::PointerEventBoundaryEventDispatcher::dispatchOut(
    EventTarget* target,
    EventTarget* relatedTarget) {
  dispatch(target, relatedTarget, EventTypeNames::pointerout, false);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::dispatchOver(
    EventTarget* target,
    EventTarget* relatedTarget) {
  dispatch(target, relatedTarget, EventTypeNames::pointerover, false);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::dispatchLeave(
    EventTarget* target,
    EventTarget* relatedTarget,
    bool checkForListener) {
  dispatch(target, relatedTarget, EventTypeNames::pointerleave,
           checkForListener);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::dispatchEnter(
    EventTarget* target,
    EventTarget* relatedTarget,
    bool checkForListener) {
  dispatch(target, relatedTarget, EventTypeNames::pointerenter,
           checkForListener);
}

AtomicString
PointerEventManager::PointerEventBoundaryEventDispatcher::getLeaveEvent() {
  return EventTypeNames::pointerleave;
}

AtomicString
PointerEventManager::PointerEventBoundaryEventDispatcher::getEnterEvent() {
  return EventTypeNames::pointerenter;
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::dispatch(
    EventTarget* target,
    EventTarget* relatedTarget,
    const AtomicString& type,
    bool checkForListener) {
  m_pointerEventManager->dispatchPointerEvent(
      target,
      m_pointerEventManager->m_pointerEventFactory.createPointerBoundaryEvent(
          m_pointerEvent, type, relatedTarget),
      checkForListener);
}

WebInputEventResult PointerEventManager::dispatchPointerEvent(
    EventTarget* target,
    PointerEvent* pointerEvent,
    bool checkForListener) {
  if (!target)
    return WebInputEventResult::NotHandled;

  // Set whether node under pointer has received pointerover or not.
  const int pointerId = pointerEvent->pointerId();
  const AtomicString& eventType = pointerEvent->type();
  if ((eventType == EventTypeNames::pointerout ||
       eventType == EventTypeNames::pointerover) &&
      m_nodeUnderPointer.contains(pointerId)) {
    EventTarget* targetUnderPointer = m_nodeUnderPointer.get(pointerId).target;
    if (targetUnderPointer == target) {
      m_nodeUnderPointer.set(
          pointerId,
          EventTargetAttributes(targetUnderPointer,
                                eventType == EventTypeNames::pointerover));
    }
  }

  if (!RuntimeEnabledFeatures::pointerEventEnabled())
    return WebInputEventResult::NotHandled;
  if (!checkForListener || target->hasEventListeners(eventType)) {
    UseCounter::count(m_frame->document(), UseCounter::PointerEventDispatch);
    if (eventType == EventTypeNames::pointerdown)
      UseCounter::count(m_frame->document(),
                        UseCounter::PointerEventDispatchPointerDown);

    DispatchEventResult dispatchResult = target->dispatchEvent(pointerEvent);
    return EventHandlingUtil::toWebInputEventResult(dispatchResult);
  }
  return WebInputEventResult::NotHandled;
}

EventTarget* PointerEventManager::getEffectiveTargetForPointerEvent(
    EventTarget* target,
    int pointerId) {
  if (EventTarget* capturingTarget = getCapturingNode(pointerId))
    return capturingTarget;
  return target;
}

void PointerEventManager::sendMouseAndPointerBoundaryEvents(
    Node* enteredNode,
    const PlatformMouseEvent& mouseEvent) {
  // Mouse event type does not matter as this pointerevent will only be used
  // to create boundary pointer events and its type will be overridden in
  // |sendBoundaryEvents| function.
  PointerEvent* dummyPointerEvent = m_pointerEventFactory.create(
      EventTypeNames::mousedown, mouseEvent, m_frame->document()->domWindow());

  // TODO(crbug/545647): This state should reset with pointercancel too.
  // This function also gets called for compat mouse events of touch at this
  // stage. So if the event is not frame boundary transition it is only a
  // compatibility mouse event and we do not need to change pointer event
  // behavior regarding preventMouseEvent state in that case.
  if (dummyPointerEvent->buttons() == 0 && dummyPointerEvent->isPrimary()) {
    m_preventMouseEventForPointerType[toPointerTypeIndex(
        mouseEvent.pointerProperties().pointerType)] = false;
  }

  processCaptureAndPositionOfPointerEvent(dummyPointerEvent, enteredNode,
                                          mouseEvent, true);
}

void PointerEventManager::sendBoundaryEvents(EventTarget* exitedTarget,
                                             EventTarget* enteredTarget,
                                             PointerEvent* pointerEvent) {
  if (RuntimeEnabledFeatures::pointerEventV1SpecCapturingEnabled()) {
    if (exitedTarget == enteredTarget)
      return;
    if (EventTarget* capturingTarget =
            getCapturingNode(pointerEvent->pointerId())) {
      if (capturingTarget == exitedTarget)
        enteredTarget = nullptr;
      else if (capturingTarget == enteredTarget)
        exitedTarget = nullptr;
      else
        return;
    }
  }
  PointerEventBoundaryEventDispatcher boundaryEventDispatcher(this,
                                                              pointerEvent);
  boundaryEventDispatcher.sendBoundaryEvents(exitedTarget, enteredTarget);
}

void PointerEventManager::setNodeUnderPointer(PointerEvent* pointerEvent,
                                              EventTarget* target) {
  if (m_nodeUnderPointer.contains(pointerEvent->pointerId())) {
    EventTargetAttributes node =
        m_nodeUnderPointer.get(pointerEvent->pointerId());
    if (!target) {
      m_nodeUnderPointer.remove(pointerEvent->pointerId());
    } else if (target !=
               m_nodeUnderPointer.get(pointerEvent->pointerId()).target) {
      m_nodeUnderPointer.set(pointerEvent->pointerId(),
                             EventTargetAttributes(target, false));
    }
    sendBoundaryEvents(node.target, target, pointerEvent);
  } else if (target) {
    m_nodeUnderPointer.add(pointerEvent->pointerId(),
                           EventTargetAttributes(target, false));
    sendBoundaryEvents(nullptr, target, pointerEvent);
  }
}

void PointerEventManager::blockTouchPointers() {
  if (m_inCanceledStateForPointerTypeTouch)
    return;
  m_inCanceledStateForPointerTypeTouch = true;

  Vector<int> touchPointerIds = m_pointerEventFactory.getPointerIdsOfType(
      WebPointerProperties::PointerType::Touch);

  for (int pointerId : touchPointerIds) {
    PointerEvent* pointerEvent = m_pointerEventFactory.createPointerCancelEvent(
        pointerId, WebPointerProperties::PointerType::Touch);

    DCHECK(m_nodeUnderPointer.contains(pointerId));
    EventTarget* target = m_nodeUnderPointer.get(pointerId).target;

    processCaptureAndPositionOfPointerEvent(pointerEvent, target);

    // TODO(nzolghadr): This event follows implicit TE capture. The actual
    // target would depend on PE capturing. Perhaps need to split TE/PE event
    // path upstream?  crbug.com/579553.
    dispatchPointerEvent(
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId()),
        pointerEvent);

    releasePointerCapture(pointerEvent->pointerId());

    // Sending the leave/out events and lostpointercapture
    // because the next touch event will have a different id. So delayed
    // sending of lostpointercapture won't work here.
    processCaptureAndPositionOfPointerEvent(pointerEvent, nullptr);

    removePointer(pointerEvent);
  }
}

void PointerEventManager::unblockTouchPointers() {
  m_inCanceledStateForPointerTypeTouch = false;
}

WebInputEventResult PointerEventManager::handleTouchEvents(
    const PlatformTouchEvent& event) {
  if (event.type() == PlatformEvent::TouchScrollStarted) {
    blockTouchPointers();
    return WebInputEventResult::HandledSystem;
  }

  bool newTouchSequence = true;
  for (const auto& touchPoint : event.touchPoints()) {
    if (touchPoint.state() != PlatformTouchPoint::TouchPressed) {
      newTouchSequence = false;
      break;
    }
  }
  if (newTouchSequence)
    unblockTouchPointers();

  // Do any necessary hit-tests and compute the event targets for all pointers
  // in the event.
  HeapVector<TouchEventManager::TouchInfo> touchInfos;
  computeTouchTargets(event, touchInfos);

  // Any finger lifting is a user gesture only when it wasn't associated with a
  // scroll.
  // https://docs.google.com/document/d/1oF1T3O7_E4t1PYHV6gyCwHxOi3ystm0eSL5xZu7nvOg/edit#
  // Re-use the same UserGesture for touchend and pointerup (but not for the
  // mouse events generated by GestureTap).
  // For the rare case of multi-finger scenarios spanning documents, it
  // seems extremely unlikely to matter which document the gesture is
  // associated with so just pick the first finger.
  RefPtr<UserGestureToken> possibleGestureToken;
  if (event.type() == PlatformEvent::TouchEnd &&
      !m_inCanceledStateForPointerTypeTouch && !touchInfos.isEmpty() &&
      touchInfos[0].targetFrame) {
    possibleGestureToken =
        DocumentUserGestureToken::create(touchInfos[0].targetFrame->document());
  }
  UserGestureIndicator holder(possibleGestureToken);

  dispatchTouchPointerEvents(event, touchInfos);

  return m_touchEventManager->handleTouchEvent(event, touchInfos);
}

void PointerEventManager::computeTouchTargets(
    const PlatformTouchEvent& event,
    HeapVector<TouchEventManager::TouchInfo>& touchInfos) {
  for (const auto& touchPoint : event.touchPoints()) {
    TouchEventManager::TouchInfo touchInfo;
    touchInfo.point = touchPoint;

    int pointerId =
        m_pointerEventFactory.getPointerEventId(touchPoint.pointerProperties());
    // Do the hit test either when the touch first starts or when the touch
    // is not captured. |m_pendingPointerCaptureTarget| indicates the target
    // that will be capturing this event. |m_pointerCaptureTarget| may not
    // have this target yet since the processing of that will be done right
    // before firing the event.
    if (RuntimeEnabledFeatures::pointerEventV1SpecCapturingEnabled() ||
        touchInfo.point.state() == PlatformTouchPoint::TouchPressed ||
        !m_pendingPointerCaptureTarget.contains(pointerId)) {
      HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent |
                                                   HitTestRequest::ReadOnly |
                                                   HitTestRequest::Active;
      LayoutPoint pagePoint = LayoutPoint(
          m_frame->view()->rootFrameToContents(touchInfo.point.pos()));
      HitTestResult hitTestTesult =
          m_frame->eventHandler().hitTestResultAtPoint(pagePoint, hitType);
      Node* node = hitTestTesult.innerNode();
      if (node) {
        touchInfo.targetFrame = node->document().frame();
        if (isHTMLCanvasElement(node)) {
          HitTestCanvasResult* hitTestCanvasResult =
              toHTMLCanvasElement(node)->getControlAndIdIfHitRegionExists(
                  hitTestTesult.pointInInnerNodeFrame());
          if (hitTestCanvasResult->getControl())
            node = hitTestCanvasResult->getControl();
          touchInfo.region = hitTestCanvasResult->getId();
        }
        // TODO(crbug.com/612456): We need to investigate whether pointer
        // events should go to text nodes or not. If so we need to
        // update the mouse code as well. Also this logic looks similar
        // to the one in TouchEventManager. We should be able to
        // refactor it better after this investigation.
        if (node->isTextNode())
          node = FlatTreeTraversal::parent(*node);
        touchInfo.touchNode = node;
      }
    } else {
      // Set the target of pointer event to the captured node as this
      // pointer is captured otherwise it would have gone to the if block
      // and perform a hit-test.
      touchInfo.touchNode =
          m_pendingPointerCaptureTarget.get(pointerId)->toNode();
      touchInfo.targetFrame = touchInfo.touchNode->document().frame();
    }

    touchInfos.append(touchInfo);
  }
}

void PointerEventManager::dispatchTouchPointerEvents(
    const PlatformTouchEvent& event,
    HeapVector<TouchEventManager::TouchInfo>& touchInfos) {
  // Iterate through the touch points, sending PointerEvents to the targets as
  // required.
  for (auto touchInfo : touchInfos) {
    const PlatformTouchPoint& touchPoint = touchInfo.point;
    // Do not send pointer events for stationary touches or null targetFrame
    if (touchInfo.touchNode && touchInfo.targetFrame &&
        touchPoint.state() != PlatformTouchPoint::TouchStationary &&
        !m_inCanceledStateForPointerTypeTouch) {
      FloatPoint pagePoint = touchInfo.targetFrame->view()->rootFrameToContents(
          touchInfo.point.pos());
      float scaleFactor = 1.0f / touchInfo.targetFrame->pageZoomFactor();
      FloatPoint scrollPosition(touchInfo.targetFrame->view()->scrollOffset());
      FloatPoint framePoint = pagePoint.scaledBy(scaleFactor);
      framePoint.moveBy(scrollPosition.scaledBy(-scaleFactor));
      PointerEvent* pointerEvent = m_pointerEventFactory.create(
          pointerEventNameForTouchPointState(touchPoint.state()), touchPoint,
          event.getModifiers(), touchPoint.radius().scaledBy(scaleFactor),
          framePoint,
          touchInfo.touchNode ? touchInfo.touchNode->document().domWindow()
                              : nullptr);

      WebInputEventResult result =
          sendTouchPointerEvent(touchInfo.touchNode, pointerEvent);

      // If a pointerdown has been canceled, queue the unique id to allow
      // suppressing mouse events from gesture events. For mouse events
      // fired from GestureTap & GestureLongPress (which are triggered by
      // single touches only), it is enough to queue the ids only for
      // primary pointers.
      // TODO(mustaq): What about other cases (e.g. GestureTwoFingerTap)?
      if (result != WebInputEventResult::NotHandled &&
          pointerEvent->type() == EventTypeNames::pointerdown &&
          pointerEvent->isPrimary()) {
        m_touchIdsForCanceledPointerdowns.append(event.uniqueTouchEventId());
      }
    }
  }
}

WebInputEventResult PointerEventManager::sendTouchPointerEvent(
    EventTarget* target,
    PointerEvent* pointerEvent) {
  if (m_inCanceledStateForPointerTypeTouch)
    return WebInputEventResult::NotHandled;

  processCaptureAndPositionOfPointerEvent(pointerEvent, target);

  // Setting the implicit capture for touch
  if (!RuntimeEnabledFeatures::pointerEventV1SpecCapturingEnabled() &&
      pointerEvent->type() == EventTypeNames::pointerdown)
    setPointerCapture(pointerEvent->pointerId(), target);

  WebInputEventResult result = dispatchPointerEvent(
      getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId()),
      pointerEvent);

  if (pointerEvent->type() == EventTypeNames::pointerup ||
      pointerEvent->type() == EventTypeNames::pointercancel) {
    releasePointerCapture(pointerEvent->pointerId());

    // Sending the leave/out events and lostpointercapture because the next
    // touch event will have a different id.
    processCaptureAndPositionOfPointerEvent(pointerEvent, nullptr);

    removePointer(pointerEvent);
  }

  return result;
}

WebInputEventResult PointerEventManager::sendMousePointerEvent(
    Node* target,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent) {
  PointerEvent* pointerEvent = m_pointerEventFactory.create(
      mouseEventType, mouseEvent, m_frame->document()->domWindow());

  // This is for when the mouse is released outside of the page.
  if (pointerEvent->type() == EventTypeNames::pointermove &&
      !pointerEvent->buttons()) {
    releasePointerCapture(pointerEvent->pointerId());
    // Send got/lostpointercapture rightaway if necessary.
    processPendingPointerCapture(pointerEvent);

    if (pointerEvent->isPrimary()) {
      m_preventMouseEventForPointerType[toPointerTypeIndex(
          mouseEvent.pointerProperties().pointerType)] = false;
    }
  }

  EventTarget* pointerEventTarget = processCaptureAndPositionOfPointerEvent(
      pointerEvent, target, mouseEvent, true);

  EventTarget* effectiveTarget = getEffectiveTargetForPointerEvent(
      pointerEventTarget, pointerEvent->pointerId());

  WebInputEventResult result =
      dispatchPointerEvent(effectiveTarget, pointerEvent);

  if (result != WebInputEventResult::NotHandled &&
      pointerEvent->type() == EventTypeNames::pointerdown &&
      pointerEvent->isPrimary()) {
    m_preventMouseEventForPointerType[toPointerTypeIndex(
        mouseEvent.pointerProperties().pointerType)] = true;
  }

  if (pointerEvent->isPrimary() &&
      !m_preventMouseEventForPointerType[toPointerTypeIndex(
          mouseEvent.pointerProperties().pointerType)]) {
    EventTarget* mouseTarget = effectiveTarget;
    // Event path could be null if pointer event is not dispatched and
    // that happens for example when pointer event feature is not enabled.
    if (!isInDocument(mouseTarget) && pointerEvent->hasEventPath()) {
      for (size_t i = 0; i < pointerEvent->eventPath().size(); i++) {
        if (isInDocument(pointerEvent->eventPath()[i].node())) {
          mouseTarget = pointerEvent->eventPath()[i].node();
          break;
        }
      }
    }
    result = EventHandlingUtil::mergeEventResult(
        result, m_mouseEventManager->dispatchMouseEvent(
                    mouseTarget, mouseEventType, mouseEvent, nullptr));
  }

  if (pointerEvent->type() == EventTypeNames::pointerup ||
      pointerEvent->type() == EventTypeNames::pointercancel) {
    releasePointerCapture(pointerEvent->pointerId());
    // Send got/lostpointercapture rightaway if necessary.
    processPendingPointerCapture(pointerEvent);

    if (pointerEvent->isPrimary()) {
      m_preventMouseEventForPointerType[toPointerTypeIndex(
          mouseEvent.pointerProperties().pointerType)] = false;
    }
  }

  return result;
}

bool PointerEventManager::getPointerCaptureState(
    int pointerId,
    EventTarget** pointerCaptureTarget,
    EventTarget** pendingPointerCaptureTarget) {
  PointerCapturingMap::const_iterator it;

  it = m_pointerCaptureTarget.find(pointerId);
  EventTarget* pointerCaptureTargetTemp =
      (it != m_pointerCaptureTarget.end()) ? it->value : nullptr;
  it = m_pendingPointerCaptureTarget.find(pointerId);
  EventTarget* pendingPointercaptureTargetTemp =
      (it != m_pendingPointerCaptureTarget.end()) ? it->value : nullptr;

  if (pointerCaptureTarget)
    *pointerCaptureTarget = pointerCaptureTargetTemp;
  if (pendingPointerCaptureTarget)
    *pendingPointerCaptureTarget = pendingPointercaptureTargetTemp;

  return pointerCaptureTargetTemp != pendingPointercaptureTargetTemp;
}

EventTarget* PointerEventManager::processCaptureAndPositionOfPointerEvent(
    PointerEvent* pointerEvent,
    EventTarget* hitTestTarget,
    const PlatformMouseEvent& mouseEvent,
    bool sendMouseEvent) {
  processPendingPointerCapture(pointerEvent);

  if (!RuntimeEnabledFeatures::pointerEventV1SpecCapturingEnabled()) {
    PointerCapturingMap::const_iterator it =
        m_pointerCaptureTarget.find(pointerEvent->pointerId());
    if (EventTarget* pointercaptureTarget =
            (it != m_pointerCaptureTarget.end()) ? it->value : nullptr)
      hitTestTarget = pointercaptureTarget;
  }

  setNodeUnderPointer(pointerEvent, hitTestTarget);
  if (sendMouseEvent) {
    m_mouseEventManager->setNodeUnderMouse(
        hitTestTarget ? hitTestTarget->toNode() : nullptr, mouseEvent);
  }
  return hitTestTarget;
}

void PointerEventManager::processPendingPointerCapture(
    PointerEvent* pointerEvent) {
  EventTarget* pointerCaptureTarget;
  EventTarget* pendingPointerCaptureTarget;
  const int pointerId = pointerEvent->pointerId();
  const bool isCaptureChanged = getPointerCaptureState(
      pointerId, &pointerCaptureTarget, &pendingPointerCaptureTarget);

  if (!isCaptureChanged)
    return;

  // We have to check whether the pointerCaptureTarget is null or not because
  // we are checking whether it is still connected to its document or not.
  if (pointerCaptureTarget) {
    // Re-target lostpointercapture to the document when the element is
    // no longer participating in the tree.
    EventTarget* target = pointerCaptureTarget;
    if (target->toNode() && !target->toNode()->isConnected()) {
      target = target->toNode()->ownerDocument();
    }
    dispatchPointerEvent(target,
                         m_pointerEventFactory.createPointerCaptureEvent(
                             pointerEvent, EventTypeNames::lostpointercapture));
  }
  // Note that If pendingPointerCaptureTarget is null dispatchPointerEvent
  // automatically does nothing.
  dispatchPointerEvent(pendingPointerCaptureTarget,
                       m_pointerEventFactory.createPointerCaptureEvent(
                           pointerEvent, EventTypeNames::gotpointercapture));

  if (pendingPointerCaptureTarget)
    m_pointerCaptureTarget.set(pointerId, pendingPointerCaptureTarget);
  else
    m_pointerCaptureTarget.remove(pointerId);
}

void PointerEventManager::removeTargetFromPointerCapturingMapping(
    PointerCapturingMap& map,
    const EventTarget* target) {
  // We could have kept a reverse mapping to make this deletion possibly
  // faster but it adds some code complication which might not be worth of
  // the performance improvement considering there might not be a lot of
  // active pointer or pointer captures at the same time.
  PointerCapturingMap tmp = map;
  for (PointerCapturingMap::iterator it = tmp.begin(); it != tmp.end(); ++it) {
    if (it->value == target)
      map.remove(it->key);
  }
}

EventTarget* PointerEventManager::getCapturingNode(int pointerId) {
  if (m_pointerCaptureTarget.contains(pointerId))
    return m_pointerCaptureTarget.get(pointerId);
  return nullptr;
}

void PointerEventManager::removePointer(PointerEvent* pointerEvent) {
  int pointerId = pointerEvent->pointerId();
  if (m_pointerEventFactory.remove(pointerId)) {
    m_pendingPointerCaptureTarget.remove(pointerId);
    m_pointerCaptureTarget.remove(pointerId);
    m_nodeUnderPointer.remove(pointerId);
  }
}

void PointerEventManager::elementRemoved(EventTarget* target) {
  removeTargetFromPointerCapturingMapping(m_pendingPointerCaptureTarget,
                                          target);
}

void PointerEventManager::setPointerCapture(int pointerId,
                                            EventTarget* target) {
  UseCounter::count(m_frame->document(), UseCounter::PointerEventSetCapture);
  if (m_pointerEventFactory.isActiveButtonsState(pointerId))
    m_pendingPointerCaptureTarget.set(pointerId, target);
}

void PointerEventManager::releasePointerCapture(int pointerId,
                                                EventTarget* target) {
  // Only the element that is going to get the next pointer event can release
  // the capture. Note that this might be different from
  // |m_pointercaptureTarget|. |m_pointercaptureTarget| holds the element
  // that had the capture until now and has been receiving the pointerevents
  // but |m_pendingPointerCaptureTarget| indicated the element that gets the
  // very next pointer event. They will be the same if there was no change in
  // capturing of a particular |pointerId|. See crbug.com/614481.
  if (m_pendingPointerCaptureTarget.get(pointerId) == target)
    releasePointerCapture(pointerId);
}

bool PointerEventManager::hasPointerCapture(int pointerId,
                                            const EventTarget* target) const {
  return m_pendingPointerCaptureTarget.get(pointerId) == target;
}

bool PointerEventManager::hasProcessedPointerCapture(
    int pointerId,
    const EventTarget* target) const {
  return m_pointerCaptureTarget.get(pointerId) == target;
}

void PointerEventManager::releasePointerCapture(int pointerId) {
  m_pendingPointerCaptureTarget.remove(pointerId);
}

bool PointerEventManager::isActive(const int pointerId) const {
  return m_pointerEventFactory.isActive(pointerId);
}

// This function checks the type of the pointer event to be touch as touch
// pointer events are the only ones that are directly dispatched from the main
// page managers to their target (event if target is in an iframe) and only
// those managers will keep track of these pointer events.
bool PointerEventManager::isTouchPointerIdActiveOnFrame(
    int pointerId,
    LocalFrame* frame) const {
  if (m_pointerEventFactory.getPointerType(pointerId) !=
      WebPointerProperties::PointerType::Touch)
    return false;
  Node* lastNodeReceivingEvent =
      m_nodeUnderPointer.contains(pointerId)
          ? m_nodeUnderPointer.get(pointerId).target->toNode()
          : nullptr;
  return lastNodeReceivingEvent &&
         lastNodeReceivingEvent->document().frame() == frame;
}

bool PointerEventManager::isAnyTouchActive() const {
  return m_touchEventManager->isAnyTouchActive();
}

bool PointerEventManager::primaryPointerdownCanceled(
    uint32_t uniqueTouchEventId) {
  // It's safe to assume that uniqueTouchEventIds won't wrap back to 0 from
  // 2^32-1 (>4.2 billion): even with a generous 100 unique ids per touch
  // sequence & one sequence per 10 second, it takes 13+ years to wrap back.
  while (!m_touchIdsForCanceledPointerdowns.isEmpty()) {
    uint32_t firstId = m_touchIdsForCanceledPointerdowns.first();
    if (firstId > uniqueTouchEventId)
      return false;
    m_touchIdsForCanceledPointerdowns.takeFirst();
    if (firstId == uniqueTouchEventId)
      return true;
  }
  return false;
}

EventTarget* PointerEventManager::getMouseCapturingNode() {
  return getCapturingNode(PointerEventFactory::s_mouseId);
}

}  // namespace blink
