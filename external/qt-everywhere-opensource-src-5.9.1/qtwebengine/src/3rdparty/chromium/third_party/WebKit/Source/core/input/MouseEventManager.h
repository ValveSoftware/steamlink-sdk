// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseEventManager_h
#define MouseEventManager_h

#include "core/CoreExport.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/input/BoundaryEventDispatcher.h"
#include "core/page/DragActions.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/Timer.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"

namespace blink {

class ContainerNode;
class DragState;
class DataTransfer;
class Element;
class FloatQuad;
class HitTestResult;
class InputDeviceCapabilities;
class LocalFrame;
class ScrollManager;

enum class DragInitiator;

// This class takes care of dispatching all mouse events and keeps track of
// positions and states of mouse.
class CORE_EXPORT MouseEventManager final
    : public GarbageCollectedFinalized<MouseEventManager>,
      public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(MouseEventManager);
  USING_GARBAGE_COLLECTED_MIXIN(MouseEventManager);

 public:
  MouseEventManager(LocalFrame*, ScrollManager*);
  virtual ~MouseEventManager();
  DECLARE_TRACE();

  WebInputEventResult dispatchMouseEvent(EventTarget*,
                                         const AtomicString&,
                                         const PlatformMouseEvent&,
                                         EventTarget* relatedTarget,
                                         bool checkForListener = false);

  WebInputEventResult setMousePositionAndDispatchMouseEvent(
      Node* targetNode,
      const AtomicString& eventType,
      const PlatformMouseEvent&);

  WebInputEventResult dispatchMouseClickIfNeeded(
      const MouseEventWithHitTestResults&);

  WebInputEventResult dispatchDragSrcEvent(const AtomicString& eventType,
                                           const PlatformMouseEvent&);
  WebInputEventResult dispatchDragEvent(const AtomicString& eventType,
                                        Node* target,
                                        const PlatformMouseEvent&,
                                        DataTransfer*);

  // Resets the internal state of this object.
  void clear();

  void sendBoundaryEvents(EventTarget* exitedTarget,
                          EventTarget* enteredTarget,
                          const PlatformMouseEvent& mousePlatformEvent);

  void setNodeUnderMouse(Node*, const PlatformMouseEvent&);

  WebInputEventResult handleMouseFocus(
      const HitTestResult&,
      InputDeviceCapabilities* sourceCapabilities);

  void fakeMouseMoveEventTimerFired(TimerBase*);

  void cancelFakeMouseMoveEvent();
  void dispatchFakeMouseMoveEventSoon();
  void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

  void setLastKnownMousePosition(const PlatformMouseEvent&);

  bool handleDragDropIfPossible(const GestureEventWithHitTestResults&);

  WebInputEventResult handleMouseDraggedEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult handleMousePressEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult handleMouseReleaseEvent(
      const MouseEventWithHitTestResults&);

  static DragState& dragState();

  void focusDocumentView();

  // Resets the state that indicates the next events could cause a drag. It is
  // called when we realize the next events should not cause drag based on the
  // drag heuristics.
  void clearDragHeuristicState();

  void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);

  void updateSelectionForMouseDrag();

  void handleMousePressEventUpdateStates(const PlatformMouseEvent&);

  // Returns whether pan is handled and resets the state on release.
  bool handleSvgPanIfNeeded(bool isReleaseEvent);

  void invalidateClick();

  // TODO: These functions ideally should be private but the code needs more
  // refactoring to be able to remove the dependency from EventHandler.
  Node* getNodeUnderMouse();
  bool isMousePositionUnknown();
  IntPoint lastKnownMousePosition();

  bool mousePressed();
  void setMousePressed(bool);

  bool capturesDragging() const;
  void setCapturesDragging(bool);

  void setMouseDownMayStartAutoscroll() {
    m_mouseDownMayStartAutoscroll = true;
  }

  Node* mousePressNode();
  void setMousePressNode(Node*);

  void setClickNode(Node*);
  void setClickCount(int);

  bool mouseDownMayStartDrag();

 private:
  class MouseEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
    WTF_MAKE_NONCOPYABLE(MouseEventBoundaryEventDispatcher);

   public:
    MouseEventBoundaryEventDispatcher(MouseEventManager*,
                                      const PlatformMouseEvent*,
                                      EventTarget* exitedTarget);

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
                  const PlatformMouseEvent&,
                  bool checkForListener);
    Member<MouseEventManager> m_mouseEventManager;
    const PlatformMouseEvent* m_platformMouseEvent;
    Member<EventTarget> m_exitedTarget;
  };

  // If the given element is a shadow host and its root has delegatesFocus=false
  // flag, slide focus to its inner element. Returns true if the resulting focus
  // is different from the given element.
  bool slideFocusOnShadowHostIfNecessary(const Element&);

  bool dragThresholdExceeded(const IntPoint&) const;
  bool handleDrag(const MouseEventWithHitTestResults&, DragInitiator);
  bool tryStartDrag(const MouseEventWithHitTestResults&);
  void clearDragDataTransfer();
  DataTransfer* createDraggingDataTransfer() const;

  // Implementations of |SynchronousMutationObserver|
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node& nodeToBeRemoved) final;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |MouseEventManager::clear()|.

  const Member<LocalFrame> m_frame;
  Member<ScrollManager> m_scrollManager;

  // The effective position of the mouse pointer.
  // See
  // https://w3c.github.io/pointerevents/#dfn-tracking-the-effective-position-of-the-legacy-mouse-pointer.
  Member<Node> m_nodeUnderMouse;

  // The last mouse movement position this frame has seen in root frame
  // coordinates.
  IntPoint m_lastKnownMousePosition;
  IntPoint m_lastKnownMouseGlobalPosition;

  unsigned m_isMousePositionUnknown : 1;
  // Current button-press state for mouse/mouse-like-stylus.
  // TODO(crbug.com/563676): Buggy for chorded buttons.
  unsigned m_mousePressed : 1;

  unsigned m_mouseDownMayStartAutoscroll : 1;
  unsigned m_svgPan : 1;
  unsigned m_capturesDragging : 1;
  unsigned m_mouseDownMayStartDrag : 1;

  Member<Node> m_mousePressNode;

  int m_clickCount;
  Member<Node> m_clickNode;

  IntPoint m_mouseDownPos;  // In our view's coords.
  double m_mouseDownTimestamp;
  PlatformMouseEvent m_mouseDown;

  LayoutPoint m_dragStartPos;

  Timer<MouseEventManager> m_fakeMouseMoveEventTimer;
};

}  // namespace blink

#endif  // MouseEventManager_h
