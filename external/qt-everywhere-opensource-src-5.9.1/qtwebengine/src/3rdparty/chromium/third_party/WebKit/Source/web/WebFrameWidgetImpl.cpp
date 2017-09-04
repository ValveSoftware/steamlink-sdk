/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/WebFrameWidgetImpl.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ContextMenuController.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "platform/KeyboardCodes.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebRange.h"
#include "public/web/WebWidgetClient.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/CompositorMutatorImpl.h"
#include "web/CompositorProxyClientImpl.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/InspectorOverlay.h"
#include "web/PageOverlay.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewFrameWidget.h"
#include "wtf/AutoReset.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// WebFrameWidget ------------------------------------------------------------

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client,
                                       WebLocalFrame* localRoot) {
  // Pass the WebFrameWidget's self-reference to the caller.
  return WebFrameWidgetImpl::create(client, localRoot);
}

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client,
                                       WebView* webView,
                                       WebLocalFrame* mainFrame) {
  return new WebViewFrameWidget(client, toWebViewImpl(*webView),
                                toWebLocalFrameImpl(*mainFrame));
}

WebFrameWidgetImpl* WebFrameWidgetImpl::create(WebWidgetClient* client,
                                               WebLocalFrame* localRoot) {
  // Pass the WebFrameWidgetImpl's self-reference to the caller.
  return new WebFrameWidgetImpl(
      client, localRoot);  // SelfKeepAlive is set in constructor.
}

// static
WebFrameWidgetsSet& WebFrameWidgetImpl::allInstances() {
  DEFINE_STATIC_LOCAL(WebFrameWidgetsSet, allInstances, ());
  return allInstances;
}

WebFrameWidgetImpl::WebFrameWidgetImpl(WebWidgetClient* client,
                                       WebLocalFrame* localRoot)
    : m_client(client),
      m_localRoot(toWebLocalFrameImpl(localRoot)),
      m_mutator(nullptr),
      m_layerTreeView(nullptr),
      m_rootLayer(nullptr),
      m_rootGraphicsLayer(nullptr),
      m_isAcceleratedCompositingActive(false),
      m_layerTreeViewClosed(false),
      m_suppressNextKeypressEvent(false),
      m_ignoreInputEvents(false),
      m_isTransparent(false),
      m_imeAcceptEvents(true),
      m_selfKeepAlive(this) {
  DCHECK(m_localRoot->frame()->isLocalRoot());
  initializeLayerTreeView();
  m_localRoot->setFrameWidget(this);
  allInstances().add(this);

  if (localRoot->parent())
    setIsTransparent(true);
}

WebFrameWidgetImpl::~WebFrameWidgetImpl() {}

DEFINE_TRACE(WebFrameWidgetImpl) {
  visitor->trace(m_localRoot);
  visitor->trace(m_mouseCaptureNode);
}

// WebWidget ------------------------------------------------------------------

void WebFrameWidgetImpl::close() {
  WebDevToolsAgentImpl::webFrameWidgetImplClosed(this);
  DCHECK(allInstances().contains(this));
  allInstances().remove(this);

  m_localRoot->setFrameWidget(nullptr);
  m_localRoot = nullptr;
  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  m_client = nullptr;

  m_mutator = nullptr;
  m_layerTreeView = nullptr;
  m_rootLayer = nullptr;
  m_rootGraphicsLayer = nullptr;

  m_selfKeepAlive.clear();
}

WebSize WebFrameWidgetImpl::size() {
  return m_size;
}

void WebFrameWidgetImpl::resize(const WebSize& newSize) {
  if (m_size == newSize)
    return;

  FrameView* view = m_localRoot->frameView();
  if (!view)
    return;

  m_size = newSize;

  updateMainFrameLayoutSize();

  view->resize(m_size);

  // FIXME: In WebViewImpl this layout was a precursor to setting the minimum
  // scale limit.  It is not clear if this is necessary for frame-level widget
  // resize.
  if (view->needsLayout())
    view->layout();

  // FIXME: Investigate whether this is needed; comment from eseidel suggests
  // that this function is flawed.
  sendResizeEventAndRepaint();
}

void WebFrameWidgetImpl::sendResizeEventAndRepaint() {
  // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
  // as part of layout. Layout is also responsible for sending invalidations
  // to the embedder. This method and all callers may be wrong. -- eseidel.
  if (m_localRoot->frameView()) {
    // Enqueues the resize event.
    m_localRoot->frame()->document()->enqueueResizeEvent();
  }

  if (m_client) {
    if (isAcceleratedCompositingActive()) {
      updateLayerTreeViewport();
    } else {
      WebRect damagedRect(0, 0, m_size.width, m_size.height);
      m_client->didInvalidateRect(damagedRect);
    }
  }
}

void WebFrameWidgetImpl::resizeVisualViewport(const WebSize& newSize) {
  // TODO(alexmos, kenrb): resizing behavior such as this should be changed
  // to use Page messages.  https://crbug.com/599688.
  page()->frameHost().visualViewport().setSize(newSize);
  page()->frameHost().visualViewport().clampToBoundaries();

  view()->didUpdateFullscreenSize();
}

void WebFrameWidgetImpl::updateMainFrameLayoutSize() {
  if (!m_localRoot)
    return;

  FrameView* view = m_localRoot->frameView();
  if (!view)
    return;

  WebSize layoutSize = m_size;

  view->setLayoutSize(layoutSize);
}

void WebFrameWidgetImpl::setIgnoreInputEvents(bool newValue) {
  DCHECK_NE(m_ignoreInputEvents, newValue);
  m_ignoreInputEvents = newValue;
}

void WebFrameWidgetImpl::didEnterFullscreen() {
  view()->didEnterFullscreen();
}

void WebFrameWidgetImpl::didExitFullscreen() {
  view()->didExitFullscreen();
}

void WebFrameWidgetImpl::beginFrame(double lastFrameTimeMonotonic) {
  TRACE_EVENT1("blink", "WebFrameWidgetImpl::beginFrame", "frameTime",
               lastFrameTimeMonotonic);
  DCHECK(lastFrameTimeMonotonic);
  PageWidgetDelegate::animate(*page(), lastFrameTimeMonotonic);
}

void WebFrameWidgetImpl::updateAllLifecyclePhases() {
  TRACE_EVENT0("blink", "WebFrameWidgetImpl::updateAllLifecyclePhases");
  if (!m_localRoot)
    return;

  if (InspectorOverlay* overlay = inspectorOverlay()) {
    overlay->updateAllLifecyclePhases();
    // TODO(chrishtr): integrate paint into the overlay's lifecycle.
    if (overlay->pageOverlay() && overlay->pageOverlay()->graphicsLayer())
      overlay->pageOverlay()->graphicsLayer()->paint(nullptr);
  }
  PageWidgetDelegate::updateAllLifecyclePhases(*page(), *m_localRoot->frame());
  updateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  // Out-of-process iframes require compositing.
  NOTREACHED();
}

void WebFrameWidgetImpl::updateLayerTreeViewport() {
  if (!page() || !m_layerTreeView)
    return;

  // Pass the limits even though this is for subframes, as the limits will be
  // needed in setting the raster scale.
  m_layerTreeView->setPageScaleFactorAndLimits(
      1, view()->minimumPageScaleFactor(), view()->maximumPageScaleFactor());
}

void WebFrameWidgetImpl::updateLayerTreeBackgroundColor() {
  if (!m_layerTreeView)
    return;

  m_layerTreeView->setBackgroundColor(backgroundColor());
}

void WebFrameWidgetImpl::updateLayerTreeDeviceScaleFactor() {
  DCHECK(page());
  DCHECK(m_layerTreeView);

  float deviceScaleFactor = page()->deviceScaleFactor();
  m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebFrameWidgetImpl::setIsTransparent(bool isTransparent) {
  m_isTransparent = isTransparent;

  if (m_layerTreeView)
    m_layerTreeView->setHasTransparentBackground(isTransparent);
}

bool WebFrameWidgetImpl::isTransparent() const {
  return m_isTransparent;
}

void WebFrameWidgetImpl::layoutAndPaintAsync(
    WebLayoutAndPaintAsyncCallback* callback) {
  m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebFrameWidgetImpl::compositeAndReadbackAsync(
    WebCompositeAndReadbackAsyncCallback* callback) {
  m_layerTreeView->compositeAndReadbackAsync(callback);
}

void WebFrameWidgetImpl::themeChanged() {
  FrameView* view = m_localRoot->frameView();

  WebRect damagedRect(0, 0, m_size.width, m_size.height);
  view->invalidateRect(damagedRect);
}

const WebInputEvent* WebFrameWidgetImpl::m_currentInputEvent = nullptr;

WebInputEventResult WebFrameWidgetImpl::handleInputEvent(
    const WebInputEvent& inputEvent) {
  TRACE_EVENT1("input", "WebFrameWidgetImpl::handleInputEvent", "type",
               WebInputEvent::GetName(inputEvent.type));

  // If a drag-and-drop operation is in progress, ignore input events.
  if (m_doingDragAndDrop)
    return WebInputEventResult::HandledSuppressed;

  // Don't handle events once we've started shutting down.
  if (!page())
    return WebInputEventResult::NotHandled;

  if (InspectorOverlay* overlay = inspectorOverlay()) {
    if (overlay->handleInputEvent(inputEvent))
      return WebInputEventResult::HandledSuppressed;
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (m_ignoreInputEvents)
    return WebInputEventResult::NotHandled;

  // FIXME: pass event to m_localRoot's WebDevToolsAgentImpl once available.

  AutoReset<const WebInputEvent*> currentEventChange(&m_currentInputEvent,
                                                     &inputEvent);

  if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
    TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
    // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
    Node* node = m_mouseCaptureNode;

    // Not all platforms call mouseCaptureLost() directly.
    if (inputEvent.type == WebInputEvent::MouseUp)
      mouseCaptureLost();

    std::unique_ptr<UserGestureIndicator> gestureIndicator;

    AtomicString eventType;
    switch (inputEvent.type) {
      case WebInputEvent::MouseMove:
        eventType = EventTypeNames::mousemove;
        break;
      case WebInputEvent::MouseLeave:
        eventType = EventTypeNames::mouseout;
        break;
      case WebInputEvent::MouseDown:
        eventType = EventTypeNames::mousedown;
        gestureIndicator = wrapUnique(
            new UserGestureIndicator(DocumentUserGestureToken::create(
                &node->document(), UserGestureToken::NewGesture)));
        m_mouseCaptureGestureToken = gestureIndicator->currentToken();
        break;
      case WebInputEvent::MouseUp:
        eventType = EventTypeNames::mouseup;
        gestureIndicator = wrapUnique(
            new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
        break;
      default:
        NOTREACHED();
    }

    node->dispatchMouseEvent(
        PlatformMouseEventBuilder(
            m_localRoot->frameView(),
            static_cast<const WebMouseEvent&>(inputEvent)),
        eventType, static_cast<const WebMouseEvent&>(inputEvent).clickCount);
    return WebInputEventResult::HandledSystem;
  }

  return PageWidgetDelegate::handleInputEvent(*this, inputEvent,
                                              m_localRoot->frame());
}

void WebFrameWidgetImpl::setCursorVisibilityState(bool isVisible) {
  page()->setIsCursorVisible(isVisible);
}

bool WebFrameWidgetImpl::hasTouchEventHandlersAt(const WebPoint& point) {
  // FIXME: Implement this. Note that the point must be divided by
  // pageScaleFactor.
  return true;
}

void WebFrameWidgetImpl::setBaseBackgroundColor(WebColor color) {
  if (m_baseBackgroundColor == color)
    return;

  m_baseBackgroundColor = color;

  m_localRoot->frameView()->setBaseBackgroundColor(color);
}

WebInputMethodControllerImpl*
WebFrameWidgetImpl::getActiveWebInputMethodController() const {
  return WebInputMethodControllerImpl::fromFrame(
      focusedLocalFrameAvailableForIme());
}

void WebFrameWidgetImpl::scheduleAnimation() {
  if (m_layerTreeView) {
    m_layerTreeView->setNeedsBeginFrame();
    return;
  }
  if (m_client)
    m_client->scheduleAnimation();
}

CompositorProxyClient* WebFrameWidgetImpl::createCompositorProxyClient() {
  if (!m_mutator) {
    std::unique_ptr<CompositorMutatorClient> mutatorClient =
        CompositorMutatorImpl::createClient();
    m_mutator = static_cast<CompositorMutatorImpl*>(mutatorClient->mutator());
    m_layerTreeView->setMutatorClient(std::move(mutatorClient));
  }
  return new CompositorProxyClientImpl(m_mutator);
}

void WebFrameWidgetImpl::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    const WebFloatSize& mainFrameDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float pageScaleDelta,
    float browserControlsDelta) {
  // FIXME: To be implemented.
}

void WebFrameWidgetImpl::mouseCaptureLost() {
  TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
  m_mouseCaptureNode = nullptr;
}

void WebFrameWidgetImpl::setFocus(bool enable) {
  page()->focusController().setFocused(enable);
  if (enable) {
    page()->focusController().setActive(true);
    LocalFrame* focusedFrame = page()->focusController().focusedFrame();
    if (focusedFrame) {
      Element* element = focusedFrame->document()->focusedElement();
      if (element && focusedFrame->selection().selection().isNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focusedFrame->document()->updateStyleAndLayoutTree();
        if (element->isTextControl()) {
          element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
        } else if (hasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focusedFrame->selection().setSelection(
              SelectionInDOMTree::Builder().collapse(position).build());
        }
      }
    }
  } else {
    LocalFrame* focusedFrame = focusedLocalFrameInWidget();
    if (focusedFrame) {
      // Finish an ongoing composition to delete the composition node.
      if (focusedFrame->inputMethodController().hasComposition()) {
        WebAutofillClient* autofillClient =
            WebLocalFrameImpl::fromFrame(focusedFrame)->autofillClient();

        if (autofillClient)
          autofillClient->setIgnoreTextChanges(true);

        // TODO(xiaochengh): The use of
        // updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
        // See http://crbug.com/590369 for more details.
        focusedFrame->document()
            ->updateStyleAndLayoutIgnorePendingStylesheets();

        focusedFrame->inputMethodController().finishComposingText(
            InputMethodController::KeepSelection);

        if (autofillClient)
          autofillClient->setIgnoreTextChanges(false);
      }
      m_imeAcceptEvents = false;
    }
  }
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
WebRange WebFrameWidgetImpl::compositionRange() {
  LocalFrame* focused = focusedLocalFrameAvailableForIme();
  if (!focused)
    return WebRange();

  const EphemeralRange range =
      focused->inputMethodController().compositionEphemeralRange();
  if (range.isNull())
    return WebRange();

  Element* editable =
      focused->selection().rootEditableElementOrDocumentElement();
  DCHECK(editable);

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  editable->document().updateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::create(*editable, range);
}

WebTextInputInfo WebFrameWidgetImpl::textInputInfo() {
  LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return WebTextInputInfo();
  return focused->inputMethodController().textInputInfo();
}

WebTextInputType WebFrameWidgetImpl::textInputType() {
  LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return WebTextInputTypeNone;
  return focused->inputMethodController().textInputType();
}

WebColor WebFrameWidgetImpl::backgroundColor() const {
  if (isTransparent())
    return Color::transparent;
  if (!m_localRoot->frameView())
    return m_baseBackgroundColor;
  FrameView* view = m_localRoot->frameView();
  return view->documentBackgroundColor().rgb();
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
bool WebFrameWidgetImpl::selectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  const LocalFrame* localFrame = focusedLocalFrameInWidget();
  if (!localFrame)
    return false;

  FrameSelection& selection = localFrame->selection();
  if (selection.isNone())
    return false;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  localFrame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      localFrame->document()->lifecycle());

  if (selection.isCaret()) {
    anchor = focus = selection.absoluteCaretBounds();
  } else {
    const EphemeralRange selectedRange =
        selection.selection().toNormalizedEphemeralRange();
    if (selectedRange.isNull())
      return false;
    anchor = localFrame->editor().firstRectForRange(
        EphemeralRange(selectedRange.startPosition()));
    focus = localFrame->editor().firstRectForRange(
        EphemeralRange(selectedRange.endPosition()));
  }

  // FIXME: This doesn't apply page scale. This should probably be contents to
  // viewport. crbug.com/459293.
  IntRect scaledAnchor(localFrame->view()->contentsToRootFrame(anchor));
  IntRect scaledFocus(localFrame->view()->contentsToRootFrame(focus));

  anchor = scaledAnchor;
  focus = scaledFocus;

  if (!selection.selection().isBaseFirst())
    std::swap(anchor, focus);
  return true;
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
bool WebFrameWidgetImpl::selectionTextDirection(WebTextDirection& start,
                                                WebTextDirection& end) const {
  const LocalFrame* frame = focusedLocalFrameInWidget();
  if (!frame)
    return false;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  FrameSelection& selection = frame->selection();
  if (selection.selection().toNormalizedEphemeralRange().isNull())
    return false;
  start =
      toWebTextDirection(primaryDirectionOf(*selection.start().anchorNode()));
  end = toWebTextDirection(primaryDirectionOf(*selection.end().anchorNode()));
  return true;
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
bool WebFrameWidgetImpl::isSelectionAnchorFirst() const {
  if (const LocalFrame* frame = focusedLocalFrameInWidget())
    return frame->selection().selection().isBaseFirst();
  return false;
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
WebRange WebFrameWidgetImpl::caretOrSelectionRange() {
  LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return WebRange();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  focused->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return focused->inputMethodController().getSelectionOffsets();
}

void WebFrameWidgetImpl::setTextDirection(WebTextDirection direction) {
  // The Editor::setBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  const LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return;

  Editor& editor = focused->editor();
  if (!editor.canEdit())
    return;

  switch (direction) {
    case WebTextDirectionDefault:
      editor.setBaseWritingDirection(NaturalWritingDirection);
      break;

    case WebTextDirectionLeftToRight:
      editor.setBaseWritingDirection(LeftToRightWritingDirection);
      break;

    case WebTextDirectionRightToLeft:
      editor.setBaseWritingDirection(RightToLeftWritingDirection);
      break;

    default:
      NOTIMPLEMENTED();
      break;
  }
}

bool WebFrameWidgetImpl::isAcceleratedCompositingActive() const {
  return m_isAcceleratedCompositingActive;
}

void WebFrameWidgetImpl::willCloseLayerTreeView() {
  if (m_layerTreeView)
    page()->willCloseLayerTreeView(*m_layerTreeView);

  setIsAcceleratedCompositingActive(false);
  m_mutator = nullptr;
  m_layerTreeView = nullptr;
  m_layerTreeViewClosed = true;
}

void WebFrameWidgetImpl::didAcquirePointerLock() {
  page()->pointerLockController().didAcquirePointerLock();
}

void WebFrameWidgetImpl::didNotAcquirePointerLock() {
  page()->pointerLockController().didNotAcquirePointerLock();
}

void WebFrameWidgetImpl::didLosePointerLock() {
  page()->pointerLockController().didLosePointerLock();
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
bool WebFrameWidgetImpl::getCompositionCharacterBounds(
    WebVector<WebRect>& bounds) {
  WebRange range = compositionRange();
  if (range.isEmpty())
    return false;

  LocalFrame* frame = focusedLocalFrameInWidget();
  if (!frame)
    return false;

  WebLocalFrameImpl* webLocalFrame = WebLocalFrameImpl::fromFrame(frame);
  size_t characterCount = range.length();
  size_t offset = range.startOffset();
  WebVector<WebRect> result(characterCount);
  WebRect webrect;
  for (size_t i = 0; i < characterCount; ++i) {
    if (!webLocalFrame->firstRectForCharacterRange(offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      return false;
    }
    result[i] = webrect;
  }

  bounds.swap(result);
  return true;
}

// TODO(ekaramad):This method is almost duplicated in WebViewImpl as well. This
// code needs to be refactored  (http://crbug.com/629721).
void WebFrameWidgetImpl::applyReplacementRange(const WebRange& range) {
  if (LocalFrame* frame = focusedLocalFrameInWidget()) {
    // TODO(dglazkov): Going from LocalFrame to WebLocalFrameImpl seems
    // silly. What is going on here?
    WebLocalFrameImpl::fromFrame(frame)->selectRange(range);
  }
}

void WebFrameWidgetImpl::handleMouseLeave(LocalFrame& mainFrame,
                                          const WebMouseEvent& event) {
  // FIXME: WebWidget doesn't have the method below.
  // m_client->setMouseOverURL(WebURL());
  PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebFrameWidgetImpl::handleMouseDown(LocalFrame& mainFrame,
                                         const WebMouseEvent& event) {
  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  IntPoint point(event.x, event.y);
  if (event.button == WebMouseEvent::Button::Left) {
    point = m_localRoot->frameView()->rootFrameToContents(point);
    HitTestResult result(
        m_localRoot->frame()->eventHandler().hitTestResultAtPoint(point));
    result.setToShadowHostIfInUserAgentShadowRoot();
    Node* hitNode = result.innerNode();

    if (!result.scrollbar() && hitNode && hitNode->layoutObject() &&
        hitNode->layoutObject()->isEmbeddedObject()) {
      m_mouseCaptureNode = hitNode;
      TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
    }
  }

  PageWidgetEventHandler::handleMouseDown(mainFrame, event);

  if (event.button == WebMouseEvent::Button::Left && m_mouseCaptureNode)
    m_mouseCaptureGestureToken =
        mainFrame.eventHandler().takeLastMouseDownGestureToken();

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!page()->settings().showContextMenuOnMouseUp()) {
#if OS(MACOSX)
    if (event.button == WebMouseEvent::Button::Right ||
        (event.button == WebMouseEvent::Button::Left &&
         event.modifiers & WebMouseEvent::ControlKey))
      mouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::Right)
      mouseContextMenu(event);
#endif
  }
}

void WebFrameWidgetImpl::mouseContextMenu(const WebMouseEvent& event) {
  page()->contextMenuController().clearContextMenu();

  PlatformMouseEventBuilder pme(m_localRoot->frameView(), event);

  // Find the right target frame. See issue 1186900.
  HitTestResult result = hitTestResultForRootFramePos(pme.position());
  Frame* targetFrame;
  if (result.innerNodeOrImageMapImage())
    targetFrame = result.innerNodeOrImageMapImage()->document().frame();
  else
    targetFrame = page()->focusController().focusedOrMainFrame();

  // This will need to be changed to a nullptr check when focus control
  // is refactored, at which point focusedOrMainFrame will never return a
  // RemoteFrame.
  // See https://crbug.com/341918.
  if (!targetFrame->isLocalFrame())
    return;

  LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);

#if OS(WIN)
  targetLocalFrame->view()->setCursor(pointerCursor());
#endif

  {
    ContextMenuAllowedScope scope;
    targetLocalFrame->eventHandler().sendContextMenuEvent(pme, nullptr);
  }
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

void WebFrameWidgetImpl::handleMouseUp(LocalFrame& mainFrame,
                                       const WebMouseEvent& event) {
  PageWidgetEventHandler::handleMouseUp(mainFrame, event);

  if (page()->settings().showContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::Right)
      mouseContextMenu(event);
  }
}

WebInputEventResult WebFrameWidgetImpl::handleMouseWheel(
    LocalFrame& mainFrame,
    const WebMouseWheelEvent& event) {
  return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

WebInputEventResult WebFrameWidgetImpl::handleGestureEvent(
    const WebGestureEvent& event) {
  WebInputEventResult eventResult = WebInputEventResult::NotHandled;
  bool eventCancelled = false;
  switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
      break;
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
      m_client->didHandleGestureEvent(event, eventCancelled);
      return WebInputEventResult::NotHandled;
    default:
      NOTREACHED();
  }
  LocalFrame* frame = m_localRoot->frame();
  eventResult = frame->eventHandler().handleGestureEvent(
      PlatformGestureEventBuilder(frame->view(), event));
  m_client->didHandleGestureEvent(event, eventCancelled);
  return eventResult;
}

WebInputEventResult WebFrameWidgetImpl::handleKeyEvent(
    const WebKeyboardEvent& event) {
  DCHECK((event.type == WebInputEvent::RawKeyDown) ||
         (event.type == WebInputEvent::KeyDown) ||
         (event.type == WebInputEvent::KeyUp));

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.
  // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  m_suppressNextKeypressEvent = false;

  Frame* focusedFrame = focusedCoreFrame();
  if (focusedFrame && focusedFrame->isRemoteFrame()) {
    WebRemoteFrameImpl* webFrame =
        WebRemoteFrameImpl::fromFrame(*toRemoteFrame(focusedFrame));
    webFrame->client()->forwardInputEvent(&event);
    return WebInputEventResult::HandledSystem;
  }

  if (!focusedFrame || !focusedFrame->isLocalFrame())
    return WebInputEventResult::NotHandled;

  LocalFrame* frame = toLocalFrame(focusedFrame);

  WebInputEventResult result = frame->eventHandler().keyEvent(event);
  if (result != WebInputEventResult::NotHandled) {
    if (WebInputEvent::RawKeyDown == event.type) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = focusedElement();
      if (!element || !element->layoutObject() ||
          !element->layoutObject()->isEmbeddedObject())
        m_suppressNextKeypressEvent = true;
    }
    return result;
  }

#if !OS(MACOSX)
  const WebInputEvent::Type contextMenuKeyTriggeringEventType =
#if OS(WIN)
      WebInputEvent::KeyUp;
#else
      WebInputEvent::RawKeyDown;
#endif
  const WebInputEvent::Type shiftF10TriggeringEventType =
      WebInputEvent::RawKeyDown;

  bool isUnmodifiedMenuKey =
      !(event.modifiers & WebInputEvent::InputModifiers) &&
      event.windowsKeyCode == VKEY_APPS;
  bool isShiftF10 = (event.modifiers & WebInputEvent::InputModifiers) ==
                        WebInputEvent::ShiftKey &&
                    event.windowsKeyCode == VKEY_F10;
  if ((isUnmodifiedMenuKey &&
       event.type == contextMenuKeyTriggeringEventType) ||
      (isShiftF10 && event.type == shiftF10TriggeringEventType)) {
    view()->sendContextMenuEvent(event);
    return WebInputEventResult::HandledSystem;
  }
#endif  // !OS(MACOSX)

  return WebInputEventResult::NotHandled;
}

WebInputEventResult WebFrameWidgetImpl::handleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.type, WebInputEvent::Char);

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = m_suppressNextKeypressEvent;
  m_suppressNextKeypressEvent = false;

  LocalFrame* frame = toLocalFrame(focusedCoreFrame());
  if (!frame)
    return suppress ? WebInputEventResult::HandledSuppressed
                    : WebInputEventResult::NotHandled;

  EventHandler& handler = frame->eventHandler();

  if (!event.isCharacterKey())
    return WebInputEventResult::HandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  // It is unclear whether a keypress should be dispatched as well
  // crbug.com/563507
  if (handler.handleAccessKey(event))
    return WebInputEventResult::HandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.isSystemKey)
    return WebInputEventResult::NotHandled;

  if (suppress)
    return WebInputEventResult::HandledSuppressed;

  WebInputEventResult result = handler.keyEvent(event);
  if (result != WebInputEventResult::NotHandled)
    return result;

  return WebInputEventResult::NotHandled;
}

Frame* WebFrameWidgetImpl::focusedCoreFrame() const {
  return page() ? page()->focusController().focusedOrMainFrame() : nullptr;
}

Element* WebFrameWidgetImpl::focusedElement() const {
  LocalFrame* frame = page()->focusController().focusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->document();
  if (!document)
    return nullptr;

  return document->focusedElement();
}

void WebFrameWidgetImpl::initializeLayerTreeView() {
  if (m_client) {
    DCHECK(!m_mutator);
    m_client->initializeLayerTreeView();
    m_layerTreeView = m_client->layerTreeView();
  }

  if (WebDevToolsAgentImpl* devTools = m_localRoot->devToolsAgentImpl())
    devTools->layerTreeViewChanged(m_layerTreeView);

  page()->settings().setAcceleratedCompositingEnabled(m_layerTreeView);
  if (m_layerTreeView)
    page()->layerTreeViewInitialized(*m_layerTreeView);

  // FIXME: only unittests, click to play, Android priting, and printing (for
  // headers and footers) make this assert necessary. We should make them not
  // hit this code and then delete allowsBrokenNullLayerTreeView.
  DCHECK(m_layerTreeView || !m_client ||
         m_client->allowsBrokenNullLayerTreeView());
}

void WebFrameWidgetImpl::setIsAcceleratedCompositingActive(bool active) {
  // In the middle of shutting down; don't try to spin back up a compositor.
  // FIXME: compositing startup/shutdown should be refactored so that it
  // turns on explicitly rather than lazily, which causes this awkwardness.
  if (m_layerTreeViewClosed)
    return;

  DCHECK(!active || m_layerTreeView);

  if (m_isAcceleratedCompositingActive == active)
    return;

  if (!m_client)
    return;

  if (active) {
    TRACE_EVENT0("blink",
                 "WebViewImpl::setIsAcceleratedCompositingActive(true)");
    m_layerTreeView->setRootLayer(*m_rootLayer);

    m_layerTreeView->setVisible(page()->isPageVisible());
    updateLayerTreeDeviceScaleFactor();
    updateLayerTreeBackgroundColor();
    m_layerTreeView->setHasTransparentBackground(isTransparent());
    updateLayerTreeViewport();
    m_isAcceleratedCompositingActive = true;
  }
}

PaintLayerCompositor* WebFrameWidgetImpl::compositor() const {
  LocalFrame* frame = m_localRoot->frame();
  if (!frame || !frame->document() ||
      frame->document()->layoutViewItem().isNull())
    return nullptr;

  return frame->document()->layoutViewItem().compositor();
}

void WebFrameWidgetImpl::setRootGraphicsLayer(GraphicsLayer* layer) {
  m_rootGraphicsLayer = layer;
  m_rootLayer = layer ? layer->platformLayer() : nullptr;

  setIsAcceleratedCompositingActive(layer);

  if (!m_layerTreeView)
    return;

  if (m_rootLayer)
    m_layerTreeView->setRootLayer(*m_rootLayer);
  else
    m_layerTreeView->clearRootLayer();
}

void WebFrameWidgetImpl::setRootLayer(WebLayer* layer) {
  m_rootLayer = layer;

  setIsAcceleratedCompositingActive(layer);

  if (!m_layerTreeView)
    return;

  if (m_rootLayer)
    m_layerTreeView->setRootLayer(*m_rootLayer);
  else
    m_layerTreeView->clearRootLayer();
}

void WebFrameWidgetImpl::attachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositorTimeline) {
  if (m_layerTreeView)
    m_layerTreeView->attachCompositorAnimationTimeline(
        compositorTimeline->animationTimeline());
}

void WebFrameWidgetImpl::detachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositorTimeline) {
  if (m_layerTreeView)
    m_layerTreeView->detachCompositorAnimationTimeline(
        compositorTimeline->animationTimeline());
}

HitTestResult WebFrameWidgetImpl::coreHitTestResultAt(
    const WebPoint& pointInViewport) {
  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      m_localRoot->frame()->document()->lifecycle());
  FrameView* view = m_localRoot->frameView();
  IntPoint pointInRootFrame =
      view->contentsToFrame(view->viewportToContents(pointInViewport));
  return hitTestResultForRootFramePos(pointInRootFrame);
}

void WebFrameWidgetImpl::setVisibilityState(
    WebPageVisibilityState visibilityState) {
  if (m_layerTreeView)
    m_layerTreeView->setVisible(visibilityState ==
                                WebPageVisibilityStateVisible);
}

HitTestResult WebFrameWidgetImpl::hitTestResultForRootFramePos(
    const IntPoint& posInRootFrame) {
  IntPoint docPoint(
      m_localRoot->frame()->view()->rootFrameToContents(posInRootFrame));
  HitTestResult result =
      m_localRoot->frame()->eventHandler().hitTestResultAtPoint(
          docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
  result.setToShadowHostIfInUserAgentShadowRoot();
  return result;
}

InspectorOverlay* WebFrameWidgetImpl::inspectorOverlay() {
  if (!m_localRoot)
    return nullptr;
  if (WebDevToolsAgentImpl* devtools = m_localRoot->devToolsAgentImpl())
    return devtools->overlay();
  return nullptr;
}

LocalFrame* WebFrameWidgetImpl::focusedLocalFrameInWidget() const {
  LocalFrame* frame = page()->focusController().focusedFrame();
  return (frame && frame->localFrameRoot() == m_localRoot->frame()) ? frame
                                                                    : nullptr;
}

WebPlugin* WebFrameWidgetImpl::focusedPluginIfInputMethodSupported(
    LocalFrame* frame) const {
  WebPluginContainerImpl* container =
      WebLocalFrameImpl::currentPluginContainer(frame);
  if (container && container->supportsInputMethod())
    return container->plugin();
  return nullptr;
}

LocalFrame* WebFrameWidgetImpl::focusedLocalFrameAvailableForIme() const {
  if (!m_imeAcceptEvents)
    return nullptr;
  return focusedLocalFrameInWidget();
}

}  // namespace blink
