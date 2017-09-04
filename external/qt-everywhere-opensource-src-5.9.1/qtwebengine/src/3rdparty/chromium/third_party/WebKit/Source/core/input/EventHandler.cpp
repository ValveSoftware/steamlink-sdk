/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
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
#include "core/clipboard/DataTransfer.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/TouchList.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionController.h"
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
#include "core/input/EventHandlingUtil.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
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
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WindowsKeyboardCodes.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/Image.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/WebInputEvent.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

namespace {

// Refetch the event target node if it is removed or currently is the shadow
// node inside an <input> element.  If a mouse event handler changes the input
// element type to one that has a widget associated, we'd like to
// EventHandler::handleMousePressEvent to pass the event to the widget and thus
// the event target node can't still be the shadow node.
bool shouldRefetchEventTarget(const MouseEventWithHitTestResults& mev) {
  Node* targetNode = mev.innerNode();
  if (!targetNode || !targetNode->parentNode())
    return true;
  return targetNode->isShadowRoot() &&
         isHTMLInputElement(toShadowRoot(targetNode)->host());
}

}  // namespace

using namespace HTMLNames;

// The amount of time to wait for a cursor update on style and layout changes
// Set to 50Hz, no need to be faster than common screen refresh rate
static const double cursorUpdateInterval = 0.02;

static const int maximumCursorSize = 128;

// It's pretty unlikely that a scale of less than one would ever be used. But
// all we really need to ensure here is that the scale isn't so small that
// integer overflow can occur when dividing cursor sizes (limited above) by the
// scale.
static const double minimumCursorScale = 0.001;

// The minimum amount of time an element stays active after a ShowPress
// This is roughly 9 frames, which should be long enough to be noticeable.
static const double minimumActiveInterval = 0.15;

enum NoCursorChangeType { NoCursorChange };

class OptionalCursor {
 public:
  OptionalCursor(NoCursorChangeType) : m_isCursorChange(false) {}
  OptionalCursor(const Cursor& cursor)
      : m_isCursorChange(true), m_cursor(cursor) {}

  bool isCursorChange() const { return m_isCursorChange; }
  const Cursor& cursor() const {
    ASSERT(m_isCursorChange);
    return m_cursor;
  }

 private:
  bool m_isCursorChange;
  Cursor m_cursor;
};

EventHandler::EventHandler(LocalFrame* frame)
    : m_frame(frame),
      m_selectionController(SelectionController::create(*frame)),
      m_hoverTimer(this, &EventHandler::hoverTimerFired),
      m_cursorUpdateTimer(this, &EventHandler::cursorUpdateTimerFired),
      m_eventHandlerWillResetCapturingMouseEventsNode(0),
      m_shouldOnlyFireDragOverEvent(false),
      m_scrollManager(new ScrollManager(frame)),
      m_mouseEventManager(new MouseEventManager(frame, m_scrollManager)),
      m_keyboardEventManager(new KeyboardEventManager(frame, m_scrollManager)),
      m_pointerEventManager(
          new PointerEventManager(frame, m_mouseEventManager)),
      m_gestureManager(new GestureManager(frame,
                                          m_scrollManager,
                                          m_mouseEventManager,
                                          m_pointerEventManager,
                                          m_selectionController)),
      m_activeIntervalTimer(this, &EventHandler::activeIntervalTimerFired) {}

DEFINE_TRACE(EventHandler) {
  visitor->trace(m_frame);
  visitor->trace(m_selectionController);
  visitor->trace(m_capturingMouseEventsNode);
  visitor->trace(m_lastMouseMoveEventSubframe);
  visitor->trace(m_lastScrollbarUnderMouse);
  visitor->trace(m_dragTarget);
  visitor->trace(m_frameSetBeingResized);
  visitor->trace(m_scrollManager);
  visitor->trace(m_mouseEventManager);
  visitor->trace(m_keyboardEventManager);
  visitor->trace(m_pointerEventManager);
  visitor->trace(m_gestureManager);
  visitor->trace(m_lastDeferredTapElement);
}

void EventHandler::clear() {
  m_hoverTimer.stop();
  m_cursorUpdateTimer.stop();
  m_activeIntervalTimer.stop();
  m_lastMouseMoveEventSubframe = nullptr;
  m_lastScrollbarUnderMouse = nullptr;
  m_frameSetBeingResized = nullptr;
  m_dragTarget = nullptr;
  m_shouldOnlyFireDragOverEvent = false;
  m_lastMouseDownUserGestureToken.clear();
  m_capturingMouseEventsNode = nullptr;
  m_pointerEventManager->clear();
  m_scrollManager->clear();
  m_gestureManager->clear();
  m_mouseEventManager->clear();
  m_lastDeferredTapElement = nullptr;
  m_eventHandlerWillResetCapturingMouseEventsNode = false;
}

void EventHandler::updateSelectionForMouseDrag() {
  m_mouseEventManager->updateSelectionForMouseDrag();
}

void EventHandler::startMiddleClickAutoscroll(LayoutObject* layoutObject) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  if (!layoutObject->isBox())
    return;
  AutoscrollController* controller = m_scrollManager->autoscrollController();
  if (!controller)
    return;
  controller->startMiddleClickAutoscroll(
      toLayoutBox(layoutObject), m_mouseEventManager->lastKnownMousePosition());
  m_mouseEventManager->invalidateClick();
}

HitTestResult EventHandler::hitTestResultAtPoint(
    const LayoutPoint& point,
    HitTestRequest::HitTestRequestType hitType,
    const LayoutSize& padding) {
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
        IntPoint mainFramePoint = mainView->rootFrameToContents(
            frameView->contentsToRootFrame(roundedIntPoint(point)));
        return mainFrame->eventHandler().hitTestResultAtPoint(mainFramePoint,
                                                              hitType, padding);
      }
    }
  }

  // hitTestResultAtPoint is specifically used to hitTest into all frames, thus
  // it always allows child frame content.
  HitTestRequest request(hitType | HitTestRequest::AllowChildFrameContent);
  HitTestResult result(request, point, padding.height().toUnsigned(),
                       padding.width().toUnsigned(),
                       padding.height().toUnsigned(),
                       padding.width().toUnsigned());

  // LayoutView::hitTest causes a layout, and we don't want to hit that until
  // the first layout because until then, there is nothing shown on the screen -
  // the user can't have intentionally clicked on something belonging to this
  // page.  Furthermore, mousemove events before the first layout should not
  // lead to a premature layout() happening, which could show a flash of white.
  // See also the similar code in Document::performMouseEventHitTest.
  if (m_frame->contentLayoutItem().isNull() || !m_frame->view() ||
      !m_frame->view()->didFirstLayout())
    return result;

  m_frame->contentLayoutItem().hitTest(result);
  if (!request.readOnly()) {
    m_frame->document()->updateHoverActiveState(request, result.innerElement(),
                                                result.scrollbar());
  }

  return result;
}

void EventHandler::stopAutoscroll() {
  m_scrollManager->stopAutoscroll();
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool EventHandler::bubblingScroll(ScrollDirection direction,
                                  ScrollGranularity granularity,
                                  Node* startingNode) {
  return m_scrollManager->bubblingScroll(direction, granularity, startingNode,
                                         m_mouseEventManager->mousePressNode());
}

IntPoint EventHandler::lastKnownMousePosition() const {
  return m_mouseEventManager->lastKnownMousePosition();
}

IntPoint EventHandler::dragDataTransferLocationForTesting() {
  if (m_mouseEventManager->dragState().m_dragDataTransfer)
    return m_mouseEventManager->dragState().m_dragDataTransfer->dragLocation();

  return IntPoint();
}

static LocalFrame* subframeForTargetNode(Node* node) {
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

static LocalFrame* subframeForHitTestResult(
    const MouseEventWithHitTestResults& hitTestResult) {
  if (!hitTestResult.isOverWidget())
    return nullptr;
  return subframeForTargetNode(hitTestResult.innerNode());
}

static bool isSubmitImage(Node* node) {
  return isHTMLInputElement(node) &&
         toHTMLInputElement(node)->type() == InputTypeNames::image;
}

bool EventHandler::useHandCursor(Node* node, bool isOverLink) {
  if (!node)
    return false;

  return ((isOverLink || isSubmitImage(node)) && !hasEditableStyle(*node));
}

void EventHandler::cursorUpdateTimerFired(TimerBase*) {
  ASSERT(m_frame);
  ASSERT(m_frame->document());

  updateCursor();
}

void EventHandler::updateCursor() {
  TRACE_EVENT0("input", "EventHandler::updateCursor");

  // We must do a cross-frame hit test because the frame that triggered the
  // cursor update could be occluded by a different frame.
  ASSERT(m_frame == m_frame->localFrameRoot());

  if (m_mouseEventManager->isMousePositionUnknown())
    return;

  FrameView* view = m_frame->view();
  if (!view || !view->shouldSetCursor())
    return;

  LayoutViewItem layoutViewItem = view->layoutViewItem();
  if (layoutViewItem.isNull())
    return;

  m_frame->document()->updateStyleAndLayout();

  HitTestRequest request(HitTestRequest::ReadOnly |
                         HitTestRequest::AllowChildFrameContent);
  HitTestResult result(
      request,
      view->rootFrameToContents(m_mouseEventManager->lastKnownMousePosition()));
  layoutViewItem.hitTest(result);

  if (LocalFrame* frame = result.innerNodeFrame()) {
    OptionalCursor optionalCursor = frame->eventHandler().selectCursor(result);
    if (optionalCursor.isCursorChange()) {
      view->setCursor(optionalCursor.cursor());
    }
  }
}

OptionalCursor EventHandler::selectCursor(const HitTestResult& result) {
  if (m_scrollManager->inResizeMode())
    return NoCursorChange;

  Page* page = m_frame->page();
  if (!page)
    return NoCursorChange;
  if (m_scrollManager->middleClickAutoscrollInProgress())
    return NoCursorChange;

  Node* node = result.innerPossiblyPseudoNode();
  if (!node)
    return selectAutoCursor(result, node, iBeamCursor());

  LayoutObject* layoutObject = node->layoutObject();
  const ComputedStyle* style = layoutObject ? layoutObject->style() : nullptr;

  if (layoutObject) {
    Cursor overrideCursor;
    switch (layoutObject->getCursor(roundedIntPoint(result.localPoint()),
                                    overrideCursor)) {
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

  switch (style ? style->cursor() : ECursor::Auto) {
    case ECursor::Auto: {
      bool horizontalText = !style || style->isHorizontalWritingMode();
      const Cursor& iBeam =
          horizontalText ? iBeamCursor() : verticalTextCursor();
      return selectAutoCursor(result, node, iBeam);
    }
    case ECursor::Cross:
      return crossCursor();
    case ECursor::Pointer:
      return handCursor();
    case ECursor::Move:
      return moveCursor();
    case ECursor::AllScroll:
      return moveCursor();
    case ECursor::EResize:
      return eastResizeCursor();
    case ECursor::WResize:
      return westResizeCursor();
    case ECursor::NResize:
      return northResizeCursor();
    case ECursor::SResize:
      return southResizeCursor();
    case ECursor::NeResize:
      return northEastResizeCursor();
    case ECursor::SwResize:
      return southWestResizeCursor();
    case ECursor::NwResize:
      return northWestResizeCursor();
    case ECursor::SeResize:
      return southEastResizeCursor();
    case ECursor::NsResize:
      return northSouthResizeCursor();
    case ECursor::EwResize:
      return eastWestResizeCursor();
    case ECursor::NeswResize:
      return northEastSouthWestResizeCursor();
    case ECursor::NwseResize:
      return northWestSouthEastResizeCursor();
    case ECursor::ColResize:
      return columnResizeCursor();
    case ECursor::RowResize:
      return rowResizeCursor();
    case ECursor::Text:
      return iBeamCursor();
    case ECursor::Wait:
      return waitCursor();
    case ECursor::Help:
      return helpCursor();
    case ECursor::VerticalText:
      return verticalTextCursor();
    case ECursor::Cell:
      return cellCursor();
    case ECursor::ContextMenu:
      return contextMenuCursor();
    case ECursor::Progress:
      return progressCursor();
    case ECursor::NoDrop:
      return noDropCursor();
    case ECursor::Alias:
      return aliasCursor();
    case ECursor::Copy:
      return copyCursor();
    case ECursor::None:
      return noneCursor();
    case ECursor::NotAllowed:
      return notAllowedCursor();
    case ECursor::Default:
      return pointerCursor();
    case ECursor::ZoomIn:
      return zoomInCursor();
    case ECursor::ZoomOut:
      return zoomOutCursor();
    case ECursor::WebkitGrab:
      return grabCursor();
    case ECursor::WebkitGrabbing:
      return grabbingCursor();
  }
  return pointerCursor();
}

OptionalCursor EventHandler::selectAutoCursor(const HitTestResult& result,
                                              Node* node,
                                              const Cursor& iBeam) {
  if (result.scrollbar()) {
    return pointerCursor();
  }

  bool editable = (node && hasEditableStyle(*node));

  const bool isOverLink =
      !selectionController().mouseDownMayStartSelect() && result.isOverLink();
  if (useHandCursor(node, isOverLink))
    return handCursor();

  bool inResizer = false;
  LayoutObject* layoutObject = node ? node->layoutObject() : nullptr;
  if (layoutObject && m_frame->view()) {
    PaintLayer* layer = layoutObject->enclosingLayer();
    inResizer = layer->getScrollableArea() &&
                layer->getScrollableArea()->isPointInResizeControl(
                    result.roundedPointInMainFrame(), ResizerForPointer);
  }

  // During selection, use an I-beam no matter what we're over.
  // If a drag may be starting or we're capturing mouse events for a particular
  // node, don't treat this as a selection.
  if (m_mouseEventManager->mousePressed() &&
      selectionController().mouseDownMayStartSelect() &&
      !m_mouseEventManager->mouseDownMayStartDrag() &&
      !m_frame->selection().isNone() && !m_capturingMouseEventsNode) {
    return iBeam;
  }

  if ((editable ||
       (layoutObject && layoutObject->isText() && node->canStartSelection())) &&
      !inResizer && !result.scrollbar())
    return iBeam;
  return pointerCursor();
}

WebInputEventResult EventHandler::handleMousePressEvent(
    const PlatformMouseEvent& mouseEvent) {
  TRACE_EVENT0("blink", "EventHandler::handleMousePressEvent");

  // For 4th/5th button in the mouse since Chrome does not yet send
  // button value to Blink but in some cases it does send the event.
  // This check is needed to suppress such an event (crbug.com/574959)
  if (mouseEvent.pointerProperties().button ==
      WebPointerProperties::Button::NoButton)
    return WebInputEventResult::HandledSuppressed;

  if (m_eventHandlerWillResetCapturingMouseEventsNode)
    m_capturingMouseEventsNode = nullptr;
  m_mouseEventManager->handleMousePressEventUpdateStates(mouseEvent);
  selectionController().setMouseDownMayStartSelect(false);
  if (!m_frame->view())
    return WebInputEventResult::NotHandled;

  HitTestRequest request(HitTestRequest::Active);
  // Save the document point we generate in case the window coordinate is
  // invalidated by what happens when we dispatch the event.
  LayoutPoint documentPoint =
      m_frame->view()->rootFrameToContents(mouseEvent.position());
  MouseEventWithHitTestResults mev =
      m_frame->document()->performMouseEventHitTest(request, documentPoint,
                                                    mouseEvent);

  if (!mev.innerNode()) {
    m_mouseEventManager->invalidateClick();
    return WebInputEventResult::NotHandled;
  }

  m_mouseEventManager->setMousePressNode(mev.innerNode());
  m_frame->document()->setSequentialFocusNavigationStartingPoint(
      mev.innerNode());

  LocalFrame* subframe = subframeForHitTestResult(mev);
  if (subframe) {
    WebInputEventResult result = passMousePressEventToSubframe(mev, subframe);
    // Start capturing future events for this frame.  We only do this if we
    // didn't clear the m_mousePressed flag, which may happen if an AppKit
    // widget entered a modal event loop.  The capturing should be done only
    // when the result indicates it has been handled. See crbug.com/269917
    m_mouseEventManager->setCapturesDragging(
        subframe->eventHandler().m_mouseEventManager->capturesDragging());
    if (m_mouseEventManager->mousePressed() &&
        m_mouseEventManager->capturesDragging()) {
      m_capturingMouseEventsNode = mev.innerNode();
      m_eventHandlerWillResetCapturingMouseEventsNode = true;
    }
    m_mouseEventManager->invalidateClick();
    return result;
  }

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(m_frame->document()));
  m_frame->localFrameRoot()->eventHandler().m_lastMouseDownUserGestureToken =
      UserGestureIndicator::currentToken();

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled()) {
    // We store whether middle click autoscroll is in progress before calling
    // stopAutoscroll() because it will set m_autoscrollType to NoAutoscroll on
    // return.
    bool isMiddleClickAutoscrollInProgress =
        m_scrollManager->middleClickAutoscrollInProgress();
    m_scrollManager->stopAutoscroll();
    if (isMiddleClickAutoscrollInProgress) {
      // We invalidate the click when exiting middle click auto scroll so that
      // we don't inadvertently navigate away from the current page (e.g. the
      // click was on a hyperlink). See <rdar://problem/6095023>.
      m_mouseEventManager->invalidateClick();
      return WebInputEventResult::HandledSuppressed;
    }
  }

  m_mouseEventManager->setClickCount(mouseEvent.clickCount());
  m_mouseEventManager->setClickNode(
      mev.innerNode()->isTextNode()
          ? FlatTreeTraversal::parent(*mev.innerNode())
          : mev.innerNode());

  if (!mouseEvent.fromTouch())
    m_frame->selection().setCaretBlinkingSuspended(true);

  WebInputEventResult eventResult = updatePointerTargetAndDispatchEvents(
      EventTypeNames::mousedown, mev.innerNode(), mev.event());

  if (eventResult == WebInputEventResult::NotHandled && m_frame->view()) {
    FrameView* view = m_frame->view();
    PaintLayer* layer = mev.innerNode()->layoutObject()
                            ? mev.innerNode()->layoutObject()->enclosingLayer()
                            : nullptr;
    IntPoint p = view->rootFrameToContents(mouseEvent.position());
    if (layer && layer->getScrollableArea() &&
        layer->getScrollableArea()->isPointInResizeControl(p,
                                                           ResizerForPointer)) {
      m_scrollManager->setResizeScrollableArea(layer, p);
      return WebInputEventResult::HandledSystem;
    }
  }

  // m_selectionInitiationState is initialized after dispatching mousedown
  // event in order not to keep the selection by DOM APIs because we can't
  // give the user the chance to handle the selection by user action like
  // dragging if we keep the selection in case of mousedown. FireFox also has
  // the same behavior and it's more compatible with other browsers.
  selectionController().initializeSelectionState();
  HitTestResult hitTestResult = EventHandlingUtil::hitTestResultInFrame(
      m_frame, documentPoint, HitTestRequest::ReadOnly);
  InputDeviceCapabilities* sourceCapabilities =
      mouseEvent.getSyntheticEventType() == PlatformMouseEvent::FromTouch
          ? InputDeviceCapabilities::firesTouchEventsSourceCapabilities()
          : InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities();
  if (eventResult == WebInputEventResult::NotHandled) {
    eventResult = m_mouseEventManager->handleMouseFocus(hitTestResult,
                                                        sourceCapabilities);
  }
  m_mouseEventManager->setCapturesDragging(
      eventResult == WebInputEventResult::NotHandled || mev.scrollbar());

  // If the hit testing originally determined the event was in a scrollbar,
  // refetch the MouseEventWithHitTestResults in case the scrollbar widget was
  // destroyed when the mouse event was handled.
  if (mev.scrollbar()) {
    const bool wasLastScrollBar =
        mev.scrollbar() == m_lastScrollbarUnderMouse.get();
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    mev = m_frame->document()->performMouseEventHitTest(request, documentPoint,
                                                        mouseEvent);
    if (wasLastScrollBar && mev.scrollbar() != m_lastScrollbarUnderMouse.get())
      m_lastScrollbarUnderMouse = nullptr;
  }

  if (eventResult != WebInputEventResult::NotHandled) {
    // Scrollbars should get events anyway, even disabled controls might be
    // scrollable.
    passMousePressEventToScrollbar(mev);
  } else {
    if (shouldRefetchEventTarget(mev)) {
      HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
      mev = m_frame->document()->performMouseEventHitTest(
          request, documentPoint, mouseEvent);
    }

    if (passMousePressEventToScrollbar(mev))
      eventResult = WebInputEventResult::HandledSystem;
    else
      eventResult = m_mouseEventManager->handleMousePressEvent(mev);
  }

  if (mev.hitTestResult().innerNode() &&
      mouseEvent.pointerProperties().button ==
          WebPointerProperties::Button::Left) {
    ASSERT(mouseEvent.type() == PlatformEvent::MousePressed);
    HitTestResult result = mev.hitTestResult();
    result.setToShadowHostIfInUserAgentShadowRoot();
    m_frame->chromeClient().onMouseDown(result.innerNode());
  }

  return eventResult;
}

WebInputEventResult EventHandler::handleMouseMoveEvent(
    const PlatformMouseEvent& event) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseMoveEvent");

  HitTestResult hoveredNode = HitTestResult();
  WebInputEventResult result = handleMouseMoveOrLeaveEvent(event, &hoveredNode);

  Page* page = m_frame->page();
  if (!page)
    return result;

  if (PaintLayer* layer =
          EventHandlingUtil::layerForNode(hoveredNode.innerNode())) {
    if (ScrollableArea* layerScrollableArea =
            EventHandlingUtil::associatedScrollableArea(layer))
      layerScrollableArea->mouseMovedInContentArea();
  }

  if (FrameView* frameView = m_frame->view())
    frameView->mouseMovedInContentArea();

  hoveredNode.setToShadowHostIfInUserAgentShadowRoot();
  page->chromeClient().mouseDidMoveOverElement(*m_frame, hoveredNode);

  return result;
}

void EventHandler::handleMouseLeaveEvent(const PlatformMouseEvent& event) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseLeaveEvent");

  handleMouseMoveOrLeaveEvent(event, 0, false, true);
}

WebInputEventResult EventHandler::handleMouseMoveOrLeaveEvent(
    const PlatformMouseEvent& mouseEvent,
    HitTestResult* hoveredNode,
    bool onlyUpdateScrollbars,
    bool forceLeave) {
  ASSERT(m_frame);
  ASSERT(m_frame->view());

  m_mouseEventManager->setLastKnownMousePosition(mouseEvent);

  m_hoverTimer.stop();
  m_cursorUpdateTimer.stop();

  m_mouseEventManager->cancelFakeMouseMoveEvent();
  m_mouseEventManager->handleSvgPanIfNeeded(false);

  if (m_frameSetBeingResized) {
    return updatePointerTargetAndDispatchEvents(
        EventTypeNames::mousemove, m_frameSetBeingResized.get(), mouseEvent);
  }

  // Send events right to a scrollbar if the mouse is pressed.
  if (m_lastScrollbarUnderMouse && m_mouseEventManager->mousePressed()) {
    m_lastScrollbarUnderMouse->mouseMoved(mouseEvent);
    return WebInputEventResult::HandledSystem;
  }

  // Mouse events simulated from touch should not hit-test again.
  ASSERT(!mouseEvent.fromTouch());

  HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move;
  if (m_mouseEventManager->mousePressed()) {
    hitType |= HitTestRequest::Active;
  } else if (onlyUpdateScrollbars) {
    // Mouse events should be treated as "read-only" if we're updating only
    // scrollbars. This means that :hover and :active freeze in the state they
    // were in, rather than updating for nodes the mouse moves while the window
    // is not key (which will be the case if onlyUpdateScrollbars is true).
    hitType |= HitTestRequest::ReadOnly;
  }

  // Treat any mouse move events as readonly if the user is currently touching
  // the screen.
  if (m_pointerEventManager->isAnyTouchActive())
    hitType |= HitTestRequest::Active | HitTestRequest::ReadOnly;
  HitTestRequest request(hitType);
  MouseEventWithHitTestResults mev = MouseEventWithHitTestResults(
      mouseEvent, HitTestResult(request, LayoutPoint()));

  // We don't want to do a hit-test in forceLeave scenarios because there might
  // actually be some other frame above this one at the specified co-ordinate.
  // So we must force the hit-test to fail, while still clearing hover/active
  // state.
  if (forceLeave) {
    m_frame->document()->updateHoverActiveState(request, nullptr, nullptr);
  } else {
    mev = EventHandlingUtil::performMouseEventHitTest(m_frame, request,
                                                      mouseEvent);
  }

  if (hoveredNode)
    *hoveredNode = mev.hitTestResult();

  Scrollbar* scrollbar = nullptr;

  if (m_scrollManager->inResizeMode()) {
    m_scrollManager->resize(mev.event());
  } else {
    if (!scrollbar)
      scrollbar = mev.scrollbar();

    updateLastScrollbarUnderMouse(scrollbar,
                                  !m_mouseEventManager->mousePressed());
    if (onlyUpdateScrollbars)
      return WebInputEventResult::HandledSuppressed;
  }

  WebInputEventResult eventResult = WebInputEventResult::NotHandled;
  LocalFrame* newSubframe =
      m_capturingMouseEventsNode.get()
          ? subframeForTargetNode(m_capturingMouseEventsNode.get())
          : subframeForHitTestResult(mev);

  // We want mouseouts to happen first, from the inside out.  First send a move
  // event to the last subframe so that it will fire mouseouts.
  if (m_lastMouseMoveEventSubframe &&
      m_lastMouseMoveEventSubframe->tree().isDescendantOf(m_frame) &&
      m_lastMouseMoveEventSubframe != newSubframe)
    m_lastMouseMoveEventSubframe->eventHandler().handleMouseLeaveEvent(
        mev.event());

  if (newSubframe) {
    // Update over/out state before passing the event to the subframe.
    m_pointerEventManager->sendMouseAndPointerBoundaryEvents(
        updateMouseEventTargetNode(mev.innerNode()), mev.event());

    // Event dispatch in sendMouseAndPointerBoundaryEvents may have caused the
    // subframe of the target node to be detached from its FrameView, in which
    // case the event should not be passed.
    if (newSubframe->view())
      eventResult = passMouseMoveEventToSubframe(mev, newSubframe, hoveredNode);
  } else {
    if (scrollbar && !m_mouseEventManager->mousePressed()) {
      // Handle hover effects on platforms that support visual feedback on
      // scrollbar hovering.
      scrollbar->mouseMoved(mev.event());
    }
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

  eventResult = updatePointerTargetAndDispatchEvents(
      EventTypeNames::mousemove, mev.innerNode(), mev.event());
  if (eventResult != WebInputEventResult::NotHandled)
    return eventResult;

  return m_mouseEventManager->handleMouseDraggedEvent(mev);
}

WebInputEventResult EventHandler::handleMouseReleaseEvent(
    const PlatformMouseEvent& mouseEvent) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseReleaseEvent");

  // For 4th/5th button in the mouse since Chrome does not yet send
  // button value to Blink but in some cases it does send the event.
  // This check is needed to suppress such an event (crbug.com/574959)
  if (mouseEvent.pointerProperties().button ==
      WebPointerProperties::Button::NoButton)
    return WebInputEventResult::HandledSuppressed;

  if (!mouseEvent.fromTouch())
    m_frame->selection().setCaretBlinkingSuspended(false);

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled()) {
    if (Page* page = m_frame->page())
      page->autoscrollController().handleMouseReleaseForMiddleClickAutoscroll(
          m_frame, mouseEvent);
  }

  m_mouseEventManager->setMousePressed(false);
  m_mouseEventManager->setLastKnownMousePosition(mouseEvent);
  m_mouseEventManager->handleSvgPanIfNeeded(true);

  if (m_frameSetBeingResized) {
    return m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
        updateMouseEventTargetNode(m_frameSetBeingResized.get()),
        EventTypeNames::mouseup, mouseEvent);
  }

  if (m_lastScrollbarUnderMouse) {
    m_mouseEventManager->invalidateClick();
    m_lastScrollbarUnderMouse->mouseUp(mouseEvent);
    return updatePointerTargetAndDispatchEvents(
        EventTypeNames::mouseup, m_mouseEventManager->getNodeUnderMouse(),
        mouseEvent);
  }

  // Mouse events simulated from touch should not hit-test again.
  ASSERT(!mouseEvent.fromTouch());

  HitTestRequest::HitTestRequestType hitType = HitTestRequest::Release;
  HitTestRequest request(hitType);
  MouseEventWithHitTestResults mev =
      EventHandlingUtil::performMouseEventHitTest(m_frame, request, mouseEvent);
  LocalFrame* subframe =
      m_capturingMouseEventsNode.get()
          ? subframeForTargetNode(m_capturingMouseEventsNode.get())
          : subframeForHitTestResult(mev);
  if (m_eventHandlerWillResetCapturingMouseEventsNode)
    m_capturingMouseEventsNode = nullptr;
  if (subframe)
    return passMouseReleaseEventToSubframe(mev, subframe);

  // Mouse events will be associated with the Document where mousedown
  // occurred. If, e.g., there is a mousedown, then a drag to a different
  // Document and mouseup there, the mouseup's gesture will be associated with
  // the mousedown's Document. It's not absolutely certain that this is the
  // correct behavior.
  std::unique_ptr<UserGestureIndicator> gestureIndicator;
  if (m_frame->localFrameRoot()
          ->eventHandler()
          .m_lastMouseDownUserGestureToken) {
    gestureIndicator = wrapUnique(new UserGestureIndicator(
        m_frame->localFrameRoot()
            ->eventHandler()
            .m_lastMouseDownUserGestureToken.release()));
  } else {
    gestureIndicator = wrapUnique(new UserGestureIndicator(
        DocumentUserGestureToken::create(m_frame->document())));
  }

  WebInputEventResult eventResult = updatePointerTargetAndDispatchEvents(
      EventTypeNames::mouseup, mev.innerNode(), mev.event());

  WebInputEventResult clickEventResult =
      m_mouseEventManager->dispatchMouseClickIfNeeded(mev);

  m_scrollManager->clearResizeScrollableArea(false);

  if (eventResult == WebInputEventResult::NotHandled)
    eventResult = m_mouseEventManager->handleMouseReleaseEvent(mev);
  m_mouseEventManager->clearDragHeuristicState();

  m_mouseEventManager->invalidateClick();

  return EventHandlingUtil::mergeEventResult(clickEventResult, eventResult);
}

static bool targetIsFrame(Node* target, LocalFrame*& frame) {
  if (!isHTMLFrameElementBase(target))
    return false;

  // Cross-process drag and drop is not yet supported.
  if (toHTMLFrameElementBase(target)->contentFrame() &&
      !toHTMLFrameElementBase(target)->contentFrame()->isLocalFrame())
    return false;

  frame = toLocalFrame(toHTMLFrameElementBase(target)->contentFrame());
  return true;
}

static bool findDropZone(Node* target, DataTransfer* dataTransfer) {
  Element* element =
      target->isElementNode() ? toElement(target) : target->parentElement();
  for (; element; element = element->parentElement()) {
    bool matched = false;
    AtomicString dropZoneStr = element->fastGetAttribute(webkitdropzoneAttr);

    if (dropZoneStr.isEmpty())
      continue;

    UseCounter::count(element->document(),
                      UseCounter::PrefixedHTMLElementDropzone);

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
        matched =
            matched || dataTransfer->hasDropZoneType(keywords[i].getString());
      }

      if (matched && dragOperation != DragOperationNone)
        break;
    }
    if (matched) {
      dataTransfer->setDropEffect(
          convertDragOperationToDropZoneOperation(dragOperation));
      return true;
    }
  }
  return false;
}

WebInputEventResult EventHandler::updateDragAndDrop(
    const PlatformMouseEvent& event,
    DataTransfer* dataTransfer) {
  WebInputEventResult eventResult = WebInputEventResult::NotHandled;

  if (!m_frame->view())
    return eventResult;

  HitTestRequest request(HitTestRequest::ReadOnly);
  MouseEventWithHitTestResults mev =
      EventHandlingUtil::performMouseEventHitTest(m_frame, request, event);

  // Drag events should never go to text nodes (following IE, and proper
  // mouseover/out dispatch)
  Node* newTarget = mev.innerNode();
  if (newTarget && newTarget->isTextNode())
    newTarget = FlatTreeTraversal::parent(*newTarget);

  if (AutoscrollController* controller =
          m_scrollManager->autoscrollController())
    controller->updateDragAndDrop(newTarget, event.position(),
                                  event.timestamp());

  if (m_dragTarget != newTarget) {
    // FIXME: this ordering was explicitly chosen to match WinIE. However,
    // it is sometimes incorrect when dragging within subframes, as seen with
    // LayoutTests/fast/events/drag-in-frames.html.
    //
    // Moreover, this ordering conforms to section 7.9.4 of the HTML 5 spec.
    // <http://dev.w3.org/html5/spec/Overview.html#drag-and-drop-processing-model>.
    LocalFrame* targetFrame;
    if (targetIsFrame(newTarget, targetFrame)) {
      if (targetFrame)
        eventResult =
            targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
    } else if (newTarget) {
      // As per section 7.9.4 of the HTML 5 spec., we must always fire a drag
      // event before firing a dragenter, dragleave, or dragover event.
      if (m_mouseEventManager->dragState().m_dragSrc) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        m_mouseEventManager->dispatchDragSrcEvent(EventTypeNames::drag, event);
      }
      eventResult = m_mouseEventManager->dispatchDragEvent(
          EventTypeNames::dragenter, newTarget, event, dataTransfer);
      if (eventResult == WebInputEventResult::NotHandled &&
          findDropZone(newTarget, dataTransfer))
        eventResult = WebInputEventResult::HandledSystem;
    }

    if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
      if (targetFrame)
        eventResult =
            targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
    } else if (m_dragTarget) {
      m_mouseEventManager->dispatchDragEvent(
          EventTypeNames::dragleave, m_dragTarget.get(), event, dataTransfer);
    }

    if (newTarget) {
      // We do not explicitly call m_mouseEventManager->dispatchDragEvent here
      // because it could ultimately result in the appearance that two dragover
      // events fired. So, we mark that we should only fire a dragover event on
      // the next call to this function.
      m_shouldOnlyFireDragOverEvent = true;
    }
  } else {
    LocalFrame* targetFrame;
    if (targetIsFrame(newTarget, targetFrame)) {
      if (targetFrame)
        eventResult =
            targetFrame->eventHandler().updateDragAndDrop(event, dataTransfer);
    } else if (newTarget) {
      // Note, when dealing with sub-frames, we may need to fire only a dragover
      // event as a drag event may have been fired earlier.
      if (!m_shouldOnlyFireDragOverEvent &&
          m_mouseEventManager->dragState().m_dragSrc) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        m_mouseEventManager->dispatchDragSrcEvent(EventTypeNames::drag, event);
      }
      eventResult = m_mouseEventManager->dispatchDragEvent(
          EventTypeNames::dragover, newTarget, event, dataTransfer);
      if (eventResult == WebInputEventResult::NotHandled &&
          findDropZone(newTarget, dataTransfer))
        eventResult = WebInputEventResult::HandledSystem;
      m_shouldOnlyFireDragOverEvent = false;
    }
  }
  m_dragTarget = newTarget;

  return eventResult;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event,
                                     DataTransfer* dataTransfer) {
  LocalFrame* targetFrame;
  if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
    if (targetFrame)
      targetFrame->eventHandler().cancelDragAndDrop(event, dataTransfer);
  } else if (m_dragTarget.get()) {
    if (m_mouseEventManager->dragState().m_dragSrc)
      m_mouseEventManager->dispatchDragSrcEvent(EventTypeNames::drag, event);
    m_mouseEventManager->dispatchDragEvent(
        EventTypeNames::dragleave, m_dragTarget.get(), event, dataTransfer);
  }
  clearDragState();
}

WebInputEventResult EventHandler::performDragAndDrop(
    const PlatformMouseEvent& event,
    DataTransfer* dataTransfer) {
  LocalFrame* targetFrame;
  WebInputEventResult result = WebInputEventResult::NotHandled;
  if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
    if (targetFrame)
      result =
          targetFrame->eventHandler().performDragAndDrop(event, dataTransfer);
  } else if (m_dragTarget.get()) {
    result = m_mouseEventManager->dispatchDragEvent(
        EventTypeNames::drop, m_dragTarget.get(), event, dataTransfer);
  }
  clearDragState();
  return result;
}

void EventHandler::clearDragState() {
  m_scrollManager->stopAutoscroll();
  m_dragTarget = nullptr;
  m_capturingMouseEventsNode = nullptr;
  m_shouldOnlyFireDragOverEvent = false;
}

void EventHandler::setCapturingMouseEventsNode(Node* n) {
  m_capturingMouseEventsNode = n;
  m_eventHandlerWillResetCapturingMouseEventsNode = false;
}

Node* EventHandler::updateMouseEventTargetNode(Node* targetNode) {
  Node* newNodeUnderMouse = targetNode;

  // If we're capturing, we always go right to that node.
  EventTarget* mousePointerCapturingNode =
      m_pointerEventManager->getMouseCapturingNode();
  if (mousePointerCapturingNode &&
      !RuntimeEnabledFeatures::pointerEventV1SpecCapturingEnabled()) {
    newNodeUnderMouse = mousePointerCapturingNode->toNode();
    DCHECK(newNodeUnderMouse);
  } else if (m_capturingMouseEventsNode) {
    newNodeUnderMouse = m_capturingMouseEventsNode.get();
  } else {
    // If the target node is a text node, dispatch on the parent node -
    // rdar://4196646
    if (newNodeUnderMouse && newNodeUnderMouse->isTextNode())
      newNodeUnderMouse = FlatTreeTraversal::parent(*newNodeUnderMouse);
  }
  return newNodeUnderMouse;
}

bool EventHandler::isTouchPointerIdActiveOnFrame(int pointerId,
                                                 LocalFrame* frame) const {
  DCHECK_EQ(m_frame, m_frame->localFrameRoot());
  return m_pointerEventManager->isTouchPointerIdActiveOnFrame(pointerId, frame);
}

bool EventHandler::rootFrameTouchPointerActiveInCurrentFrame(
    int pointerId) const {
  return m_frame != m_frame->localFrameRoot() &&
         m_frame->localFrameRoot()
             ->eventHandler()
             .isTouchPointerIdActiveOnFrame(pointerId, m_frame);
}

bool EventHandler::isPointerEventActive(int pointerId) {
  return m_pointerEventManager->isActive(pointerId) ||
         rootFrameTouchPointerActiveInCurrentFrame(pointerId);
}

void EventHandler::setPointerCapture(int pointerId, EventTarget* target) {
  // TODO(crbug.com/591387): This functionality should be per page not per
  // frame.
  if (rootFrameTouchPointerActiveInCurrentFrame(pointerId)) {
    m_frame->localFrameRoot()->eventHandler().setPointerCapture(pointerId,
                                                                target);
  } else {
    m_pointerEventManager->setPointerCapture(pointerId, target);
  }
}

void EventHandler::releasePointerCapture(int pointerId, EventTarget* target) {
  if (rootFrameTouchPointerActiveInCurrentFrame(pointerId)) {
    m_frame->localFrameRoot()->eventHandler().releasePointerCapture(pointerId,
                                                                    target);
  } else {
    m_pointerEventManager->releasePointerCapture(pointerId, target);
  }
}

bool EventHandler::hasPointerCapture(int pointerId,
                                     const EventTarget* target) const {
  if (rootFrameTouchPointerActiveInCurrentFrame(pointerId)) {
    return m_frame->localFrameRoot()->eventHandler().hasPointerCapture(
        pointerId, target);
  } else {
    return m_pointerEventManager->hasPointerCapture(pointerId, target);
  }
}

bool EventHandler::hasProcessedPointerCapture(int pointerId,
                                              const EventTarget* target) const {
  return m_pointerEventManager->hasProcessedPointerCapture(pointerId, target);
}

void EventHandler::elementRemoved(EventTarget* target) {
  m_pointerEventManager->elementRemoved(target);
}

WebInputEventResult EventHandler::updatePointerTargetAndDispatchEvents(
    const AtomicString& mouseEventType,
    Node* targetNode,
    const PlatformMouseEvent& mouseEvent) {
  ASSERT(mouseEventType == EventTypeNames::mousedown ||
         mouseEventType == EventTypeNames::mousemove ||
         mouseEventType == EventTypeNames::mouseup);

  const auto& eventResult = m_pointerEventManager->sendMousePointerEvent(
      updateMouseEventTargetNode(targetNode), mouseEventType, mouseEvent);
  return eventResult;
}

WebInputEventResult EventHandler::handleWheelEvent(
    const PlatformWheelEvent& event) {
#if OS(MACOSX)
  // Filter Mac OS specific phases, usually with a zero-delta.
  // https://crbug.com/553732
  // TODO(chongz): EventSender sends events with |PlatformWheelEventPhaseNone|,
  // but it shouldn't.
  const int kPlatformWheelEventPhaseNoEventMask =
      PlatformWheelEventPhaseEnded | PlatformWheelEventPhaseCancelled |
      PlatformWheelEventPhaseMayBegin;
  if ((event.phase() & kPlatformWheelEventPhaseNoEventMask) ||
      (event.momentumPhase() & kPlatformWheelEventPhaseNoEventMask))
    return WebInputEventResult::NotHandled;
#endif
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
    WebInputEventResult result =
        subframe->eventHandler().handleWheelEvent(event);
    if (result != WebInputEventResult::NotHandled)
      m_scrollManager->setFrameWasScrolledByUser();
    return result;
  }

  if (node) {
    WheelEvent* domEvent =
        WheelEvent::create(event, node->document().domWindow());
    DispatchEventResult domEventResult = node->dispatchEvent(domEvent);
    if (domEventResult != DispatchEventResult::NotCanceled)
      return EventHandlingUtil::toWebInputEventResult(domEventResult);
  }

  return WebInputEventResult::NotHandled;
}

WebInputEventResult EventHandler::handleGestureEvent(
    const PlatformGestureEvent& gestureEvent) {
  // Propagation to inner frames is handled below this function.
  ASSERT(m_frame == m_frame->localFrameRoot());

  // Scrolling-related gesture events invoke EventHandler recursively for each
  // frame down the chain, doing a single-frame hit-test per frame. This matches
  // handleWheelEvent.
  // FIXME: Add a test that traverses this path, e.g. for devtools overlay.
  if (gestureEvent.isScrollEvent())
    return handleGestureScrollEvent(gestureEvent);

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targetedEvent =
      targetGestureEvent(gestureEvent);

  return handleGestureEvent(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureEvent(
    const GestureEventWithHitTestResults& targetedEvent) {
  TRACE_EVENT0("input", "EventHandler::handleGestureEvent");

  // Propagation to inner frames is handled below this function.
  ASSERT(m_frame == m_frame->localFrameRoot());

  // Non-scrolling related gesture events do a single cross-frame hit-test and
  // jump directly to the inner most frame. This matches handleMousePressEvent
  // etc.
  ASSERT(!targetedEvent.event().isScrollEvent());

  // Update mouseout/leave/over/enter events before jumping directly to the
  // inner most frame.
  if (targetedEvent.event().type() == PlatformEvent::GestureTap)
    updateGestureTargetNodeForMouseEvent(targetedEvent);

  // Route to the correct frame.
  if (LocalFrame* innerFrame = targetedEvent.hitTestResult().innerNodeFrame())
    return innerFrame->eventHandler().handleGestureEventInFrame(targetedEvent);

  // No hit test result, handle in root instance. Perhaps we should just return
  // false instead?
  return m_gestureManager->handleGestureEventInFrame(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureEventInFrame(
    const GestureEventWithHitTestResults& targetedEvent) {
  return m_gestureManager->handleGestureEventInFrame(targetedEvent);
}

WebInputEventResult EventHandler::handleGestureScrollEvent(
    const PlatformGestureEvent& gestureEvent) {
  TRACE_EVENT0("input", "EventHandler::handleGestureScrollEvent");

  return m_scrollManager->handleGestureScrollEvent(gestureEvent);
}

WebInputEventResult EventHandler::handleGestureScrollEnd(
    const PlatformGestureEvent& gestureEvent) {
  return m_scrollManager->handleGestureScrollEnd(gestureEvent);
}

void EventHandler::setMouseDownMayStartAutoscroll() {
  m_mouseEventManager->setMouseDownMayStartAutoscroll();
}

bool EventHandler::isScrollbarHandlingGestures() const {
  return m_scrollManager->isScrollbarHandlingGestures();
}

bool EventHandler::shouldApplyTouchAdjustment(
    const PlatformGestureEvent& event) const {
  if (m_frame->settings() && !m_frame->settings()->touchAdjustmentEnabled())
    return false;
  return !event.area().isEmpty();
}

bool EventHandler::bestClickableNodeForHitTestResult(
    const HitTestResult& result,
    IntPoint& targetPoint,
    Node*& targetNode) {
  // FIXME: Unify this with the other best* functions which are very similar.

  TRACE_EVENT0("input", "EventHandler::bestClickableNodeForHitTestResult");
  ASSERT(result.isRectBasedTest());

  // If the touch is over a scrollbar, don't adjust the touch point since touch
  // adjustment only takes into account DOM nodes so a touch over a scrollbar
  // will be adjusted towards nearby nodes. This leads to things like textarea
  // scrollbars being untouchable.
  if (result.scrollbar()) {
    targetNode = 0;
    return false;
  }

  IntPoint touchCenter =
      m_frame->view()->contentsToRootFrame(result.roundedPointInMainFrame());
  IntRect touchRect = m_frame->view()->contentsToRootFrame(
      result.hitTestLocation().boundingBox());

  HeapVector<Member<Node>, 11> nodes;
  copyToVector(result.listBasedTestResult(), nodes);

  // FIXME: the explicit Vector conversion copies into a temporary and is
  // wasteful.
  return findBestClickableCandidate(targetNode, targetPoint, touchCenter,
                                    touchRect, HeapVector<Member<Node>>(nodes));
}

bool EventHandler::bestContextMenuNodeForHitTestResult(
    const HitTestResult& result,
    IntPoint& targetPoint,
    Node*& targetNode) {
  ASSERT(result.isRectBasedTest());
  IntPoint touchCenter =
      m_frame->view()->contentsToRootFrame(result.roundedPointInMainFrame());
  IntRect touchRect = m_frame->view()->contentsToRootFrame(
      result.hitTestLocation().boundingBox());
  HeapVector<Member<Node>, 11> nodes;
  copyToVector(result.listBasedTestResult(), nodes);

  // FIXME: the explicit Vector conversion copies into a temporary and is
  // wasteful.
  return findBestContextMenuCandidate(targetNode, targetPoint, touchCenter,
                                      touchRect,
                                      HeapVector<Member<Node>>(nodes));
}

bool EventHandler::bestZoomableAreaForTouchPoint(const IntPoint& touchCenter,
                                                 const IntSize& touchRadius,
                                                 IntRect& targetArea,
                                                 Node*& targetNode) {
  if (touchRadius.isEmpty())
    return false;

  IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(touchCenter);

  HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly |
                                               HitTestRequest::Active |
                                               HitTestRequest::ListBased;
  HitTestResult result =
      hitTestResultAtPoint(hitTestPoint, hitType, LayoutSize(touchRadius));

  IntRect touchRect(touchCenter - touchRadius, touchRadius + touchRadius);
  HeapVector<Member<Node>, 11> nodes;
  copyToVector(result.listBasedTestResult(), nodes);

  // FIXME: the explicit Vector conversion copies into a temporary and is
  // wasteful.
  return findBestZoomableArea(targetNode, targetArea, touchCenter, touchRect,
                              HeapVector<Member<Node>>(nodes));
}

// Update the hover and active state across all frames for this gesture.
// This logic is different than the mouse case because mice send MouseLeave
// events to frames as they're exited.  With gestures, a single event
// conceptually both 'leaves' whatever frame currently had hover and enters a
// new frame
void EventHandler::updateGestureHoverActiveState(const HitTestRequest& request,
                                                 Element* innerElement) {
  ASSERT(m_frame == m_frame->localFrameRoot());

  HeapVector<Member<LocalFrame>> newHoverFrameChain;
  LocalFrame* newHoverFrameInDocument =
      innerElement ? innerElement->document().frame() : nullptr;
  // Insert the ancestors of the frame having the new hovered node to the frame
  // chain The frame chain doesn't include the main frame to avoid the redundant
  // work that cleans the hover state.  Because the hover state for the main
  // frame is updated by calling Document::updateHoverActiveState
  while (newHoverFrameInDocument && newHoverFrameInDocument != m_frame) {
    newHoverFrameChain.append(newHoverFrameInDocument);
    Frame* parentFrame = newHoverFrameInDocument->tree().parent();
    newHoverFrameInDocument = parentFrame && parentFrame->isLocalFrame()
                                  ? toLocalFrame(parentFrame)
                                  : nullptr;
  }

  Node* oldHoverNodeInCurDoc = m_frame->document()->hoverNode();
  Node* newInnermostHoverNode = innerElement;

  if (newInnermostHoverNode != oldHoverNodeInCurDoc) {
    size_t indexFrameChain = newHoverFrameChain.size();

    // Clear the hover state on any frames which are no longer in the frame
    // chain of the hovered element.
    while (oldHoverNodeInCurDoc &&
           oldHoverNodeInCurDoc->isFrameOwnerElement()) {
      LocalFrame* newHoverFrame = nullptr;
      // If we can't get the frame from the new hover frame chain,
      // the newHoverFrame will be null and the old hover state will be cleared.
      if (indexFrameChain > 0)
        newHoverFrame = newHoverFrameChain[--indexFrameChain];

      HTMLFrameOwnerElement* owner =
          toHTMLFrameOwnerElement(oldHoverNodeInCurDoc);
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
        doc->updateHoverActiveState(request, nullptr, nullptr);
    }
  }

  // Recursively set the new active/hover states on every frame in the chain of
  // innerElement.
  m_frame->document()->updateHoverActiveState(request, innerElement, nullptr);
}

// Update the mouseover/mouseenter/mouseout/mouseleave events across all frames
// for this gesture, before passing the targeted gesture event directly to a hit
// frame.
void EventHandler::updateGestureTargetNodeForMouseEvent(
    const GestureEventWithHitTestResults& targetedEvent) {
  ASSERT(m_frame == m_frame->localFrameRoot());

  // Behaviour of this function is as follows:
  // - Create the chain of all entered frames.
  // - Compare the last frame chain under the gesture to newly entered frame
  //   chain from the main frame one by one.
  // - If the last frame doesn't match with the entered frame, then create the
  //   chain of exited frames from the last frame chain.
  // - Dispatch mouseout/mouseleave events of the exited frames from the inside
  //   out.
  // - Dispatch mouseover/mouseenter events of the entered frames into the
  //   inside.

  // Insert the ancestors of the frame having the new target node to the entered
  // frame chain.
  HeapVector<Member<LocalFrame>> enteredFrameChain;
  LocalFrame* enteredFrameInDocument =
      targetedEvent.hitTestResult().innerNodeFrame();
  while (enteredFrameInDocument) {
    enteredFrameChain.append(enteredFrameInDocument);
    Frame* parentFrame = enteredFrameInDocument->tree().parent();
    enteredFrameInDocument = parentFrame && parentFrame->isLocalFrame()
                                 ? toLocalFrame(parentFrame)
                                 : nullptr;
  }

  size_t indexEnteredFrameChain = enteredFrameChain.size();
  LocalFrame* exitedFrameInDocument = m_frame;
  HeapVector<Member<LocalFrame>> exitedFrameChain;
  // Insert the frame from the disagreement between last frames and entered
  // frames.
  while (exitedFrameInDocument) {
    Node* lastNodeUnderTap = exitedFrameInDocument->eventHandler()
                                 .m_mouseEventManager->getNodeUnderMouse();
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
      LocalFrame* lastEnteredFrameInDocument =
          indexEnteredFrameChain ? enteredFrameChain[indexEnteredFrameChain - 1]
                                 : nullptr;
      if (exitedFrameInDocument != lastEnteredFrameInDocument)
        exitedFrameChain.append(exitedFrameInDocument);
      else if (nextExitedFrameInDocument && indexEnteredFrameChain)
        --indexEnteredFrameChain;
    }
    exitedFrameInDocument = nextExitedFrameInDocument;
  }

  const PlatformGestureEvent& gestureEvent = targetedEvent.event();
  unsigned modifiers = gestureEvent.getModifiers();
  PlatformMouseEvent fakeMouseMove(
      gestureEvent.position(), gestureEvent.globalPosition(),
      WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved,
      /* clickCount */ 0, static_cast<PlatformEvent::Modifiers>(modifiers),
      PlatformMouseEvent::FromTouch, gestureEvent.timestamp(),
      WebPointerProperties::PointerType::Mouse);

  // Update the mouseout/mouseleave event
  size_t indexExitedFrameChain = exitedFrameChain.size();
  while (indexExitedFrameChain) {
    LocalFrame* leaveFrame = exitedFrameChain[--indexExitedFrameChain];
    leaveFrame->eventHandler().m_mouseEventManager->setNodeUnderMouse(
        updateMouseEventTargetNode(nullptr), fakeMouseMove);
  }

  // update the mouseover/mouseenter event
  while (indexEnteredFrameChain) {
    Frame* parentFrame =
        enteredFrameChain[--indexEnteredFrameChain]->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame())
      toLocalFrame(parentFrame)
          ->eventHandler()
          .m_mouseEventManager->setNodeUnderMouse(
              updateMouseEventTargetNode(toHTMLFrameOwnerElement(
                  enteredFrameChain[indexEnteredFrameChain]->owner())),
              fakeMouseMove);
  }
}

GestureEventWithHitTestResults EventHandler::targetGestureEvent(
    const PlatformGestureEvent& gestureEvent,
    bool readOnly) {
  TRACE_EVENT0("input", "EventHandler::targetGestureEvent");

  ASSERT(m_frame == m_frame->localFrameRoot());
  // Scrolling events get hit tested per frame (like wheel events do).
  ASSERT(!gestureEvent.isScrollEvent());

  HitTestRequest::HitTestRequestType hitType =
      m_gestureManager->getHitTypeForGestureType(gestureEvent.type());
  double activeInterval = 0;
  bool shouldKeepActiveForMinInterval = false;
  if (readOnly) {
    hitType |= HitTestRequest::ReadOnly;
  } else if (gestureEvent.type() == PlatformEvent::GestureTap) {
    // If the Tap is received very shortly after ShowPress, we want to
    // delay clearing of the active state so that it's visible to the user
    // for at least a couple of frames.
    activeInterval = WTF::monotonicallyIncreasingTime() -
                     m_gestureManager->getLastShowPressTimestamp();
    shouldKeepActiveForMinInterval =
        m_gestureManager->getLastShowPressTimestamp() &&
        activeInterval < minimumActiveInterval;
    if (shouldKeepActiveForMinInterval)
      hitType |= HitTestRequest::ReadOnly;
  }

  GestureEventWithHitTestResults eventWithHitTestResults =
      hitTestResultForGestureEvent(gestureEvent, hitType);
  // Now apply hover/active state to the final target.
  HitTestRequest request(hitType | HitTestRequest::AllowChildFrameContent);
  if (!request.readOnly())
    updateGestureHoverActiveState(
        request, eventWithHitTestResults.hitTestResult().innerElement());

  if (shouldKeepActiveForMinInterval) {
    m_lastDeferredTapElement =
        eventWithHitTestResults.hitTestResult().innerElement();
    m_activeIntervalTimer.startOneShot(minimumActiveInterval - activeInterval,
                                       BLINK_FROM_HERE);
  }

  return eventWithHitTestResults;
}

GestureEventWithHitTestResults EventHandler::hitTestResultForGestureEvent(
    const PlatformGestureEvent& gestureEvent,
    HitTestRequest::HitTestRequestType hitType) {
  // Perform the rect-based hit-test (or point-based if adjustment is disabled).
  // Note that we don't yet apply hover/active state here because we need to
  // resolve touch adjustment first so that we apply hover/active it to the
  // final adjusted node.
  IntPoint hitTestPoint =
      m_frame->view()->rootFrameToContents(gestureEvent.position());
  LayoutSize padding;
  if (shouldApplyTouchAdjustment(gestureEvent)) {
    padding = LayoutSize(gestureEvent.area());
    if (!padding.isEmpty()) {
      padding.scale(1.f / 2);
      hitType |= HitTestRequest::ListBased;
    }
  }
  HitTestResult hitTestResult = hitTestResultAtPoint(
      hitTestPoint, hitType | HitTestRequest::ReadOnly, padding);

  // Adjust the location of the gesture to the most likely nearby node, as
  // appropriate for the type of event.
  PlatformGestureEvent adjustedEvent = gestureEvent;
  applyTouchAdjustment(&adjustedEvent, &hitTestResult);

  // Do a new hit-test at the (adjusted) gesture co-ordinates. This is necessary
  // because rect-based hit testing and touch adjustment sometimes return a
  // different node than what a point-based hit test would return for the same
  // point.
  // FIXME: Fix touch adjustment to avoid the need for a redundant hit test.
  // http://crbug.com/398914
  if (shouldApplyTouchAdjustment(gestureEvent)) {
    LocalFrame* hitFrame = hitTestResult.innerNodeFrame();
    if (!hitFrame)
      hitFrame = m_frame;
    hitTestResult = EventHandlingUtil::hitTestResultInFrame(
        hitFrame,
        hitFrame->view()->rootFrameToContents(adjustedEvent.position()),
        (hitType | HitTestRequest::ReadOnly) & ~HitTestRequest::ListBased);
  }

  // If we did a rect-based hit test it must be resolved to the best single node
  // by now to ensure consumers don't accidentally use one of the other
  // candidates.
  ASSERT(!hitTestResult.isRectBasedTest());

  return GestureEventWithHitTestResults(adjustedEvent, hitTestResult);
}

void EventHandler::applyTouchAdjustment(PlatformGestureEvent* gestureEvent,
                                        HitTestResult* hitTestResult) {
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
      adjusted = bestClickableNodeForHitTestResult(*hitTestResult,
                                                   adjustedPoint, adjustedNode);
      break;
    case PlatformEvent::GestureLongPress:
    case PlatformEvent::GestureLongTap:
    case PlatformEvent::GestureTwoFingerTap:
      adjusted = bestContextMenuNodeForHitTestResult(
          *hitTestResult, adjustedPoint, adjustedNode);
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  // Update the hit-test result to be a point-based result instead of a
  // rect-based result.
  // FIXME: We should do this even when no candidate matches the node filter.
  // crbug.com/398914
  if (adjusted) {
    hitTestResult->resolveRectBasedTest(
        adjustedNode, m_frame->view()->rootFrameToContents(adjustedPoint));
    gestureEvent->applyTouchAdjustment(adjustedPoint);
  }
}

WebInputEventResult EventHandler::sendContextMenuEvent(
    const PlatformMouseEvent& event,
    Node* overrideTargetNode) {
  FrameView* v = m_frame->view();
  if (!v)
    return WebInputEventResult::NotHandled;

  // Clear mouse press state to avoid initiating a drag while context menu is
  // up.
  m_mouseEventManager->setMousePressed(false);
  LayoutPoint positionInContents = v->rootFrameToContents(event.position());
  HitTestRequest request(HitTestRequest::Active);
  MouseEventWithHitTestResults mev =
      m_frame->document()->performMouseEventHitTest(request, positionInContents,
                                                    event);
  // Since |Document::performMouseEventHitTest()| modifies layout tree for
  // setting hover element, we need to update layout tree for requirement of
  // |SelectionController::sendContextMenuEvent()|.
  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  selectionController().sendContextMenuEvent(mev, positionInContents);

  Node* targetNode = overrideTargetNode ? overrideTargetNode : mev.innerNode();
  return m_mouseEventManager->dispatchMouseEvent(
      updateMouseEventTargetNode(targetNode), EventTypeNames::contextmenu,
      event, 0);
}

WebInputEventResult EventHandler::sendContextMenuEventForKey(
    Element* overrideTargetElement) {
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

  Element* focusedElement =
      overrideTargetElement ? overrideTargetElement : doc->focusedElement();
  FrameSelection& selection = m_frame->selection();
  Position start = selection.selection().start();
  VisualViewport& visualViewport = frameHost()->visualViewport();

  if (!overrideTargetElement && start.anchorNode() &&
      (selection.rootEditableElement() || selection.isRange())) {
    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    doc->updateStyleAndLayoutIgnorePendingStylesheets();

    IntRect firstRect = m_frame->editor().firstRectForRange(
        selection.selection().toNormalizedEphemeralRange());

    int x = rightAligned ? firstRect.maxX() : firstRect.x();
    // In a multiline edit, firstRect.maxY() would end up on the next line, so
    // -1.
    int y = firstRect.maxY() ? firstRect.maxY() - 1 : 0;
    locationInRootFrame = view->contentsToRootFrame(IntPoint(x, y));
  } else if (focusedElement) {
    IntRect clippedRect = focusedElement->boundsInViewport();
    locationInRootFrame =
        visualViewport.viewportToRootFrame(clippedRect.center());
  } else {
    locationInRootFrame = IntPoint(
        rightAligned
            ? visualViewport.visibleRect().maxX() - kContextMenuMargin
            : visualViewport.scrollOffset().width() + kContextMenuMargin,
        visualViewport.scrollOffset().height() + kContextMenuMargin);
  }

  m_frame->view()->setCursor(pointerCursor());
  IntPoint locationInViewport =
      visualViewport.rootFrameToViewport(locationInRootFrame);
  IntPoint globalPosition =
      view->getHostWindow()
          ->viewportToScreen(IntRect(locationInViewport, IntSize()),
                             m_frame->view())
          .location();

  Node* targetNode =
      overrideTargetElement ? overrideTargetElement : doc->focusedElement();
  if (!targetNode)
    targetNode = doc;

  // Use the focused node as the target for hover and active.
  HitTestRequest request(HitTestRequest::Active);
  HitTestResult result(request, locationInRootFrame);
  result.setInnerNode(targetNode);
  doc->updateHoverActiveState(request, result.innerElement(),
                              result.scrollbar());

  // The contextmenu event is a mouse event even when invoked using the
  // keyboard.  This is required for web compatibility.
  PlatformEvent::EventType eventType = PlatformEvent::MousePressed;
  if (m_frame->settings() && m_frame->settings()->showContextMenuOnMouseUp())
    eventType = PlatformEvent::MouseReleased;

  PlatformMouseEvent mouseEvent(
      locationInRootFrame, globalPosition,
      WebPointerProperties::Button::NoButton, eventType, /* clickCount */ 0,
      PlatformEvent::NoModifiers, PlatformMouseEvent::RealOrIndistinguishable,
      WTF::monotonicallyIncreasingTime(),
      WebPointerProperties::PointerType::Mouse);

  return sendContextMenuEvent(mouseEvent, overrideTargetElement);
}

void EventHandler::scheduleHoverStateUpdate() {
  if (!m_hoverTimer.isActive())
    m_hoverTimer.startOneShot(0, BLINK_FROM_HERE);
}

void EventHandler::scheduleCursorUpdate() {
  // We only want one timer for the page, rather than each frame having it's own
  // timer competing which eachother (since there's only one mouse cursor).
  ASSERT(m_frame == m_frame->localFrameRoot());

  if (!m_cursorUpdateTimer.isActive())
    m_cursorUpdateTimer.startOneShot(cursorUpdateInterval, BLINK_FROM_HERE);
}

bool EventHandler::cursorUpdatePending() {
  return m_cursorUpdateTimer.isActive();
}

void EventHandler::dispatchFakeMouseMoveEventSoon() {
  m_mouseEventManager->dispatchFakeMouseMoveEventSoon();
}

void EventHandler::dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad& quad) {
  m_mouseEventManager->dispatchFakeMouseMoveEventSoonInQuad(quad);
}

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet) {
  m_frameSetBeingResized = frameSet;
}

void EventHandler::resizeScrollableAreaDestroyed() {
  m_scrollManager->clearResizeScrollableArea(true);
}

void EventHandler::hoverTimerFired(TimerBase*) {
  TRACE_EVENT0("input", "EventHandler::hoverTimerFired");
  m_hoverTimer.stop();

  ASSERT(m_frame);
  ASSERT(m_frame->document());

  if (LayoutViewItem layoutItem = m_frame->contentLayoutItem()) {
    if (FrameView* view = m_frame->view()) {
      HitTestRequest request(HitTestRequest::Move);
      HitTestResult result(request,
                           view->rootFrameToContents(
                               m_mouseEventManager->lastKnownMousePosition()));
      layoutItem.hitTest(result);
      m_frame->document()->updateHoverActiveState(
          request, result.innerElement(), result.scrollbar());
    }
  }
}

void EventHandler::activeIntervalTimerFired(TimerBase*) {
  TRACE_EVENT0("input", "EventHandler::activeIntervalTimerFired");
  m_activeIntervalTimer.stop();

  if (m_frame && m_frame->document() && m_lastDeferredTapElement) {
    // FIXME: Enable condition when http://crbug.com/226842 lands
    // m_lastDeferredTapElement.get() == m_frame->document()->activeElement()
    HitTestRequest request(HitTestRequest::TouchEvent |
                           HitTestRequest::Release);
    m_frame->document()->updateHoverActiveState(
        request, m_lastDeferredTapElement.get(), nullptr);
  }
  m_lastDeferredTapElement = nullptr;
}

void EventHandler::notifyElementActivated() {
  // Since another element has been set to active, stop current timer and clear
  // reference.
  m_activeIntervalTimer.stop();
  m_lastDeferredTapElement = nullptr;
}

bool EventHandler::handleAccessKey(const WebKeyboardEvent& evt) {
  return m_keyboardEventManager->handleAccessKey(evt);
}

WebInputEventResult EventHandler::keyEvent(
    const WebKeyboardEvent& initialKeyEvent) {
  return m_keyboardEventManager->keyEvent(initialKeyEvent);
}

void EventHandler::defaultKeyboardEventHandler(KeyboardEvent* event) {
  m_keyboardEventManager->defaultKeyboardEventHandler(
      event, m_mouseEventManager->mousePressNode());
}

void EventHandler::dragSourceEndedAt(const PlatformMouseEvent& event,
                                     DragOperation operation) {
  // Asides from routing the event to the correct frame, the hit test is also an
  // opportunity for Layer to update the :hover and :active pseudoclasses.
  HitTestRequest request(HitTestRequest::Release);
  MouseEventWithHitTestResults mev =
      EventHandlingUtil::performMouseEventHitTest(m_frame, request, event);

  LocalFrame* targetFrame;
  if (targetIsFrame(mev.innerNode(), targetFrame)) {
    if (targetFrame) {
      targetFrame->eventHandler().dragSourceEndedAt(event, operation);
      return;
    }
  }

  m_mouseEventManager->dragSourceEndedAt(event, operation);
}

void EventHandler::updateDragStateAfterEditDragIfNeeded(
    Element* rootEditableElement) {
  // If inserting the dragged contents removed the drag source, we still want to
  // fire dragend at the root editble element.
  if (m_mouseEventManager->dragState().m_dragSrc &&
      !m_mouseEventManager->dragState().m_dragSrc->isConnected())
    m_mouseEventManager->dragState().m_dragSrc = rootEditableElement;
}

bool EventHandler::handleTextInputEvent(const String& text,
                                        Event* underlyingEvent,
                                        TextEventInputType inputType) {
  // Platforms should differentiate real commands like selectAll from text input
  // in disguise (like insertNewline), and avoid dispatching text input events
  // from keydown default handlers.
  ASSERT(!underlyingEvent || !underlyingEvent->isKeyboardEvent() ||
         toKeyboardEvent(underlyingEvent)->type() == EventTypeNames::keypress);

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

void EventHandler::defaultTextInputEventHandler(TextEvent* event) {
  if (m_frame->editor().handleTextEvent(event))
    event->setDefaultHandled();
}

void EventHandler::capsLockStateMayHaveChanged() {
  m_keyboardEventManager->capsLockStateMayHaveChanged();
}

bool EventHandler::passMousePressEventToScrollbar(
    MouseEventWithHitTestResults& mev) {
  Scrollbar* scrollbar = mev.scrollbar();
  updateLastScrollbarUnderMouse(scrollbar, true);

  if (!scrollbar || !scrollbar->enabled())
    return false;
  m_scrollManager->setFrameWasScrolledByUser();
  scrollbar->mouseDown(mev.event());
  return true;
}

// If scrollbar (under mouse) is different from last, send a mouse exited. Set
// last to scrollbar if setLast is true; else set last to 0.
void EventHandler::updateLastScrollbarUnderMouse(Scrollbar* scrollbar,
                                                 bool setLast) {
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

WebInputEventResult EventHandler::handleTouchEvent(
    const PlatformTouchEvent& event) {
  TRACE_EVENT0("blink", "EventHandler::handleTouchEvent");
  return m_pointerEventManager->handleTouchEvents(event);
}

WebInputEventResult EventHandler::passMousePressEventToSubframe(
    MouseEventWithHitTestResults& mev,
    LocalFrame* subframe) {
  selectionController().passMousePressEventToSubframe(mev);
  WebInputEventResult result =
      subframe->eventHandler().handleMousePressEvent(mev.event());
  if (result != WebInputEventResult::NotHandled)
    return result;
  return WebInputEventResult::HandledSystem;
}

WebInputEventResult EventHandler::passMouseMoveEventToSubframe(
    MouseEventWithHitTestResults& mev,
    LocalFrame* subframe,
    HitTestResult* hoveredNode) {
  if (m_mouseEventManager->mouseDownMayStartDrag())
    return WebInputEventResult::NotHandled;
  WebInputEventResult result =
      subframe->eventHandler().handleMouseMoveOrLeaveEvent(mev.event(),
                                                           hoveredNode);
  if (result != WebInputEventResult::NotHandled)
    return result;
  return WebInputEventResult::HandledSystem;
}

WebInputEventResult EventHandler::passMouseReleaseEventToSubframe(
    MouseEventWithHitTestResults& mev,
    LocalFrame* subframe) {
  WebInputEventResult result =
      subframe->eventHandler().handleMouseReleaseEvent(mev.event());
  if (result != WebInputEventResult::NotHandled)
    return result;
  return WebInputEventResult::HandledSystem;
}

FrameHost* EventHandler::frameHost() const {
  if (!m_frame->page())
    return nullptr;

  return &m_frame->page()->frameHost();
}

}  // namespace blink
