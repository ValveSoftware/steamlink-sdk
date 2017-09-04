/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "core/layout/compositing/PaintLayerCompositor.h"

#include "core/animation/DocumentAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Fullscreen.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/CompositingInputsUpdater.h"
#include "core/layout/compositing/CompositingLayerAssigner.h"
#include "core/layout/compositing/CompositingRequirementsUpdater.h"
#include "core/layout/compositing/GraphicsLayerTreeBuilder.h"
#include "core/layout/compositing/GraphicsLayerUpdater.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/FramePainter.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/TransformRecorder.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/json/JSONValues.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

PaintLayerCompositor::PaintLayerCompositor(LayoutView& layoutView)
    : m_layoutView(layoutView),
      m_compositingReasonFinder(layoutView),
      m_pendingUpdateType(CompositingUpdateNone),
      m_hasAcceleratedCompositing(true),
      m_compositing(false),
      m_rootShouldAlwaysCompositeDirty(true),
      m_needsUpdateFixedBackground(false),
      m_isTrackingRasterInvalidations(
          layoutView.frameView()->isTrackingPaintInvalidations()),
      m_inOverlayFullscreenVideo(false),
      m_needsUpdateDescendantDependentFlags(false),
      m_rootLayerAttachment(RootLayerUnattached) {
  updateAcceleratedCompositingSettings();
}

PaintLayerCompositor::~PaintLayerCompositor() {
  ASSERT(m_rootLayerAttachment == RootLayerUnattached);
}

bool PaintLayerCompositor::inCompositingMode() const {
  // FIXME: This should assert that lifecycle is >= CompositingClean since
  // the last step of updateIfNeeded can set this bit to false.
  ASSERT(m_layoutView.layer()->isAllowedToQueryCompositingState());
  return m_compositing;
}

bool PaintLayerCompositor::staleInCompositingMode() const {
  return m_compositing;
}

void PaintLayerCompositor::setCompositingModeEnabled(bool enable) {
  if (enable == m_compositing)
    return;

  m_compositing = enable;

  if (m_compositing)
    ensureRootLayer();
  else
    destroyRootLayer();

  // Schedule an update in the parent frame so the <iframe>'s layer in the owner
  // document matches the compositing state here.
  if (HTMLFrameOwnerElement* ownerElement =
          m_layoutView.document().localOwner())
    ownerElement->setNeedsCompositingUpdate();
}

void PaintLayerCompositor::enableCompositingModeIfNeeded() {
  if (!m_rootShouldAlwaysCompositeDirty)
    return;

  m_rootShouldAlwaysCompositeDirty = false;
  if (m_compositing)
    return;

  if (rootShouldAlwaysComposite()) {
    // FIXME: Is this needed? It was added in
    // https://bugs.webkit.org/show_bug.cgi?id=26651.
    // No tests fail if it's deleted.
    setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
    setCompositingModeEnabled(true);
  }
}

bool PaintLayerCompositor::rootShouldAlwaysComposite() const {
  if (!m_hasAcceleratedCompositing)
    return false;
  return m_layoutView.frame()->isLocalRoot() ||
         m_compositingReasonFinder.requiresCompositingForScrollableFrame();
}

void PaintLayerCompositor::updateAcceleratedCompositingSettings() {
  m_compositingReasonFinder.updateTriggers();
  m_hasAcceleratedCompositing =
      m_layoutView.document().settings()->acceleratedCompositingEnabled();
  m_rootShouldAlwaysCompositeDirty = true;
  if (m_rootLayerAttachment != RootLayerUnattached)
    rootLayer()->setNeedsCompositingInputsUpdate();
}

bool PaintLayerCompositor::preferCompositingToLCDTextEnabled() const {
  return m_layoutView.document()
      .settings()
      ->preferCompositingToLCDTextEnabled();
}

static LayoutVideo* findFullscreenVideoLayoutObject(Document& document) {
  // Recursively find the document that is in fullscreen.
  Element* fullscreenElement = Fullscreen::fullscreenElementFrom(document);
  Document* contentDocument = &document;
  while (fullscreenElement && fullscreenElement->isFrameOwnerElement()) {
    contentDocument =
        toHTMLFrameOwnerElement(fullscreenElement)->contentDocument();
    if (!contentDocument)
      return nullptr;
    fullscreenElement = Fullscreen::fullscreenElementFrom(*contentDocument);
  }
  // Get the current fullscreen element from the document.
  // TODO(foolip): When |currentFullScreenElementFrom| is removed, this will
  // become a no-op and can be removed. https://crbug.com/402421
  fullscreenElement =
      Fullscreen::currentFullScreenElementFrom(*contentDocument);
  if (!isHTMLVideoElement(fullscreenElement))
    return nullptr;
  LayoutObject* layoutObject = fullscreenElement->layoutObject();
  if (!layoutObject)
    return nullptr;
  return toLayoutVideo(layoutObject);
}

// The descendant-dependent flags system is badly broken because we clean dirty
// bits in upward tree walks, which means we need to call
// updateDescendantDependentFlags at every node in the tree to fully clean all
// the dirty bits. While we'll in the process of fixing this issue,
// updateDescendantDependentFlagsForEntireSubtree provides a big hammer for
// actually cleaning all the dirty bits in a subtree.
//
// FIXME: Remove this function once the descendant-dependent flags system keeps
// its dirty bits scoped to subtrees.
void updateDescendantDependentFlagsForEntireSubtree(PaintLayer& layer) {
  layer.updateDescendantDependentFlags();

  for (PaintLayer* child = layer.firstChild(); child;
       child = child->nextSibling())
    updateDescendantDependentFlagsForEntireSubtree(*child);
}

void PaintLayerCompositor::updateIfNeededRecursive() {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Compositing.UpdateTime");
  updateIfNeededRecursiveInternal();
}

void PaintLayerCompositor::updateIfNeededRecursiveInternal() {
  FrameView* view = m_layoutView.frameView();
  if (view->shouldThrottleRendering())
    return;

  for (Frame* child = m_layoutView.frameView()->frame().tree().firstChild();
       child; child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    LocalFrame* localFrame = toLocalFrame(child);
    // It's possible for trusted Pepper plugins to force hit testing in
    // situations where the frame tree is in an inconsistent state, such as in
    // the middle of frame detach.
    // TODO(bbudge) Remove this check when trusted Pepper plugins are gone.
    if (localFrame->document()->isActive() &&
        !localFrame->contentLayoutItem().isNull())
      localFrame->contentLayoutItem()
          .compositor()
          ->updateIfNeededRecursiveInternal();
  }

  TRACE_EVENT0("blink", "PaintLayerCompositor::updateIfNeededRecursive");

  ASSERT(!m_layoutView.needsLayout());

  ScriptForbiddenScope forbidScript;

  // FIXME: enableCompositingModeIfNeeded can trigger a
  // CompositingUpdateRebuildTree, which asserts that it's not
  // InCompositingUpdate.
  enableCompositingModeIfNeeded();

  if (m_needsUpdateDescendantDependentFlags) {
    updateDescendantDependentFlagsForEntireSubtree(*rootLayer());
    m_needsUpdateDescendantDependentFlags = false;
  }

  m_layoutView.commitPendingSelection();

  lifecycle().advanceTo(DocumentLifecycle::InCompositingUpdate);
  updateIfNeeded();
  lifecycle().advanceTo(DocumentLifecycle::CompositingClean);

  DocumentAnimations::updateAnimations(m_layoutView.document());

  m_layoutView.frameView()
      ->getScrollableArea()
      ->updateCompositorScrollAnimations();
  if (const FrameView::ScrollableAreaSet* animatingScrollableAreas =
          m_layoutView.frameView()->animatingScrollableAreas()) {
    for (ScrollableArea* scrollableArea : *animatingScrollableAreas)
      scrollableArea->updateCompositorScrollAnimations();
  }

#if ENABLE(ASSERT)
  ASSERT(lifecycle().state() == DocumentLifecycle::CompositingClean);
  assertNoUnresolvedDirtyBits();
  for (Frame* child = m_layoutView.frameView()->frame().tree().firstChild();
       child; child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    LocalFrame* localFrame = toLocalFrame(child);
    if (localFrame->shouldThrottleRendering() ||
        localFrame->contentLayoutItem().isNull())
      continue;
    localFrame->contentLayoutItem().compositor()->assertNoUnresolvedDirtyBits();
  }
#endif
}

void PaintLayerCompositor::setNeedsCompositingUpdate(
    CompositingUpdateType updateType) {
  ASSERT(updateType != CompositingUpdateNone);
  m_pendingUpdateType = std::max(m_pendingUpdateType, updateType);
  if (Page* page = this->page())
    page->animator().scheduleVisualUpdate(m_layoutView.frame());
  lifecycle().ensureStateAtMost(DocumentLifecycle::LayoutClean);
}

void PaintLayerCompositor::didLayout() {
  // FIXME: Technically we only need to do this when the FrameView's
  // isScrollable method would return a different value.
  m_rootShouldAlwaysCompositeDirty = true;
  enableCompositingModeIfNeeded();

  // FIXME: Rather than marking the entire LayoutView as dirty, we should
  // track which Layers moved during layout and only dirty those
  // specific Layers.
  rootLayer()->setNeedsCompositingInputsUpdate();
}

#if ENABLE(ASSERT)

void PaintLayerCompositor::assertNoUnresolvedDirtyBits() {
  ASSERT(m_pendingUpdateType == CompositingUpdateNone);
  ASSERT(!m_rootShouldAlwaysCompositeDirty);
}

#endif

void PaintLayerCompositor::applyOverlayFullscreenVideoAdjustmentIfNeeded() {
  m_inOverlayFullscreenVideo = false;
  if (!m_rootContentLayer)
    return;

  bool isLocalRoot = m_layoutView.frame()->isLocalRoot();
  LayoutVideo* video = findFullscreenVideoLayoutObject(m_layoutView.document());
  if (!video || !video->layer()->hasCompositedLayerMapping() ||
      !video->videoElement()->usesOverlayFullscreenVideo()) {
    if (isLocalRoot) {
      GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer();
      if (backgroundLayer && !backgroundLayer->parent())
        rootFixedBackgroundsChanged();
    }
    return;
  }

  GraphicsLayer* videoLayer =
      video->layer()->compositedLayerMapping()->mainGraphicsLayer();

  // The fullscreen video has layer position equal to its enclosing frame's
  // scroll position because fullscreen container is fixed-positioned.
  // We should reset layer position here since we are going to reattach the
  // layer at the very top level.
  videoLayer->setPosition(IntPoint());

  // Only steal fullscreen video layer and clear all other layers if we are the
  // main frame.
  if (!isLocalRoot)
    return;

  m_rootContentLayer->removeAllChildren();
  m_overflowControlsHostLayer->addChild(videoLayer);
  if (GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer())
    backgroundLayer->removeFromParent();
  m_inOverlayFullscreenVideo = true;
}

void PaintLayerCompositor::updateWithoutAcceleratedCompositing(
    CompositingUpdateType updateType) {
  ASSERT(!hasAcceleratedCompositing());

  if (updateType >= CompositingUpdateAfterCompositingInputChange)
    CompositingInputsUpdater(rootLayer()).update();

#if ENABLE(ASSERT)
  CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(
      rootLayer());
#endif
}

static void forceRecomputeVisualRectsIncludingNonCompositingDescendants(
    LayoutObject* layoutObject) {
  // We clear the previous visual rect as it's wrong (paint invalidation
  // container changed, ...). Forcing a full invalidation will make us recompute
  // it. Also we are not changing the previous position from our paint
  // invalidation container, which is fine as we want a full paint invalidation
  // anyway.
  layoutObject->clearPreviousVisualRects();

  for (LayoutObject* child = layoutObject->slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->isPaintInvalidationContainer())
      forceRecomputeVisualRectsIncludingNonCompositingDescendants(child);
  }
}

void PaintLayerCompositor::updateIfNeeded() {
  CompositingUpdateType updateType = m_pendingUpdateType;
  m_pendingUpdateType = CompositingUpdateNone;

  if (!hasAcceleratedCompositing()) {
    updateWithoutAcceleratedCompositing(updateType);
    return;
  }

  if (updateType == CompositingUpdateNone)
    return;

  PaintLayer* updateRoot = rootLayer();

  Vector<PaintLayer*> layersNeedingPaintInvalidation;

  if (updateType >= CompositingUpdateAfterCompositingInputChange) {
    CompositingInputsUpdater(updateRoot).update();

#if ENABLE(ASSERT)
    // FIXME: Move this check to the end of the compositing update.
    CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(
        updateRoot);
#endif

    CompositingRequirementsUpdater(m_layoutView, m_compositingReasonFinder)
        .update(updateRoot);

    CompositingLayerAssigner layerAssigner(this);
    layerAssigner.assign(updateRoot, layersNeedingPaintInvalidation);

    bool layersChanged = layerAssigner.layersChanged();

    {
      TRACE_EVENT0("blink",
                   "PaintLayerCompositor::updateAfterCompositingChange");
      if (const FrameView::ScrollableAreaSet* scrollableAreas =
              m_layoutView.frameView()->scrollableAreas()) {
        for (ScrollableArea* scrollableArea : *scrollableAreas)
          layersChanged |= scrollableArea->updateAfterCompositingChange();
      }
    }

    if (layersChanged) {
      updateType = std::max(updateType, CompositingUpdateRebuildTree);
      if (ScrollingCoordinator* scrollingCoordinator =
              this->scrollingCoordinator())
        scrollingCoordinator->notifyGeometryChanged();
    }
  }

  if (updateType != CompositingUpdateNone) {
    if (RuntimeEnabledFeatures::compositorWorkerEnabled() && m_scrollLayer) {
      // If rootLayerScrolls is enabled, these properties are applied in
      // CompositedLayerMapping::updateElementIdAndCompositorMutableProperties.
      if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        if (Element* scrollingElement =
                m_layoutView.document().scrollingElement()) {
          uint32_t mutableProperties = CompositorMutableProperty::kNone;
          if (scrollingElement->hasCompositorProxy())
            mutableProperties = (CompositorMutableProperty::kScrollLeft |
                                 CompositorMutableProperty::kScrollTop) &
                                scrollingElement->compositorMutableProperties();
          m_scrollLayer->setCompositorMutableProperties(mutableProperties);
        }
      }
    }

    updateClippingOnCompositorLayers();

    GraphicsLayerUpdater updater;
    updater.update(*updateRoot, layersNeedingPaintInvalidation);

    if (updater.needsRebuildTree())
      updateType = std::max(updateType, CompositingUpdateRebuildTree);

#if ENABLE(ASSERT)
    // FIXME: Move this check to the end of the compositing update.
    GraphicsLayerUpdater::assertNeedsToUpdateGraphicsLayerBitsCleared(
        *updateRoot);
#endif
  }

  if (updateType >= CompositingUpdateRebuildTree) {
    GraphicsLayerTreeBuilder::AncestorInfo ancestorInfo;
    GraphicsLayerVector childList;
    ancestorInfo.childLayersOfEnclosingCompositedLayer = &childList;
    {
      TRACE_EVENT0("blink", "GraphicsLayerTreeBuilder::rebuild");
      GraphicsLayerTreeBuilder().rebuild(*updateRoot, ancestorInfo);
    }

    if (!childList.isEmpty()) {
      CHECK(m_rootContentLayer && m_compositing);
      m_rootContentLayer->setChildren(childList);
    }

    applyOverlayFullscreenVideoAdjustmentIfNeeded();
  }

  if (m_needsUpdateFixedBackground) {
    rootFixedBackgroundsChanged();
    m_needsUpdateFixedBackground = false;
  }

  for (unsigned i = 0; i < layersNeedingPaintInvalidation.size(); i++)
    forceRecomputeVisualRectsIncludingNonCompositingDescendants(
        layersNeedingPaintInvalidation[i]->layoutObject());

  // Inform the inspector that the layer tree has changed.
  if (m_layoutView.frame()->isMainFrame())
    InspectorInstrumentation::layerTreeDidChange(m_layoutView.frame());
}

void PaintLayerCompositor::updateClippingOnCompositorLayers() {
  bool shouldClip = !rootLayer()->hasRootScrollerAsDescendant();
  if (m_rootContentLayer) {
    // FIXME: with rootLayerScrolls, we probably don't even need
    // m_rootContentLayer?
    m_rootContentLayer->setMasksToBounds(
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled() && shouldClip);
  }

  const TopDocumentRootScrollerController& globalRootScrollerController =
      m_layoutView.document().frameHost()->globalRootScrollerController();

  Element* documentElement = m_layoutView.document().documentElement();
  bool frameIsRootScroller =
      documentElement &&
      documentElement->isSameNode(
          globalRootScrollerController.globalRootScroller());

  // We normally clip iframes' (but not the root frame) overflow controls
  // host and container layers but if the root scroller is the iframe itself
  // we want it to behave like the root frame.
  shouldClip &= !frameIsRootScroller && !m_layoutView.frame()->isLocalRoot();

  if (m_containerLayer)
    m_containerLayer->setMasksToBounds(shouldClip);
  if (m_overflowControlsHostLayer)
    m_overflowControlsHostLayer->setMasksToBounds(shouldClip);
}

static void restartAnimationOnCompositor(const LayoutObject& layoutObject) {
  Node* node = layoutObject.node();
  ElementAnimations* elementAnimations =
      (node && node->isElementNode()) ? toElement(node)->elementAnimations()
                                      : nullptr;
  if (elementAnimations)
    elementAnimations->restartAnimationOnCompositor();
}

bool PaintLayerCompositor::allocateOrClearCompositedLayerMapping(
    PaintLayer* layer,
    const CompositingStateTransitionType compositedLayerUpdate) {
  bool compositedLayerMappingChanged = false;

  // FIXME: It would be nice to directly use the layer's compositing reason,
  // but allocateOrClearCompositedLayerMapping also gets called without having
  // updated compositing requirements fully.
  switch (compositedLayerUpdate) {
    case AllocateOwnCompositedLayerMapping:
      ASSERT(!layer->hasCompositedLayerMapping());
      setCompositingModeEnabled(true);

      // If we need to issue paint invalidations, do so before allocating the
      // compositedLayerMapping and clearing out the groupedMapping.
      paintInvalidationOnCompositingChange(layer);

      // If this layer was previously squashed, we need to remove its reference
      // to a groupedMapping right away, so that computing paint invalidation
      // rects will know the layer's correct compositingState.
      // FIXME: do we need to also remove the layer from it's location in the
      // squashing list of its groupedMapping?  Need to create a test where a
      // squashed layer pops into compositing. And also to cover all other sorts
      // of compositingState transitions.
      layer->setLostGroupedMapping(false);
      layer->setGroupedMapping(nullptr,
                               PaintLayer::InvalidateLayerAndRemoveFromMapping);

      layer->ensureCompositedLayerMapping();
      compositedLayerMappingChanged = true;

      restartAnimationOnCompositor(*layer->layoutObject());

      // At this time, the ScrollingCoordinator only supports the top-level
      // frame.
      if (layer->isRootLayer() && m_layoutView.frame()->isLocalRoot()) {
        if (ScrollingCoordinator* scrollingCoordinator =
                this->scrollingCoordinator())
          scrollingCoordinator->frameViewRootLayerDidChange(
              m_layoutView.frameView());
      }
      break;
    case RemoveOwnCompositedLayerMapping:
    // PutInSquashingLayer means you might have to remove the composited layer
    // mapping first.
    case PutInSquashingLayer:
      if (layer->hasCompositedLayerMapping()) {
        layer->clearCompositedLayerMapping();
        compositedLayerMappingChanged = true;
      }

      break;
    case RemoveFromSquashingLayer:
    case NoCompositingStateChange:
      // Do nothing.
      break;
  }

  if (compositedLayerMappingChanged && layer->layoutObject()->isLayoutPart()) {
    PaintLayerCompositor* innerCompositor =
        frameContentsCompositor(toLayoutPart(layer->layoutObject()));
    if (innerCompositor && innerCompositor->staleInCompositingMode())
      innerCompositor->updateRootLayerAttachment();
  }

  if (compositedLayerMappingChanged)
    layer->clipper().clearClipRectsIncludingDescendants(PaintingClipRects);

  // If a fixed position layer gained/lost a compositedLayerMapping or the
  // reason not compositing it changed, the scrolling coordinator needs to
  // recalculate whether it can do fast scrolling.
  if (compositedLayerMappingChanged) {
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->frameViewFixedObjectsDidChange(
          m_layoutView.frameView());
  }

  return compositedLayerMappingChanged;
}

void PaintLayerCompositor::paintInvalidationOnCompositingChange(
    PaintLayer* layer) {
  // If the layoutObject is not attached yet, no need to issue paint
  // invalidations.
  if (layer->layoutObject() != &m_layoutView &&
      !layer->layoutObject()->parent())
    return;

  // For querying Layer::compositingState()
  // Eager invalidation here is correct, since we are invalidating with respect
  // to the previous frame's compositing state when changing the compositing
  // backing of the layer.
  DisableCompositingQueryAsserts disabler;
  // FIXME: We should not allow paint invalidation out of paint invalidation
  // state. crbug.com/457415
  DisablePaintInvalidationStateAsserts paintInvalidationAssertisabler;

  ObjectPaintInvalidator(*layer->layoutObject())
      .invalidatePaintIncludingNonCompositingDescendants();
}

void PaintLayerCompositor::frameViewDidChangeLocation(
    const IntPoint& contentsOffset) {
  if (m_overflowControlsHostLayer)
    m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void PaintLayerCompositor::frameViewDidChangeSize() {
  if (m_containerLayer) {
    FrameView* frameView = m_layoutView.frameView();
    m_containerLayer->setSize(FloatSize(frameView->visibleContentSize()));
    m_overflowControlsHostLayer->setSize(
        FloatSize(frameView->visibleContentSize(IncludeScrollbars)));

    frameViewDidScroll();
    updateOverflowControlsLayers();
  }
}

enum AcceleratedFixedRootBackgroundHistogramBuckets {
  ScrolledMainFrameBucket = 0,
  ScrolledMainFrameWithAcceleratedFixedRootBackground = 1,
  ScrolledMainFrameWithUnacceleratedFixedRootBackground = 2,
  AcceleratedFixedRootBackgroundHistogramMax = 3
};

void PaintLayerCompositor::frameViewDidScroll() {
  FrameView* frameView = m_layoutView.frameView();
  IntSize scrollOffset = frameView->scrollOffsetInt();

  if (!m_scrollLayer)
    return;

  bool scrollingCoordinatorHandlesOffset = false;
  if (ScrollingCoordinator* scrollingCoordinator =
          this->scrollingCoordinator()) {
    scrollingCoordinatorHandlesOffset =
        scrollingCoordinator->scrollableAreaScrollLayerDidChange(frameView);
  }

  // Scroll position = scroll origin + scroll offset. Adjust the layer's
  // position to handle whatever the scroll coordinator isn't handling.
  // The scroll origin is non-zero for RTL pages with overflow.
  if (scrollingCoordinatorHandlesOffset)
    m_scrollLayer->setPosition(frameView->scrollOrigin());
  else
    m_scrollLayer->setPosition(IntPoint(-scrollOffset));

  DEFINE_STATIC_LOCAL(EnumerationHistogram, acceleratedBackgroundHistogram,
                      ("Renderer.AcceleratedFixedRootBackground",
                       AcceleratedFixedRootBackgroundHistogramMax));
  acceleratedBackgroundHistogram.count(ScrolledMainFrameBucket);
}

void PaintLayerCompositor::frameViewScrollbarsExistenceDidChange() {
  if (m_containerLayer)
    updateOverflowControlsLayers();
}

void PaintLayerCompositor::rootFixedBackgroundsChanged() {
  if (!supportsFixedRootBackgroundCompositing())
    return;

  // To avoid having to make the fixed root background layer fixed positioned to
  // stay put, we position it in the layer tree as follows:
  //
  // + Overflow controls host
  //   + LocalFrame clip
  //     + (Fixed root background) <-- Here.
  //     + LocalFrame scroll
  //       + Root content layer
  //   + Scrollbars
  //
  // That is, it needs to be the first child of the frame clip, the sibling of
  // the frame scroll layer. The compositor does not own the background layer,
  // it just positions it (like the foreground layer).
  if (GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer())
    m_containerLayer->addChildBelow(backgroundLayer, m_scrollLayer.get());
}

bool PaintLayerCompositor::scrollingLayerDidChange(PaintLayer* layer) {
  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    return scrollingCoordinator->scrollableAreaScrollLayerDidChange(
        layer->getScrollableArea());
  return false;
}

std::unique_ptr<JSONObject> PaintLayerCompositor::layerTreeAsJSON(
    LayerTreeFlags flags) const {
  ASSERT(lifecycle().state() >= DocumentLifecycle::PaintInvalidationClean ||
         m_layoutView.frameView()->shouldThrottleRendering());

  if (!m_rootContentLayer)
    return nullptr;

  // We skip dumping the scroll and clip layers to keep layerTreeAsText output
  // similar between platforms (unless we explicitly request dumping from the
  // root.
  GraphicsLayer* rootLayer = m_rootContentLayer.get();
  if (flags & LayerTreeIncludesRootLayer)
    rootLayer = rootGraphicsLayer();

  return rootLayer->layerTreeAsJSON(flags);
}

PaintLayerCompositor* PaintLayerCompositor::frameContentsCompositor(
    LayoutPart* layoutObject) {
  if (!layoutObject->node()->isFrameOwnerElement())
    return nullptr;

  HTMLFrameOwnerElement* element =
      toHTMLFrameOwnerElement(layoutObject->node());
  if (Document* contentDocument = element->contentDocument()) {
    if (LayoutViewItem view = contentDocument->layoutViewItem())
      return view.compositor();
  }
  return nullptr;
}

bool PaintLayerCompositor::attachFrameContentLayersToIframeLayer(
    LayoutPart* layoutObject) {
  PaintLayerCompositor* innerCompositor = frameContentsCompositor(layoutObject);
  if (!innerCompositor || !innerCompositor->staleInCompositingMode() ||
      innerCompositor->getRootLayerAttachment() !=
          RootLayerAttachedViaEnclosingFrame)
    return false;

  PaintLayer* layer = layoutObject->layer();
  if (!layer->hasCompositedLayerMapping())
    return false;

  layer->compositedLayerMapping()->setSublayers(
      GraphicsLayerVector(1, innerCompositor->rootGraphicsLayer()));
  return true;
}

static void fullyInvalidatePaintRecursive(PaintLayer* layer) {
  if (layer->compositingState() == PaintsIntoOwnBacking) {
    layer->compositedLayerMapping()->setContentsNeedDisplay();
    layer->compositedLayerMapping()->setSquashingContentsNeedDisplay();
  }

  for (PaintLayer* child = layer->firstChild(); child;
       child = child->nextSibling())
    fullyInvalidatePaintRecursive(child);
}

void PaintLayerCompositor::fullyInvalidatePaint() {
  // We're walking all compositing layers and invalidating them, so there's
  // no need to have up-to-date compositing state.
  DisableCompositingQueryAsserts disabler;
  fullyInvalidatePaintRecursive(rootLayer());
}

PaintLayer* PaintLayerCompositor::rootLayer() const {
  return m_layoutView.layer();
}

GraphicsLayer* PaintLayerCompositor::rootGraphicsLayer() const {
  return m_overflowControlsHostLayer.get();
}

GraphicsLayer* PaintLayerCompositor::frameScrollLayer() const {
  return m_scrollLayer.get();
}

GraphicsLayer* PaintLayerCompositor::scrollLayer() const {
  if (ScrollableArea* scrollableArea =
          m_layoutView.frameView()->getScrollableArea())
    return scrollableArea->layerForScrolling();
  return nullptr;
}

GraphicsLayer* PaintLayerCompositor::containerLayer() const {
  return m_containerLayer.get();
}

GraphicsLayer* PaintLayerCompositor::rootContentLayer() const {
  return m_rootContentLayer.get();
}

void PaintLayerCompositor::setIsInWindow(bool isInWindow) {
  if (!staleInCompositingMode())
    return;

  if (isInWindow) {
    if (m_rootLayerAttachment != RootLayerUnattached)
      return;

    RootLayerAttachment attachment = m_layoutView.frame()->isLocalRoot()
                                         ? RootLayerAttachedViaChromeClient
                                         : RootLayerAttachedViaEnclosingFrame;
    attachCompositorTimeline();
    attachRootLayer(attachment);
  } else {
    if (m_rootLayerAttachment == RootLayerUnattached)
      return;

    detachRootLayer();
    detachCompositorTimeline();
  }
}

void PaintLayerCompositor::updateRootLayerPosition() {
  if (m_rootContentLayer) {
    IntRect documentRect = m_layoutView.documentRect();

    // Ensure the root content layer is at least the size of the outer viewport
    // so that we don't end up clipping position: fixed elements if the
    // document is smaller.
    documentRect.unite(IntRect(documentRect.location(),
                               m_layoutView.frameView()->visibleContentSize()));

    m_rootContentLayer->setSize(FloatSize(documentRect.size()));
    m_rootContentLayer->setPosition(documentRect.location());
  }
  if (m_containerLayer) {
    FrameView* frameView = m_layoutView.frameView();
    m_containerLayer->setSize(FloatSize(frameView->visibleContentSize()));
    m_overflowControlsHostLayer->setSize(
        FloatSize(frameView->visibleContentSize(IncludeScrollbars)));
  }
}

void PaintLayerCompositor::updatePotentialCompositingReasonsFromStyle(
    PaintLayer* layer) {
  layer->setPotentialCompositingReasonsFromStyle(
      m_compositingReasonFinder.potentialCompositingReasonsFromStyle(
          layer->layoutObject()));
}

void PaintLayerCompositor::updateDirectCompositingReasons(PaintLayer* layer) {
  layer->setCompositingReasons(m_compositingReasonFinder.directReasons(layer),
                               CompositingReasonComboAllDirectReasons);
}

bool PaintLayerCompositor::canBeComposited(const PaintLayer* layer) const {
  FrameView* frameView = layer->layoutObject()->frameView();
  // Elements within an invisible frame must not be composited because they are
  // not drawn.
  if (frameView && !frameView->isVisible())
    return false;

  const bool hasCompositorAnimation =
      m_compositingReasonFinder.requiresCompositingForAnimation(
          *layer->layoutObject()->style());
  return m_hasAcceleratedCompositing &&
         (hasCompositorAnimation || !layer->subtreeIsInvisible()) &&
         layer->isSelfPaintingLayer() &&
         !layer->layoutObject()->isLayoutFlowThread();
}

// Return true if the given layer is a stacking context and has compositing
// child layers that it needs to clip. In this case we insert a clipping
// GraphicsLayer into the hierarchy between this layer and its children in the
// z-order hierarchy.
bool PaintLayerCompositor::clipsCompositingDescendants(
    const PaintLayer* layer) const {
  return layer->hasCompositingDescendant() &&
         layer->layoutObject()->hasClipRelatedProperty();
}

// If an element has composited negative z-index children, those children paint
// in front of the layer background, so we need an extra 'contents' layer for
// the foreground of the layer object.
bool PaintLayerCompositor::needsContentsCompositingLayer(
    const PaintLayer* layer) const {
  if (!layer->hasCompositingDescendant())
    return false;
  return layer->stackingNode()->hasNegativeZOrderList();
}

static void paintScrollbar(const GraphicsLayer* graphicsLayer,
                           const Scrollbar* scrollbar,
                           GraphicsContext& context,
                           const IntRect& clip) {
  // Frame scrollbars are painted in the space of the containing frame, not the
  // local space of the scrollbar.
  const IntPoint& paintOffset = scrollbar->frameRect().location();
  IntRect transformedClip = clip;
  transformedClip.moveBy(paintOffset);

  AffineTransform translation;
  translation.translate(-paintOffset.x(), -paintOffset.y());
  TransformRecorder transformRecorder(context, *scrollbar, translation);
  scrollbar->paint(context, CullRect(transformedClip));
}

IntRect PaintLayerCompositor::computeInterestRect(
    const GraphicsLayer* graphicsLayer,
    const IntRect&) const {
  return enclosingIntRect(FloatRect(FloatPoint(), graphicsLayer->size()));
}

void PaintLayerCompositor::paintContents(const GraphicsLayer* graphicsLayer,
                                         GraphicsContext& context,
                                         GraphicsLayerPaintingPhase,
                                         const IntRect& interestRect) const {
  // Note the composited scrollable area painted below is always associated with
  // a frame. For painting non-frame ScrollableAreas, see
  // CompositedLayerMapping::paintScrollableArea.

  const Scrollbar* scrollbar = graphicsLayerToScrollbar(graphicsLayer);
  if (!scrollbar && graphicsLayer != layerForScrollCorner())
    return;

  if (DrawingRecorder::useCachedDrawingIfPossible(
          context, *graphicsLayer, DisplayItem::kScrollbarCompositedScrollbar))
    return;

  FloatRect layerBounds(FloatPoint(), graphicsLayer->size());
  SkPictureBuilder pictureBuilder(layerBounds, nullptr, &context);

  if (scrollbar)
    paintScrollbar(graphicsLayer, scrollbar, pictureBuilder.context(),
                   interestRect);
  else
    FramePainter(*m_layoutView.frameView())
        .paintScrollCorner(pictureBuilder.context(), interestRect);

  // Replay the painted scrollbar content with the GraphicsLayer backing as the
  // DisplayItemClient in order for the resulting DrawingDisplayItem to produce
  // the correct visualRect (i.e., the bounds of the involved GraphicsLayer).
  DrawingRecorder drawingRecorder(context, *graphicsLayer,
                                  DisplayItem::kScrollbarCompositedScrollbar,
                                  layerBounds);
  pictureBuilder.endRecording()->playback(context.canvas());
}

Scrollbar* PaintLayerCompositor::graphicsLayerToScrollbar(
    const GraphicsLayer* graphicsLayer) const {
  if (graphicsLayer == layerForHorizontalScrollbar()) {
    return m_layoutView.frameView()->horizontalScrollbar();
  }
  if (graphicsLayer == layerForVerticalScrollbar()) {
    return m_layoutView.frameView()->verticalScrollbar();
  }
  return nullptr;
}

bool PaintLayerCompositor::supportsFixedRootBackgroundCompositing() const {
  if (Settings* settings = m_layoutView.document().settings())
    return settings->preferCompositingToLCDTextEnabled();
  return false;
}

bool PaintLayerCompositor::needsFixedRootBackgroundLayer(
    const PaintLayer* layer) const {
  if (layer != m_layoutView.layer())
    return false;

  return supportsFixedRootBackgroundCompositing() &&
         m_layoutView.rootBackgroundIsEntirelyFixed();
}

GraphicsLayer* PaintLayerCompositor::fixedRootBackgroundLayer() const {
  // Get the fixed root background from the LayoutView layer's
  // compositedLayerMapping.
  PaintLayer* viewLayer = m_layoutView.layer();
  if (!viewLayer)
    return nullptr;

  if (viewLayer->compositingState() == PaintsIntoOwnBacking &&
      viewLayer->compositedLayerMapping()
          ->backgroundLayerPaintsFixedRootBackground())
    return viewLayer->compositedLayerMapping()->backgroundLayer();

  return nullptr;
}

static void setTracksRasterInvalidationsRecursive(
    GraphicsLayer* graphicsLayer,
    bool tracksPaintInvalidations) {
  if (!graphicsLayer)
    return;

  graphicsLayer->setTracksRasterInvalidations(tracksPaintInvalidations);

  for (size_t i = 0; i < graphicsLayer->children().size(); ++i) {
    setTracksRasterInvalidationsRecursive(graphicsLayer->children()[i],
                                          tracksPaintInvalidations);
  }

  if (GraphicsLayer* maskLayer = graphicsLayer->maskLayer())
    setTracksRasterInvalidationsRecursive(maskLayer, tracksPaintInvalidations);

  if (GraphicsLayer* clippingMaskLayer =
          graphicsLayer->contentsClippingMaskLayer()) {
    setTracksRasterInvalidationsRecursive(clippingMaskLayer,
                                          tracksPaintInvalidations);
  }
}

void PaintLayerCompositor::setTracksRasterInvalidations(
    bool tracksRasterInvalidations) {
#if ENABLE(ASSERT)
  FrameView* view = m_layoutView.frameView();
  ASSERT(lifecycle().state() == DocumentLifecycle::PaintClean ||
         (view && view->shouldThrottleRendering()));
#endif

  m_isTrackingRasterInvalidations = tracksRasterInvalidations;
  if (GraphicsLayer* rootLayer = rootGraphicsLayer())
    setTracksRasterInvalidationsRecursive(rootLayer, tracksRasterInvalidations);
}

bool PaintLayerCompositor::isTrackingRasterInvalidations() const {
  return m_isTrackingRasterInvalidations;
}

bool PaintLayerCompositor::requiresHorizontalScrollbarLayer() const {
  return m_layoutView.frameView()->horizontalScrollbar();
}

bool PaintLayerCompositor::requiresVerticalScrollbarLayer() const {
  return m_layoutView.frameView()->verticalScrollbar();
}

bool PaintLayerCompositor::requiresScrollCornerLayer() const {
  return m_layoutView.frameView()->isScrollCornerVisible();
}

void PaintLayerCompositor::updateOverflowControlsLayers() {
  GraphicsLayer* controlsParent = m_overflowControlsHostLayer.get();
  // Main frame scrollbars should always be stuck to the sides of the screen (in
  // overscroll and in pinch-zoom), so make the parent for the scrollbars be the
  // viewport container layer.
  if (m_layoutView.frame()->isMainFrame()) {
    VisualViewport& visualViewport =
        m_layoutView.frameView()->page()->frameHost().visualViewport();
    controlsParent = visualViewport.containerLayer();
  }

  if (requiresHorizontalScrollbarLayer()) {
    if (!m_layerForHorizontalScrollbar) {
      m_layerForHorizontalScrollbar = GraphicsLayer::create(this);
    }

    if (m_layerForHorizontalScrollbar->parent() != controlsParent) {
      controlsParent->addChild(m_layerForHorizontalScrollbar.get());

      if (ScrollingCoordinator* scrollingCoordinator =
              this->scrollingCoordinator())
        scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
            m_layoutView.frameView(), HorizontalScrollbar);
    }
  } else if (m_layerForHorizontalScrollbar) {
    m_layerForHorizontalScrollbar->removeFromParent();
    m_layerForHorizontalScrollbar = nullptr;

    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
          m_layoutView.frameView(), HorizontalScrollbar);
  }

  if (requiresVerticalScrollbarLayer()) {
    if (!m_layerForVerticalScrollbar) {
      m_layerForVerticalScrollbar = GraphicsLayer::create(this);
    }

    if (m_layerForVerticalScrollbar->parent() != controlsParent) {
      controlsParent->addChild(m_layerForVerticalScrollbar.get());

      if (ScrollingCoordinator* scrollingCoordinator =
              this->scrollingCoordinator())
        scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
            m_layoutView.frameView(), VerticalScrollbar);
    }
  } else if (m_layerForVerticalScrollbar) {
    m_layerForVerticalScrollbar->removeFromParent();
    m_layerForVerticalScrollbar = nullptr;

    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
          m_layoutView.frameView(), VerticalScrollbar);
  }

  if (requiresScrollCornerLayer()) {
    if (!m_layerForScrollCorner)
      m_layerForScrollCorner = GraphicsLayer::create(this);

    if (m_layerForScrollCorner->parent() != controlsParent)
      controlsParent->addChild(m_layerForScrollCorner.get());
  } else if (m_layerForScrollCorner) {
    m_layerForScrollCorner->removeFromParent();
    m_layerForScrollCorner = nullptr;
  }

  m_layoutView.frameView()->positionScrollbarLayers();
}

void PaintLayerCompositor::ensureRootLayer() {
  RootLayerAttachment expectedAttachment =
      m_layoutView.frame()->isLocalRoot() ? RootLayerAttachedViaChromeClient
                                          : RootLayerAttachedViaEnclosingFrame;
  if (expectedAttachment == m_rootLayerAttachment)
    return;

  if (!m_rootContentLayer) {
    m_rootContentLayer = GraphicsLayer::create(this);
    IntRect overflowRect = m_layoutView.pixelSnappedLayoutOverflowRect();
    m_rootContentLayer->setSize(
        FloatSize(overflowRect.maxX(), overflowRect.maxY()));
    m_rootContentLayer->setPosition(FloatPoint());
    m_rootContentLayer->setOwnerNodeId(
        DOMNodeIds::idForNode(m_layoutView.node()));
  }

  if (!m_overflowControlsHostLayer) {
    ASSERT(!m_scrollLayer);
    ASSERT(!m_containerLayer);

    // Create a layer to host the clipping layer and the overflow controls
    // layers.  Whether these layers mask the content below is determined
    // in updateClippingOnCompositorLayers.
    m_overflowControlsHostLayer = GraphicsLayer::create(this);
    m_containerLayer = GraphicsLayer::create(this);

    m_scrollLayer = GraphicsLayer::create(this);
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(
          m_scrollLayer.get(), true);

    m_scrollLayer->setElementId(createCompositorElementId(
        DOMNodeIds::idForNode(&m_layoutView.document()),
        CompositorSubElementId::Scroll));

    // Hook them up
    m_overflowControlsHostLayer->addChild(m_containerLayer.get());
    m_containerLayer->addChild(m_scrollLayer.get());
    m_scrollLayer->addChild(m_rootContentLayer.get());

    frameViewDidChangeSize();
  }

  // Check to see if we have to change the attachment
  if (m_rootLayerAttachment != RootLayerUnattached) {
    detachRootLayer();
    detachCompositorTimeline();
  }

  attachCompositorTimeline();
  attachRootLayer(expectedAttachment);
}

void PaintLayerCompositor::destroyRootLayer() {
  if (!m_rootContentLayer)
    return;

  detachRootLayer();

  if (m_layerForHorizontalScrollbar) {
    m_layerForHorizontalScrollbar->removeFromParent();
    m_layerForHorizontalScrollbar = nullptr;
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
          m_layoutView.frameView(), HorizontalScrollbar);
    m_layoutView.frameView()->setScrollbarNeedsPaintInvalidation(
        HorizontalScrollbar);
  }

  if (m_layerForVerticalScrollbar) {
    m_layerForVerticalScrollbar->removeFromParent();
    m_layerForVerticalScrollbar = nullptr;
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(
          m_layoutView.frameView(), VerticalScrollbar);
    m_layoutView.frameView()->setScrollbarNeedsPaintInvalidation(
        VerticalScrollbar);
  }

  if (m_layerForScrollCorner) {
    m_layerForScrollCorner = nullptr;
    m_layoutView.frameView()->setScrollCornerNeedsPaintInvalidation();
  }

  if (m_overflowControlsHostLayer) {
    m_overflowControlsHostLayer = nullptr;
    m_containerLayer = nullptr;
    m_scrollLayer = nullptr;
  }
  ASSERT(!m_scrollLayer);
  m_rootContentLayer = nullptr;
}

void PaintLayerCompositor::attachRootLayer(RootLayerAttachment attachment) {
  if (!m_rootContentLayer)
    return;

  // In Slimming Paint v2, PaintArtifactCompositor is responsible for the root
  // layer.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  switch (attachment) {
    case RootLayerUnattached:
      ASSERT_NOT_REACHED();
      break;
    case RootLayerAttachedViaChromeClient: {
      LocalFrame& frame = m_layoutView.frameView()->frame();
      Page* page = frame.page();
      if (!page)
        return;
      page->chromeClient().attachRootGraphicsLayer(rootGraphicsLayer(), &frame);
      break;
    }
    case RootLayerAttachedViaEnclosingFrame: {
      HTMLFrameOwnerElement* ownerElement =
          m_layoutView.document().localOwner();
      ASSERT(ownerElement);
      // The layer will get hooked up via
      // CompositedLayerMapping::updateGraphicsLayerConfiguration() for the
      // frame's layoutObject in the parent document.
      ownerElement->setNeedsCompositingUpdate();
      break;
    }
  }

  m_rootLayerAttachment = attachment;
}

void PaintLayerCompositor::detachRootLayer() {
  if (!m_rootContentLayer || m_rootLayerAttachment == RootLayerUnattached)
    return;

  switch (m_rootLayerAttachment) {
    case RootLayerAttachedViaEnclosingFrame: {
      // The layer will get unhooked up via
      // CompositedLayerMapping::updateGraphicsLayerConfiguration() for the
      // frame's layoutObject in the parent document.
      if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->removeFromParent();
      else
        m_rootContentLayer->removeFromParent();

      if (HTMLFrameOwnerElement* ownerElement =
              m_layoutView.document().localOwner())
        ownerElement->setNeedsCompositingUpdate();
      break;
    }
    case RootLayerAttachedViaChromeClient: {
      LocalFrame& frame = m_layoutView.frameView()->frame();
      Page* page = frame.page();
      if (!page)
        return;
      page->chromeClient().attachRootGraphicsLayer(0, &frame);
      break;
    }
    case RootLayerUnattached:
      break;
  }

  m_rootLayerAttachment = RootLayerUnattached;
}

void PaintLayerCompositor::updateRootLayerAttachment() {
  ensureRootLayer();
}

void PaintLayerCompositor::attachCompositorTimeline() {
  LocalFrame& frame = m_layoutView.frameView()->frame();
  Page* page = frame.page();
  if (!page)
    return;

  CompositorAnimationTimeline* compositorTimeline =
      frame.document() ? frame.document()->timeline().compositorTimeline()
                       : nullptr;
  if (compositorTimeline)
    page->chromeClient().attachCompositorAnimationTimeline(compositorTimeline,
                                                           &frame);
}

void PaintLayerCompositor::detachCompositorTimeline() {
  LocalFrame& frame = m_layoutView.frameView()->frame();
  Page* page = frame.page();
  if (!page)
    return;

  CompositorAnimationTimeline* compositorTimeline =
      frame.document() ? frame.document()->timeline().compositorTimeline()
                       : nullptr;
  if (compositorTimeline)
    page->chromeClient().detachCompositorAnimationTimeline(compositorTimeline,
                                                           &frame);
}

ScrollingCoordinator* PaintLayerCompositor::scrollingCoordinator() const {
  if (Page* page = this->page())
    return page->scrollingCoordinator();

  return nullptr;
}

Page* PaintLayerCompositor::page() const {
  return m_layoutView.frameView()->frame().page();
}

DocumentLifecycle& PaintLayerCompositor::lifecycle() const {
  return m_layoutView.document().lifecycle();
}

String PaintLayerCompositor::debugName(
    const GraphicsLayer* graphicsLayer) const {
  String name;
  if (graphicsLayer == m_rootContentLayer.get()) {
    name = "Content Root Layer";
  } else if (graphicsLayer == m_overflowControlsHostLayer.get()) {
    name = "Frame Overflow Controls Host Layer";
  } else if (graphicsLayer == m_layerForHorizontalScrollbar.get()) {
    name = "Frame Horizontal Scrollbar Layer";
  } else if (graphicsLayer == m_layerForVerticalScrollbar.get()) {
    name = "Frame Vertical Scrollbar Layer";
  } else if (graphicsLayer == m_layerForScrollCorner.get()) {
    name = "Frame Scroll Corner Layer";
  } else if (graphicsLayer == m_containerLayer.get()) {
    name = "Frame Clipping Layer";
  } else if (graphicsLayer == m_scrollLayer.get()) {
    name = "Frame Scrolling Layer";
  } else {
    ASSERT_NOT_REACHED();
  }

  return name;
}

}  // namespace blink
