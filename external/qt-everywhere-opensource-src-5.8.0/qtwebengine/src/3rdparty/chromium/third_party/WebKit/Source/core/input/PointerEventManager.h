// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEventManager_h
#define PointerEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEvent.h"
#include "core/events/PointerEventFactory.h"
#include "core/input/TouchEventManager.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebPointerProperties.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {

class LocalFrame;

// This class takes care of dispatching all pointer events and keeps track of
// properties of active pointer events.
class CORE_EXPORT PointerEventManager {
    WTF_MAKE_NONCOPYABLE(PointerEventManager);
    DISALLOW_NEW();
public:
    explicit PointerEventManager(LocalFrame*);
    ~PointerEventManager();
    DECLARE_TRACE();

    WebInputEventResult sendMousePointerEvent(
        Node*, const AtomicString& type,
        int clickCount, const PlatformMouseEvent&,
        Node* relatedTarget,
        Node* lastNodeUnderMouse);

    WebInputEventResult handleTouchEvents(
        const PlatformTouchEvent&);

    // Sends boundary events mouseout/leave/over/enter to the
    // corresponding targets. This function sends pointerout/leave/over/enter
    // only when isFrameBoundaryTransition is true which indicates the
    // transition is over the document boundary and not only the elements border
    // inside the document. If isFrameBoundaryTransition is false,
    // then the event is a compatibility event like those created by touch
    // and in that case the corresponding pointer events will be handled by
    // sendTouchPointerEvent for example and there is no need to send pointer
    // boundary events. Note that normal mouse events (e.g. mousemove/down/up)
    // and their corresponding boundary events will be handled altogether by
    // sendMousePointerEvent function.
    void sendMouseAndPossiblyPointerBoundaryEvents(
        Node* exitedNode,
        Node* enteredNode,
        const PlatformMouseEvent&,
        bool isFrameBoundaryTransition);

    // Resets the internal state of this object.
    void clear();

    void elementRemoved(EventTarget*);
    void setPointerCapture(int, EventTarget*);
    void releasePointerCapture(int, EventTarget*);
    bool isActive(const int) const;

    // Returns whether there is any touch on the screen.
    bool isAnyTouchActive() const;

    // Returns true if the primary pointerdown corresponding to the given
    // |uniqueTouchEventId| was canceled. Also drops stale ids from
    // |m_touchIdsForCanceledPointerdowns|.
    bool primaryPointerdownCanceled(uint32_t uniqueTouchEventId);

private:
    typedef HeapHashMap<int, Member<EventTarget>, WTF::IntHash<int>,
        WTF::UnsignedWithZeroKeyHashTraits<int>> PointerCapturingMap;
    class EventTargetAttributes {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(target);
        }
        Member<EventTarget> target;
        bool hasRecievedOverEvent;
        EventTargetAttributes()
        : target(nullptr)
        , hasRecievedOverEvent(false) {}
        EventTargetAttributes(EventTarget* target,
            bool hasRecievedOverEvent)
        : target(target)
        , hasRecievedOverEvent(hasRecievedOverEvent) {}
    };

    // Inhibits firing of touch-type PointerEvents until unblocked by unblockTouchPointers(). Also
    // sends pointercancels for existing touch-type PointerEvents.
    // See: www.w3.org/TR/pointerevents/#declaring-candidate-regions-for-default-touch-behaviors
    void blockTouchPointers();

    // Enables firing of touch-type PointerEvents after they were inhibited by blockTouchPointers().
    void unblockTouchPointers();

    // Sends touch pointer events and sets consumed bits in TouchInfo array
    // based on the return value of pointer event handlers.
    void dispatchTouchPointerEvents(
        const PlatformTouchEvent&,
        HeapVector<TouchEventManager::TouchInfo>&);

    // Returns whether the event is consumed or not.
    WebInputEventResult sendTouchPointerEvent(EventTarget*, PointerEvent*);

    void sendBoundaryEvents(
        EventTarget* exitedTarget,
        EventTarget* enteredTarget,
        PointerEvent*,
        const PlatformMouseEvent& = PlatformMouseEvent(),
        bool sendMouseEvent = false);
    void setNodeUnderPointer(PointerEvent*,
        EventTarget*, bool sendEvent = true);

    // Processes the assignment of |m_pointerCaptureTarget| from |m_pendingPointerCaptureTarget|
    // and sends the got/lostpointercapture events, as per the spec:
    // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
    // Returns whether the pointer capture is changed. When pointer capture is changed,
    // this function will take care of boundary events.
    bool processPendingPointerCapture(
        PointerEvent*,
        EventTarget*,
        const PlatformMouseEvent& = PlatformMouseEvent(),
        bool sendMouseEvent = false);

    // Processes the capture state of a pointer, updates node under
    // pointer, and sends corresponding boundary events for pointer if
    // setPointerPosition is true. It also sends corresponding boundary events
    // for mouse if sendMouseEvent is true.
    void processCaptureAndPositionOfPointerEvent(
        PointerEvent*,
        EventTarget* hitTestTarget,
        EventTarget* lastNodeUnderMouse = nullptr,
        const PlatformMouseEvent& = PlatformMouseEvent(),
        bool sendMouseEvent = false,
        bool setPointerPosition = true);

    void removeTargetFromPointerCapturingMapping(
        PointerCapturingMap&, const EventTarget*);
    EventTarget* getEffectiveTargetForPointerEvent(
        EventTarget*, int);
    EventTarget* getCapturingNode(int);
    void removePointer(PointerEvent*);
    WebInputEventResult dispatchPointerEvent(
        EventTarget*,
        PointerEvent*,
        bool checkForListener = false);
    void releasePointerCapture(int);

    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |PointerEventManager::clear()|.

    const Member<LocalFrame> m_frame;

    // Prevents firing mousedown, mousemove & mouseup in-between a canceled pointerdown and next pointerup/pointercancel.
    // See "PREVENT MOUSE EVENT flag" in the spec:
    //   https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
    bool m_preventMouseEventForPointerType[static_cast<size_t>(WebPointerProperties::PointerType::LastEntry) + 1];

    // Set upon sending a pointercancel for touch, prevents PE dispatches for touches until
    // all touch-points become inactive.
    bool m_inCanceledStateForPointerTypeTouch;

    Deque<uint32_t> m_touchIdsForCanceledPointerdowns;

    // Note that this map keeps track of node under pointer with id=1 as well
    // which might be different than m_nodeUnderMouse in EventHandler. That one
    // keeps track of any compatibility mouse event positions but this map for
    // the pointer with id=1 is only taking care of true mouse related events.
    using NodeUnderPointerMap = HeapHashMap<int, EventTargetAttributes,
        WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>>;
    NodeUnderPointerMap m_nodeUnderPointer;

    PointerCapturingMap m_pointerCaptureTarget;
    PointerCapturingMap m_pendingPointerCaptureTarget;

    PointerEventFactory m_pointerEventFactory;
    TouchEventManager m_touchEventManager;
};

} // namespace blink

#endif // PointerEventManager_h
