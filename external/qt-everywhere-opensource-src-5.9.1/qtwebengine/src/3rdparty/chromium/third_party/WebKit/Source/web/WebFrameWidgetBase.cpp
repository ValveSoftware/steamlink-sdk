// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebFrameWidgetBase.h"

#include "core/frame/FrameHost.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/page/DragActions.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/Page.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebDocument.h"
#include "public/web/WebWidgetClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace {

// Helper to get LocalFrame* from WebLocalFrame*.
// TODO(dcheng): This should be moved into WebLocalFrame.
LocalFrame* toCoreFrame(WebLocalFrame* frame) {
  return toWebLocalFrameImpl(frame)->frame();
}

}  // namespace

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum : " #a)
STATIC_ASSERT_ENUM(DragOperationNone, WebDragOperationNone);
STATIC_ASSERT_ENUM(DragOperationCopy, WebDragOperationCopy);
STATIC_ASSERT_ENUM(DragOperationLink, WebDragOperationLink);
STATIC_ASSERT_ENUM(DragOperationGeneric, WebDragOperationGeneric);
STATIC_ASSERT_ENUM(DragOperationPrivate, WebDragOperationPrivate);
STATIC_ASSERT_ENUM(DragOperationMove, WebDragOperationMove);
STATIC_ASSERT_ENUM(DragOperationDelete, WebDragOperationDelete);
STATIC_ASSERT_ENUM(DragOperationEvery, WebDragOperationEvery);

WebDragOperation WebFrameWidgetBase::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& pointInViewport,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int modifiers) {
  DCHECK(!m_currentDragData);

  m_currentDragData = DataObject::create(webDragData);
  m_operationsAllowed = operationsAllowed;

  return dragTargetDragEnterOrOver(pointInViewport, screenPoint, DragEnter,
                                   modifiers);
}

WebDragOperation WebFrameWidgetBase::dragTargetDragOver(
    const WebPoint& pointInViewport,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int modifiers) {
  m_operationsAllowed = operationsAllowed;

  return dragTargetDragEnterOrOver(pointInViewport, screenPoint, DragOver,
                                   modifiers);
}

void WebFrameWidgetBase::dragTargetDragLeave() {
  DCHECK(m_currentDragData);

  DragData dragData(m_currentDragData.get(), IntPoint(), IntPoint(),
                    static_cast<DragOperation>(m_operationsAllowed));

  page()->dragController().dragExited(&dragData, *toCoreFrame(localRoot()));

  // FIXME: why is the drag scroll timer not stopped here?

  m_dragOperation = WebDragOperationNone;
  m_currentDragData = nullptr;
}

void WebFrameWidgetBase::dragTargetDrop(const WebDragData& webDragData,
                                        const WebPoint& pointInViewport,
                                        const WebPoint& screenPoint,
                                        int modifiers) {
  WebPoint pointInRootFrame(viewportToRootFrame(pointInViewport));

  DCHECK(m_currentDragData);
  m_currentDragData = DataObject::create(webDragData);
  WebViewImpl::UserGestureNotifier notifier(view());

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (m_dragOperation == WebDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    dragTargetDragLeave();
    return;
  }

  m_currentDragData->setModifiers(modifiers);
  DragData dragData(m_currentDragData.get(), pointInRootFrame, screenPoint,
                    static_cast<DragOperation>(m_operationsAllowed));

  page()->dragController().performDrag(&dragData, *toCoreFrame(localRoot()));

  m_dragOperation = WebDragOperationNone;
  m_currentDragData = nullptr;
}

void WebFrameWidgetBase::dragSourceEndedAt(const WebPoint& pointInViewport,
                                           const WebPoint& screenPoint,
                                           WebDragOperation operation) {
  WebPoint pointInRootFrame(
      page()->frameHost().visualViewport().viewportToRootFrame(
          pointInViewport));
  PlatformMouseEvent pme(
      pointInRootFrame, screenPoint, WebPointerProperties::Button::Left,
      PlatformEvent::MouseMoved, 0, PlatformEvent::NoModifiers,
      PlatformMouseEvent::RealOrIndistinguishable,
      WTF::monotonicallyIncreasingTime());
  toCoreFrame(localRoot())
      ->eventHandler()
      .dragSourceEndedAt(pme, static_cast<DragOperation>(operation));
}

void WebFrameWidgetBase::dragSourceSystemDragEnded() {
  // It's possible for us to get this callback while not doing a drag if it's
  // from a previous page that got unloaded.
  if (m_doingDragAndDrop) {
    page()->dragController().dragEnded();
    m_doingDragAndDrop = false;
  }
}

void WebFrameWidgetBase::startDragging(WebReferrerPolicy policy,
                                       const WebDragData& data,
                                       WebDragOperationsMask mask,
                                       const WebImage& dragImage,
                                       const WebPoint& dragImageOffset) {
  m_doingDragAndDrop = true;
  client()->startDragging(policy, data, mask, dragImage, dragImageOffset);
}

WebDragOperation WebFrameWidgetBase::dragTargetDragEnterOrOver(
    const WebPoint& pointInViewport,
    const WebPoint& screenPoint,
    DragAction dragAction,
    int modifiers) {
  DCHECK(m_currentDragData);

  WebPoint pointInRootFrame(viewportToRootFrame(pointInViewport));

  m_currentDragData->setModifiers(modifiers);
  DragData dragData(m_currentDragData.get(), pointInRootFrame, screenPoint,
                    static_cast<DragOperation>(m_operationsAllowed));

  DragSession dragSession;
  dragSession = page()->dragController().dragEnteredOrUpdated(
      &dragData, *toCoreFrame(localRoot()));

  DragOperation dropEffect = dragSession.operation;

  // Mask the drop effect operation against the drag source's allowed
  // operations.
  if (!(dropEffect & dragData.draggingSourceOperationMask()))
    dropEffect = DragOperationNone;

  m_dragOperation = static_cast<WebDragOperation>(dropEffect);

  return m_dragOperation;
}

WebPoint WebFrameWidgetBase::viewportToRootFrame(
    const WebPoint& pointInViewport) const {
  return page()->frameHost().visualViewport().viewportToRootFrame(
      pointInViewport);
}

WebViewImpl* WebFrameWidgetBase::view() const {
  return toWebLocalFrameImpl(localRoot())->viewImpl();
}

Page* WebFrameWidgetBase::page() const {
  return view()->page();
}

}  // namespace blink
