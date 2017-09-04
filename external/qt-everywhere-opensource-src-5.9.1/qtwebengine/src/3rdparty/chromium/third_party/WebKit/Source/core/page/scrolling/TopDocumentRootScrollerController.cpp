// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/TopDocumentRootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

// static
TopDocumentRootScrollerController* TopDocumentRootScrollerController::create(
    FrameHost& host) {
  return new TopDocumentRootScrollerController(host);
}

TopDocumentRootScrollerController::TopDocumentRootScrollerController(
    FrameHost& host)
    : m_frameHost(&host) {}

DEFINE_TRACE(TopDocumentRootScrollerController) {
  visitor->trace(m_viewportApplyScroll);
  visitor->trace(m_globalRootScroller);
  visitor->trace(m_frameHost);
}

void TopDocumentRootScrollerController::didChangeRootScroller() {
  recomputeGlobalRootScroller();
}

Element* TopDocumentRootScrollerController::findGlobalRootScrollerElement() {
  if (!topDocument())
    return nullptr;

  DCHECK(topDocument()->rootScrollerController());
  Element* element =
      topDocument()->rootScrollerController()->effectiveRootScroller();

  while (element && element->isFrameOwnerElement()) {
    HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(element);
    DCHECK(frameOwner);

    Document* iframeDocument = frameOwner->contentDocument();
    if (!iframeDocument)
      return element;

    DCHECK(iframeDocument->rootScrollerController());
    element = iframeDocument->rootScrollerController()->effectiveRootScroller();
  }

  return element;
}

void TopDocumentRootScrollerController::recomputeGlobalRootScroller() {
  if (!m_viewportApplyScroll)
    return;

  Element* target = findGlobalRootScrollerElement();
  if (!target || target == m_globalRootScroller)
    return;

  ScrollableArea* targetScroller = RootScrollerUtil::scrollableAreaFor(*target);

  if (!targetScroller)
    return;

  if (m_globalRootScroller)
    m_globalRootScroller->removeApplyScroll();

  // Use disable-native-scroll since the ViewportScrollCallback needs to
  // apply scroll actions both before (BrowserControls) and after (overscroll)
  // scrolling the element so it will apply scroll to the element itself.
  target->setApplyScroll(m_viewportApplyScroll, "disable-native-scroll");

  // A change in global root scroller requires a compositing inputs update to
  // the new and old global root scroller since it might change how the
  // ancestor layers are clipped. e.g. An iframe that's the global root
  // scroller clips its layers like the root frame.  Normally this is set
  // when the local effective root scroller changes but the global root
  // scroller can change because the parent's effective root scroller
  // changes.
  setNeedsCompositingInputsUpdateOnGlobalRootScroller();

  ScrollableArea* oldRootScrollerArea =
      m_globalRootScroller
          ? RootScrollerUtil::scrollableAreaFor(*m_globalRootScroller.get())
          : nullptr;

  m_globalRootScroller = target;

  setNeedsCompositingInputsUpdateOnGlobalRootScroller();

  // Ideally, scroll customization would pass the current element to scroll to
  // the apply scroll callback but this doesn't happen today so we set it
  // through a back door here. This is also needed by the
  // ViewportScrollCallback to swap the target into the layout viewport
  // in RootFrameViewport.
  m_viewportApplyScroll->setScroller(targetScroller);

  // The scrollers may need to stop using their own scrollbars as Android
  // Chrome's VisualViewport provides the scrollbars for the root scroller.
  if (oldRootScrollerArea)
    oldRootScrollerArea->didChangeGlobalRootScroller();

  targetScroller->didChangeGlobalRootScroller();
}

Document* TopDocumentRootScrollerController::topDocument() const {
  if (!m_frameHost)
    return nullptr;

  if (!m_frameHost->page().mainFrame() ||
      !m_frameHost->page().mainFrame()->isLocalFrame())
    return nullptr;

  return toLocalFrame(m_frameHost->page().mainFrame())->document();
}

void TopDocumentRootScrollerController::
    setNeedsCompositingInputsUpdateOnGlobalRootScroller() {
  if (!m_globalRootScroller)
    return;

  PaintLayer* layer = m_globalRootScroller->document()
                          .rootScrollerController()
                          ->rootScrollerPaintLayer();

  if (layer)
    layer->setNeedsCompositingInputsUpdate();

  if (LayoutView* view = m_globalRootScroller->document().layoutView()) {
    view->compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
  }
}

void TopDocumentRootScrollerController::didUpdateCompositing() {
  if (!m_frameHost)
    return;

  // Let the compositor-side counterpart know about this change.
  m_frameHost->chromeClient().registerViewportLayers();
}

void TopDocumentRootScrollerController::initializeViewportScrollCallback(
    RootFrameViewport& rootFrameViewport) {
  DCHECK(m_frameHost);
  m_viewportApplyScroll = ViewportScrollCallback::create(
      &m_frameHost->browserControls(), &m_frameHost->overscrollController(),
      rootFrameViewport);

  recomputeGlobalRootScroller();
}

bool TopDocumentRootScrollerController::isViewportScrollCallback(
    const ScrollStateCallback* callback) const {
  if (!callback)
    return false;

  return callback == m_viewportApplyScroll.get();
}

GraphicsLayer* TopDocumentRootScrollerController::rootScrollerLayer() const {
  if (!m_globalRootScroller)
    return nullptr;

  ScrollableArea* area =
      RootScrollerUtil::scrollableAreaFor(*m_globalRootScroller);

  if (!area)
    return nullptr;

  GraphicsLayer* graphicsLayer = area->layerForScrolling();

  // TODO(bokan): We should assert graphicsLayer here and
  // RootScrollerController should do whatever needs to happen to ensure
  // the root scroller gets composited.

  return graphicsLayer;
}

Element* TopDocumentRootScrollerController::globalRootScroller() const {
  return m_globalRootScroller.get();
}

}  // namespace blink
