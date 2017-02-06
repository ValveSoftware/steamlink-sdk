// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/PointerEventManager.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandler.h"
#include "core/input/TouchActionUtil.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/PlatformTouchEvent.h"

namespace blink {

namespace {

size_t toPointerTypeIndex(WebPointerProperties::PointerType t) { return static_cast<size_t>(t); }

const AtomicString& pointerEventNameForTouchPointState(PlatformTouchPoint::TouchState state)
{
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
        ASSERT_NOT_REACHED();
        return emptyAtom;
    }
}

bool isInDocument(EventTarget* n)
{
    return n && n->toNode() && n->toNode()->inShadowIncludingDocument();
}

WebInputEventResult dispatchMouseEvent(
    EventTarget* target,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent,
    EventTarget* relatedTarget,
    int detail = 0,
    bool checkForListener = false)
{
    if (target && target->toNode()
        && (!checkForListener || target->hasEventListeners(mouseEventType))) {
        Node* targetNode = target->toNode();
        MouseEvent* event = MouseEvent::create(mouseEventType,
            targetNode->document().domWindow(), mouseEvent, detail,
            relatedTarget ? relatedTarget->toNode() : nullptr);
        DispatchEventResult dispatchResult = target->dispatchEvent(event);
        return EventHandler::toWebInputEventResult(dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

PlatformMouseEvent mouseEventWithRegion(Node* node, const PlatformMouseEvent& mouseEvent)
{
    if (!node->isElementNode())
        return mouseEvent;

    Element* element = toElement(node);
    if (!element->isInCanvasSubtree())
        return mouseEvent;

    HTMLCanvasElement* canvas = Traversal<HTMLCanvasElement>::firstAncestorOrSelf(*element);
    // In this case, the event target is canvas and mouse rerouting doesn't happen.
    if (canvas == element)
        return mouseEvent;
    String region = canvas->getIdFromControl(element);
    PlatformMouseEvent newMouseEvent = mouseEvent;
    newMouseEvent.setRegion(region);
    return newMouseEvent;
}

void buildAncestorChain(
    EventTarget* target,
    HeapVector<Member<Node>, 20>* ancestors)
{
    if (!isInDocument(target))
        return;
    Node* targetNode = target->toNode();
    DCHECK(targetNode);
    targetNode->updateDistribution();
    // Index 0 element in the ancestors arrays will be the corresponding
    // target. So the root of their document will be their last element.
    for (Node* node = targetNode; node; node = FlatTreeTraversal::parent(*node))
        ancestors->append(node);
}

void buildAncestorChainsAndFindCommonAncestors(
    EventTarget* exitedTarget, EventTarget* enteredTarget,
    HeapVector<Member<Node>, 20>* exitedAncestorsOut,
    HeapVector<Member<Node>, 20>* enteredAncestorsOut,
    size_t* exitedAncestorsCommonParentIndexOut,
    size_t* enteredAncestorsCommonParentIndexOut)
{
    DCHECK(exitedAncestorsOut);
    DCHECK(enteredAncestorsOut);
    DCHECK(exitedAncestorsCommonParentIndexOut);
    DCHECK(enteredAncestorsCommonParentIndexOut);

    buildAncestorChain(exitedTarget, exitedAncestorsOut);
    buildAncestorChain(enteredTarget, enteredAncestorsOut);

    *exitedAncestorsCommonParentIndexOut = exitedAncestorsOut->size();
    *enteredAncestorsCommonParentIndexOut = enteredAncestorsOut->size();
    while (*exitedAncestorsCommonParentIndexOut > 0
        && *enteredAncestorsCommonParentIndexOut > 0) {
        if ((*exitedAncestorsOut)[(*exitedAncestorsCommonParentIndexOut)-1]
            != (*enteredAncestorsOut)[(*enteredAncestorsCommonParentIndexOut)-1])
            break;
        (*exitedAncestorsCommonParentIndexOut)--;
        (*enteredAncestorsCommonParentIndexOut)--;
    }
}

} // namespace

WebInputEventResult PointerEventManager::dispatchPointerEvent(
    EventTarget* target,
    PointerEvent* pointerEvent,
    bool checkForListener)
{
    if (!target)
        return WebInputEventResult::NotHandled;

    // Set whether node under pointer has received pointerover or not.
    const int pointerId = pointerEvent->pointerId();
    const AtomicString& eventType = pointerEvent->type();
    if ((eventType == EventTypeNames::pointerout
        || eventType == EventTypeNames::pointerover)
        && m_nodeUnderPointer.contains(pointerId)) {
        EventTarget* targetUnderPointer =
            m_nodeUnderPointer.get(pointerId).target;
        if (targetUnderPointer == target) {
            m_nodeUnderPointer.set(pointerId, EventTargetAttributes
                (targetUnderPointer,
                eventType == EventTypeNames::pointerover));
        }
    }

    if (!RuntimeEnabledFeatures::pointerEventEnabled())
        return WebInputEventResult::NotHandled;
    if (!checkForListener || target->hasEventListeners(eventType)) {
        DispatchEventResult dispatchResult = target->dispatchEvent(pointerEvent);
        return EventHandler::toWebInputEventResult(dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

EventTarget* PointerEventManager::getEffectiveTargetForPointerEvent(
    EventTarget* target, int pointerId)
{
    if (EventTarget* capturingTarget = getCapturingNode(pointerId))
        return capturingTarget;
    return target;
}

void PointerEventManager::sendMouseAndPossiblyPointerBoundaryEvents(
    Node* exitedNode,
    Node* enteredNode,
    const PlatformMouseEvent& mouseEvent,
    bool isFrameBoundaryTransition)
{
    // Mouse event type does not matter as this pointerevent will only be used
    // to create boundary pointer events and its type will be overridden in
    // |sendBoundaryEvents| function.
    PointerEvent* dummyPointerEvent =
        m_pointerEventFactory.create(EventTypeNames::mousedown, mouseEvent,
        nullptr, m_frame->document()->domWindow());

    // TODO(crbug/545647): This state should reset with pointercancel too.
    // This function also gets called for compat mouse events of touch at this
    // stage. So if the event is not frame boundary transition it is only a
    // compatibility mouse event and we do not need to change pointer event
    // behavior regarding preventMouseEvent state in that case.
    if (isFrameBoundaryTransition && dummyPointerEvent->buttons() == 0
        && dummyPointerEvent->isPrimary()) {
        m_preventMouseEventForPointerType[toPointerTypeIndex(
            mouseEvent.pointerProperties().pointerType)] = false;
    }

    processCaptureAndPositionOfPointerEvent(dummyPointerEvent, enteredNode,
        exitedNode, mouseEvent, true, isFrameBoundaryTransition);
}

void PointerEventManager::sendBoundaryEvents(
    EventTarget* exitedTarget,
    EventTarget* enteredTarget,
    PointerEvent* pointerEvent,
    const PlatformMouseEvent& mouseEvent, bool sendMouseEvent)
{
    if (exitedTarget == enteredTarget)
        return;

    if (EventTarget* capturingTarget = getCapturingNode(pointerEvent->pointerId())) {
        if (capturingTarget == exitedTarget)
            enteredTarget = nullptr;
        else if (capturingTarget == enteredTarget)
            exitedTarget = nullptr;
        else
            return;
    }

    // Dispatch pointerout/mouseout events
    if (isInDocument(exitedTarget)) {
        if (!sendMouseEvent) {
            dispatchPointerEvent(exitedTarget, m_pointerEventFactory.createPointerBoundaryEvent(
                pointerEvent, EventTypeNames::pointerout, enteredTarget));
        } else {
            dispatchMouseEvent(exitedTarget,
                EventTypeNames::mouseout,
                mouseEventWithRegion(exitedTarget->toNode(), mouseEvent),
                enteredTarget);
        }
    }

    // Create lists of all exited/entered ancestors, locate the common ancestor
    // Based on httparchive, in more than 97% cases the depth of DOM is less
    // than 20.
    HeapVector<Member<Node>, 20> exitedAncestors;
    HeapVector<Member<Node>, 20> enteredAncestors;
    size_t exitedAncestorsCommonParentIndex = 0;
    size_t enteredAncestorsCommonParentIndex = 0;

    // A note on mouseenter and mouseleave: These are non-bubbling events, and they are dispatched if there
    // is a capturing event handler on an ancestor or a normal event handler on the element itself. This special
    // handling is necessary to avoid O(n^2) capturing event handler checks.
    //
    //   Note, however, that this optimization can possibly cause some unanswered/missing/redundant mouseenter or
    // mouseleave events in certain contrived eventhandling scenarios, e.g., when:
    // - the mouseleave handler for a node sets the only capturing-mouseleave-listener in its ancestor, or
    // - DOM mods in any mouseenter/mouseleave handler changes the common ancestor of exited & entered nodes, etc.
    // We think the spec specifies a "frozen" state to avoid such corner cases (check the discussion on "candidate event
    // listeners" at http://www.w3.org/TR/uievents), but our code below preserves one such behavior from past only to
    // match Firefox and IE behavior.
    //
    // TODO(mustaq): Confirm spec conformance, double-check with other browsers.

    buildAncestorChainsAndFindCommonAncestors(
        exitedTarget, enteredTarget,
        &exitedAncestors, &enteredAncestors,
        &exitedAncestorsCommonParentIndex, &enteredAncestorsCommonParentIndex);

    bool exitedNodeHasCapturingAncestor = false;
    for (size_t j = 0; j < exitedAncestors.size(); j++) {
        if (exitedAncestors[j]->hasCapturingEventListeners(EventTypeNames::mouseleave)
            || (RuntimeEnabledFeatures::pointerEventEnabled()
            && exitedAncestors[j]->hasCapturingEventListeners(EventTypeNames::pointerleave)))
            exitedNodeHasCapturingAncestor = true;
    }

    // Dispatch pointerleave/mouseleave events, in child-to-parent order.
    for (size_t j = 0; j < exitedAncestorsCommonParentIndex; j++) {
        if (!sendMouseEvent) {
            dispatchPointerEvent(exitedAncestors[j].get(),
                m_pointerEventFactory.createPointerBoundaryEvent(
                    pointerEvent, EventTypeNames::pointerleave, enteredTarget),
                !exitedNodeHasCapturingAncestor);
        } else {
            dispatchMouseEvent(exitedAncestors[j].get(),
                EventTypeNames::mouseleave,
                mouseEventWithRegion(exitedTarget->toNode(), mouseEvent),
                enteredTarget, 0, !exitedNodeHasCapturingAncestor);
        }
    }

    // Dispatch pointerover/mouseover.
    if (isInDocument(enteredTarget)) {
        if (!sendMouseEvent) {
            dispatchPointerEvent(enteredTarget, m_pointerEventFactory.createPointerBoundaryEvent(
                pointerEvent, EventTypeNames::pointerover, exitedTarget));
        } else {
            dispatchMouseEvent(enteredTarget,
                EventTypeNames::mouseover, mouseEvent, exitedTarget);
        }
    }

    // Defer locating capturing pointeenter/mouseenter listener until /after/ dispatching the leave events because
    // the leave handlers might set a capturing enter handler.
    bool enteredNodeHasCapturingAncestor = false;
    for (size_t i = 0; i < enteredAncestors.size(); i++) {
        if (enteredAncestors[i]->hasCapturingEventListeners(EventTypeNames::mouseenter)
            || (RuntimeEnabledFeatures::pointerEventEnabled()
            && enteredAncestors[i]->hasCapturingEventListeners(EventTypeNames::pointerenter)))
            enteredNodeHasCapturingAncestor = true;
    }

    // Dispatch pointerenter/mouseenter events, in parent-to-child order.
    for (size_t i = enteredAncestorsCommonParentIndex; i > 0; i--) {
        if (!sendMouseEvent) {
            dispatchPointerEvent(enteredAncestors[i-1].get(),
                m_pointerEventFactory.createPointerBoundaryEvent(
                    pointerEvent, EventTypeNames::pointerenter, exitedTarget),
                !enteredNodeHasCapturingAncestor);
        } else {
            dispatchMouseEvent(enteredAncestors[i-1].get(),
                EventTypeNames::mouseenter, mouseEvent, exitedTarget,
                0, !enteredNodeHasCapturingAncestor);
        }
    }
}

void PointerEventManager::setNodeUnderPointer(
    PointerEvent* pointerEvent,
    EventTarget* target,
    bool sendEvent)
{
    if (m_nodeUnderPointer.contains(pointerEvent->pointerId())) {
        EventTargetAttributes node = m_nodeUnderPointer.get(
            pointerEvent->pointerId());
        if (!target) {
            m_nodeUnderPointer.remove(pointerEvent->pointerId());
        } else if (target
            != m_nodeUnderPointer.get(pointerEvent->pointerId()).target) {
            m_nodeUnderPointer.set(pointerEvent->pointerId(),
                EventTargetAttributes(target, false));
        }
        if (sendEvent)
            sendBoundaryEvents(node.target, target, pointerEvent);
    } else if (target) {
        m_nodeUnderPointer.add(pointerEvent->pointerId(),
            EventTargetAttributes(target, false));
        if (sendEvent)
            sendBoundaryEvents(nullptr, target, pointerEvent);
    }
}

void PointerEventManager::blockTouchPointers()
{
    if (m_inCanceledStateForPointerTypeTouch)
        return;
    m_inCanceledStateForPointerTypeTouch = true;

    Vector<int> touchPointerIds
        = m_pointerEventFactory.getPointerIdsOfType(WebPointerProperties::PointerType::Touch);

    for (int pointerId : touchPointerIds) {
        PointerEvent* pointerEvent
            = m_pointerEventFactory.createPointerCancelEvent(
                pointerId, WebPointerProperties::PointerType::Touch);

        ASSERT(m_nodeUnderPointer.contains(pointerId));
        EventTarget* target = m_nodeUnderPointer.get(pointerId).target;

        processCaptureAndPositionOfPointerEvent(pointerEvent, target);

        // TODO(nzolghadr): This event follows implicit TE capture. The actual target
        // would depend on PE capturing. Perhaps need to split TE/PE event path upstream?
        // crbug.com/579553.
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

void PointerEventManager::unblockTouchPointers()
{
    m_inCanceledStateForPointerTypeTouch = false;
}

WebInputEventResult PointerEventManager::handleTouchEvents(
    const PlatformTouchEvent& event)
{

    if (event.type() == PlatformEvent::TouchScrollStarted) {
        blockTouchPointers();
        m_touchEventManager.setTouchScrollStarted();
        return WebInputEventResult::HandledSystem;
    }

    bool newTouchSequence = true;
    for (const auto &touchPoint : event.touchPoints()) {
        if (touchPoint.state() != PlatformTouchPoint::TouchPressed) {
            newTouchSequence = false;
            break;
        }
    }
    if (newTouchSequence)
        unblockTouchPointers();
    HeapVector<TouchEventManager::TouchInfo> touchInfos;

    dispatchTouchPointerEvents(event, touchInfos);

    return m_touchEventManager.handleTouchEvent(event, touchInfos);
}

void PointerEventManager::dispatchTouchPointerEvents(
    const PlatformTouchEvent& event,
    HeapVector<TouchEventManager::TouchInfo>& touchInfos)
{
    // Iterate through the touch points, sending PointerEvents to the targets as required.
    for (const auto& touchPoint : event.touchPoints()) {
        TouchEventManager::TouchInfo touchInfo;
        touchInfo.point = touchPoint;

        int pointerId = m_pointerEventFactory.getPointerEventId(
            touchPoint.pointerProperties());
        // Do the hit test either when the touch first starts or when the touch
        // is not captured. |m_pendingPointerCaptureTarget| indicates the target
        // that will be capturing this event. |m_pointerCaptureTarget| may not
        // have this target yet since the processing of that will be done right
        // before firing the event.
        if (touchInfo.point.state() == PlatformTouchPoint::TouchPressed
            || !m_pendingPointerCaptureTarget.contains(pointerId)) {
            HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent | HitTestRequest::ReadOnly | HitTestRequest::Active;
            LayoutPoint pagePoint = roundedLayoutPoint(m_frame->view()->rootFrameToContents(touchInfo.point.pos()));
            HitTestResult hitTestTesult = m_frame->eventHandler().hitTestResultAtPoint(pagePoint, hitType);
            Node* node = hitTestTesult.innerNode();
            if (node) {
                touchInfo.targetFrame = node->document().frame();
                if (isHTMLCanvasElement(node)) {
                    std::pair<Element*, String> regionInfo = toHTMLCanvasElement(node)->getControlAndIdIfHitRegionExists(hitTestTesult.pointInInnerNodeFrame());
                    if (regionInfo.first)
                        node = regionInfo.first;
                    touchInfo.region = regionInfo.second;
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
            touchInfo.touchNode = m_pendingPointerCaptureTarget
                .get(pointerId)->toNode();
            touchInfo.targetFrame = touchInfo.touchNode->document().frame();
        }

        // Do not send pointer events for stationary touches or null targetFrame
        if (touchInfo.touchNode && touchInfo.targetFrame
            && touchPoint.state() != PlatformTouchPoint::TouchStationary
            && !m_inCanceledStateForPointerTypeTouch) {
            FloatPoint pagePoint = touchInfo.targetFrame->view()
                ->rootFrameToContents(touchInfo.point.pos());
            float scaleFactor = 1.0f / touchInfo.targetFrame->pageZoomFactor();
            FloatPoint scrollPosition = touchInfo.targetFrame->view()->scrollPosition();
            FloatPoint framePoint = pagePoint.scaledBy(scaleFactor);
            framePoint.moveBy(scrollPosition.scaledBy(-scaleFactor));
            PointerEvent* pointerEvent = m_pointerEventFactory.create(
                pointerEventNameForTouchPointState(touchPoint.state()),
                touchPoint, event.getModifiers(),
                touchPoint.radius().scaledBy(scaleFactor),
                framePoint,
                touchInfo.touchNode ?
                    touchInfo.touchNode->document().domWindow() : nullptr);

            WebInputEventResult result = sendTouchPointerEvent(touchInfo.touchNode, pointerEvent);

            // If a pointerdown has been canceled, queue the unique id to allow
            // suppressing mouse events from gesture events. For mouse events
            // fired from GestureTap & GestureLongPress (which are triggered by
            // single touches only), it is enough to queue the ids only for
            // primary pointers.
            // TODO(mustaq): What about other cases (e.g. GestureTwoFingerTap)?
            if (result != WebInputEventResult::NotHandled
                && pointerEvent->type() == EventTypeNames::pointerdown
                && pointerEvent->isPrimary()) {
                m_touchIdsForCanceledPointerdowns.append(event.uniqueTouchEventId());
            }
        }

        touchInfos.append(touchInfo);
    }
}

WebInputEventResult PointerEventManager::sendTouchPointerEvent(
    EventTarget* target, PointerEvent* pointerEvent)
{
    if (m_inCanceledStateForPointerTypeTouch)
        return WebInputEventResult::NotHandled;

    processCaptureAndPositionOfPointerEvent(pointerEvent, target);

    // Setting the implicit capture for touch
    if (pointerEvent->type() == EventTypeNames::pointerdown)
        setPointerCapture(pointerEvent->pointerId(), target);

    WebInputEventResult result = dispatchPointerEvent(
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId()),
        pointerEvent);

    if (pointerEvent->type() == EventTypeNames::pointerup
        || pointerEvent->type() == EventTypeNames::pointercancel) {
        releasePointerCapture(pointerEvent->pointerId());

        // Sending the leave/out events and lostpointercapture
        // because the next touch event will have a different id. So delayed
        // sending of lostpointercapture won't work here.
        processCaptureAndPositionOfPointerEvent(pointerEvent, nullptr);

        removePointer(pointerEvent);
    }

    return result;
}

WebInputEventResult PointerEventManager::sendMousePointerEvent(
    Node* target, const AtomicString& mouseEventType,
    int clickCount, const PlatformMouseEvent& mouseEvent,
    Node* relatedTarget,
    Node* lastNodeUnderMouse)
{
    PointerEvent* pointerEvent =
        m_pointerEventFactory.create(mouseEventType, mouseEvent,
        relatedTarget, m_frame->document()->domWindow());

    // This is for when the mouse is released outside of the page.
    if (pointerEvent->type() == EventTypeNames::pointermove
        && !pointerEvent->buttons()
        && pointerEvent->isPrimary()) {
        m_preventMouseEventForPointerType[toPointerTypeIndex(
            mouseEvent.pointerProperties().pointerType)] = false;
    }

    processCaptureAndPositionOfPointerEvent(pointerEvent, target,
        lastNodeUnderMouse, mouseEvent, true, true);

    EventTarget* effectiveTarget =
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId());

    WebInputEventResult result =
        dispatchPointerEvent(effectiveTarget, pointerEvent);

    if (result != WebInputEventResult::NotHandled
        && pointerEvent->type() == EventTypeNames::pointerdown
        && pointerEvent->isPrimary()) {
        m_preventMouseEventForPointerType[toPointerTypeIndex(
            mouseEvent.pointerProperties().pointerType)] = true;
    }

    if (pointerEvent->isPrimary() && !m_preventMouseEventForPointerType[toPointerTypeIndex(
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
        result = EventHandler::mergeEventResult(result,
            dispatchMouseEvent(mouseTarget, mouseEventType, mouseEvent,
            nullptr, clickCount));
    }

    if (pointerEvent->buttons() == 0) {
        releasePointerCapture(pointerEvent->pointerId());
        if (pointerEvent->isPrimary()) {
            m_preventMouseEventForPointerType[toPointerTypeIndex(
                mouseEvent.pointerProperties().pointerType)] = false;
        }
    }

    return result;
}

PointerEventManager::PointerEventManager(LocalFrame* frame)
: m_frame(frame)
, m_touchEventManager(frame)
{
    clear();
}

PointerEventManager::~PointerEventManager()
{
}

void PointerEventManager::clear()
{
    for (auto& entry : m_preventMouseEventForPointerType)
        entry = false;
    m_touchEventManager.clear();
    m_inCanceledStateForPointerTypeTouch = false;
    m_pointerEventFactory.clear();
    m_touchIdsForCanceledPointerdowns.clear();
    m_nodeUnderPointer.clear();
    m_pointerCaptureTarget.clear();
    m_pendingPointerCaptureTarget.clear();
}

void PointerEventManager::processCaptureAndPositionOfPointerEvent(
    PointerEvent* pointerEvent,
    EventTarget* hitTestTarget,
    EventTarget* lastNodeUnderMouse,
    const PlatformMouseEvent& mouseEvent,
    bool sendMouseEvent, bool setPointerPosition)
{
    bool isCaptureChanged = false;

    if (setPointerPosition) {
        isCaptureChanged = processPendingPointerCapture(pointerEvent,
            hitTestTarget, mouseEvent, sendMouseEvent);
        // If there was a change in capturing processPendingPointerCapture has
        // already taken care of transition events. So we don't need to send the
        // transition events here.
        setNodeUnderPointer(pointerEvent, hitTestTarget, !isCaptureChanged);
    }
    if (sendMouseEvent && !isCaptureChanged) {
        // lastNodeUnderMouse is needed here because it is still stored in EventHandler.
        sendBoundaryEvents(lastNodeUnderMouse, hitTestTarget,
            pointerEvent, mouseEvent, true);
    }
}

bool PointerEventManager::processPendingPointerCapture(
    PointerEvent* pointerEvent,
    EventTarget* hitTestTarget,
    const PlatformMouseEvent& mouseEvent, bool sendMouseEvent)
{
    int pointerId = pointerEvent->pointerId();
    EventTarget* pointerCaptureTarget =
        m_pointerCaptureTarget.contains(pointerId)
        ? m_pointerCaptureTarget.get(pointerId) : nullptr;
    EventTarget* pendingPointerCaptureTarget =
        m_pendingPointerCaptureTarget.contains(pointerId)
        ? m_pendingPointerCaptureTarget.get(pointerId) : nullptr;
    const EventTargetAttributes &nodeUnderPointerAtt =
        m_nodeUnderPointer.contains(pointerId)
        ? m_nodeUnderPointer.get(pointerId) : EventTargetAttributes();
    const bool isCaptureChanged =
        pointerCaptureTarget != pendingPointerCaptureTarget;

    if (isCaptureChanged) {
        if ((hitTestTarget != nodeUnderPointerAtt.target
            || (pendingPointerCaptureTarget
            && pendingPointerCaptureTarget != nodeUnderPointerAtt.target))
            && nodeUnderPointerAtt.hasRecievedOverEvent) {
            if (sendMouseEvent) {
                // Send pointer event transitions as the line after this if
                // block sends the mouse events
                sendBoundaryEvents(nodeUnderPointerAtt.target, nullptr,
                    pointerEvent);
            }
            sendBoundaryEvents(nodeUnderPointerAtt.target, nullptr,
                pointerEvent, mouseEvent, sendMouseEvent);
        }
        if (pointerCaptureTarget) {
            // Re-target lostpointercapture to the document when the element is
            // no longer participating in the tree.
            EventTarget* target = pointerCaptureTarget;
            if (target->toNode()
                && !target->toNode()->inShadowIncludingDocument()) {
                target = target->toNode()->ownerDocument();
            }
            dispatchPointerEvent(target,
                m_pointerEventFactory.createPointerCaptureEvent(
                pointerEvent, EventTypeNames::lostpointercapture));
        }
    }

    // Set pointerCaptureTarget from pendingPointerCaptureTarget. This does
    // affect the behavior of sendBoundaryEvents function. So the
    // ordering of the surrounding blocks of code for sending transition events
    // are important.
    if (pendingPointerCaptureTarget)
        m_pointerCaptureTarget.set(pointerId, pendingPointerCaptureTarget);
    else
        m_pointerCaptureTarget.remove(pointerId);

    if (isCaptureChanged) {
        dispatchPointerEvent(pendingPointerCaptureTarget,
            m_pointerEventFactory.createPointerCaptureEvent(
            pointerEvent, EventTypeNames::gotpointercapture));
        if ((pendingPointerCaptureTarget == hitTestTarget
            || !pendingPointerCaptureTarget)
            && (nodeUnderPointerAtt.target != hitTestTarget
            || !nodeUnderPointerAtt.hasRecievedOverEvent)) {
            if (sendMouseEvent) {
                // Send pointer event transitions as the line after this if
                // block sends the mouse events
                sendBoundaryEvents(nullptr, hitTestTarget, pointerEvent);
            }
            sendBoundaryEvents(nullptr, hitTestTarget, pointerEvent,
                mouseEvent, sendMouseEvent);
        }
    }
    return isCaptureChanged;
}

void PointerEventManager::removeTargetFromPointerCapturingMapping(
    PointerCapturingMap& map, const EventTarget* target)
{
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

EventTarget* PointerEventManager::getCapturingNode(int pointerId)
{
    if (m_pointerCaptureTarget.contains(pointerId))
        return m_pointerCaptureTarget.get(pointerId);
    return nullptr;
}

void PointerEventManager::removePointer(
    PointerEvent* pointerEvent)
{
    int pointerId = pointerEvent->pointerId();
    if (m_pointerEventFactory.remove(pointerId)) {
        m_pendingPointerCaptureTarget.remove(pointerId);
        m_pointerCaptureTarget.remove(pointerId);
        m_nodeUnderPointer.remove(pointerId);
    }
}

void PointerEventManager::elementRemoved(EventTarget* target)
{
    removeTargetFromPointerCapturingMapping(m_pendingPointerCaptureTarget, target);
}

void PointerEventManager::setPointerCapture(int pointerId, EventTarget* target)
{
    if (m_pointerEventFactory.isActiveButtonsState(pointerId))
        m_pendingPointerCaptureTarget.set(pointerId, target);
}

void PointerEventManager::releasePointerCapture(int pointerId, EventTarget* target)
{
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

void PointerEventManager::releasePointerCapture(int pointerId)
{
    m_pendingPointerCaptureTarget.remove(pointerId);
}

bool PointerEventManager::isActive(const int pointerId) const
{
    return m_pointerEventFactory.isActive(pointerId);
}

bool PointerEventManager::isAnyTouchActive() const
{
    return m_touchEventManager.isAnyTouchActive();
}

bool PointerEventManager::primaryPointerdownCanceled(uint32_t uniqueTouchEventId)
{
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

DEFINE_TRACE(PointerEventManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_nodeUnderPointer);
    visitor->trace(m_pointerCaptureTarget);
    visitor->trace(m_pendingPointerCaptureTarget);
    visitor->trace(m_touchEventManager);
}


} // namespace blink
