/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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

#include "core/input/EventHandler.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/TouchList.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionController.h"
#include "core/events/DragEvent.h"
#include "core/events/EventPath.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/PointerEvent.h"
#include "core/events/TextEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/Deprecation.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameSetElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/DragController.h"
#include "core/page/DragState.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/TouchAdjustment.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "core/style/CursorData.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/WindowsKeyboardCodes.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/Image.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/Scrollbar.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"
#include <memory>

namespace blink {

namespace {

// Refetch the event target node if it is removed or currently is the shadow node inside an <input> element.
// If a mouse event handler changes the input element type to one that has a widget associated,
// we'd like to EventHandler::handleMousePressEvent to pass the event to the widget and thus the
// event target node can't still be the shadow node.
bool shouldRefetchEventTarget(const MouseEventWithHitTestResults& mev)
{
    Node* targetNode = mev.innerNode();
    if (!targetNode || !targetNode->parentNode())
        return true;
    return targetNode->isShadowRoot() && isHTMLInputElement(toShadowRoot(targetNode)->host());
}

} // namespace

using namespace HTMLNames;

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
static const int LinkDragHysteresis = 40;
static const int ImageDragHysteresis = 5;
static const int TextDragHysteresis = 3;
static const int GeneralDragHysteresis = 3;

// The amount of time to wait before sending a fake mouse event triggered
// during a scroll.
static const double fakeMouseMoveInterval = 0.1;

// The amount of time to wait for a cursor update on style and layout changes
// Set to 50Hz, no need to be faster than common screen refresh rate
static const double cursorUpdateInterval = 0.02;

static const int maximumCursorSize = 128;

// It's pretty unlikely that a scale of less than one would ever be used. But all we really
// need to ensure here is that the scale isn't so small that integer overflow can occur when
// dividing cursor sizes (limited above) by the scale.
static const double minimumCursorScale = 0.001;

// The minimum amount of time an element stays active after a ShowPress
// This is roughly 9 frames, which should be long enough to be noticeable.
static const double minimumActiveInterval = 0.15;

#if OS(MACOSX)
static const double TextDragDelay = 0.15;
#else
static const double TextDragDelay = 0.0;
#endif

enum NoCursorChangeType { NoCursorChange };

enum class DragInitiator { Mouse, Touch };

class OptionalCursor {
public:
    OptionalCursor(NoCursorChangeType) : m_isCursorChange(false) { }
    OptionalCursor(const Cursor& cursor) : m_isCursorChange(true), m_cursor(cursor) { }

    bool isCursorChange() const { return m_isCursorChange; }
    const Cursor& cursor() const { ASSERT(m_isCursorChange); return m_cursor; }

private:
    bool m_isCursorChange;
    Cursor m_cursor;
};

EventHandler::EventHandler(LocalFrame* frame)
    : m_frame(frame)
    , m_mousePressed(false)
    , m_capturesDragging(false)
    , m_mouseDownMayStartDrag(false)
    , m_selectionController(SelectionController::create(*frame))
    , m_hoverTimer(this, &EventHandler::hoverTimerFired)
    , m_cursorUpdateTimer(this, &EventHandler::cursorUpdateTimerFired)
    , m_mouseDownMayStartAutoscroll(false)
    , m_fakeMouseMoveEventTimer(this, &EventHandler::fakeMouseMoveEventTimerFired)
    , m_svgPan(false)
    , m_eventHandlerWillResetCapturingMouseEventsNode(0)
    , m_clickCount(0)
    , m_shouldOnlyFireDragOverEvent(false)
    , m_mousePositionIsUnknown(true)
    , m_mouseDownTimestamp(0)
    , m_pointerEventManager(frame)
    , m_scrollManager(frame)
    , m_keyboardEventManager(frame, &m_scrollManager)
    , m_longTapShouldInvokeContextMenu(false)
    , m_activeIntervalTimer(this, &EventHandler::activeIntervalTimerFired)
    , m_lastShowPressTimestamp(0)
    , m_suppressMouseEventsFromGestures(false)
{
}

EventHandler::~EventHandler()
{
    ASSERT(!m_fakeMouseMoveEventTimer.isActive());
}

DEFINE_TRACE(EventHandler)
{
    visitor->trace(m_frame);
    visitor->trace(m_mousePressNode);
    visitor->trace(m_capturingMouseEventsNode);
    visitor->trace(m_nodeUnderMouse);
    visitor->trace(m_lastMouseMoveEventSubframe);
    visitor->trace(m_lastScrollbarUnderMouse);
    visitor->trace(m_clickNode);
    visitor->trace(m_dragTarget);
    visitor->trace(m_frameSetBeingResized);
    visitor->trace(m_lastDeferredTapElement);
    visitor->trace(m_selectionController);
    visitor->trace(m_pointerEventManager);
    visitor->trace(m_scrollManager);
    visitor->trace(m_keyboardEventManager);
}

DragState& EventHandler::dragState()
{
    DEFINE_STATIC_LOCAL(DragState, state, (new DragState));
    return state;
}

void EventHandler::clear()
{
    m_hoverTimer.stop();
    m_cursorUpdateTimer.stop();
    m_fakeMouseMoveEventTimer.stop();
    m_activeIntervalTimer.stop();
    m_nodeUnderMouse = nullptr;
    m_lastMouseMoveEventSubframe = nullptr;
    m_lastScrollbarUnderMouse = nullptr;
    m_clickCount = 0;
    m_clickNode = nullptr;
    m_frameSetBeingResized = nullptr;
    m_dragTarget = nullptr;
    m_shouldOnlyFireDragOverEvent = false;
    m_mousePositionIsUnknown = true;
    m_lastKnownMousePosition = IntPoint();
    m_lastKnownMouseGlobalPosition = IntPoint();
    m_lastMouseDownUserGestureToken.clear();
    m_mousePressNode = nullptr;
    m_mousePressed = false;
    m_capturesDragging = false;
    m_capturingMouseEventsNode = nullptr;
    m_pointerEventManager.clear();
    m_scrollManager.clear();
    m_mouseDownMayStartDrag = false;
    m_lastShowPressTimestamp = 0;
    m_lastDeferredTapElement = nullptr;
    m_eventHandlerWillResetCapturingMouseEventsNode = false;
    m_mouseDownMayStartAutoscroll = false;
    m_svgPan = false;
    m_mouseDownPos = IntPoint();
    m_mouseDownTimestamp = 0;
    m_longTapShouldInvokeContextMenu = false;
    m_dragStartPos = LayoutPoint();
    m_mouseDown = PlatformMouseEvent();
    m_suppressMouseEventsFromGestures = false;
}

WebInputEventResult EventHandler::mergeEventResult(
    WebInputEventResult resultA, WebInputEventResult resultB)
{
    // The ordering of the enumeration is specific. There are times that
    // multiple events fire and we need to combine them into a single
    // result code. The enumeration is based on the level of consumption that
    // is most significant. The enumeration is ordered with smaller specified
    // numbers first. Examples of merged results are:
    // (HandledApplication, HandledSystem) -> HandledSystem
    // (NotHandled, HandledApplication) -> HandledApplication
    static_assert(static_cast<int>(WebInputEventResult::NotHandled) == 0, "WebInputEventResult not ordered");
    static_assert(static_cast<int>(WebInputEventResult::HandledSuppressed) < static_cast<int>(WebInputEventResult::HandledApplication), "WebInputEventResult not ordered");
    static_assert(static_cast<int>(WebInputEventResult::HandledApplication) < static_cast<int>(WebInputEventResult::HandledSystem), "WebInputEventResult not ordered");
    return static_cast<WebInputEventResult>(max(static_cast<int>(resultA), static_cast<int>(resultB)));
}

WebInputEventResult EventHandler::toWebInputEventResult(
    DispatchEventResult result)
{
    switch (result) {
    case DispatchEventResult::NotCanceled:
        return WebInputEventResult::NotHandled;
    case DispatchEventResult::CanceledByEventHandler:
        return WebInputEventResult::HandledApplication;
    case DispatchEventResult::CanceledByDefaultEventHandler:
        return WebInputEventResult::HandledSystem;
    case DispatchEventResult::CanceledBeforeDispatch:
        return WebInputEventResult::HandledSuppressed;
    default:
        ASSERT_NOT_REACHED();
        return WebInputEventResult::HandledSystem;
    }
}

void EventHandler::nodeWillBeRemoved(Node& nodeToBeRemoved)
{
    if (nodeToBeRemoved.isShadowIncludingInclusiveAncestorOf(m_clickNode.get())) {
        // We don't dispatch click events if the mousedown node is removed
        // before a mouseup event. It is compatible with IE and Firefox.
        m_clickNode = nullptr;
    }
}

WebInputEventResult EventHandler::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    TRACE_EVENT0("blink", "EventHandler::handleMousePressEvent");

    // Reset drag state.
    dragState().m_dragSrc = nullptr;

    cancelFakeMouseMoveEvent();

    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    if (FrameView* frameView = m_frame->view()) {
        if (frameView->isPointInScrollbarCorner(event.event().position()))
            return WebInputEventResult::NotHandled;
    }

    bool singleClick = event.event().clickCount() <= 1;

    m_mouseDownMayStartDrag = singleClick && !isLinkSelection(event) && !isExtendingSelection(event);

    selectionController().handleMousePressEvent(event);

    m_mouseDown = event.event();

    if (m_frame->document()->isSVGDocument() && m_frame->document()->accessSVGExtensions().zoomAndPanEnabled()) {
        if (event.event().shiftKey() && singleClick) {
            m_svgPan = true;
            m_frame->document()->accessSVGExtensions().startPan(m_frame->view()->rootFrameToContents(event.event().position()));
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

    if (event.event().clickCount() == 2)
        swallowEvent = selectionController().handleMousePressEventDoubleClick(event);
    else if (event.event().clickCount() >= 3)
        swallowEvent = selectionController().handleMousePressEventTripleClick(event);
    else
        swallowEvent = selectionController().handleMousePressEventSingleClick(event);

    m_mouseDownMayStartAutoscroll = selectionController().mouseDownMayStartSelect()
        || (m_mousePressNode && m_mousePressNode->layoutBox() && m_mousePressNode->layoutBox()->canBeProgramaticallyScrolled());

    return swallowEvent ? WebInputEventResult::HandledSystem : WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleMouseDraggedEvent(const MouseEventWithHitTestResults& event)
{
    TRACE_EVENT0("blink", "EventHandler::handleMouseDraggedEvent");

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
    if (event.event().button() != LeftButton)
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

    if (m_mouseDownMayStartAutoscroll && !m_scrollManager.panScrollInProgress()) {
        if (AutoscrollController* controller = m_scrollManager.autoscrollController()) {
            controller->startAutoscrollForSelection(layoutObject);
            m_mouseDownMayStartAutoscroll = false;
        }
    }

    selectionController().handleMouseDraggedEvent(event, m_mouseDownPos, m_dragStartPos, m_mousePressNode.get(), m_lastKnownMousePosition);
    return WebInputEventResult::HandledSystem;
}

void EventHandler::updateSelectionForMouseDrag()
{
    selectionController().updateSelectionForMouseDrag(m_mousePressNode.get(), m_dragStartPos, m_lastKnownMousePosition);
}

WebInputEventResult EventHandler::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    AutoscrollController* controller = m_scrollManager.autoscrollController();
    if (controller && controller->autoscrollInProgress())
        m_scrollManager.stopAutoscroll();

    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    m_mousePressed = false;
    m_capturesDragging = false;
    m_mouseDownMayStartDrag = false;
    m_mouseDownMayStartAutoscroll = false;

    return selectionController().handleMouseReleaseEvent(event, m_dragStartPos) ? WebInputEventResult::HandledSystem : WebInputEventResult::NotHandled;
}

#if OS(WIN)

void EventHandler::startPanScrolling(LayoutObject* layoutObject)
{
    if (!layoutObject->isBox())
        return;
    AutoscrollController* controller = m_scrollManager.autoscrollController();
    if (!controller)
        return;
    controller->startPanScrolling(toLayoutBox(layoutObject), lastKnownMousePosition());
    invalidateClick();
}

#endif // OS(WIN)

HitTestResult EventHandler::hitTestResultAtPoint(const LayoutPoint& point, HitTestRequest::HitTestRequestType hitType, const LayoutSize& padding)
{
    TRACE_EVENT0("blink", "EventHandler::hitTestResultAtPoint");

    ASSERT((hitType & HitTestRequest::ListBased) || padding.isEmpty());

    // We always send hitTestResultAtPoint to the main frame if we have one,
    // otherwise we might hit areas that are obscured by higher frames.
    if (m_frame->page()) {
        LocalFrame* mainFrame = m_frame->localFrameRoot();
        if (mainFrame && m_frame != mainFrame) {
            FrameView* frameView = m_frame->view();
            FrameView* mainView = mainFrame->view();
            if (frameView && mainView) {
                IntPoint mainFramePoint = mainView->rootFrameToContents(frameView->contentsToRootFrame(roundedIntPoint(point)));
                return mainFrame->eventHandler().hitTestResultAtPoint(mainFramePoint, hitType, padding);
            }
        }
    }

    // hitTestResultAtPoint is specifically used to hitTest into all frames, thus it always allows child frame content.
    HitTestRequest request(hitType | HitTestRequest::AllowChildFrameContent);
    HitTestResult result(request, point, padding.height(), padding.width(), padding.height(), padding.width());

    // LayoutView::hitTest causes a layout, and we don't want to hit that until the first
    // layout because until then, there is nothing shown on the screen - the user can't
    // have intentionally clicked on something belonging to this page. Furthermore,
    // mousemove events before the first layout should not lead to a premature layout()
    // happening, which could show a flash of white.
    // See also the similar code in Document::prepareMouseEvent.
    if (m_frame->contentLayoutItem().isNull() || !m_frame->view() || !m_frame->view()->didFirstLayout())
        return result;

    m_frame->contentLayoutItem().hitTest(result);
    if (!request.readOnly())
        m_frame->document()->updateHoverActiveState(request, result.innerElement());

    return result;
}

void EventHandler::stopAutoscroll()
{
    m_scrollManager.stopAutoscroll();
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool EventHandler::bubblingScroll(ScrollDirection direction, ScrollGranularity granularity, Node* startingNode)
{
    return m_scrollManager.bubblingScroll(direction, granularity, startingNode, m_mousePressNode);
}

IntPoint EventHandler::lastKnownMousePosition() const
{
    return m_lastKnownMousePosition;
}

IntPoint EventHandler::dragDataTransferLocationForTesting()
{
    if (dragState().m_dragDataTransfer)
        return dragState().m_dragDataTransfer->dragLocation();

    return IntPoint();
}

static LocalFrame* subframeForTargetNode(Node* node)
{
    if (!node)
        return nullptr;

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject || !layoutObject->isLayoutPart())
        return nullptr;

    Widget* widget = toLayoutPart(layoutObject)->widget();
    if (!widget || !widget->isFrameView())
        return nullptr;

    return &toFrameView(widget)->frame();
}

static LocalFrame* subframeForHitTestResult(const MouseEventWithHitTestResults& hitTestResult)
{
    if (!hitTestResult.isOverWidget())
        return nullptr;
    return subframeForTargetNode(hitTestResult.innerNode());
}

static bool isSubmitImage(Node* node)
{
    return isHTMLInputElement(node) && toHTMLInputElement(node)->type() == InputTypeNames::image;
}

bool EventHandler::useHandCursor(Node* node, bool isOverLink)
{
    if (!node)
        return false;

    return ((isOverLink || isSubmitImage(node)) && !node->hasEditableStyle());
}

void EventHandler::cursorUpdateTimerFired(Timer<EventHandler>*)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());

    updateCursor();
}

void EventHandler::updateCursor()
{
    TRACE_EVENT0("input", "EventHandler::updateCursor");

    // We must do a cross-frame hit test because the frame that triggered the cursor
    // update could be occluded by a different frame.
    ASSERT(m_frame == m_frame->localFrameRoot());

    if (m_mousePositionIsUnknown)
        return;

    FrameView* view = m_frame->view();
    if (!view || !view->shouldSetCursor())
        return;

    LayoutViewItem layoutViewItem = view->layoutViewItem();
    if (layoutViewItem.isNull())
        return;

    m_frame->document()->updateStyleAndLayout();

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent);
    HitTestResult result(request, view->rootFrameToContents(m_lastKnownMousePosition));
    layoutViewItem.hitTest(result);

    if (LocalFrame* frame = result.innerNodeFrame()) {
        OptionalCursor optionalCursor = frame->eventHandler().selectCursor(result);
        if (optionalCursor.isCursorChange()) {
            view->setCursor(optionalCursor.cursor());
        }
    }
}

OptionalCursor EventHandler::selectCursor(const HitTestResult& result)
{
    if (m_scrollManager.inResizeMode())
        return NoCursorChange;

    Page* page = m_frame->page();
    if (!page)
        return NoCursorChange;
    if (m_scrollManager.panScrollInProgress())
        return NoCursorChange;

    Node* node = result.innerPossiblyPseudoNode();
    if (!node)
        return selectAutoCursor(result, node, iBeamCursor());

    LayoutObject* layoutObject = node->layoutObject();
    const ComputedStyle* style = layoutObject ? layoutObject->style() : nullptr;

    if (layoutObject) {
        Cursor overrideCursor;
        switch (layoutObject->getCursor(roundedIntPoint(result.localPoint()), overrideCursor)) {
        case SetCursorBasedOnStyle:
            break;
        case SetCursor:
            return overrideCursor;
        case DoNotSetCursor:
            return NoCursorChange;
        }
    }

    if (style && style->cursors()) {
        const CursorList* cursors = style->cursors();
        for (unsigned i = 0; i < cursors->size(); ++i) {
            StyleImage* styleImage = (*cursors)[i].image();
            if (!styleImage)
                continue;
            ImageResource* cachedImage = styleImage->cachedImage();
            if (!cachedImage)
                continue;
            float scale = styleImage->imageScaleFactor();
            bool hotSpotSpecified = (*cursors)[i].hotSpotSpecified();
            // Get hotspot and convert from logical pixels to physical pixels.
            IntPoint hotSpot = (*cursors)[i].hotSpot();
            hotSpot.scale(scale, scale);
            IntSize size = cachedImage->getImage()->size();
            if (cachedImage->errorOccurred())
                continue;
            // Limit the size of cursors (in UI pixels) so that they cannot be
            // used to cover UI elements in chrome.
            size.scale(1 / scale);
            if (size.width() > maximumCursorSize || size.height() > maximumCursorSize)
                continue;

            Image* image = cachedImage->getImage();
            // Ensure no overflow possible in calculations above.
            if (scale < minimumCursorScale)
                continue;
            return Cursor(image, hotSpotSpecified, hotSpot, scale);
        }
    }

    switch (style ? style->cursor() : CURSOR_AUTO) {
    case CURSOR_AUTO: {
        bool horizontalText = !style || style->isHorizontalWritingMode();
        const Cursor& iBeam = horizontalText ? iBeamCursor() : verticalTextCursor();
        return selectAutoCursor(result, node, iBeam);
    }
    case CURSOR_CROSS:
        return crossCursor();
    case CURSOR_POINTER:
        return handCursor();
    case CURSOR_MOVE:
        return moveCursor();
    case CURSOR_ALL_SCROLL:
        return moveCursor();
    case CURSOR_E_RESIZE:
        return eastResizeCursor();
    case CURSOR_W_RESIZE:
        return westResizeCursor();
    case CURSOR_N_RESIZE:
        return northResizeCursor();
    case CURSOR_S_RESIZE:
        return southResizeCursor();
    case CURSOR_NE_RESIZE:
        return northEastResizeCursor();
    case CURSOR_SW_RESIZE:
        return southWestResizeCursor();
    case CURSOR_NW_RESIZE:
        return northWestResizeCursor();
    case CURSOR_SE_RESIZE:
        return southEastResizeCursor();
    case CURSOR_NS_RESIZE:
        return northSouthResizeCursor();
    case CURSOR_EW_RESIZE:
        return eastWestResizeCursor();
    case CURSOR_NESW_RESIZE:
        return northEastSouthWestResizeCursor();
    case CURSOR_NWSE_RESIZE:
        return northWestSouthEastResizeCursor();
    case CURSOR_COL_RESIZE:
        return columnResizeCursor();
    case CURSOR_ROW_RESIZE:
        return rowResizeCursor();
    case CURSOR_TEXT:
        return iBeamCursor();
    case CURSOR_WAIT:
        return waitCursor();
    case CURSOR_HELP:
        return helpCursor();
    case CURSOR_VERTICAL_TEXT:
        return verticalTextCursor();
    case CURSOR_CELL:
        return cellCursor();
    case CURSOR_CONTEXT_MENU:
        return contextMenuCursor();
    case CURSOR_PROGRESS:
        return progressCursor();
    case CURSOR_NO_DROP:
        return noDropCursor();
    case CURSOR_ALIAS:
        return aliasCursor();
    case CURSOR_COPY:
        return copyCursor();
    case CURSOR_NONE:
        return noneCursor();
    case CURSOR_NOT_ALLOWED:
        return notAllowedCursor();
    case CURSOR_DEFAULT:
        return pointerCursor();
    case CURSOR_ZOOM_IN:
        return zoomInCursor();
    case CURSOR_ZOOM_OUT:
        return zoomOutCursor();
    case CURSOR_WEBKIT_GRAB:
        return grabCursor();
    case CURSOR_WEBKIT_GRABBING:
        return grabbingCursor();
    }
    return pointerCursor();
}

OptionalCursor EventHandler::selectAutoCursor(const HitTestResult& result, Node* node, const Cursor& iBeam)
{
    bool editable = (node && node->hasEditableStyle());

    const bool isOverLink = !selectionController().mouseDownMayStartSelect() && result.isOverLink();
    if (useHandCursor(node, isOverLink))
        return handCursor();

    bool inResizer = false;
    LayoutObject* layoutObject = node ? node->layoutObject() : nullptr;
    if (layoutObject && m_frame->view()) {
        PaintLayer* layer = layoutObject->enclosingLayer();
        inResizer = layer->getScrollableArea() && layer->getScrollableArea()->isPointInResizeControl(result.roundedPointInMainFrame(), ResizerForPointer);
    }

    // During selection, use an I-beam no matter what we're over.
    // If a drag may be starting or we're capturing mouse events for a particular node, don't treat this as a selection.
    if (m_mousePressed && selectionController().mouseDownMayStartSelect()
        && !m_mouseDownMayStartDrag
        && !m_frame->selection().isNone()
        && !m_capturingMouseEventsNode) {
        return iBeam;
    }

    if ((editable || (layoutObject && layoutObject->isText() && node->canStartSelection())) && !inResizer && !result.scrollbar())
        return iBeam;
    return pointerCursor();
}

static LayoutPoint contentPointFromRootFrame(LocalFrame* frame, const IntPoint& pointInRootFrame)
{
    FrameView* view = frame->view();
    // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
    // Historically the code would just crash; this is clearly no worse than that.
    return view ? view->rootFrameToContents(pointInRootFrame) : pointInRootFrame;
}

WebInputEventResult EventHandler::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    TRACE_EVENT0("blink", "EventHandler::handleMousePressEvent");

    // For 4th/5th button in the mouse since Chrome does not yet send
    // button value to Blink but in some cases it does send the event.
    // This check is needed to suppress such an event (crbug.com/574959)
    if (mouseEvent.button() == NoButton)
        return WebInputEventResult::HandledSuppressed;

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    m_frame->localFrameRoot()->eventHandler().m_lastMouseDownUserGestureToken = UserGestureIndicator::currentToken();

    cancelFakeMouseMoveEvent();
    if (m_eventHandlerWillResetCapturingMouseEventsNode)
        m_capturingMouseEventsNode = nullptr;
    m_mousePressed = true;
    m_capturesDragging = true;
    setLastKnownMousePosition(mouseEvent);
    m_mouseDownTimestamp = mouseEvent.timestamp();
    m_mouseDownMayStartDrag = false;
    selectionController().setMouseDownMayStartSelect(false);
    m_mouseDownMayStartAutoscroll = false;
    if (FrameView* view = m_frame->view()) {
        m_mouseDownPos = view->rootFrameToContents(mouseEvent.position());
    } else {
        invalidateClick();
        return WebInputEventResult::NotHandled;
    }

    HitTestRequest request(HitTestRequest::Active);
    // Save the document point we generate in case the window coordinate is invalidated by what happens
    // when we dispatch the event.
    LayoutPoint documentPoint = contentPointFromRootFrame(m_frame, mouseEvent.position());
    MouseEventWithHitTestResults mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);

    if (!mev.innerNode()) {
        invalidateClick();
        return WebInputEventResult::NotHandled;
    }

    m_mousePressNode = mev.innerNode();
    m_frame->document()->setSequentialFocusNavigationStartingPoint(mev.innerNode());

    LocalFrame* subframe = subframeForHitTestResult(mev);
    if (subframe) {
        WebInputEventResult result = passMousePressEventToSubframe(mev, subframe);
        // Start capturing future events for this frame.  We only do this if we didn't clear
        // the m_mousePressed flag, which may happen if an AppKit widget entered a modal event loop.
        // The capturing should be done only when the result indicates it
        // has been handled. See crbug.com/269917
        m_capturesDragging = subframe->eventHandler().capturesDragging();
        if (m_mousePressed && m_capturesDragging) {
            m_capturingMouseEventsNode = mev.innerNode();
            m_eventHandlerWillResetCapturingMouseEventsNode = true;
        }
        invalidateClick();
        return result;
    }

#if OS(WIN)
    // We store whether pan scrolling is in progress before calling stopAutoscroll()
    // because it will set m_autoscrollType to NoAutoscroll on return.
    bool isPanScrollInProgress = m_scrollManager.panScrollInProgress();
    m_scrollManager.stopAutoscroll();
    if (isPanScrollInProgress) {
        // We invalidate the click when exiting pan scrolling so that we don't inadvertently navigate
        // away from the current page (e.g. the click was on a hyperlink). See <rdar://problem/6095023>.
        invalidateClick();
        return WebInputEventResult::HandledSuppressed;
    }
#endif

    m_clickCount = mouseEvent.clickCount();
    m_clickNode = mev.innerNode()->isTextNode() ?  FlatTreeTraversal::parent(*mev.innerNode()) : mev.innerNode();

    if (!mouseEvent.fromTouch())
        m_frame->selection().setCaretBlinkingSuspended(true);

    WebInputEventResult eventResult = updatePointerTargetAndDispatchEvents(EventTypeNames::mousedown, mev.innerNode(), m_clickCount, mev.event());

    if (eventResult == WebInputEventResult::NotHandled && m_frame->view()) {
        FrameView* view = m_frame->view();
        PaintLayer* layer = mev.innerNode()->layoutObject() ? mev.innerNode()->layoutObject()->enclosingLayer() : nullptr;
        IntPoint p = view->rootFrameToContents(mouseEvent.position());
        if (layer && layer->getScrollableArea() && layer->getScrollableArea()->isPointInResizeControl(p, ResizerForPointer)) {
            m_scrollManager.setResizeScrollableArea(layer, p);
            return WebInputEventResult::HandledSystem;
        }
    }

    // m_selectionInitiationState is initialized after dispatching mousedown
    // event in order not to keep the selection by DOM APIs Because we can't
    // give the user the chance to handle the selection by user action like
    // dragging if we keep the selection in case of mousedown. FireFox also has
    // the same behavior and it's more compatible with other browsers.
    selectionController().initializeSelectionState();
    HitTestResult hitTestResult = hitTestResultInFrame(m_frame, documentPoint, HitTestRequest::ReadOnly);
    InputDeviceCapabilities* sourceCapabilities = mouseEvent.getSyntheticEventType() == PlatformMouseEvent::FromTouch ? InputDeviceCapabilities::firesTouchEventsSourceCapabilities() :
        InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities();
    if (eventResult == WebInputEventResult::NotHandled)
        eventResult = handleMouseFocus(MouseEventWithHitTestResults(mev.event(), hitTestResult), sourceCapabilities);
    m_capturesDragging = eventResult == WebInputEventResult::NotHandled || mev.scrollbar();

    // If the hit testing originally determined the event was in a scrollbar, refetch the MouseEventWithHitTestResults
    // in case the scrollbar widget was destroyed when the mouse event was handled.
    if (mev.scrollbar()) {
        const bool wasLastScrollBar = mev.scrollbar() == m_lastScrollbarUnderMouse.get();
        HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
        mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);
        if (wasLastScrollBar && mev.scrollbar() != m_lastScrollbarUnderMouse.get())
            m_lastScrollbarUnderMouse = nullptr;
    }

    if (eventResult != WebInputEventResult::NotHandled) {
        // scrollbars should get events anyway, even disabled controls might be scrollable
        passMousePressEventToScrollbar(mev);
    } else {
        if (shouldRefetchEventTarget(mev)) {
            HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
            mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);
        }

        if (passMousePressEventToScrollbar(mev))
            eventResult = WebInputEventResult::HandledSystem;
        else
            eventResult = handleMousePressEvent(mev);
    }

    if (mev.hitTestResult().innerNode() && mouseEvent.button() == LeftButton) {
        ASSERT(mouseEvent.type() == PlatformEvent::MousePressed);
        HitTestResult result = mev.hitTestResult();
        result.setToShadowHostIfInUserAgentShadowRoot();
        m_frame->chromeClient().onMouseDown(result.innerNode());
    }

    return eventResult;
}

static PaintLayer* layerForNode(Node* node)
{
    if (!node)
        return nullptr;

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return nullptr;

    PaintLayer* layer = layoutObject->enclosingLayer();
    if (!layer)
        return nullptr;

    return layer;
}

ScrollableArea* EventHandler::associatedScrollableArea(const PaintLayer* layer) const
{
    if (PaintLayerScrollableArea* scrollableArea = layer->getScrollableArea()) {
        if (scrollableArea->scrollsOverflow())
            return scrollableArea;
    }

    return nullptr;
}

WebInputEventResult EventHandler::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    TRACE_EVENT0("blink", "EventHandler::handleMouseMoveEvent");

    HitTestResult hoveredNode = HitTestResult();
    WebInputEventResult result = handleMouseMoveOrLeaveEvent(event, &hoveredNode);

    Page* page = m_frame->page();
    if (!page)
        return result;

    if (PaintLayer* layer = layerForNode(hoveredNode.innerNode())) {
        if (ScrollableArea* layerScrollableArea = associatedScrollableArea(layer))
            layerScrollableArea->mouseMovedInContentArea();
    }

    if (FrameView* frameView = m_frame->view())
        frameView->mouseMovedInContentArea();

    hoveredNode.setToShadowHostIfInUserAgentShadowRoot();
    page->chromeClient().mouseDidMoveOverElement(hoveredNode);

    return result;
}

void EventHandler::handleMouseLeaveEvent(const PlatformMouseEvent& event)
{
    TRACE_EVENT0("blink", "EventHandler::handleMouseLeaveEvent");

    handleMouseMoveOrLeaveEvent(event, 0, false, true);
}

WebInputEventResult EventHandler::handleMouseMoveOrLeaveEvent(const PlatformMouseEvent& mouseEvent, HitTestResult* hoveredNode, bool onlyUpdateScrollbars, bool forceLeave)
{
    ASSERT(m_frame);
    ASSERT(m_frame->view());

    setLastKnownMousePosition(mouseEvent);

    if (m_hoverTimer.isActive())
        m_hoverTimer.stop();

    m_cursorUpdateTimer.stop();

    cancelFakeMouseMoveEvent();

    if (m_svgPan) {
        m_frame->document()->accessSVGExtensions().updatePan(m_frame->view()->rootFrameToContents(m_lastKnownMousePosition));
        return WebInputEventResult::HandledSuppressed;
    }

    if (m_frameSetBeingResized)
        return updatePointerTargetAndDispatchEvents(EventTypeNames::mousemove, m_frameSetBeingResized.get(), 0, mouseEvent);

    // Send events right to a scrollbar if the mouse is pressed.
    if (m_lastScrollbarUnderMouse && m_mousePressed) {
        m_lastScrollbarUnderMouse->mouseMoved(mouseEvent);
        return WebInputEventResult::HandledSystem;
    }

    // Mouse events simulated from touch should not hit-test again.
    ASSERT(!mouseEvent.fromTouch());

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move;
    if (m_mousePressed) {
        hitType |= HitTestRequest::Active;
    } else if (onlyUpdateScrollbars) {
        // Mouse events should be treated as "read-only" if we're updating only scrollbars. This
        // means that :hover and :active freeze in the state they were in, rather than updating
        // for nodes the mouse moves while the window is not key (which will be the case if
        // onlyUpdateScrollbars is true).
        hitType |= HitTestRequest::ReadOnly;
    }

    // Treat any mouse move events as readonly if the user is currently touching the screen.
    if (m_pointerEventManager.isAnyTouchActive())
        hitType |= HitTestRequest::Active | HitTestRequest::ReadOnly;
    HitTestRequest request(hitType);
    MouseEventWithHitTestResults mev = MouseEventWithHitTestResults(mouseEvent, HitTestResult(request, LayoutPoint()));

    // We don't want to do a hit-test in forceLeave scenarios because there might actually be some other frame above this one at the specified co-ordinate.
    // So we must force the hit-test to fail, while still clearing hover/active state.
    if (forceLeave)
        m_frame->document()->updateHoverActiveState(request, 0);
    else
        mev = prepareMouseEvent(request, mouseEvent);

    if (hoveredNode)
        *hoveredNode = mev.hitTestResult();

    Scrollbar* scrollbar = nullptr;

    if (m_scrollManager.inResizeMode()) {
        m_scrollManager.resize(mev.event());
    } else {
        if (!scrollbar)
            scrollbar = mev.scrollbar();

        updateLastScrollbarUnderMouse(scrollbar, !m_mousePressed);
        if (onlyUpdateScrollbars)
            return WebInputEventResult::HandledSuppressed;
    }

    WebInputEventResult eventResult = WebInputEventResult::NotHandled;
    LocalFrame* newSubframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);

    // We want mouseouts to happen first, from the inside out.  First send a move event to the last subframe so that it will fire mouseouts.
    if (m_lastMouseMoveEventSubframe && m_lastMouseMoveEventSubframe->tree().isDescendantOf(m_frame) && m_lastMouseMoveEventSubframe != newSubframe)
        m_lastMouseMoveEventSubframe->eventHandler().handleMouseLeaveEvent(mev.event());

    if (newSubframe) {
        // Update over/out state before passing the event to the subframe.
        updateMouseEventTargetNodeAndSendEvents(mev.innerNode(), mev.event(), true);

        // Event dispatch in updateMouseEventTargetNodeAndSendEvents may have caused the subframe of the target
        // node to be detached from its FrameView, in which case the event should not be passed.
        if (newSubframe->view())
            eventResult = passMouseMoveEventToSubframe(mev, newSubframe, hoveredNode);
    } else {
        if (scrollbar && !m_mousePressed)
            scrollbar->mouseMoved(mev.event()); // Handle hover effects on platforms that support visual feedback on scrollbar hovering.
        if (FrameView* view = m_frame->view()) {
            OptionalCursor optionalCursor = selectCursor(mev.hitTestResult());
            if (optionalCursor.isCursorChange()) {
                view->setCursor(optionalCursor.cursor());
            }
        }
    }

    m_lastMouseMoveEventSubframe = newSubframe;

    if (eventResult != WebInputEventResult::NotHandled)
        return eventResult;

    eventResult = updatePointerTargetAndDispatchEvents(EventTypeNames::mousemove, mev.innerNode(), 0, mev.event());
    if (eventResult != WebInputEventResult::NotHandled)
        return eventResult;

    return handleMouseDraggedEvent(mev);
}

void EventHandler::invalidateClick()
{
    m_clickCount = 0;
    m_clickNode = nullptr;
}

static ContainerNode* parentForClickEvent(const Node& node)
{
    // IE doesn't dispatch click events for mousedown/mouseup events across form
    // controls.
    if (node.isHTMLElement() && toHTMLElement(node).isInteractiveContent())
        return nullptr;

    return FlatTreeTraversal::parent(node);
}

WebInputEventResult EventHandler::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    TRACE_EVENT0("blink", "EventHandler::handleMouseReleaseEvent");

    // For 4th/5th button in the mouse since Chrome does not yet send
    // button value to Blink but in some cases it does send the event.
    // This check is needed to suppress such an event (crbug.com/574959)
    if (mouseEvent.button() == NoButton)
        return WebInputEventResult::HandledSuppressed;

    if (!mouseEvent.fromTouch())
        m_frame->selection().setCaretBlinkingSuspended(false);

    std::unique_ptr<UserGestureIndicator> gestureIndicator;

    if (m_frame->localFrameRoot()->eventHandler().m_lastMouseDownUserGestureToken)
        gestureIndicator = wrapUnique(new UserGestureIndicator(m_frame->localFrameRoot()->eventHandler().m_lastMouseDownUserGestureToken.release()));
    else
        gestureIndicator = wrapUnique(new UserGestureIndicator(DefinitelyProcessingUserGesture));

#if OS(WIN)
    if (Page* page = m_frame->page())
        page->autoscrollController().handleMouseReleaseForPanScrolling(m_frame, mouseEvent);
#endif

    m_mousePressed = false;
    setLastKnownMousePosition(mouseEvent);

    if (m_svgPan) {
        m_svgPan = false;
        m_frame->document()->accessSVGExtensions().updatePan(m_frame->view()->rootFrameToContents(m_lastKnownMousePosition));
        return WebInputEventResult::HandledSuppressed;
    }

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(EventTypeNames::mouseup, m_frameSetBeingResized.get(), m_clickCount, mouseEvent);

    if (m_lastScrollbarUnderMouse) {
        invalidateClick();
        m_lastScrollbarUnderMouse->mouseUp(mouseEvent);
        return updatePointerTargetAndDispatchEvents(EventTypeNames::mouseup, m_nodeUnderMouse.get(), m_clickCount, mouseEvent);
    }

    // Mouse events simulated from touch should not hit-test again.
    ASSERT(!mouseEvent.fromTouch());

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Release;
    HitTestRequest request(hitType);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);
    LocalFrame* subframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);
    if (m_eventHandlerWillResetCapturingMouseEventsNode)
        m_capturingMouseEventsNode = nullptr;
    if (subframe)
        return passMouseReleaseEventToSubframe(mev, subframe);

    WebInputEventResult eventResult = updatePointerTargetAndDispatchEvents(EventTypeNames::mouseup, mev.innerNode(), m_clickCount, mev.event());

    bool contextMenuEvent = mouseEvent.button() == RightButton;
#if OS(MACOSX)
    // FIXME: The Mac port achieves the same behavior by checking whether the context menu is currently open in WebPage::mouseEvent(). Consider merging the implementations.
    if (mouseEvent.button() == LeftButton && mouseEvent.getModifiers() & PlatformEvent::CtrlKey)
        contextMenuEvent = true;
#endif

    WebInputEventResult clickEventResult = WebInputEventResult::NotHandled;
    const bool shouldDispatchClickEvent = m_clickCount > 0
        && !contextMenuEvent
        && mev.innerNode() && m_clickNode
        && mev.innerNode()->canParticipateInFlatTree() && m_clickNode->canParticipateInFlatTree()
        && !(selectionController().hasExtendedSelection() && isLinkSelection(mev));
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
                *m_clickNode, parentForClickEvent);
        }
        if (clickTargetNode) {
            // Dispatch mouseup directly w/o calling updateMouseEventTargetNodeAndSendEvents
            // because the mouseup dispatch above has already updated it
            // correctly. Moreover, clickTargetNode is different from
            // mev.innerNode at drag-release.
            clickEventResult = toWebInputEventResult(clickTargetNode->dispatchMouseEvent(mev.event(),
                EventTypeNames::click, m_clickCount));
        }
    }

    m_scrollManager.clearResizeScrollableArea(false);

    if (eventResult == WebInputEventResult::NotHandled)
        eventResult = handleMouseReleaseEvent(mev);

    invalidateClick();

    return mergeEventResult(clickEventResult, eventResult);
}

WebInputEventResult EventHandler::dispatchDragEvent(const AtomicString& eventType, Node* dragTarget, const PlatformMouseEvent& event, DataTransfer* dataTransfer)
{
    FrameView* view = m_frame->view();

    // FIXME: We might want to dispatch a dragleave even if the view is gone.
    if (!view)
        return WebInputEventResult::NotHandled;

    DragEvent* me = DragEvent::create(eventType,
        true, true, m_frame->document()->domWindow(),
        0, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y(),
        event.movementDelta().x(), event.movementDelta().y(),
        event.getModifiers(),
        0, MouseEvent::platformModifiersToButtons(event.getModifiers()), nullptr, event.timestamp(), dataTransfer, event.getSyntheticEventType());

    return toWebInputEventResult(dragTarget->dispatchEvent(me));
}

static bool targetIsFrame(Node* target, LocalFrame*& frame)
{
    if (!isHTMLFrameElementBase(target))
        return false;

    // Cross-process drag and drop is not yet supported.
    if (toHTMLFrameElementBase(target)->contentFrame() && !toHTMLFrameElementBase(target)->contentFrame()->isLocalFrame())
        return false;

    frame = toLocalFrame(toHTMLFrameElementBase(target)->contentFrame());
    return true;
}

static bool findDropZone(Node* target, DataTransfer* dataTransfer)
{
    Element* element = target->isElementNode() ? toElement(target) : target->parentElement();
    for (; element; element = element->parentElement()) {
        bool matched = false;
        AtomicString dropZoneStr = element->fastGetAttribute(webkitdropzoneAttr);

        if (dropZoneStr.isEmpty())
            continue;

        UseCounter::count(element->document(), UseCounter::PrefixedHTMLElementDropzone);

        dropZoneStr = dropZoneStr.lower();

        SpaceSplitString keywords(dropZoneStr, SpaceSplitString::ShouldNotFoldCase);
        if (keywords.isNull())
            continue;

        DragOperation dragOperation = DragOperationNone;
        for (unsigned i = 0; i < keywords.size(); i++) {
            DragOperation op = convertDropZoneOperationToDragOperation(keywords[i]);
            if (op != DragOperationNone) {
                if (dragOperation == DragOperationNone)
                    dragOperation = op;
            } else {
                matched = matched || dataTransfer->hasDropZoneType(keywords[i].getString());
            }

            if (matched && dragOperation != DragOperationNone)
                break;
        }
        if (matched) {
            dataTransfer->setDropEffect(convertDragOperationToDropZoneOperation(dragOperation));
            return true;
        }
    }
    return false;
}

WebInputEventResult EventHandler::updateDragAndDrop(const PlatformMouseEvent& event, DataTransfer* dataTransfer)
{
    WebInputEventResult eventResult = WebInputEventResult::NotHandled;

    if (!m_frame->view())
        return eventResult;

    HitTestRequest request(HitTestRequest::ReadOnly);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, event);

    // Drag events should never go to text nodes (following IE, and proper mouseover/out dispatch)
    Node* newTarget = mev.innerNode();
    if (newTarget && newTarget->isTextNode())
        newTarget = FlatTreeTraversal::parent(*newTarget);

    if (AutoscrollController* controller = m_scrollManager.autoscrollController())
        controller->updateDragAndDrop(newTarget, event.position(), event.timestamp());

    if (m_dragTarget != newTarget) {
        // FIXME: this ordering was explicitly chosen to match WinIE. However,
        // it is sometimes incorrect when dragging within subframes, as seen with
        // LayoutTests/fast/events/drag-in-frames.html.
        //
        // Moreover, this ordering conforms to section 7.9.4 of the HTML 5 spec. <http://dev.w3.org/html5/spec/Overview.html#drag-and-drop-processing-model>.
        LocalFrame* targetFrame;
        if (targetIsFrame(newTarget, targetFrame)) {
            if (targetFrame)
                eventResult = targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
        } else if (newTarget) {
            // As per section 7.9.4 of the HTML 5 spec., we must always fire a drag event before firing a dragenter, dragleave, or dragover event.
            if (dragState().m_dragSrc) {
                // for now we don't care if event handler cancels default behavior, since there is none
                dispatchDragSrcEvent(EventTypeNames::drag, event);
            }
            eventResult = dispatchDragEvent(EventTypeNames::dragenter, newTarget, event, dataTransfer);
            if (eventResult == WebInputEventResult::NotHandled && findDropZone(newTarget, dataTransfer))
                eventResult = WebInputEventResult::HandledSystem;
        }

        if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
            if (targetFrame)
                eventResult = targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
        } else if (m_dragTarget) {
            dispatchDragEvent(EventTypeNames::dragleave, m_dragTarget.get(), event, dataTransfer);
        }

        if (newTarget) {
            // We do not explicitly call dispatchDragEvent here because it could ultimately result in the appearance that
            // two dragover events fired. So, we mark that we should only fire a dragover event on the next call to this function.
            m_shouldOnlyFireDragOverEvent = true;
        }
    } else {
        LocalFrame* targetFrame;
        if (targetIsFrame(newTarget, targetFrame)) {
            if (targetFrame)
                eventResult = targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
        } else if (newTarget) {
            // Note, when dealing with sub-frames, we may need to fire only a dragover event as a drag event may have been fired earlier.
            if (!m_shouldOnlyFireDragOverEvent && dragState().m_dragSrc) {
                // for now we don't care if event handler cancels default behavior, since there is none
                dispatchDragSrcEvent(EventTypeNames::drag, event);
            }
            eventResult = dispatchDragEvent(EventTypeNames::dragover, newTarget, event, dataTransfer);
            if (eventResult == WebInputEventResult::NotHandled && findDropZone(newTarget, dataTransfer))
                eventResult = WebInputEventResult::HandledSystem;
            m_shouldOnlyFireDragOverEvent = false;
        }
    }
    m_dragTarget = newTarget;

    return eventResult;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event, DataTransfer* dataTransfer)
{
    LocalFrame* targetFrame;
    if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
        if (targetFrame)
            targetFrame->eventHandler().cancelDragAndDrop(event, dataTransfer);
    } else if (m_dragTarget.get()) {
        if (dragState().m_dragSrc)
            dispatchDragSrcEvent(EventTypeNames::drag, event);
        dispatchDragEvent(EventTypeNames::dragleave, m_dragTarget.get(), event, dataTransfer);
    }
    clearDragState();
}

WebInputEventResult EventHandler::performDragAndDrop(const PlatformMouseEvent& event, DataTransfer* dataTransfer)
{
    LocalFrame* targetFrame;
    WebInputEventResult result = WebInputEventResult::NotHandled;
    if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
        if (targetFrame)
            result = targetFrame->eventHandler().performDragAndDrop(event, dataTransfer);
    } else if (m_dragTarget.get()) {
        result = dispatchDragEvent(EventTypeNames::drop, m_dragTarget.get(), event, dataTransfer);
    }
    clearDragState();
    return result;
}

void EventHandler::clearDragState()
{
    m_scrollManager.stopAutoscroll();
    m_dragTarget = nullptr;
    m_capturingMouseEventsNode = nullptr;
    m_shouldOnlyFireDragOverEvent = false;
}

void EventHandler::setCapturingMouseEventsNode(Node* n)
{
    m_capturingMouseEventsNode = n;
    m_eventHandlerWillResetCapturingMouseEventsNode = false;
}

MouseEventWithHitTestResults EventHandler::prepareMouseEvent(const HitTestRequest& request, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());

    return m_frame->document()->prepareMouseEvent(request, contentPointFromRootFrame(m_frame, mev.position()), mev);
}

Node* EventHandler::updateMouseEventTargetNode(Node* targetNode,
    const PlatformMouseEvent& mouseEvent)
{
    Node* result = targetNode;

    // If we're capturing, we always go right to that node.
    if (m_capturingMouseEventsNode) {
        result = m_capturingMouseEventsNode.get();
    } else {
        // If the target node is a text node, dispatch on the parent node - rdar://4196646
        if (result && result->isTextNode())
            result = FlatTreeTraversal::parent(*result);
    }
    Node* lastNodeUnderMouse = m_nodeUnderMouse;
    m_nodeUnderMouse = result;

    PaintLayer* layerForLastNode = layerForNode(lastNodeUnderMouse);
    PaintLayer* layerForNodeUnderMouse = layerForNode(m_nodeUnderMouse.get());
    Page* page = m_frame->page();

    if (lastNodeUnderMouse && (!m_nodeUnderMouse || m_nodeUnderMouse->document() != m_frame->document())) {
        // The mouse has moved between frames.
        if (LocalFrame* frame = lastNodeUnderMouse->document().frame()) {
            if (FrameView* frameView = frame->view())
                frameView->mouseExitedContentArea();
        }
    } else if (page && (layerForLastNode && (!layerForNodeUnderMouse || layerForNodeUnderMouse != layerForLastNode))) {
        // The mouse has moved between layers.
        if (ScrollableArea* scrollableAreaForLastNode = associatedScrollableArea(layerForLastNode))
            scrollableAreaForLastNode->mouseExitedContentArea();
    }

    if (m_nodeUnderMouse && (!lastNodeUnderMouse || lastNodeUnderMouse->document() != m_frame->document())) {
        // The mouse has moved between frames.
        if (LocalFrame* frame = m_nodeUnderMouse->document().frame()) {
            if (FrameView* frameView = frame->view())
                frameView->mouseEnteredContentArea();
        }
    } else if (page && (layerForNodeUnderMouse && (!layerForLastNode || layerForNodeUnderMouse != layerForLastNode))) {
        // The mouse has moved between layers.
        if (ScrollableArea* scrollableAreaForNodeUnderMouse = associatedScrollableArea(layerForNodeUnderMouse))
            scrollableAreaForNodeUnderMouse->mouseEnteredContentArea();
    }

    if (lastNodeUnderMouse && lastNodeUnderMouse->document() != m_frame->document()) {
        lastNodeUnderMouse = nullptr;
        m_lastScrollbarUnderMouse = nullptr;
    }

    return lastNodeUnderMouse;
}

void EventHandler::updateMouseEventTargetNodeAndSendEvents(Node* targetNode,
    const PlatformMouseEvent& mouseEvent, bool isFrameBoundaryTransition)
{
    Node* lastNodeUnderMouse = updateMouseEventTargetNode(targetNode, mouseEvent);
    m_pointerEventManager.sendMouseAndPossiblyPointerBoundaryEvents(
        lastNodeUnderMouse, m_nodeUnderMouse, mouseEvent,
        isFrameBoundaryTransition);
}

WebInputEventResult EventHandler::dispatchMouseEvent(const AtomicString& eventType, Node* targetNode, int clickCount, const PlatformMouseEvent& mouseEvent)
{
    updateMouseEventTargetNodeAndSendEvents(targetNode, mouseEvent);
    if (!m_nodeUnderMouse)
        return WebInputEventResult::NotHandled;

    MouseEvent* event = MouseEvent::create(eventType, m_nodeUnderMouse->document().domWindow(), mouseEvent, clickCount, nullptr);
    return toWebInputEventResult(m_nodeUnderMouse->dispatchEvent(event));
}

bool EventHandler::isPointerEventActive(int pointerId)
{
    return m_pointerEventManager.isActive(pointerId);
}

void EventHandler::setPointerCapture(int pointerId, EventTarget* target)
{
    // TODO(crbug.com/591387): This functionality should be per page not per frame.
    m_pointerEventManager.setPointerCapture(pointerId, target);
}

void EventHandler::releasePointerCapture(int pointerId, EventTarget* target)
{
    m_pointerEventManager.releasePointerCapture(pointerId, target);
}

void EventHandler::elementRemoved(EventTarget* target)
{
    m_pointerEventManager.elementRemoved(target);
}

// TODO(mustaq): Make PE drive ME dispatch & bookkeeping in EventHandler.
WebInputEventResult EventHandler::updatePointerTargetAndDispatchEvents(const AtomicString& mouseEventType, Node* targetNode, int clickCount, const PlatformMouseEvent& mouseEvent)
{
    ASSERT(mouseEventType == EventTypeNames::mousedown
        || mouseEventType == EventTypeNames::mousemove
        || mouseEventType == EventTypeNames::mouseup);

    Node* lastNodeUnderMouse = updateMouseEventTargetNode(targetNode, mouseEvent);

    return m_pointerEventManager.sendMousePointerEvent(
        m_nodeUnderMouse, mouseEventType, clickCount, mouseEvent, nullptr,
        lastNodeUnderMouse);
}

WebInputEventResult EventHandler::handleMouseFocus(const MouseEventWithHitTestResults& targetedEvent, InputDeviceCapabilities* sourceCapabilities)
{
    // If clicking on a frame scrollbar, do not mess up with content focus.
    if (targetedEvent.hitTestResult().scrollbar() && !m_frame->contentLayoutItem().isNull()) {
        if (targetedEvent.hitTestResult().scrollbar()->getScrollableArea() == m_frame->contentLayoutItem().getScrollableArea())
            return WebInputEventResult::NotHandled;
    }

    // The layout needs to be up to date to determine if an element is focusable.
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    Element* element = nullptr;
    if (m_nodeUnderMouse)
        element = m_nodeUnderMouse->isElementNode() ? toElement(m_nodeUnderMouse) : m_nodeUnderMouse->parentOrShadowHostElement();
    for (; element; element = element->parentOrShadowHostElement()) {
        if (element->isFocusable() && element->isFocusedElementInDocument())
            return WebInputEventResult::NotHandled;
        if (element->isMouseFocusable())
            break;
    }
    ASSERT(!element || element->isMouseFocusable());

    // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus
    // a node on mouse down if it's selected and inside a focused node. It will
    // be focused if the user does a mouseup over it, however, because the
    // mouseup will set a selection inside it, which will call
    // FrameSelection::setFocusedNodeIfNeeded.
    if (element && m_frame->selection().isRange()) {
        // TODO(yosin) We should not create |Range| object for calling
        // |isNodeFullyContained()|.
        if (createRange(m_frame->selection().selection().toNormalizedEphemeralRange())->isNodeFullyContained(*element)
            && element->isDescendantOf(m_frame->document()->focusedElement()))
            return WebInputEventResult::NotHandled;
    }


    // Only change the focus when clicking scrollbars if it can transfered to a
    // mouse focusable node.
    if (!element && targetedEvent.hitTestResult().scrollbar())
        return WebInputEventResult::HandledSystem;

    if (Page* page = m_frame->page()) {
        // If focus shift is blocked, we eat the event. Note we should never
        // clear swallowEvent if the page already set it (e.g., by canceling
        // default behavior).
        if (element) {
            if (slideFocusOnShadowHostIfNecessary(*element))
                return WebInputEventResult::HandledSystem;
            if (!page->focusController().setFocusedElement(element, m_frame, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeMouse, sourceCapabilities)))
                return WebInputEventResult::HandledSystem;
        } else {
            // We call setFocusedElement even with !element in order to blur
            // current focus element when a link is clicked; this is expected by
            // some sites that rely on onChange handlers running from form
            // fields before the button click is processed.
            if (!page->focusController().setFocusedElement(nullptr, m_frame, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, sourceCapabilities)))
                return WebInputEventResult::HandledSystem;
        }
    }

    return WebInputEventResult::NotHandled;
}

bool EventHandler::slideFocusOnShadowHostIfNecessary(const Element& element)
{
    if (element.authorShadowRoot() && element.authorShadowRoot()->delegatesFocus()) {
        Document* doc = m_frame->document();
        if (element.isShadowIncludingInclusiveAncestorOf(doc->focusedElement())) {
            // If the inner element is already focused, do nothing.
            return true;
        }

        // If the host has a focusable inner element, focus it. Otherwise, the host takes focus.
        Page* page = m_frame->page();
        ASSERT(page);
        Element* found = page->focusController().findFocusableElementInShadowHost(element);
        if (found && element.isShadowIncludingInclusiveAncestorOf(found)) {
            // Use WebFocusTypeForward instead of WebFocusTypeMouse here to mean the focus has slided.
            found->focus(FocusParams(SelectionBehaviorOnFocus::Reset, WebFocusTypeForward, nullptr));
            return true;
        }
    }
    return false;
}

WebInputEventResult EventHandler::handleWheelEvent(const PlatformWheelEvent& event)
{
    Document* doc = m_frame->document();

    if (doc->layoutViewItem().isNull())
        return WebInputEventResult::NotHandled;

    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;

    LayoutPoint vPoint = view->rootFrameToContents(event.position());

    HitTestRequest request(HitTestRequest::ReadOnly);
    HitTestResult result(request, vPoint);
    doc->layoutViewItem().hitTest(result);

    Node* node = result.innerNode();
    // Wheel events should not dispatch to text nodes.
    if (node && node->isTextNode())
        node = FlatTreeTraversal::parent(*node);

    // If we're over the frame scrollbar, scroll the document.
    if (!node && result.scrollbar())
        node = doc->documentElement();

    LocalFrame* subframe = subframeForTargetNode(node);
    if (subframe) {
        WebInputEventResult result = subframe->eventHandler().handleWheelEvent(event);
        if (result != WebInputEventResult::NotHandled)
            m_scrollManager.setFrameWasScrolledByUser();
        return result;
    }

    if (node) {
        WheelEvent* domEvent = WheelEvent::create(event, node->document().domWindow());
        DispatchEventResult domEventResult = node->dispatchEvent(domEvent);
        if (domEventResult != DispatchEventResult::NotCanceled)
            return toWebInputEventResult(domEventResult);
    }

    return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureShowPress()
{
    m_lastShowPressTimestamp = WTF::monotonicallyIncreasingTime();

    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;
    if (ScrollAnimatorBase* scrollAnimator = view->existingScrollAnimator())
        scrollAnimator->cancelAnimation();
    const FrameView::ScrollableAreaSet* areas = view->scrollableAreas();
    if (!areas)
        return WebInputEventResult::NotHandled;
    for (const ScrollableArea* scrollableArea : *areas) {
        ScrollAnimatorBase* animator = scrollableArea->existingScrollAnimator();
        if (animator)
            animator->cancelAnimation();
    }
    return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    // Propagation to inner frames is handled below this function.
    ASSERT(m_frame == m_frame->localFrameRoot());

    // Scrolling-related gesture events invoke EventHandler recursively for each frame down
    // the chain, doing a single-frame hit-test per frame. This matches handleWheelEvent.
    // FIXME: Add a test that traverses this path, e.g. for devtools overlay.
    if (gestureEvent.isScrollEvent())
        return handleGestureScrollEvent(gestureEvent);

    // Hit test across all frames and do touch adjustment as necessary for the event type.
    GestureEventWithHitTestResults targetedEvent = targetGestureEvent(gestureEvent);

    return handleGestureEvent(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureEvent(const GestureEventWithHitTestResults& targetedEvent)
{
    TRACE_EVENT0("input", "EventHandler::handleGestureEvent");

    // Propagation to inner frames is handled below this function.
    ASSERT(m_frame == m_frame->localFrameRoot());

    // Non-scrolling related gesture events do a single cross-frame hit-test and jump
    // directly to the inner most frame. This matches handleMousePressEvent etc.
    ASSERT(!targetedEvent.event().isScrollEvent());

    // update mouseout/leave/over/enter events before jumping directly to the inner most frame
    if (targetedEvent.event().type() == PlatformEvent::GestureTap)
        updateGestureTargetNodeForMouseEvent(targetedEvent);

    // Route to the correct frame.
    if (LocalFrame* innerFrame = targetedEvent.hitTestResult().innerNodeFrame())
        return innerFrame->eventHandler().handleGestureEventInFrame(targetedEvent);

    // No hit test result, handle in root instance. Perhaps we should just return false instead?
    return handleGestureEventInFrame(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureEventInFrame(const GestureEventWithHitTestResults& targetedEvent)
{
    ASSERT(!targetedEvent.event().isScrollEvent());

    Node* eventTarget = targetedEvent.hitTestResult().innerNode();
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();

    if (m_scrollManager.canHandleGestureEvent(targetedEvent))
        return WebInputEventResult::HandledSuppressed;

    if (eventTarget) {
        GestureEvent* gestureDomEvent = GestureEvent::create(eventTarget->document().domWindow(), gestureEvent);
        if (gestureDomEvent) {
            DispatchEventResult gestureDomEventResult = eventTarget->dispatchEvent(gestureDomEvent);
            if (gestureDomEventResult != DispatchEventResult::NotCanceled) {
                ASSERT(gestureDomEventResult != DispatchEventResult::CanceledByEventHandler);
                return toWebInputEventResult(gestureDomEventResult);
            }
        }
    }

    switch (gestureEvent.type()) {
    case PlatformEvent::GestureTapDown:
        return handleGestureTapDown(targetedEvent);
    case PlatformEvent::GestureTap:
        return handleGestureTap(targetedEvent);
    case PlatformEvent::GestureShowPress:
        return handleGestureShowPress();
    case PlatformEvent::GestureLongPress:
        return handleGestureLongPress(targetedEvent);
    case PlatformEvent::GestureLongTap:
        return handleGestureLongTap(targetedEvent);
    case PlatformEvent::GestureTwoFingerTap:
        return sendContextMenuEventForGesture(targetedEvent);
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
    case PlatformEvent::GestureTapDownCancel:
    case PlatformEvent::GestureTapUnconfirmed:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureScrollEvent(const PlatformGestureEvent& gestureEvent)
{
    TRACE_EVENT0("input", "EventHandler::handleGestureScrollEvent");

    return m_scrollManager.handleGestureScrollEvent(gestureEvent);
}

WebInputEventResult EventHandler::handleGestureTapDown(const GestureEventWithHitTestResults& targetedEvent)
{
    m_suppressMouseEventsFromGestures =
        m_pointerEventManager.primaryPointerdownCanceled(targetedEvent.event().uniqueTouchEventId());
    return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureTap(const GestureEventWithHitTestResults& targetedEvent)
{
    FrameView* frameView(m_frame->view());
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    HitTestRequest::HitTestRequestType hitType = getHitTypeForGestureType(gestureEvent.type());
    uint64_t preDispatchDomTreeVersion = m_frame->document()->domTreeVersion();
    uint64_t preDispatchStyleVersion = m_frame->document()->styleVersion();

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    HitTestResult currentHitTest = targetedEvent.hitTestResult();

    // We use the adjusted position so the application isn't surprised to see a event with
    // co-ordinates outside the target's bounds.
    IntPoint adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());

    const unsigned modifiers = gestureEvent.getModifiers();

    if (!m_suppressMouseEventsFromGestures) {
        PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(),
            NoButton, PlatformEvent::MouseMoved, /* clickCount */ 0,
            static_cast<PlatformEvent::Modifiers>(modifiers),
            PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
        dispatchMouseEvent(EventTypeNames::mousemove, currentHitTest.innerNode(), 0, fakeMouseMove);
    }

    // Do a new hit-test in case the mousemove event changed the DOM.
    // Note that if the original hit test wasn't over an element (eg. was over a scrollbar) we
    // don't want to re-hit-test because it may be in the wrong frame (and there's no way the page
    // could have seen the event anyway).
    // Also note that the position of the frame may have changed, so we need to recompute the content
    // co-ordinates (updating layout/style as hitTestResultAtPoint normally would).
    // FIXME: Use a hit-test cache to avoid unnecessary hit tests. http://crbug.com/398920
    if (currentHitTest.innerNode()) {
        LocalFrame* mainFrame = m_frame->localFrameRoot();
        if (mainFrame && mainFrame->view())
            mainFrame->view()->updateLifecycleToCompositingCleanPlusScrolling();
        adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());
        currentHitTest = hitTestResultInFrame(m_frame, adjustedPoint, hitType);
    }
    m_clickNode = currentHitTest.innerNode();

    // Capture data for showUnhandledTapUIIfNeeded.
    Node* tappedNode = m_clickNode;
    IntPoint tappedPosition = gestureEvent.position();

    if (m_clickNode && m_clickNode->isTextNode())
        m_clickNode = FlatTreeTraversal::parent(*m_clickNode);

    PlatformMouseEvent fakeMouseDown(gestureEvent.position(), gestureEvent.globalPosition(),
        LeftButton, PlatformEvent::MousePressed, gestureEvent.tapCount(),
        static_cast<PlatformEvent::Modifiers>(modifiers | PlatformEvent::LeftButtonDown),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);

    // TODO(mustaq): We suppress MEs plus all it's side effects. What would that
    // mean for for TEs?  What's the right balance here? crbug.com/617255
    WebInputEventResult mouseDownEventResult = WebInputEventResult::HandledSuppressed;
    if (!m_suppressMouseEventsFromGestures) {
        mouseDownEventResult = dispatchMouseEvent(EventTypeNames::mousedown, currentHitTest.innerNode(), gestureEvent.tapCount(), fakeMouseDown);
        selectionController().initializeSelectionState();
        if (mouseDownEventResult == WebInputEventResult::NotHandled)
            mouseDownEventResult = handleMouseFocus(MouseEventWithHitTestResults(fakeMouseDown, currentHitTest), InputDeviceCapabilities::firesTouchEventsSourceCapabilities());
        if (mouseDownEventResult == WebInputEventResult::NotHandled)
            mouseDownEventResult = handleMousePressEvent(MouseEventWithHitTestResults(fakeMouseDown, currentHitTest));
    }

    if (currentHitTest.innerNode()) {
        ASSERT(gestureEvent.type() == PlatformEvent::GestureTap);
        HitTestResult result = currentHitTest;
        result.setToShadowHostIfInUserAgentShadowRoot();
        m_frame->chromeClient().onMouseDown(result.innerNode());
    }

    // FIXME: Use a hit-test cache to avoid unnecessary hit tests. http://crbug.com/398920
    if (currentHitTest.innerNode()) {
        LocalFrame* mainFrame = m_frame->localFrameRoot();
        if (mainFrame && mainFrame->view())
            mainFrame->view()->updateAllLifecyclePhases();
        adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());
        currentHitTest = hitTestResultInFrame(m_frame, adjustedPoint, hitType);
    }

    PlatformMouseEvent fakeMouseUp(gestureEvent.position(), gestureEvent.globalPosition(),
        LeftButton, PlatformEvent::MouseReleased, gestureEvent.tapCount(),
        static_cast<PlatformEvent::Modifiers>(modifiers),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
    WebInputEventResult mouseUpEventResult = m_suppressMouseEventsFromGestures
        ? WebInputEventResult::HandledSuppressed
        : dispatchMouseEvent(EventTypeNames::mouseup, currentHitTest.innerNode(), gestureEvent.tapCount(), fakeMouseUp);

    WebInputEventResult clickEventResult = WebInputEventResult::NotHandled;
    if (m_clickNode) {
        if (currentHitTest.innerNode()) {
            // Updates distribution because a mouseup (or mousedown) event listener can make the
            // tree dirty at dispatchMouseEvent() invocation above.
            // Unless distribution is updated, commonAncestor would hit ASSERT.
            // Both m_clickNode and currentHitTest.innerNode()) don't need to be updated
            // because commonAncestor() will exit early if their documents are different.
            m_clickNode->updateDistribution();
            Node* clickTargetNode = currentHitTest.innerNode()->commonAncestor(*m_clickNode, parentForClickEvent);
            clickEventResult = dispatchMouseEvent(EventTypeNames::click, clickTargetNode, gestureEvent.tapCount(), fakeMouseUp);
        }
        m_clickNode = nullptr;
    }

    if (mouseUpEventResult == WebInputEventResult::NotHandled)
        mouseUpEventResult = handleMouseReleaseEvent(MouseEventWithHitTestResults(fakeMouseUp, currentHitTest));

    WebInputEventResult eventResult = mergeEventResult(mergeEventResult(mouseDownEventResult, mouseUpEventResult), clickEventResult);
    if (eventResult == WebInputEventResult::NotHandled && tappedNode && m_frame->page()) {
        bool domTreeChanged = preDispatchDomTreeVersion != m_frame->document()->domTreeVersion();
        bool styleChanged = preDispatchStyleVersion != m_frame->document()->styleVersion();

        IntPoint tappedPositionInViewport = frameHost()->visualViewport().rootFrameToViewport(tappedPosition);
        m_frame->chromeClient().showUnhandledTapUIIfNeeded(tappedPositionInViewport, tappedNode, domTreeChanged || styleChanged);
    }
    return eventResult;
}

WebInputEventResult EventHandler::handleGestureLongPress(const GestureEventWithHitTestResults& targetedEvent)
{
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    IntPoint adjustedPoint = gestureEvent.position();

    unsigned modifiers = gestureEvent.getModifiers();

    // FIXME: Ideally we should try to remove the extra mouse-specific hit-tests here (re-using the
    // supplied HitTestResult), but that will require some overhaul of the touch drag-and-drop code
    // and LongPress is such a special scenario that it's unlikely to matter much in practice.

    m_longTapShouldInvokeContextMenu = false;
    if (m_frame->settings() && m_frame->settings()->touchDragDropEnabled() && m_frame->view()) {
        // TODO(mustaq): Suppressing long-tap MouseEvents could break
        // drag-drop. Will do separately because of the risk. crbug.com/606938.
        PlatformMouseEvent mouseDownEvent(adjustedPoint, gestureEvent.globalPosition(), LeftButton, PlatformEvent::MousePressed, 1,
            static_cast<PlatformEvent::Modifiers>(modifiers | PlatformEvent::LeftButtonDown),
            PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
        m_mouseDown = mouseDownEvent;

        PlatformMouseEvent mouseDragEvent(adjustedPoint, gestureEvent.globalPosition(), LeftButton, PlatformEvent::MouseMoved, 1,
            static_cast<PlatformEvent::Modifiers>(modifiers | PlatformEvent::LeftButtonDown),
            PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
        HitTestRequest request(HitTestRequest::ReadOnly);
        MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseDragEvent);
        m_mouseDownMayStartDrag = true;
        dragState().m_dragSrc = nullptr;
        m_mouseDownPos = m_frame->view()->rootFrameToContents(mouseDragEvent.position());
        if (handleDrag(mev, DragInitiator::Touch)) {
            m_longTapShouldInvokeContextMenu = true;
            return WebInputEventResult::HandledSystem;
        }
    }

    IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(gestureEvent.position());
    HitTestResult result = hitTestResultAtPoint(hitTestPoint);
    if (selectionController().handleGestureLongPress(gestureEvent, result)) {
        focusDocumentView();
        return WebInputEventResult::HandledSystem;
    }

    return sendContextMenuEventForGesture(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureLongTap(const GestureEventWithHitTestResults& targetedEvent)
{
#if !OS(ANDROID)
    if (m_longTapShouldInvokeContextMenu) {
        m_longTapShouldInvokeContextMenu = false;
        return sendContextMenuEventForGesture(targetedEvent);
    }
#endif
    return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureScrollEnd(const PlatformGestureEvent& gestureEvent)
{
    return m_scrollManager.handleGestureScrollEnd(gestureEvent);
}

bool EventHandler::isScrollbarHandlingGestures() const
{
    return m_scrollManager.isScrollbarHandlingGestures();
}

bool EventHandler::shouldApplyTouchAdjustment(const PlatformGestureEvent& event) const
{
    if (m_frame->settings() && !m_frame->settings()->touchAdjustmentEnabled())
        return false;
    return !event.area().isEmpty();
}

bool EventHandler::bestClickableNodeForHitTestResult(const HitTestResult& result, IntPoint& targetPoint, Node*& targetNode)
{
    // FIXME: Unify this with the other best* functions which are very similar.

    TRACE_EVENT0("input", "EventHandler::bestClickableNodeForHitTestResult");
    ASSERT(result.isRectBasedTest());

    // If the touch is over a scrollbar, don't adjust the touch point since touch adjustment only takes into account
    // DOM nodes so a touch over a scrollbar will be adjusted towards nearby nodes. This leads to things like textarea
    // scrollbars being untouchable.
    if (result.scrollbar()) {
        targetNode = 0;
        return false;
    }

    IntPoint touchCenter = m_frame->view()->contentsToRootFrame(result.roundedPointInMainFrame());
    IntRect touchRect = m_frame->view()->contentsToRootFrame(result.hitTestLocation().boundingBox());

    HeapVector<Member<Node>, 11> nodes;
    copyToVector(result.listBasedTestResult(), nodes);

    // FIXME: the explicit Vector conversion copies into a temporary and is wasteful.
    return findBestClickableCandidate(targetNode, targetPoint, touchCenter, touchRect, HeapVector<Member<Node>> (nodes));
}

bool EventHandler::bestContextMenuNodeForHitTestResult(const HitTestResult& result, IntPoint& targetPoint, Node*& targetNode)
{
    ASSERT(result.isRectBasedTest());
    IntPoint touchCenter = m_frame->view()->contentsToRootFrame(result.roundedPointInMainFrame());
    IntRect touchRect = m_frame->view()->contentsToRootFrame(result.hitTestLocation().boundingBox());
    HeapVector<Member<Node>, 11> nodes;
    copyToVector(result.listBasedTestResult(), nodes);

    // FIXME: the explicit Vector conversion copies into a temporary and is wasteful.
    return findBestContextMenuCandidate(targetNode, targetPoint, touchCenter, touchRect, HeapVector<Member<Node>>(nodes));
}

bool EventHandler::bestZoomableAreaForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntRect& targetArea, Node*& targetNode)
{
    if (touchRadius.isEmpty())
        return false;

    IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(touchCenter);

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ListBased;
    HitTestResult result = hitTestResultAtPoint(hitTestPoint, hitType, LayoutSize(touchRadius));

    IntRect touchRect(touchCenter - touchRadius, touchRadius + touchRadius);
    HeapVector<Member<Node>, 11> nodes;
    copyToVector(result.listBasedTestResult(), nodes);

    // FIXME: the explicit Vector conversion copies into a temporary and is wasteful.
    return findBestZoomableArea(targetNode, targetArea, touchCenter, touchRect, HeapVector<Member<Node>>(nodes));
}

// Update the hover and active state across all frames for this gesture.
// This logic is different than the mouse case because mice send MouseLeave events to frames as they're exited.
// With gestures, a single event conceptually both 'leaves' whatever frame currently had hover and enters a new frame
void EventHandler::updateGestureHoverActiveState(const HitTestRequest& request, Element* innerElement)
{
    ASSERT(m_frame == m_frame->localFrameRoot());

    HeapVector<Member<LocalFrame>> newHoverFrameChain;
    LocalFrame* newHoverFrameInDocument = innerElement ? innerElement->document().frame() : nullptr;
    // Insert the ancestors of the frame having the new hovered node to the frame chain
    // The frame chain doesn't include the main frame to avoid the redundant work that cleans the hover state.
    // Because the hover state for the main frame is updated by calling Document::updateHoverActiveState
    while (newHoverFrameInDocument && newHoverFrameInDocument != m_frame) {
        newHoverFrameChain.append(newHoverFrameInDocument);
        Frame* parentFrame = newHoverFrameInDocument->tree().parent();
        newHoverFrameInDocument = parentFrame && parentFrame->isLocalFrame() ? toLocalFrame(parentFrame) : nullptr;
    }

    Node* oldHoverNodeInCurDoc = m_frame->document()->hoverNode();
    Node* newInnermostHoverNode = innerElement;

    if (newInnermostHoverNode != oldHoverNodeInCurDoc) {
        size_t indexFrameChain = newHoverFrameChain.size();

        // Clear the hover state on any frames which are no longer in the frame chain of the hovered elemen
        while (oldHoverNodeInCurDoc && oldHoverNodeInCurDoc->isFrameOwnerElement()) {
            LocalFrame* newHoverFrame = nullptr;
            // If we can't get the frame from the new hover frame chain,
            // the newHoverFrame will be null and the old hover state will be cleared.
            if (indexFrameChain > 0)
                newHoverFrame = newHoverFrameChain[--indexFrameChain];

            HTMLFrameOwnerElement* owner = toHTMLFrameOwnerElement(oldHoverNodeInCurDoc);
            if (!owner->contentFrame() || !owner->contentFrame()->isLocalFrame())
                break;

            LocalFrame* oldHoverFrame = toLocalFrame(owner->contentFrame());
            Document* doc = oldHoverFrame->document();
            if (!doc)
                break;

            oldHoverNodeInCurDoc = doc->hoverNode();
            // If the old hovered frame is different from the new hovered frame.
            // we should clear the old hovered node from the old hovered frame.
            if (newHoverFrame != oldHoverFrame)
                doc->updateHoverActiveState(request, nullptr);
        }
    }

    // Recursively set the new active/hover states on every frame in the chain of innerElement.
    m_frame->document()->updateHoverActiveState(request, innerElement);
}

// Update the mouseover/mouseenter/mouseout/mouseleave events across all frames for this gesture,
// before passing the targeted gesture event directly to a hit frame.
void EventHandler::updateGestureTargetNodeForMouseEvent(const GestureEventWithHitTestResults& targetedEvent)
{
    ASSERT(m_frame == m_frame->localFrameRoot());

    // Behaviour of this function is as follows:
    // - Create the chain of all entered frames.
    // - Compare the last frame chain under the gesture to newly entered frame chain from the main frame one by one.
    // - If the last frame doesn't match with the entered frame, then create the chain of exited frames from the last frame chain.
    // - Dispatch mouseout/mouseleave events of the exited frames from the inside out.
    // - Dispatch mouseover/mouseenter events of the entered frames into the inside.

    // Insert the ancestors of the frame having the new target node to the entered frame chain
    HeapVector<Member<LocalFrame>> enteredFrameChain;
    LocalFrame* enteredFrameInDocument = targetedEvent.hitTestResult().innerNodeFrame();
    while (enteredFrameInDocument) {
        enteredFrameChain.append(enteredFrameInDocument);
        Frame* parentFrame = enteredFrameInDocument->tree().parent();
        enteredFrameInDocument = parentFrame && parentFrame->isLocalFrame() ? toLocalFrame(parentFrame) : nullptr;
    }

    size_t indexEnteredFrameChain = enteredFrameChain.size();
    LocalFrame* exitedFrameInDocument = m_frame;
    HeapVector<Member<LocalFrame>> exitedFrameChain;
    // Insert the frame from the disagreement between last frames and entered frames
    while (exitedFrameInDocument) {
        Node* lastNodeUnderTap = exitedFrameInDocument->eventHandler().m_nodeUnderMouse.get();
        if (!lastNodeUnderTap)
            break;

        LocalFrame* nextExitedFrameInDocument = nullptr;
        if (lastNodeUnderTap->isFrameOwnerElement()) {
            HTMLFrameOwnerElement* owner = toHTMLFrameOwnerElement(lastNodeUnderTap);
            if (owner->contentFrame() && owner->contentFrame()->isLocalFrame())
                nextExitedFrameInDocument = toLocalFrame(owner->contentFrame());
        }

        if (exitedFrameChain.size() > 0) {
            exitedFrameChain.append(exitedFrameInDocument);
        } else {
            LocalFrame* lastEnteredFrameInDocument = indexEnteredFrameChain ? enteredFrameChain[indexEnteredFrameChain-1] : nullptr;
            if (exitedFrameInDocument != lastEnteredFrameInDocument)
                exitedFrameChain.append(exitedFrameInDocument);
            else if (nextExitedFrameInDocument && indexEnteredFrameChain)
                --indexEnteredFrameChain;
        }
        exitedFrameInDocument = nextExitedFrameInDocument;
    }

    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    unsigned modifiers = gestureEvent.getModifiers();
    PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(),
        NoButton, PlatformEvent::MouseMoved, /* clickCount */ 0,
        static_cast<PlatformEvent::Modifiers>(modifiers),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);

    // Update the mouseout/mouseleave event
    size_t indexExitedFrameChain = exitedFrameChain.size();
    while (indexExitedFrameChain) {
        LocalFrame* leaveFrame = exitedFrameChain[--indexExitedFrameChain];
        leaveFrame->eventHandler().updateMouseEventTargetNodeAndSendEvents(nullptr, fakeMouseMove);
    }

    // update the mouseover/mouseenter event
    while (indexEnteredFrameChain) {
        Frame* parentFrame = enteredFrameChain[--indexEnteredFrameChain]->tree().parent();
        if (parentFrame && parentFrame->isLocalFrame())
            toLocalFrame(parentFrame)->eventHandler().updateMouseEventTargetNodeAndSendEvents(toHTMLFrameOwnerElement(enteredFrameChain[indexEnteredFrameChain]->owner()), fakeMouseMove);
    }
}

GestureEventWithHitTestResults EventHandler::targetGestureEvent(const PlatformGestureEvent& gestureEvent, bool readOnly)
{
    TRACE_EVENT0("input", "EventHandler::targetGestureEvent");

    ASSERT(m_frame == m_frame->localFrameRoot());
    // Scrolling events get hit tested per frame (like wheel events do).
    ASSERT(!gestureEvent.isScrollEvent());

    HitTestRequest::HitTestRequestType hitType = getHitTypeForGestureType(gestureEvent.type());
    double activeInterval = 0;
    bool shouldKeepActiveForMinInterval = false;
    if (readOnly) {
        hitType |= HitTestRequest::ReadOnly;
    } else if (gestureEvent.type() == PlatformEvent::GestureTap) {
        // If the Tap is received very shortly after ShowPress, we want to
        // delay clearing of the active state so that it's visible to the user
        // for at least a couple of frames.
        activeInterval = WTF::monotonicallyIncreasingTime() - m_lastShowPressTimestamp;
        shouldKeepActiveForMinInterval = m_lastShowPressTimestamp && activeInterval < minimumActiveInterval;
        if (shouldKeepActiveForMinInterval)
            hitType |= HitTestRequest::ReadOnly;
    }

    GestureEventWithHitTestResults eventWithHitTestResults = hitTestResultForGestureEvent(gestureEvent, hitType);
    // Now apply hover/active state to the final target.
    HitTestRequest request(hitType | HitTestRequest::AllowChildFrameContent);
    if (!request.readOnly())
        updateGestureHoverActiveState(request, eventWithHitTestResults.hitTestResult().innerElement());

    if (shouldKeepActiveForMinInterval) {
        m_lastDeferredTapElement = eventWithHitTestResults.hitTestResult().innerElement();
        m_activeIntervalTimer.startOneShot(minimumActiveInterval - activeInterval, BLINK_FROM_HERE);
    }

    return eventWithHitTestResults;
}

GestureEventWithHitTestResults EventHandler::hitTestResultForGestureEvent(const PlatformGestureEvent& gestureEvent, HitTestRequest::HitTestRequestType hitType)
{
    // Perform the rect-based hit-test (or point-based if adjustment is disabled). Note that
    // we don't yet apply hover/active state here because we need to resolve touch adjustment
    // first so that we apply hover/active it to the final adjusted node.
    IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(gestureEvent.position());
    LayoutSize padding;
    if (shouldApplyTouchAdjustment(gestureEvent)) {
        padding = LayoutSize(gestureEvent.area());
        if (!padding.isEmpty()) {
            padding.scale(1.f / 2);
            hitType |= HitTestRequest::ListBased;
        }
    }
    HitTestResult hitTestResult = hitTestResultAtPoint(hitTestPoint, hitType | HitTestRequest::ReadOnly, padding);

    // Adjust the location of the gesture to the most likely nearby node, as appropriate for the
    // type of event.
    PlatformGestureEvent adjustedEvent = gestureEvent;
    applyTouchAdjustment(&adjustedEvent, &hitTestResult);

    // Do a new hit-test at the (adjusted) gesture co-ordinates. This is necessary because
    // rect-based hit testing and touch adjustment sometimes return a different node than
    // what a point-based hit test would return for the same point.
    // FIXME: Fix touch adjustment to avoid the need for a redundant hit test. http://crbug.com/398914
    if (shouldApplyTouchAdjustment(gestureEvent)) {
        LocalFrame* hitFrame = hitTestResult.innerNodeFrame();
        if (!hitFrame)
            hitFrame = m_frame;
        hitTestResult = hitTestResultInFrame(hitFrame, hitFrame->view()->rootFrameToContents(adjustedEvent.position()), (hitType | HitTestRequest::ReadOnly) & ~HitTestRequest::ListBased);
    }

    // If we did a rect-based hit test it must be resolved to the best single node by now to
    // ensure consumers don't accidentally use one of the other candidates.
    ASSERT(!hitTestResult.isRectBasedTest());

    return GestureEventWithHitTestResults(adjustedEvent, hitTestResult);
}

HitTestRequest::HitTestRequestType EventHandler::getHitTypeForGestureType(PlatformEvent::EventType type)
{
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent;
    switch (type) {
    case PlatformEvent::GestureShowPress:
    case PlatformEvent::GestureTapUnconfirmed:
        return hitType | HitTestRequest::Active;
    case PlatformEvent::GestureTapDownCancel:
        // A TapDownCancel received when no element is active shouldn't really be changing hover state.
        if (!m_frame->document()->activeHoverElement())
            hitType |= HitTestRequest::ReadOnly;
        return hitType | HitTestRequest::Release;
    case PlatformEvent::GestureTap:
        return hitType | HitTestRequest::Release;
    case PlatformEvent::GestureTapDown:
    case PlatformEvent::GestureLongPress:
    case PlatformEvent::GestureLongTap:
    case PlatformEvent::GestureTwoFingerTap:
        // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
        return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
    default:
        ASSERT_NOT_REACHED();
        return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
    }
}

void EventHandler::applyTouchAdjustment(PlatformGestureEvent* gestureEvent, HitTestResult* hitTestResult)
{
    if (!shouldApplyTouchAdjustment(*gestureEvent))
        return;

    Node* adjustedNode = nullptr;
    IntPoint adjustedPoint = gestureEvent->position();
    bool adjusted = false;
    switch (gestureEvent->type()) {
    case PlatformEvent::GestureTap:
    case PlatformEvent::GestureTapUnconfirmed:
    case PlatformEvent::GestureTapDown:
    case PlatformEvent::GestureShowPress:
        adjusted = bestClickableNodeForHitTestResult(*hitTestResult, adjustedPoint, adjustedNode);
        break;
    case PlatformEvent::GestureLongPress:
    case PlatformEvent::GestureLongTap:
    case PlatformEvent::GestureTwoFingerTap:
        adjusted = bestContextMenuNodeForHitTestResult(*hitTestResult, adjustedPoint, adjustedNode);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // Update the hit-test result to be a point-based result instead of a rect-based result.
    // FIXME: We should do this even when no candidate matches the node filter. crbug.com/398914
    if (adjusted) {
        hitTestResult->resolveRectBasedTest(adjustedNode, m_frame->view()->rootFrameToContents(adjustedPoint));
        gestureEvent->applyTouchAdjustment(adjustedPoint);
    }
}

WebInputEventResult EventHandler::sendContextMenuEvent(const PlatformMouseEvent& event, Node* overrideTargetNode)
{
    FrameView* v = m_frame->view();
    if (!v)
        return WebInputEventResult::NotHandled;

    // Clear mouse press state to avoid initiating a drag while context menu is up.
    m_mousePressed = false;
    LayoutPoint positionInContents = v->rootFrameToContents(event.position());
    HitTestRequest request(HitTestRequest::Active);
    MouseEventWithHitTestResults mev = m_frame->document()->prepareMouseEvent(request, positionInContents, event);

    selectionController().sendContextMenuEvent(mev, positionInContents);

    Node* targetNode = overrideTargetNode ? overrideTargetNode : mev.innerNode();
    return dispatchMouseEvent(EventTypeNames::contextmenu, targetNode, 0, event);
}

WebInputEventResult EventHandler::sendContextMenuEventForKey(Element* overrideTargetElement)
{
    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;

    Document* doc = m_frame->document();
    if (!doc)
        return WebInputEventResult::NotHandled;

    static const int kContextMenuMargin = 1;

#if OS(WIN)
    int rightAligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
#else
    int rightAligned = 0;
#endif
    IntPoint locationInRootFrame;

    Element* focusedElement = overrideTargetElement ? overrideTargetElement : doc->focusedElement();
    FrameSelection& selection = m_frame->selection();
    Position start = selection.selection().start();
    VisualViewport& visualViewport = frameHost()->visualViewport();

    if (!overrideTargetElement && start.anchorNode() && (selection.rootEditableElement() || selection.isRange())) {
        IntRect firstRect = m_frame->editor().firstRectForRange(selection.selection().toNormalizedEphemeralRange());

        int x = rightAligned ? firstRect.maxX() : firstRect.x();
        // In a multiline edit, firstRect.maxY() would endup on the next line, so -1.
        int y = firstRect.maxY() ? firstRect.maxY() - 1 : 0;
        locationInRootFrame = view->contentsToRootFrame(IntPoint(x, y));
    } else if (focusedElement) {
        IntRect clippedRect = focusedElement->boundsInViewport();
        locationInRootFrame = visualViewport.viewportToRootFrame(clippedRect.center());
    } else {
        locationInRootFrame = IntPoint(
            rightAligned
                ? visualViewport.visibleRect().maxX() - kContextMenuMargin
                : visualViewport.location().x() + kContextMenuMargin,
            visualViewport.location().y() + kContextMenuMargin);
    }

    m_frame->view()->setCursor(pointerCursor());
    IntPoint locationInViewport = visualViewport.rootFrameToViewport(locationInRootFrame);
    IntPoint globalPosition = view->getHostWindow()->viewportToScreen(IntRect(locationInViewport, IntSize()), m_frame->view()).location();

    Node* targetNode = overrideTargetElement ? overrideTargetElement : doc->focusedElement();
    if (!targetNode)
        targetNode = doc;

    // Use the focused node as the target for hover and active.
    HitTestRequest request(HitTestRequest::Active);
    HitTestResult result(request, locationInRootFrame);
    result.setInnerNode(targetNode);
    doc->updateHoverActiveState(request, result.innerElement());

    // The contextmenu event is a mouse event even when invoked using the keyboard.
    // This is required for web compatibility.
    PlatformEvent::EventType eventType = PlatformEvent::MousePressed;
    if (m_frame->settings() && m_frame->settings()->showContextMenuOnMouseUp())
        eventType = PlatformEvent::MouseReleased;

    PlatformMouseEvent mouseEvent(locationInRootFrame, globalPosition,
        RightButton, eventType, 1,
        PlatformEvent::NoModifiers, PlatformMouseEvent::RealOrIndistinguishable,
        WTF::monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);

    return sendContextMenuEvent(mouseEvent, overrideTargetElement);
}

WebInputEventResult EventHandler::sendContextMenuEventForGesture(const GestureEventWithHitTestResults& targetedEvent)
{
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    unsigned modifiers = gestureEvent.getModifiers();

    // Send MouseMoved event prior to handling (https://crbug.com/485290).
    PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(),
        NoButton, PlatformEvent::MouseMoved, /* clickCount */ 0,
        static_cast<PlatformEvent::Modifiers>(modifiers),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
    dispatchMouseEvent(EventTypeNames::mousemove, targetedEvent.hitTestResult().innerNode(), 0, fakeMouseMove);

    PlatformEvent::EventType eventType = PlatformEvent::MousePressed;

    if (m_frame->settings() && m_frame->settings()->showContextMenuOnMouseUp())
        eventType = PlatformEvent::MouseReleased;

    // Always set right button down as we are sending mousedown event regardless
    modifiers |= PlatformEvent::RightButtonDown;

    // TODO(crbug.com/579564): Maybe we should not send mouse down at all
    PlatformMouseEvent mouseEvent(targetedEvent.event().position(), targetedEvent.event().globalPosition(), RightButton, eventType, 1,
        static_cast<PlatformEvent::Modifiers>(modifiers),
        PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
    // To simulate right-click behavior, we send a right mouse down and then
    // context menu event.
    // FIXME: Send HitTestResults to avoid redundant hit tests.
    handleMousePressEvent(mouseEvent);
    return sendContextMenuEvent(mouseEvent);
    // We do not need to send a corresponding mouse release because in case of
    // right-click, the context menu takes capture and consumes all events.
}

void EventHandler::scheduleHoverStateUpdate()
{
    if (!m_hoverTimer.isActive())
        m_hoverTimer.startOneShot(0, BLINK_FROM_HERE);
}

void EventHandler::scheduleCursorUpdate()
{
    // We only want one timer for the page, rather than each frame having it's own timer
    // competing which eachother (since there's only one mouse cursor).
    ASSERT(m_frame == m_frame->localFrameRoot());

    if (!m_cursorUpdateTimer.isActive())
        m_cursorUpdateTimer.startOneShot(cursorUpdateInterval, BLINK_FROM_HERE);
}

bool EventHandler::cursorUpdatePending()
{
    return m_cursorUpdateTimer.isActive();
}

void EventHandler::dispatchFakeMouseMoveEventSoon()
{
    if (m_mousePressed)
        return;

    if (m_mousePositionIsUnknown)
        return;

    Settings* settings = m_frame->settings();
    if (settings && !settings->deviceSupportsMouse())
        return;

    // Reschedule the timer, to prevent dispatching mouse move events
    // during a scroll. This avoids a potential source of scroll jank.
    if (m_fakeMouseMoveEventTimer.isActive())
        m_fakeMouseMoveEventTimer.stop();
    m_fakeMouseMoveEventTimer.startOneShot(fakeMouseMoveInterval, BLINK_FROM_HERE);
}

void EventHandler::dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad& quad)
{
    FrameView* view = m_frame->view();
    if (!view)
        return;

    if (!quad.containsPoint(view->rootFrameToContents(m_lastKnownMousePosition)))
        return;

    dispatchFakeMouseMoveEventSoon();
}

void EventHandler::fakeMouseMoveEventTimerFired(Timer<EventHandler>* timer)
{
    TRACE_EVENT0("input", "EventHandler::fakeMouseMoveEventTimerFired");
    ASSERT_UNUSED(timer, timer == &m_fakeMouseMoveEventTimer);
    ASSERT(!m_mousePressed);

    Settings* settings = m_frame->settings();
    if (settings && !settings->deviceSupportsMouse())
        return;

    FrameView* view = m_frame->view();
    if (!view)
        return;

    if (!m_frame->page() || !m_frame->page()->focusController().isActive())
        return;

    // Don't dispatch a synthetic mouse move event if the mouse cursor is not visible to the user.
    if (!isCursorVisible())
        return;

    PlatformMouseEvent fakeMouseMoveEvent(m_lastKnownMousePosition, m_lastKnownMouseGlobalPosition, NoButton, PlatformEvent::MouseMoved, 0, PlatformKeyboardEvent::getCurrentModifierState(), PlatformMouseEvent::RealOrIndistinguishable, monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
    handleMouseMoveEvent(fakeMouseMoveEvent);
}

void EventHandler::cancelFakeMouseMoveEvent()
{
    m_fakeMouseMoveEventTimer.stop();
}

bool EventHandler::isCursorVisible() const
{
    return m_frame->page()->isCursorVisible();
}

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    m_frameSetBeingResized = frameSet;
}

void EventHandler::resizeScrollableAreaDestroyed()
{
    m_scrollManager.clearResizeScrollableArea(true);
}

void EventHandler::hoverTimerFired(Timer<EventHandler>*)
{
    TRACE_EVENT0("input", "EventHandler::hoverTimerFired");
    m_hoverTimer.stop();

    ASSERT(m_frame);
    ASSERT(m_frame->document());

    if (LayoutViewItem layoutItem = m_frame->contentLayoutItem()) {
        if (FrameView* view = m_frame->view()) {
            HitTestRequest request(HitTestRequest::Move);
            HitTestResult result(request, view->rootFrameToContents(m_lastKnownMousePosition));
            layoutItem.hitTest(result);
            m_frame->document()->updateHoverActiveState(request, result.innerElement());
        }
    }
}

void EventHandler::activeIntervalTimerFired(Timer<EventHandler>*)
{
    TRACE_EVENT0("input", "EventHandler::activeIntervalTimerFired");
    m_activeIntervalTimer.stop();

    if (m_frame
        && m_frame->document()
        && m_lastDeferredTapElement) {
        // FIXME: Enable condition when http://crbug.com/226842 lands
        // m_lastDeferredTapElement.get() == m_frame->document()->activeElement()
        HitTestRequest request(HitTestRequest::TouchEvent | HitTestRequest::Release);
        m_frame->document()->updateHoverActiveState(request, m_lastDeferredTapElement.get());
    }
    m_lastDeferredTapElement = nullptr;
}

void EventHandler::notifyElementActivated()
{
    // Since another element has been set to active, stop current timer and clear reference.
    if (m_activeIntervalTimer.isActive())
        m_activeIntervalTimer.stop();
    m_lastDeferredTapElement = nullptr;
}

bool EventHandler::handleAccessKey(const PlatformKeyboardEvent& evt)
{
    return m_keyboardEventManager.handleAccessKey(evt);
}

WebInputEventResult EventHandler::keyEvent(const PlatformKeyboardEvent& initialKeyEvent)
{
    return m_keyboardEventManager.keyEvent(initialKeyEvent);
}

void EventHandler::defaultKeyboardEventHandler(KeyboardEvent* event)
{
    m_keyboardEventManager.defaultKeyboardEventHandler(event, m_mousePressNode);
}

bool EventHandler::dragHysteresisExceeded(const IntPoint& dragLocationInRootFrame) const
{
    FrameView* view = m_frame->view();
    if (!view)
        return false;
    IntPoint dragLocation = view->rootFrameToContents(dragLocationInRootFrame);
    IntSize delta = dragLocation - m_mouseDownPos;

    int threshold = GeneralDragHysteresis;
    switch (dragState().m_dragType) {
    case DragSourceActionSelection:
        threshold = TextDragHysteresis;
        break;
    case DragSourceActionImage:
        threshold = ImageDragHysteresis;
        break;
    case DragSourceActionLink:
        threshold = LinkDragHysteresis;
        break;
    case DragSourceActionDHTML:
        break;
    case DragSourceActionNone:
        ASSERT_NOT_REACHED();
    }

    return abs(delta.width()) >= threshold || abs(delta.height()) >= threshold;
}

void EventHandler::clearDragDataTransfer()
{
    if (dragState().m_dragDataTransfer) {
        dragState().m_dragDataTransfer->clearDragImage();
        dragState().m_dragDataTransfer->setAccessPolicy(DataTransferNumb);
    }
}

void EventHandler::dragSourceEndedAt(const PlatformMouseEvent& event, DragOperation operation)
{
    // Send a hit test request so that Layer gets a chance to update the :hover and :active pseudoclasses.
    HitTestRequest request(HitTestRequest::Release);
    prepareMouseEvent(request, event);

    if (dragState().m_dragSrc) {
        dragState().m_dragDataTransfer->setDestinationOperation(operation);
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(EventTypeNames::dragend, event);
    }
    clearDragDataTransfer();
    dragState().m_dragSrc = nullptr;
    // In case the drag was ended due to an escape key press we need to ensure
    // that consecutive mousemove events don't reinitiate the drag and drop.
    m_mouseDownMayStartDrag = false;
}

void EventHandler::updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement)
{
    // If inserting the dragged contents removed the drag source, we still want to fire dragend at the root editble element.
    if (dragState().m_dragSrc && !dragState().m_dragSrc->inShadowIncludingDocument())
        dragState().m_dragSrc = rootEditableElement;
}

// returns if we should continue "default processing", i.e., whether eventhandler canceled
WebInputEventResult EventHandler::dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent& event)
{
    return dispatchDragEvent(eventType, dragState().m_dragSrc.get(), event, dragState().m_dragDataTransfer.get());
}

bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event, DragInitiator initiator)
{
    ASSERT(event.event().type() == PlatformEvent::MouseMoved);
    // Callers must protect the reference to FrameView, since this function may dispatch DOM
    // events, causing page/FrameView to go away.
    ASSERT(m_frame);
    ASSERT(m_frame->view());
    if (!m_frame->page())
        return false;

    if (m_mouseDownMayStartDrag) {
        HitTestRequest request(HitTestRequest::ReadOnly);
        HitTestResult result(request, m_mouseDownPos);
        m_frame->contentLayoutItem().hitTest(result);
        Node* node = result.innerNode();
        if (node) {
            DragController::SelectionDragPolicy selectionDragPolicy = event.event().timestamp() - m_mouseDownTimestamp < TextDragDelay
                ? DragController::DelayedSelectionDragResolution
                : DragController::ImmediateSelectionDragResolution;
            dragState().m_dragSrc = m_frame->page()->dragController().draggableNode(m_frame, node, m_mouseDownPos, selectionDragPolicy, dragState().m_dragType);
        } else {
            dragState().m_dragSrc = nullptr;
        }

        if (!dragState().m_dragSrc)
            m_mouseDownMayStartDrag = false; // no element is draggable
    }

    if (!m_mouseDownMayStartDrag)
        return initiator == DragInitiator::Mouse && !selectionController().mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll;

    // We are starting a text/image/url drag, so the cursor should be an arrow
    // FIXME <rdar://7577595>: Custom cursors aren't supported during drag and drop (default to pointer).
    m_frame->view()->setCursor(pointerCursor());

    if (initiator == DragInitiator::Mouse && !dragHysteresisExceeded(event.event().position()))
        return true;

    // Once we're past the hysteresis point, we don't want to treat this gesture as a click
    invalidateClick();

    if (!tryStartDrag(event)) {
        // Something failed to start the drag, clean up.
        clearDragDataTransfer();
        dragState().m_dragSrc = nullptr;
    }

    m_mouseDownMayStartDrag = false;
    // Whether or not the drag actually started, no more default handling (like selection).
    return true;
}

bool EventHandler::tryStartDrag(const MouseEventWithHitTestResults& event)
{
    // The DataTransfer would only be non-empty if we missed a dragEnd.
    // Clear it anyway, just to make sure it gets numbified.
    clearDragDataTransfer();

    dragState().m_dragDataTransfer = createDraggingDataTransfer();

    // Check to see if this a DOM based drag, if it is get the DOM specified drag
    // image and offset
    if (dragState().m_dragType == DragSourceActionDHTML) {
        if (LayoutObject* layoutObject = dragState().m_dragSrc->layoutObject()) {
            IntRect boundingIncludingDescendants = layoutObject->absoluteBoundingBoxRectIncludingDescendants();
            IntSize delta = m_mouseDownPos - boundingIncludingDescendants.location();
            dragState().m_dragDataTransfer->setDragImageElement(dragState().m_dragSrc.get(), IntPoint(delta));
        } else {
            // The layoutObject has disappeared, this can happen if the onStartDrag handler has hidden
            // the element in some way. In this case we just kill the drag.
            return false;
        }
    }

    DragController& dragController = m_frame->page()->dragController();
    if (!dragController.populateDragDataTransfer(m_frame, dragState(), m_mouseDownPos))
        return false;

    // If dispatching dragstart brings about another mouse down -- one way
    // this will happen is if a DevTools user breaks within a dragstart
    // handler and then clicks on the suspended page -- the drag state is
    // reset. Hence, need to check if this particular drag operation can
    // continue even if dispatchEvent() indicates no (direct) cancellation.
    // Do that by checking if m_dragSrc is still set.
    m_mouseDownMayStartDrag = dispatchDragSrcEvent(EventTypeNames::dragstart, m_mouseDown) == WebInputEventResult::NotHandled
        && !m_frame->selection().isInPasswordField() && dragState().m_dragSrc;

    // Invalidate clipboard here against anymore pasteboard writing for security. The drag
    // image can still be changed as we drag, but not the pasteboard data.
    dragState().m_dragDataTransfer->setAccessPolicy(DataTransferImageWritable);

    if (m_mouseDownMayStartDrag) {
        // Dispatching the event could cause Page to go away. Make sure it's still valid before trying to use DragController.
        if (m_frame->page() && dragController.startDrag(m_frame, dragState(), event.event(), m_mouseDownPos))
            return true;
        // Drag was canned at the last minute - we owe m_dragSrc a DRAGEND event
        dispatchDragSrcEvent(EventTypeNames::dragend, event.event());
    }

    return false;
}

bool EventHandler::handleTextInputEvent(const String& text, Event* underlyingEvent, TextEventInputType inputType)
{
    // Platforms should differentiate real commands like selectAll from text input in disguise (like insertNewline),
    // and avoid dispatching text input events from keydown default handlers.
    ASSERT(!underlyingEvent || !underlyingEvent->isKeyboardEvent() || toKeyboardEvent(underlyingEvent)->type() == EventTypeNames::keypress);

    if (!m_frame)
        return false;

    EventTarget* target;
    if (underlyingEvent)
        target = underlyingEvent->target();
    else
        target = eventTargetNodeForDocument(m_frame->document());
    if (!target)
        return false;

    TextEvent* event = TextEvent::create(m_frame->domWindow(), text, inputType);
    event->setUnderlyingEvent(underlyingEvent);

    target->dispatchEvent(event);
    return event->defaultHandled() || event->defaultPrevented();
}

void EventHandler::defaultTextInputEventHandler(TextEvent* event)
{
    if (m_frame->editor().handleTextEvent(event))
        event->setDefaultHandled();
}

void EventHandler::capsLockStateMayHaveChanged()
{
    m_keyboardEventManager.capsLockStateMayHaveChanged();
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults& mev)
{
    Scrollbar* scrollbar = mev.scrollbar();
    updateLastScrollbarUnderMouse(scrollbar, true);

    if (!scrollbar || !scrollbar->enabled())
        return false;
    m_scrollManager.setFrameWasScrolledByUser();
    scrollbar->mouseDown(mev.event());
    return true;
}

// If scrollbar (under mouse) is different from last, send a mouse exited. Set
// last to scrollbar if setLast is true; else set last to 0.
void EventHandler::updateLastScrollbarUnderMouse(Scrollbar* scrollbar, bool setLast)
{
    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->mouseExited();

        // Send mouse entered if we're setting a new scrollbar.
        if (scrollbar && setLast)
            scrollbar->mouseEntered();

        m_lastScrollbarUnderMouse = setLast ? scrollbar : nullptr;
    }
}

HitTestResult EventHandler::hitTestResultInFrame(LocalFrame* frame, const LayoutPoint& point, HitTestRequest::HitTestRequestType hitType)
{
    HitTestResult result(HitTestRequest(hitType), point);

    if (!frame || frame->contentLayoutItem().isNull())
        return result;
    if (frame->view()) {
        IntRect rect = frame->view()->visibleContentRect(IncludeScrollbars);
        if (!rect.contains(roundedIntPoint(point)))
            return result;
    }
    frame->contentLayoutItem().hitTest(result);
    return result;
}

WebInputEventResult EventHandler::handleTouchEvent(const PlatformTouchEvent& event)
{
    TRACE_EVENT0("blink", "EventHandler::handleTouchEvent");
    return m_pointerEventManager.handleTouchEvents(event);
}

void EventHandler::setLastKnownMousePosition(const PlatformMouseEvent& event)
{
    m_mousePositionIsUnknown = false;
    m_lastKnownMousePosition = event.position();
    m_lastKnownMouseGlobalPosition = event.globalPosition();
}

WebInputEventResult EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame* subframe)
{
    selectionController().passMousePressEventToSubframe(mev);
    WebInputEventResult result = subframe->eventHandler().handleMousePressEvent(mev.event());
    if (result != WebInputEventResult::NotHandled)
        return result;
    return WebInputEventResult::HandledSystem;
}

WebInputEventResult EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame* subframe, HitTestResult* hoveredNode)
{
    if (m_mouseDownMayStartDrag)
        return WebInputEventResult::NotHandled;
    WebInputEventResult result = subframe->eventHandler().handleMouseMoveOrLeaveEvent(mev.event(), hoveredNode);
    if (result != WebInputEventResult::NotHandled)
        return result;
    return WebInputEventResult::HandledSystem;
}

WebInputEventResult EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame* subframe)
{
    WebInputEventResult result = subframe->eventHandler().handleMouseReleaseEvent(mev.event());
    if (result != WebInputEventResult::NotHandled)
        return result;
    return WebInputEventResult::HandledSystem;
}

DataTransfer* EventHandler::createDraggingDataTransfer() const
{
    return DataTransfer::create(DataTransfer::DragAndDrop, DataTransferWritable, DataObject::create());
}

void EventHandler::focusDocumentView()
{
    Page* page = m_frame->page();
    if (!page)
        return;
    page->focusController().focusDocumentView(m_frame);
}

FrameHost* EventHandler::frameHost() const
{
    if (!m_frame->page())
        return nullptr;

    return &m_frame->page()->frameHost();
}

} // namespace blink
