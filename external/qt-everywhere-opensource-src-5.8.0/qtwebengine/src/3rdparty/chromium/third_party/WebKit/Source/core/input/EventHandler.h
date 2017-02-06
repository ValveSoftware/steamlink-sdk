/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventHandler_h
#define EventHandler_h

#include "core/CoreExport.h"
#include "core/events/TextEventInputType.h"
#include "core/input/KeyboardEventManager.h"
#include "core/input/PointerEventManager.h"
#include "core/input/ScrollManager.h"
#include "core/layout/HitTestRequest.h"
#include "core/page/DragActions.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchPoint.h"
#include "platform/Timer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashTraits.h"
#include "wtf/RefPtr.h"

namespace blink {

class DataTransfer;
class PaintLayer;
class Document;
class DragState;
class Element;
class Event;
class EventTarget;
template <typename EventType>
class EventWithHitTestResults;
class FloatPoint;
class FloatQuad;
class FrameHost;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class LayoutObject;
class LocalFrame;
class Node;
class OptionalCursor;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class ScrollableArea;
class Scrollbar;
class SelectionController;
class TextEvent;
class WheelEvent;
class Widget;

enum class DragInitiator;

class CORE_EXPORT EventHandler final : public GarbageCollectedFinalized<EventHandler> {

    WTF_MAKE_NONCOPYABLE(EventHandler);
public:
    explicit EventHandler(LocalFrame*);
    ~EventHandler();
    DECLARE_TRACE();

    void clear();
    void nodeWillBeRemoved(Node&);

    void updateSelectionForMouseDrag();

#if OS(WIN)
    void startPanScrolling(LayoutObject*);
#endif

    // TODO(nzolghadr): Some of the APIs in this class only forward the action
    // to the corresponding Manager class. We need to investigate whether it is
    // better to expose the manager instance itself later or can the access to
    // those APIs be more limited or removed.

    void stopAutoscroll();

    void dispatchFakeMouseMoveEventSoon();
    void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

    static HitTestResult hitTestResultInFrame(LocalFrame*, const LayoutPoint&, HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active);

    HitTestResult hitTestResultAtPoint(const LayoutPoint&,
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active,
        const LayoutSize& padding = LayoutSize());

    bool mousePressed() const { return m_mousePressed; }

    void setCapturingMouseEventsNode(Node*); // A caller is responsible for resetting capturing node to 0.

    WebInputEventResult updateDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    void cancelDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    WebInputEventResult performDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    void updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement);

    void scheduleHoverStateUpdate();
    void scheduleCursorUpdate();

    // Return whether a mouse cursor update is currently pending.  Used for testing.
    bool cursorUpdatePending();

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeScrollableAreaDestroyed();

    IntPoint lastKnownMousePosition() const;

    IntPoint dragDataTransferLocationForTesting();

    // Performs a logical scroll that chains, crossing frames, starting from
    // the given node or a reasonable default (focus/last clicked).
    bool bubblingScroll(ScrollDirection, ScrollGranularity, Node* startingNode = nullptr);

    WebInputEventResult handleMouseMoveEvent(const PlatformMouseEvent&);
    void handleMouseLeaveEvent(const PlatformMouseEvent&);

    WebInputEventResult handleMousePressEvent(const PlatformMouseEvent&);
    WebInputEventResult handleMouseReleaseEvent(const PlatformMouseEvent&);
    WebInputEventResult handleWheelEvent(const PlatformWheelEvent&);

    // Called on the local root frame exactly once per gesture event.
    WebInputEventResult handleGestureEvent(const PlatformGestureEvent&);
    WebInputEventResult handleGestureEvent(const GestureEventWithHitTestResults&);

    // Clear the old hover/active state within frames before moving the hover state to the another frame
    void updateGestureHoverActiveState(const HitTestRequest&, Element*);

    // Hit-test the provided (non-scroll) gesture event, applying touch-adjustment and updating
    // hover/active state across all frames if necessary. This should be called at most once
    // per gesture event, and called on the local root frame.
    // Note: This is similar to (the less clearly named) prepareMouseEvent.
    // FIXME: Remove readOnly param when there is only ever a single call to this.
    GestureEventWithHitTestResults targetGestureEvent(const PlatformGestureEvent&, bool readOnly = false);
    GestureEventWithHitTestResults hitTestResultForGestureEvent(const PlatformGestureEvent&, HitTestRequest::HitTestRequestType);
    // Handle the provided non-scroll gesture event. Should be called only on the inner frame.
    WebInputEventResult handleGestureEventInFrame(const GestureEventWithHitTestResults&);

    // Handle the provided scroll gesture event, propagating down to child frames as necessary.
    WebInputEventResult handleGestureScrollEvent(const PlatformGestureEvent&);
    WebInputEventResult handleGestureScrollEnd(const PlatformGestureEvent&);
    bool isScrollbarHandlingGestures() const;

    bool bestClickableNodeForHitTestResult(const HitTestResult&, IntPoint& targetPoint, Node*& targetNode);
    bool bestContextMenuNodeForHitTestResult(const HitTestResult&, IntPoint& targetPoint, Node*& targetNode);
    // FIXME: This doesn't appear to be used outside tests anymore, what path are we using now and is it tested?
    bool bestZoomableAreaForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntRect& targetArea, Node*& targetNode);

    WebInputEventResult sendContextMenuEvent(const PlatformMouseEvent&, Node* overrideTargetNode = nullptr);
    WebInputEventResult sendContextMenuEventForKey(Element* overrideTargetElement = nullptr);
    WebInputEventResult sendContextMenuEventForGesture(const GestureEventWithHitTestResults&);

    // Returns whether pointerId is active or not
    bool isPointerEventActive(int);

    void setPointerCapture(int, EventTarget*);
    void releasePointerCapture(int, EventTarget*);
    void elementRemoved(EventTarget*);

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    static WebInputEventResult mergeEventResult(WebInputEventResult resultA, WebInputEventResult resultB);
    static WebInputEventResult toWebInputEventResult(DispatchEventResult);

    bool handleAccessKey(const PlatformKeyboardEvent&);
    WebInputEventResult keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = nullptr, TextEventInputType = TextEventInputKeyboard);
    void defaultTextInputEventHandler(TextEvent*);

    void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);

    void focusDocumentView();

    void capsLockStateMayHaveChanged(); // Only called by FrameSelection

    WebInputEventResult handleTouchEvent(const PlatformTouchEvent&);

    bool useHandCursor(Node*, bool isOverLink);

    void notifyElementActivated();

    PassRefPtr<UserGestureToken> takeLastMouseDownGestureToken() { return m_lastMouseDownUserGestureToken.release(); }

    int clickCount() { return m_clickCount; }

    SelectionController& selectionController() const { return *m_selectionController; }

    // FIXME(nzolghadr): This function is technically a private function of
    // EventHandler class. Making it public temporary to make it possible to
    // move some code around in the refactoring process.
    // Performs a chaining logical scroll, within a *single* frame, starting
    // from either a provided starting node or a default based on the focused or
    // most recently clicked node, falling back to the frame.
    // Returns true if the scroll was consumed.
    // direction - The logical direction to scroll in. This will be converted to
    //             a physical direction for each LayoutBox we try to scroll
    //             based on that box's writing mode.
    // granularity - The units that the  scroll delta parameter is in.
    // startNode - Optional. If provided, start chaining from the given node.
    //             If not, use the current focus or last clicked node.
    bool logicalScroll(ScrollDirection, ScrollGranularity, Node* startNode = nullptr);

private:
    static DragState& dragState();

    DataTransfer* createDraggingDataTransfer() const;

    WebInputEventResult handleMouseMoveOrLeaveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = nullptr, bool onlyUpdateScrollbars = false, bool forceLeave = false);
    WebInputEventResult handleMousePressEvent(const MouseEventWithHitTestResults&);
    WebInputEventResult handleMouseFocus(const MouseEventWithHitTestResults&, InputDeviceCapabilities* sourceCapabilities);
    WebInputEventResult handleMouseDraggedEvent(const MouseEventWithHitTestResults&);
    WebInputEventResult handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    HitTestRequest::HitTestRequestType getHitTypeForGestureType(PlatformEvent::EventType);
    void applyTouchAdjustment(PlatformGestureEvent*, HitTestResult*);
    WebInputEventResult handleGestureTapDown(const GestureEventWithHitTestResults&);
    WebInputEventResult handleGestureTap(const GestureEventWithHitTestResults&);
    WebInputEventResult handleGestureLongPress(const GestureEventWithHitTestResults&);
    WebInputEventResult handleGestureLongTap(const GestureEventWithHitTestResults&);

    void updateGestureTargetNodeForMouseEvent(const GestureEventWithHitTestResults&);

    bool shouldApplyTouchAdjustment(const PlatformGestureEvent&) const;

    OptionalCursor selectCursor(const HitTestResult&);
    OptionalCursor selectAutoCursor(const HitTestResult&, Node*, const Cursor& iBeam);

    void hoverTimerFired(Timer<EventHandler>*);
    void cursorUpdateTimerFired(Timer<EventHandler>*);
    void activeIntervalTimerFired(Timer<EventHandler>*);

    void fakeMouseMoveEventTimerFired(Timer<EventHandler>*);
    void cancelFakeMouseMoveEvent();
    bool isCursorVisible() const;
    void updateCursor();

    ScrollableArea* associatedScrollableArea(const PaintLayer*) const;

    void invalidateClick();

    Node* updateMouseEventTargetNode(Node*, const PlatformMouseEvent&);
    void updateMouseEventTargetNodeAndSendEvents(Node*, const PlatformMouseEvent&, bool isFrameBoundaryTransition = false);


    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    WebInputEventResult dispatchMouseEvent(const AtomicString& eventType, Node* target, int clickCount, const PlatformMouseEvent&);

    // Dispatches ME after corresponding PE provided the PE has not been canceled. The eventType arg
    // must be a mouse event that can be gated though a preventDefaulted pointerdown (i.e., one of
    // {mousedown, mousemove, mouseup}).
    // TODO(mustaq): Can we avoid the clickCount param, instead use PlatformMouseEvent's count?
    //     Same applied to dispatchMouseEvent() above.
    WebInputEventResult updatePointerTargetAndDispatchEvents(const AtomicString& mouseEventType, Node* target,
        int clickCount, const PlatformMouseEvent&);

    WebInputEventResult dispatchDragEvent(const AtomicString& eventType, Node* target, const PlatformMouseEvent&, DataTransfer*);

    void clearDragDataTransfer();

    bool handleDrag(const MouseEventWithHitTestResults&, DragInitiator);
    bool tryStartDrag(const MouseEventWithHitTestResults&);
    void clearDragState();

    WebInputEventResult dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const IntPoint&) const;

    WebInputEventResult passMousePressEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe);
    WebInputEventResult passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe, HitTestResult* hoveredNode = nullptr);
    WebInputEventResult passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&);

    void defaultSpaceEventHandler(KeyboardEvent*);
    void defaultBackspaceEventHandler(KeyboardEvent*);
    void defaultTabEventHandler(KeyboardEvent*);
    void defaultEscapeEventHandler(KeyboardEvent*);
    void defaultArrowEventHandler(WebFocusType, KeyboardEvent*);

    void updateLastScrollbarUnderMouse(Scrollbar*, bool);

    bool capturesDragging() const { return m_capturesDragging; }

    WebInputEventResult handleGestureShowPress();

    void setLastKnownMousePosition(const PlatformMouseEvent&);

    bool shouldTopControlsConsumeScroll(FloatSize) const;

    // If the given element is a shadow host and its root has delegatesFocus=false flag,
    // slide focus to its inner element. Returns true if the resulting focus is different from
    // the given element.
    bool slideFocusOnShadowHostIfNecessary(const Element&);

    FrameHost* frameHost() const;

    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |EventHandler::clear()|.

    const Member<LocalFrame> m_frame;

    // Current button-press state for mouse/mouse-like-stylus.
    // TODO(crbug.com/563676): Buggy for chorded buttons.
    bool m_mousePressed;

    bool m_capturesDragging;
    Member<Node> m_mousePressNode;

    bool m_mouseDownMayStartDrag;
    const Member<SelectionController> m_selectionController;

    LayoutPoint m_dragStartPos;

    Timer<EventHandler> m_hoverTimer;

    // TODO(rbyers): Mouse cursor update is page-wide, not per-frame.  Page-wide state
    // should move out of EventHandler to a new PageEventHandler class. crbug.com/449649
    Timer<EventHandler> m_cursorUpdateTimer;

    bool m_mouseDownMayStartAutoscroll;

    Timer<EventHandler> m_fakeMouseMoveEventTimer;

    bool m_svgPan;

    Member<Node> m_capturingMouseEventsNode;
    bool m_eventHandlerWillResetCapturingMouseEventsNode;

    // Note the difference of this and m_nodeUnderPointer in PointerEventManager
    Member<Node> m_nodeUnderMouse;

    Member<LocalFrame> m_lastMouseMoveEventSubframe;
    Member<Scrollbar> m_lastScrollbarUnderMouse;

    int m_clickCount;
    Member<Node> m_clickNode;

    Member<Node> m_dragTarget;
    bool m_shouldOnlyFireDragOverEvent;

    Member<HTMLFrameSetElement> m_frameSetBeingResized;

    bool m_mousePositionIsUnknown;
    // The last mouse movement position this frame has seen in root frame coordinates.
    IntPoint m_lastKnownMousePosition;
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownPos; // In our view's coords.
    double m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;
    RefPtr<UserGestureToken> m_lastMouseDownUserGestureToken;

    PointerEventManager m_pointerEventManager;
    ScrollManager m_scrollManager;
    KeyboardEventManager m_keyboardEventManager;

    double m_maxMouseMovedDuration;

    bool m_longTapShouldInvokeContextMenu;

    Timer<EventHandler> m_activeIntervalTimer;
    double m_lastShowPressTimestamp;
    Member<Element> m_lastDeferredTapElement;

    // Set on GestureTapDown if the |pointerdown| event corresponding to the
    // triggering |touchstart| event was canceled. This suppresses mouse event
    // firing for the current gesture sequence (i.e. until next GestureTapDown).
    bool m_suppressMouseEventsFromGestures;
};

} // namespace blink

#endif // EventHandler_h
