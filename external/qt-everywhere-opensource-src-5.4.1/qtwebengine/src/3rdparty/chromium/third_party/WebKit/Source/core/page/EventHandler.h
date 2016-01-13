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

#include "core/editing/TextGranularity.h"
#include "core/events/TextEventInputType.h"
#include "core/page/DragActions.h"
#include "core/page/FocusType.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/Timer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashTraits.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class AutoscrollController;
class Clipboard;
class Document;
class Element;
class Event;
class EventTarget;
class FloatPoint;
class FloatQuad;
class FullscreenElementStack;
class LocalFrame;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class OptionalCursor;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class RenderLayer;
class RenderLayerScrollableArea;
class RenderObject;
class RenderWidget;
class ScrollableArea;
class Scrollbar;
class TextEvent;
class TouchEvent;
class VisibleSelection;
class WheelEvent;
class Widget;

class DragState;

enum AppendTrailingWhitespace { ShouldAppendTrailingWhitespace, DontAppendTrailingWhitespace };
enum CheckDragHysteresis { ShouldCheckDragHysteresis, DontCheckDragHysteresis };

class EventHandler : public NoBaseWillBeGarbageCollectedFinalized<EventHandler> {
    WTF_MAKE_NONCOPYABLE(EventHandler);
public:
    explicit EventHandler(LocalFrame*);
    ~EventHandler();
    void trace(Visitor*);

    void clear();
    void nodeWillBeRemoved(Node&);

    void updateSelectionForMouseDrag();

    Node* mousePressNode() const;

#if OS(WIN)
    void startPanScrolling(RenderObject*);
#endif

    void stopAutoscroll();

    void dispatchFakeMouseMoveEventSoon();
    void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

    HitTestResult hitTestResultAtPoint(const LayoutPoint&,
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active,
        const LayoutSize& padding = LayoutSize());

    bool mousePressed() const { return m_mousePressed; }
    void setMousePressed(bool pressed) { m_mousePressed = pressed; }

    void setCapturingMouseEventsNode(PassRefPtrWillBeRawPtr<Node>); // A caller is responsible for resetting capturing node to 0.

    bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement);

    void scheduleHoverStateUpdate();
    void scheduleCursorUpdate();

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeScrollableAreaDestroyed();

    IntPoint lastKnownMousePosition() const;
    Cursor currentMouseCursor() const { return m_currentMouseCursor; }

    // Attempts to scroll the DOM tree. If that fails, scrolls the view.
    // If the view can't be scrolled either, recursively bubble to the parent frame.
    bool bubblingScroll(ScrollDirection, ScrollGranularity, Node* startingNode = 0);

    bool handleMouseMoveEvent(const PlatformMouseEvent&);
    void handleMouseLeaveEvent(const PlatformMouseEvent&);

    bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&);
    void defaultWheelEventHandler(Node*, WheelEvent*);

    bool handleGestureEvent(const PlatformGestureEvent&);
    bool handleGestureScrollEnd(const PlatformGestureEvent&);
    bool isScrollbarHandlingGestures() const;

    bool bestClickableNodeForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntPoint& targetPoint, Node*& targetNode);
    bool bestContextMenuNodeForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntPoint& targetPoint, Node*& targetNode);
    bool bestZoomableAreaForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntRect& targetArea, Node*& targetNode);

    void adjustGesturePosition(const PlatformGestureEvent&, IntPoint& adjustedPoint);

    bool sendContextMenuEvent(const PlatformMouseEvent&);
    bool sendContextMenuEventForKey();
    bool sendContextMenuEventForGesture(const PlatformGestureEvent&);

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    static unsigned accessKeyModifiers();
    bool handleAccessKey(const PlatformKeyboardEvent&);
    bool keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = 0, TextEventInputType = TextEventInputKeyboard);
    void defaultTextInputEventHandler(TextEvent*);

    void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);

    void focusDocumentView();

    void capsLockStateMayHaveChanged(); // Only called by FrameSelection

    bool handleTouchEvent(const PlatformTouchEvent&);

    bool useHandCursor(Node*, bool isOverLink);

    void notifyElementActivated();

    PassRefPtr<UserGestureToken> takeLastMouseDownGestureToken() { return m_lastMouseDownUserGestureToken.release(); }

private:
    static DragState& dragState();

    PassRefPtrWillBeRawPtr<Clipboard> createDraggingClipboard() const;

    bool updateSelectionForMouseDownDispatchingSelectStart(Node*, const VisibleSelection&, TextGranularity);
    void selectClosestWordFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    void selectClosestMisspellingFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    void selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestMisspellingFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults&);

    bool handleMouseMoveOrLeaveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = 0, bool onlyUpdateScrollbars = false);
    bool handleMousePressEvent(const MouseEventWithHitTestResults&);
    bool handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);
    bool handleMouseFocus(const PlatformMouseEvent&);
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&);
    bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    bool handlePasteGlobalSelection(const PlatformMouseEvent&);

    bool handleGestureTap(const PlatformGestureEvent&, const IntPoint& adjustedPoint);
    bool handleGestureLongPress(const PlatformGestureEvent&, const IntPoint& adjustedPoint);
    bool handleGestureLongTap(const PlatformGestureEvent&, const IntPoint& adjustedPoint);
    bool handleGestureTwoFingerTap(const PlatformGestureEvent&, const IntPoint& adjustedPoint);
    bool handleGestureScrollUpdate(const PlatformGestureEvent&);
    bool handleGestureScrollBegin(const PlatformGestureEvent&);
    void clearGestureScrollNodes();

    bool shouldApplyTouchAdjustment(const PlatformGestureEvent&) const;

    OptionalCursor selectCursor(const HitTestResult&);
    OptionalCursor selectAutoCursor(const HitTestResult&, Node*, const Cursor& iBeam);

    void hoverTimerFired(Timer<EventHandler>*);
    void cursorUpdateTimerFired(Timer<EventHandler>*);
    void activeIntervalTimerFired(Timer<EventHandler>*);

    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }

    void fakeMouseMoveEventTimerFired(Timer<EventHandler>*);
    void cancelFakeMouseMoveEvent();
    bool isCursorVisible() const;
    void updateCursor();

    bool isInsideScrollbar(const IntPoint&) const;

    ScrollableArea* associatedScrollableArea(const RenderLayer*) const;

    // Scrolls the elements of the DOM tree. Returns true if a node was scrolled.
    // False if we reached the root and couldn't scroll anything.
    // direction - The direction to scroll in. If this is a logicl direction, it will be
    //             converted to the physical direction based on a node's writing mode.
    // granularity - The units that the  scroll delta parameter is in.
    // startNode - The node to start bubbling the scroll from. If a node can't scroll,
    //             the scroll bubbles up to the containing block.
    // stopNode - On input, if provided and non-null, the node at which we should stop bubbling on input.
    //            On output, if provided and a node was scrolled stopNode will point to that node.
    // delta - The delta to scroll by, in the units of the granularity parameter. (e.g. pixels, lines, pages, etc.)
    // absolutePoint - For wheel scrolls - the location, in absolute coordinates, where the event occured.
    bool scroll(ScrollDirection, ScrollGranularity, Node* startNode = 0, Node** stopNode = 0, float delta = 1.0f, IntPoint absolutePoint = IntPoint());

    TouchAction intersectTouchAction(const TouchAction, const TouchAction);
    TouchAction computeEffectiveTouchAction(const Node&);

    HitTestResult hitTestResultInFrame(LocalFrame*, const LayoutPoint&, HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);

    void invalidateClick();

    void updateMouseEventTargetNode(Node*, const PlatformMouseEvent&, bool fireMouseOverOut);

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target, int clickCount, const PlatformMouseEvent&, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, Node* target, const PlatformMouseEvent&, Clipboard*);

    void freeClipboard();

    bool handleDrag(const MouseEventWithHitTestResults&, CheckDragHysteresis);
    bool tryStartDrag(const MouseEventWithHitTestResults&);
    void clearDragState();

    bool dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe, HitTestResult* hoveredNode = 0);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, LocalFrame* subframe);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);

    bool passWheelEventToWidget(const PlatformWheelEvent&, Widget*);

    void defaultSpaceEventHandler(KeyboardEvent*);
    void defaultBackspaceEventHandler(KeyboardEvent*);
    void defaultTabEventHandler(KeyboardEvent*);
    void defaultEscapeEventHandler(KeyboardEvent*);
    void defaultArrowEventHandler(FocusType, KeyboardEvent*);

    void updateSelectionForMouseDrag(const HitTestResult&);

    void updateLastScrollbarUnderMouse(Scrollbar*, bool);

    void setFrameWasScrolledByUser();

    bool capturesDragging() const { return m_capturesDragging; }

    bool isKeyEventAllowedInFullScreen(FullscreenElementStack*, const PlatformKeyboardEvent&) const;

    bool handleGestureShowPress();

    bool handleScrollGestureOnResizer(Node*, const PlatformGestureEvent&);

    bool passGestureEventToWidget(const PlatformGestureEvent&, Widget*);
    bool passGestureEventToWidgetIfPossible(const PlatformGestureEvent&, RenderObject*);
    bool sendScrollEventToView(const PlatformGestureEvent&, const FloatSize&);
    LocalFrame* getSubFrameForGestureEvent(const IntPoint& touchAdjustedPoint, const PlatformGestureEvent&);

    AutoscrollController* autoscrollController() const;
    bool panScrollInProgress() const;
    void setLastKnownMousePosition(const PlatformMouseEvent&);

    LocalFrame* const m_frame;

    bool m_mousePressed;
    bool m_capturesDragging;
    RefPtrWillBeMember<Node> m_mousePressNode;

    bool m_mouseDownMayStartSelect;
    bool m_mouseDownMayStartDrag;
    bool m_mouseDownWasSingleClickInSelection;
    enum SelectionInitiationState { HaveNotStartedSelection, PlacedCaret, ExtendedSelection };
    SelectionInitiationState m_selectionInitiationState;

    LayoutPoint m_dragStartPos;

    Timer<EventHandler> m_hoverTimer;
    Timer<EventHandler> m_cursorUpdateTimer;

    bool m_mouseDownMayStartAutoscroll;
    bool m_mouseDownWasInSubframe;

    Timer<EventHandler> m_fakeMouseMoveEventTimer;

    bool m_svgPan;

    RenderLayerScrollableArea* m_resizeScrollableArea;

    RefPtrWillBeMember<Node> m_capturingMouseEventsNode;
    bool m_eventHandlerWillResetCapturingMouseEventsNode;

    RefPtrWillBeMember<Node> m_nodeUnderMouse;
    RefPtrWillBeMember<Node> m_lastNodeUnderMouse;
    RefPtr<LocalFrame> m_lastMouseMoveEventSubframe;
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;
    Cursor m_currentMouseCursor;

    int m_clickCount;
    RefPtrWillBeMember<Node> m_clickNode;

    RefPtrWillBeMember<Node> m_dragTarget;
    bool m_shouldOnlyFireDragOverEvent;

    RefPtrWillBeMember<HTMLFrameSetElement> m_frameSetBeingResized;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeScrollableArea.

    bool m_mousePositionIsUnknown;
    IntPoint m_lastKnownMousePosition;
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownPos; // In our view's coords.
    double m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;
    RefPtr<UserGestureToken> m_lastMouseDownUserGestureToken;

    RefPtrWillBeMember<Node> m_latchedWheelEventNode;
    bool m_widgetIsLatched;

    RefPtrWillBeMember<Node> m_previousWheelScrolledNode;

    // The target of each active touch point indexed by the touch ID.
    typedef WillBeHeapHashMap<unsigned, RefPtrWillBeMember<EventTarget>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned> > TouchTargetMap;
    TouchTargetMap m_targetForTouchID;

    // If set, the document of the active touch sequence. Unset if no touch sequence active.
    RefPtrWillBeMember<Document> m_touchSequenceDocument;
    RefPtr<UserGestureToken> m_touchSequenceUserGestureToken;

    bool m_touchPressed;

    RefPtrWillBeMember<Node> m_scrollGestureHandlingNode;
    bool m_lastHitTestResultOverWidget;
    RefPtrWillBeMember<Node> m_previousGestureScrolledNode;
    RefPtr<Scrollbar> m_scrollbarHandlingScrollGesture;

    double m_maxMouseMovedDuration;
    PlatformEvent::Type m_baseEventType;
    bool m_didStartDrag;

    bool m_longTapShouldInvokeContextMenu;

    Timer<EventHandler> m_activeIntervalTimer;
    double m_lastShowPressTimestamp;
    RefPtrWillBeMember<Element> m_lastDeferredTapElement;
};

} // namespace WebCore

#endif // EventHandler_h
