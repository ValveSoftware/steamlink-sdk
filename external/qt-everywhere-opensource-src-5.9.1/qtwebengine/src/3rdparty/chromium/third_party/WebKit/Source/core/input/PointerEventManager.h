// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEventManager_h
#define PointerEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEvent.h"
#include "core/events/PointerEventFactory.h"
#include "core/input/BoundaryEventDispatcher.h"
#include "core/input/TouchEventManager.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebPointerProperties.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {

class LocalFrame;
class MouseEventManager;

// This class takes care of dispatching all pointer events and keeps track of
// properties of active pointer events.
class CORE_EXPORT PointerEventManager
    : public GarbageCollectedFinalized<PointerEventManager> {
  WTF_MAKE_NONCOPYABLE(PointerEventManager);

 public:
  PointerEventManager(LocalFrame*, MouseEventManager*);
  DECLARE_TRACE();

  // Sends the mouse pointer events and the boundary events
  // that it may cause. It also sends the compat mouse events
  // and sets the newNodeUnderMouse if the capturing is set
  // in this function.
  WebInputEventResult sendMousePointerEvent(Node* target,
                                            const AtomicString& type,
                                            const PlatformMouseEvent&);

  WebInputEventResult handleTouchEvents(const PlatformTouchEvent&);

  // Sends boundary events pointerout/leave/over/enter and
  // mouseout/leave/over/enter to the corresponding targets.
  // inside the document. This functions handles the cases that pointer is
  // leaving a frame. Note that normal mouse events (e.g. mousemove/down/up)
  // and their corresponding boundary events will be handled altogether by
  // sendMousePointerEvent function.
  void sendMouseAndPointerBoundaryEvents(Node* enteredNode,
                                         const PlatformMouseEvent&);

  // Resets the internal state of this object.
  void clear();

  void elementRemoved(EventTarget*);

  void setPointerCapture(int, EventTarget*);
  void releasePointerCapture(int, EventTarget*);

  // See Element::hasPointerCapture(int).
  bool hasPointerCapture(int, const EventTarget*) const;

  // See Element::hasProcessedPointerCapture(int).
  bool hasProcessedPointerCapture(int, const EventTarget*) const;

  bool isActive(const int) const;

  // Returns whether there is any touch on the screen.
  bool isAnyTouchActive() const;

  // Returns whether pointerId is for an active touch pointerevent and whether
  // the last event was sent to the given frame.
  bool isTouchPointerIdActiveOnFrame(int, LocalFrame*) const;

  // TODO(crbug.com/625843): This can be hidden when mouse refactoring in
  // EventHandler is done.
  EventTarget* getMouseCapturingNode();

  // Returns true if the primary pointerdown corresponding to the given
  // |uniqueTouchEventId| was canceled. Also drops stale ids from
  // |m_touchIdsForCanceledPointerdowns|.
  bool primaryPointerdownCanceled(uint32_t uniqueTouchEventId);

 private:
  typedef HeapHashMap<int,
                      Member<EventTarget>,
                      WTF::IntHash<int>,
                      WTF::UnsignedWithZeroKeyHashTraits<int>>
      PointerCapturingMap;
  class EventTargetAttributes {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    DEFINE_INLINE_TRACE() { visitor->trace(target); }
    Member<EventTarget> target;
    bool hasRecievedOverEvent;
    EventTargetAttributes() : target(nullptr), hasRecievedOverEvent(false) {}
    EventTargetAttributes(EventTarget* target, bool hasRecievedOverEvent)
        : target(target), hasRecievedOverEvent(hasRecievedOverEvent) {}
  };

  class PointerEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
    WTF_MAKE_NONCOPYABLE(PointerEventBoundaryEventDispatcher);

   public:
    PointerEventBoundaryEventDispatcher(PointerEventManager*, PointerEvent*);

   protected:
    void dispatchOut(EventTarget*, EventTarget* relatedTarget) override;
    void dispatchOver(EventTarget*, EventTarget* relatedTarget) override;
    void dispatchLeave(EventTarget*,
                       EventTarget* relatedTarget,
                       bool checkForListener) override;
    void dispatchEnter(EventTarget*,
                       EventTarget* relatedTarget,
                       bool checkForListener) override;
    AtomicString getLeaveEvent() override;
    AtomicString getEnterEvent() override;

   private:
    void dispatch(EventTarget*,
                  EventTarget* relatedTarget,
                  const AtomicString&,
                  bool checkForListener);
    Member<PointerEventManager> m_pointerEventManager;
    Member<PointerEvent> m_pointerEvent;
  };

  // Inhibits firing of touch-type PointerEvents until unblocked by
  // unblockTouchPointers(). Also sends pointercancels for existing touch-type
  // PointerEvents.  See:
  // www.w3.org/TR/pointerevents/#declaring-candidate-regions-for-default-touch-behaviors
  void blockTouchPointers();

  // Enables firing of touch-type PointerEvents after they were inhibited by
  // blockTouchPointers().
  void unblockTouchPointers();

  // Generate the TouchInfos for a PlatformTouchEvent, hit-testing as necessary.
  void computeTouchTargets(const PlatformTouchEvent&,
                           HeapVector<TouchEventManager::TouchInfo>&);

  // Sends touch pointer events and sets consumed bits in TouchInfo array
  // based on the return value of pointer event handlers.
  void dispatchTouchPointerEvents(const PlatformTouchEvent&,
                                  HeapVector<TouchEventManager::TouchInfo>&);

  // Returns whether the event is consumed or not.
  WebInputEventResult sendTouchPointerEvent(EventTarget*, PointerEvent*);

  void sendBoundaryEvents(EventTarget* exitedTarget,
                          EventTarget* enteredTarget,
                          PointerEvent*);
  void setNodeUnderPointer(PointerEvent*, EventTarget*);

  // Processes the assignment of |m_pointerCaptureTarget| from
  // |m_pendingPointerCaptureTarget| and sends the got/lostpointercapture
  // events, as per the spec:
  // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
  void processPendingPointerCapture(PointerEvent*);

  // Processes the capture state of a pointer, updates node under
  // pointer, and sends corresponding boundary events for pointer if
  // setPointerPosition is true. It also sends corresponding boundary events
  // for mouse if sendMouseEvent is true.
  // Returns the target that the pointer event is supposed to be fired at.
  EventTarget* processCaptureAndPositionOfPointerEvent(
      PointerEvent*,
      EventTarget* hitTestTarget,
      const PlatformMouseEvent& = PlatformMouseEvent(),
      bool sendMouseEvent = false);

  void removeTargetFromPointerCapturingMapping(PointerCapturingMap&,
                                               const EventTarget*);
  EventTarget* getEffectiveTargetForPointerEvent(EventTarget*, int);
  EventTarget* getCapturingNode(int);
  void removePointer(PointerEvent*);
  WebInputEventResult dispatchPointerEvent(EventTarget*,
                                           PointerEvent*,
                                           bool checkForListener = false);
  void releasePointerCapture(int);
  // Returns true if capture target and pending capture target were different.
  bool getPointerCaptureState(int pointerId,
                              EventTarget** pointerCaptureTarget,
                              EventTarget** pendingPointerCaptureTarget);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |PointerEventManager::clear()|.

  const Member<LocalFrame> m_frame;

  // Prevents firing mousedown, mousemove & mouseup in-between a canceled
  // pointerdown and next pointerup/pointercancel.
  // See "PREVENT MOUSE EVENT flag" in the spec:
  //   https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
  bool m_preventMouseEventForPointerType
      [static_cast<size_t>(WebPointerProperties::PointerType::LastEntry) + 1];

  // Set upon TouchScrollStarted when sending a pointercancel, prevents PE
  // dispatches for touches until all touch-points become inactive.
  bool m_inCanceledStateForPointerTypeTouch;

  Deque<uint32_t> m_touchIdsForCanceledPointerdowns;

  // Note that this map keeps track of node under pointer with id=1 as well
  // which might be different than m_nodeUnderMouse in EventHandler. That one
  // keeps track of any compatibility mouse event positions but this map for
  // the pointer with id=1 is only taking care of true mouse related events.
  using NodeUnderPointerMap =
      HeapHashMap<int,
                  EventTargetAttributes,
                  WTF::IntHash<int>,
                  WTF::UnsignedWithZeroKeyHashTraits<int>>;
  NodeUnderPointerMap m_nodeUnderPointer;

  PointerCapturingMap m_pointerCaptureTarget;
  PointerCapturingMap m_pendingPointerCaptureTarget;

  PointerEventFactory m_pointerEventFactory;
  Member<TouchEventManager> m_touchEventManager;
  Member<MouseEventManager> m_mouseEventManager;
};

}  // namespace blink

#endif  // PointerEventManager_h
