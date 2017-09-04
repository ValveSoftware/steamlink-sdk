// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class RootFrameViewport;

namespace {

bool fillsViewport(const Element& element) {
  DCHECK(element.layoutObject());
  DCHECK(element.layoutObject()->isBox());

  LayoutObject* layoutObject = element.layoutObject();

  // TODO(bokan): Broken for OOPIF. crbug.com/642378.
  Document& topDocument = element.document().topDocument();

  Vector<FloatQuad> quads;
  layoutObject->absoluteQuads(quads);
  DCHECK_EQ(quads.size(), 1u);

  if (!quads[0].isRectilinear())
    return false;

  LayoutRect boundingBox(quads[0].boundingBox());

  return boundingBox.location() == LayoutPoint::zero() &&
         boundingBox.size() == topDocument.layoutViewItem().size();
}

}  // namespace

// static
RootScrollerController* RootScrollerController::create(Document& document) {
  return new RootScrollerController(document);
}

RootScrollerController::RootScrollerController(Document& document)
    : m_document(&document) {}

DEFINE_TRACE(RootScrollerController) {
  visitor->trace(m_document);
  visitor->trace(m_rootScroller);
  visitor->trace(m_effectiveRootScroller);
}

void RootScrollerController::set(Element* newRootScroller) {
  m_rootScroller = newRootScroller;
  recomputeEffectiveRootScroller();
}

Element* RootScrollerController::get() const {
  return m_rootScroller;
}

Element* RootScrollerController::effectiveRootScroller() const {
  return m_effectiveRootScroller;
}

void RootScrollerController::didUpdateLayout() {
  recomputeEffectiveRootScroller();
}

void RootScrollerController::didDisposePaintLayerScrollableArea(
    PaintLayerScrollableArea& area) {
  // If the document is being torn down we'll skip a bunch of notifications
  // so recomputing the effective root scroller could touch dead objects.
  // (e.g. ScrollAnchor keeps a pointer to dead LayoutObjects).
  if (!m_effectiveRootScroller || area.box().documentBeingDestroyed())
    return;

  if (&area.box() == m_effectiveRootScroller->layoutObject())
    recomputeEffectiveRootScroller();
}

void RootScrollerController::recomputeEffectiveRootScroller() {
  bool rootScrollerValid =
      m_rootScroller && isValidRootScroller(*m_rootScroller);

  Element* newEffectiveRootScroller =
      rootScrollerValid ? m_rootScroller.get() : defaultEffectiveRootScroller();

  if (m_effectiveRootScroller == newEffectiveRootScroller)
    return;

  PaintLayer* oldRootScrollerLayer = rootScrollerPaintLayer();

  m_effectiveRootScroller = newEffectiveRootScroller;

  // This change affects both the old and new layers.
  if (oldRootScrollerLayer)
    oldRootScrollerLayer->setNeedsCompositingInputsUpdate();
  if (rootScrollerPaintLayer())
    rootScrollerPaintLayer()->setNeedsCompositingInputsUpdate();

  // The above may not be enough as we need to update existing ancestor
  // GraphicsLayers. This will force us to rebuild the GraphicsLayer tree.
  if (LayoutView* layoutView = m_document->layoutView()) {
    layoutView->compositor()->setNeedsCompositingUpdate(
        CompositingUpdateRebuildTree);
  }

  if (FrameHost* frameHost = m_document->frameHost())
    frameHost->globalRootScrollerController().didChangeRootScroller();
}

bool RootScrollerController::isValidRootScroller(const Element& element) const {
  if (!element.layoutObject())
    return false;

  if (!RootScrollerUtil::scrollableAreaFor(element))
    return false;

  if (!fillsViewport(element))
    return false;

  return true;
}

PaintLayer* RootScrollerController::rootScrollerPaintLayer() const {
  if (!m_effectiveRootScroller || !m_effectiveRootScroller->layoutObject() ||
      !m_effectiveRootScroller->layoutObject()->isBox())
    return nullptr;

  LayoutBox* box = toLayoutBox(m_effectiveRootScroller->layoutObject());
  PaintLayer* layer = box->layer();

  // If the root scroller is the <html> element we do a bit of a fake out
  // because while <html> has a PaintLayer, scrolling for it is handled by the
  // #document's PaintLayer (i.e. the PaintLayerCompositor's root layer). The
  // reason the root scroller is the <html> layer and not #document is because
  // the latter is a Node but not an Element.
  if (m_effectiveRootScroller->isSameNode(m_document->documentElement())) {
    if (!layer || !layer->compositor())
      return nullptr;
    return layer->compositor()->rootLayer();
  }

  return layer;
}

Element* RootScrollerController::defaultEffectiveRootScroller() {
  DCHECK(m_document);
  return m_document->documentElement();
}

}  // namespace blink
