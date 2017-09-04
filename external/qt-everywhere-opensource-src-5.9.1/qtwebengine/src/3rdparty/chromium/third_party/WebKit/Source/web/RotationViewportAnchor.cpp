// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/RotationViewportAnchor.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "platform/geometry/DoubleRect.h"

namespace blink {

namespace {

static const float viewportAnchorRelativeEpsilon = 0.1f;
static const int viewportToNodeMaxRelativeArea = 2;

Node* findNonEmptyAnchorNode(const IntPoint& point,
                             const IntRect& viewRect,
                             EventHandler& eventHandler) {
  Node* node = eventHandler
                   .hitTestResultAtPoint(
                       point, HitTestRequest::ReadOnly | HitTestRequest::Active)
                   .innerNode();

  // If the node bounding box is sufficiently large, make a single attempt to
  // find a smaller node; the larger the node bounds, the greater the
  // variability under resize.
  const int maxNodeArea =
      viewRect.width() * viewRect.height() * viewportToNodeMaxRelativeArea;
  if (node &&
      node->boundingBox().width() * node->boundingBox().height() >
          maxNodeArea) {
    IntSize pointOffset = viewRect.size();
    pointOffset.scale(viewportAnchorRelativeEpsilon);
    node = eventHandler
               .hitTestResultAtPoint(
                   point + pointOffset,
                   HitTestRequest::ReadOnly | HitTestRequest::Active)
               .innerNode();
  }

  while (node && node->boundingBox().isEmpty())
    node = node->parentNode();

  return node;
}

void moveToEncloseRect(IntRect& outer, const FloatRect& inner) {
  IntPoint minimumPosition =
      ceiledIntPoint(inner.location() + inner.size() - FloatSize(outer.size()));
  IntPoint maximumPosition = flooredIntPoint(inner.location());

  IntPoint outerOrigin = outer.location();
  outerOrigin = outerOrigin.expandedTo(minimumPosition);
  outerOrigin = outerOrigin.shrunkTo(maximumPosition);

  outer.setLocation(outerOrigin);
}

void moveIntoRect(FloatRect& inner, const IntRect& outer) {
  FloatPoint minimumPosition = FloatPoint(outer.location());
  FloatPoint maximumPosition = minimumPosition + outer.size() - inner.size();

  // Adjust maximumPosition to the nearest lower integer because
  // VisualViewport::maximumScrollPosition() does the same.
  // The value of minumumPosition is already adjusted since it is
  // constructed from an integer point.
  maximumPosition = flooredIntPoint(maximumPosition);

  FloatPoint innerOrigin = inner.location();
  innerOrigin = innerOrigin.expandedTo(minimumPosition);
  innerOrigin = innerOrigin.shrunkTo(maximumPosition);

  inner.setLocation(innerOrigin);
}

}  // namespace

RotationViewportAnchor::RotationViewportAnchor(
    FrameView& rootFrameView,
    VisualViewport& visualViewport,
    const FloatSize& anchorInInnerViewCoords,
    PageScaleConstraintsSet& pageScaleConstraintsSet)
    : m_rootFrameView(&rootFrameView),
      m_visualViewport(&visualViewport),
      m_anchorInInnerViewCoords(anchorInInnerViewCoords),
      m_pageScaleConstraintsSet(pageScaleConstraintsSet) {
  setAnchor();
}

RotationViewportAnchor::~RotationViewportAnchor() {
  restoreToAnchor();
}

void RotationViewportAnchor::setAnchor() {
  RootFrameViewport* rootFrameViewport =
      m_rootFrameView->getRootFrameViewport();
  DCHECK(rootFrameViewport);

  m_oldPageScaleFactor = m_visualViewport->scale();
  m_oldMinimumPageScaleFactor =
      m_pageScaleConstraintsSet.finalConstraints().minimumScale;

  // Save the absolute location in case we won't find the anchor node, we'll
  // fall back to that.
  m_visualViewportInDocument =
      FloatPoint(rootFrameViewport->visibleContentRect().location());

  m_anchorNode.clear();
  m_anchorNodeBounds = LayoutRect();
  m_anchorInNodeCoords = FloatSize();
  m_normalizedVisualViewportOffset = FloatSize();

  IntRect innerViewRect = rootFrameViewport->visibleContentRect();

  // Preserve origins at the absolute screen origin.
  if (innerViewRect.location() == IntPoint::zero() || innerViewRect.isEmpty())
    return;

  IntRect outerViewRect =
      layoutViewport().visibleContentRect(IncludeScrollbars);

  // Normalize by the size of the outer rect
  DCHECK(!outerViewRect.isEmpty());
  m_normalizedVisualViewportOffset = m_visualViewport->scrollOffset();
  m_normalizedVisualViewportOffset.scale(1.0 / outerViewRect.width(),
                                         1.0 / outerViewRect.height());

  // Note, we specifically use the unscaled visual viewport size here as the
  // conversion into content-space below will apply the scale.
  FloatPoint anchorOffset(m_visualViewport->size());
  anchorOffset.scale(m_anchorInInnerViewCoords.width(),
                     m_anchorInInnerViewCoords.height());

  // Note, we specifically convert to the rootFrameView contents here, rather
  // than the layout viewport. That's because hit testing works from the
  // FrameView's content coordinates even if it's not the layout viewport.
  const FloatPoint anchorPointInContents = m_rootFrameView->frameToContents(
      m_visualViewport->viewportToRootFrame(anchorOffset));

  Node* node = findNonEmptyAnchorNode(flooredIntPoint(anchorPointInContents),
                                      innerViewRect,
                                      m_rootFrameView->frame().eventHandler());
  if (!node)
    return;

  m_anchorNode = node;
  m_anchorNodeBounds = node->boundingBox();
  m_anchorInNodeCoords =
      anchorPointInContents - FloatPoint(m_anchorNodeBounds.location());
  m_anchorInNodeCoords.scale(1.f / m_anchorNodeBounds.width(),
                             1.f / m_anchorNodeBounds.height());
}

void RotationViewportAnchor::restoreToAnchor() {
  float newPageScaleFactor =
      m_oldPageScaleFactor / m_oldMinimumPageScaleFactor *
      m_pageScaleConstraintsSet.finalConstraints().minimumScale;
  newPageScaleFactor =
      m_pageScaleConstraintsSet.finalConstraints().clampToConstraints(
          newPageScaleFactor);

  FloatSize visualViewportSize(m_visualViewport->size());
  visualViewportSize.scale(1 / newPageScaleFactor);

  IntPoint mainFrameOrigin;
  FloatPoint visualViewportOrigin;

  computeOrigins(visualViewportSize, mainFrameOrigin, visualViewportOrigin);

  layoutViewport().setScrollOffset(toScrollOffset(mainFrameOrigin),
                                   ProgrammaticScroll);

  // Set scale before location, since location can be clamped on setting scale.
  m_visualViewport->setScale(newPageScaleFactor);
  m_visualViewport->setLocation(visualViewportOrigin);
}

ScrollableArea& RotationViewportAnchor::layoutViewport() const {
  RootFrameViewport* rootFrameViewport =
      m_rootFrameView->getRootFrameViewport();
  DCHECK(rootFrameViewport);
  return rootFrameViewport->layoutViewport();
}

void RotationViewportAnchor::computeOrigins(
    const FloatSize& innerSize,
    IntPoint& mainFrameOffset,
    FloatPoint& visualViewportOffset) const {
  IntSize outerSize = layoutViewport().visibleContentRect().size();

  // Compute the viewport origins in CSS pixels relative to the document.
  FloatSize absVisualViewportOffset = m_normalizedVisualViewportOffset;
  absVisualViewportOffset.scale(outerSize.width(), outerSize.height());

  FloatPoint innerOrigin = getInnerOrigin(innerSize);
  FloatPoint outerOrigin = innerOrigin - absVisualViewportOffset;

  IntRect outerRect = IntRect(flooredIntPoint(outerOrigin), outerSize);
  FloatRect innerRect = FloatRect(innerOrigin, innerSize);

  moveToEncloseRect(outerRect, innerRect);

  outerRect.setLocation(IntPoint(
      layoutViewport().clampScrollOffset(toIntSize(outerRect.location()))));

  moveIntoRect(innerRect, outerRect);

  mainFrameOffset = outerRect.location();
  visualViewportOffset =
      FloatPoint(innerRect.location() - outerRect.location());
}

FloatPoint RotationViewportAnchor::getInnerOrigin(
    const FloatSize& innerSize) const {
  if (!m_anchorNode || !m_anchorNode->isConnected())
    return m_visualViewportInDocument;

  const LayoutRect currentNodeBounds = m_anchorNode->boundingBox();
  if (m_anchorNodeBounds == currentNodeBounds)
    return m_visualViewportInDocument;

  RootFrameViewport* rootFrameViewport =
      m_rootFrameView->getRootFrameViewport();
  const LayoutRect currentNodeBoundsInLayoutViewport =
      rootFrameViewport->rootContentsToLayoutViewportContents(
          *m_rootFrameView.get(), currentNodeBounds);

  // Compute the new anchor point relative to the node position
  FloatSize anchorOffsetFromNode(currentNodeBoundsInLayoutViewport.size());
  anchorOffsetFromNode.scale(m_anchorInNodeCoords.width(),
                             m_anchorInNodeCoords.height());
  FloatPoint anchorPoint =
      FloatPoint(currentNodeBoundsInLayoutViewport.location()) +
      anchorOffsetFromNode;

  // Compute the new origin point relative to the new anchor point
  FloatSize anchorOffsetFromOrigin = innerSize;
  anchorOffsetFromOrigin.scale(m_anchorInInnerViewCoords.width(),
                               m_anchorInInnerViewCoords.height());
  return anchorPoint - anchorOffsetFromOrigin;
}

}  // namespace blink
