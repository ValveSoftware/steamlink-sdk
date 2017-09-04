// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerPainter.h"

#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ClipPathClipper.h"
#include "core/paint/FilterPainter.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/Transform3DRecorder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Optional.h"

namespace blink {

static inline bool shouldSuppressPaintingLayer(const PaintLayer& layer) {
  // Avoid painting descendants of the root layer when stylesheets haven't
  // loaded. This avoids some FOUC.  It's ok not to draw, because later on, when
  // all the stylesheets do load, Document::styleResolverMayHaveChanged() will
  // invalidate all painted output via a call to
  // LayoutView::invalidatePaintForViewAndCompositedLayers().  We also avoid
  // caching subsequences in this mode; see shouldCreateSubsequence().
  if (layer.layoutObject()->document().didLayoutWithPendingStylesheets() &&
      !layer.isRootLayer() && !layer.layoutObject()->isDocumentElement())
    return true;

  return false;
}

void PaintLayerPainter::paint(GraphicsContext& context,
                              const LayoutRect& damageRect,
                              const GlobalPaintFlags globalPaintFlags,
                              PaintLayerFlags paintFlags) {
  PaintLayerPaintingInfo paintingInfo(&m_paintLayer,
                                      LayoutRect(enclosingIntRect(damageRect)),
                                      globalPaintFlags, LayoutSize());
  if (shouldPaintLayerInSoftwareMode(globalPaintFlags, paintFlags))
    paintLayer(context, paintingInfo, paintFlags);
}

static ShouldRespectOverflowClipType shouldRespectOverflowClip(
    PaintLayerFlags paintFlags,
    const LayoutObject* layoutObject) {
  return (paintFlags & PaintLayerPaintingOverflowContents ||
          (paintFlags & PaintLayerPaintingChildClippingMaskPhase &&
           layoutObject->hasClipPath()))
             ? IgnoreOverflowClip
             : RespectOverflowClip;
}

PaintResult PaintLayerPainter::paintLayer(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags) {
  // https://code.google.com/p/chromium/issues/detail?id=343772
  DisableCompositingQueryAsserts disabler;

  if (m_paintLayer.compositingState() != NotComposited) {
    if (paintingInfo.getGlobalPaintFlags() &
        GlobalPaintFlattenCompositingLayers) {
      // FIXME: ok, but what about GlobalPaintFlattenCompositingLayers? That's
      // for printing and drag-image.
      // FIXME: why isn't the code here global, as opposed to being set on each
      // paintLayer() call?
      paintFlags |= PaintLayerUncachedClipRects;
    }
  }

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!m_paintLayer.isSelfPaintingLayer() &&
      !m_paintLayer.hasSelfPaintingLayerDescendant())
    return FullyPainted;

  if (shouldSuppressPaintingLayer(m_paintLayer))
    return FullyPainted;

  if (m_paintLayer.layoutObject()->view()->frame() &&
      m_paintLayer.layoutObject()->view()->frame()->shouldThrottleRendering())
    return FullyPainted;

  // If this layer is totally invisible then there is nothing to paint.
  if (!m_paintLayer.layoutObject()->opacity() &&
      !m_paintLayer.layoutObject()->hasBackdropFilter())
    return FullyPainted;

  if (m_paintLayer.paintsWithTransparency(paintingInfo.getGlobalPaintFlags()))
    paintFlags |= PaintLayerHaveTransparency;

  if (m_paintLayer.paintsWithTransform(paintingInfo.getGlobalPaintFlags()) &&
      !(paintFlags & PaintLayerAppliedTransform))
    return paintLayerWithTransform(context, paintingInfo, paintFlags);

  return paintLayerContentsCompositingAllPhases(context, paintingInfo,
                                                paintFlags);
}

PaintResult PaintLayerPainter::paintLayerContentsCompositingAllPhases(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags,
    FragmentPolicy fragmentPolicy) {
  DCHECK(m_paintLayer.isSelfPaintingLayer() ||
         m_paintLayer.hasSelfPaintingLayerDescendant());

  PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);
  localPaintFlags |= PaintLayerPaintingCompositingAllPhases;
  return paintLayerContents(context, paintingInfo, localPaintFlags,
                            fragmentPolicy);
}

static bool shouldCreateSubsequence(const PaintLayer& paintLayer,
                                    GraphicsContext& context,
                                    const PaintLayerPaintingInfo& paintingInfo,
                                    PaintLayerFlags paintFlags) {
  // Caching is not needed during printing.
  if (context.printing())
    return false;

  // Don't create subsequence for a composited layer because if it can be
  // cached, we can skip the whole painting in GraphicsLayer::paint() with
  // CachedDisplayItemList.  This also avoids conflict of
  // PaintLayer::previousXXX() when paintLayer is composited scrolling and is
  // painted twice for GraphicsLayers of container and scrolling contents.
  if (paintLayer.compositingState() == PaintsIntoOwnBacking)
    return false;

  // Don't create subsequence during special painting to avoid cache conflict
  // with normal painting.
  if (paintingInfo.getGlobalPaintFlags() & GlobalPaintFlattenCompositingLayers)
    return false;
  if (paintFlags &
      (PaintLayerPaintingRootBackgroundOnly |
       PaintLayerPaintingOverlayScrollbars | PaintLayerUncachedClipRects))
    return false;

  // Create subsequence for only stacking contexts whose painting are atomic.
  // SVG is also painted atomically.
  if (!paintLayer.stackingNode()->isStackingContext() &&
      !paintLayer.layoutObject()->isSVGRoot())
    return false;

  // The layer doesn't have children. Subsequence caching is not worth because
  // normally the actual painting will be cheap.
  // SVG is also painted atomically.
  if (!PaintLayerStackingNodeIterator(*paintLayer.stackingNode(), AllChildren)
           .next() &&
      !paintLayer.layoutObject()->isSVGRoot())
    return false;

  // When in FOUC-avoidance mode, don't cache any subsequences, to avoid having
  // to invalidate all of them when leaving this mode. There is an early-out in
  // BlockPainter::paintContents that may result in nothing getting painted in
  // this mode, in addition to early-out logic in PaintLayerPainter.
  if (paintLayer.layoutObject()->document().didLayoutWithPendingStylesheets())
    return false;

  return true;
}

static bool shouldRepaintSubsequence(
    PaintLayer& paintLayer,
    const PaintLayerPaintingInfo& paintingInfo,
    ShouldRespectOverflowClipType respectOverflowClip,
    const LayoutSize& subpixelAccumulation,
    bool& shouldClearEmptyPaintPhaseFlags) {
  bool needsRepaint = false;

  // We should set shouldResetEmptyPaintPhaseFlags if some previously unpainted
  // objects may begin to be painted, causing a previously empty paint phase to
  // become non-empty.

  // Repaint subsequence if the layer is marked for needing repaint.
  // We don't set needsResetEmptyPaintPhase here, but clear the empty paint
  // phase flags in PaintLayer::setNeedsPaintPhaseXXX(), to ensure that we won't
  // clear previousPaintPhaseXXXEmpty flags when unrelated things changed which
  // won't cause the paint phases to become non-empty.
  if (paintLayer.needsRepaint())
    needsRepaint = true;

  // Repaint if layer's clip changes.
  // TODO(chrishtr): implement detecting clipping changes in SPv2.
  // crbug.com/645667
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    ClipRects& clipRects = paintLayer.clipper().paintingClipRects(
        paintingInfo.rootLayer, respectOverflowClip, subpixelAccumulation);
    ClipRects* previousClipRects = paintLayer.previousPaintingClipRects();
    if (&clipRects != previousClipRects &&
        (!previousClipRects || clipRects != *previousClipRects)) {
      needsRepaint = true;
      shouldClearEmptyPaintPhaseFlags = true;
    }
    paintLayer.setPreviousPaintingClipRects(clipRects);
  }

  // Repaint if previously the layer might be clipped by paintDirtyRect and
  // paintDirtyRect changes.
  if (paintLayer.previousPaintResult() == MayBeClippedByPaintDirtyRect &&
      paintLayer.previousPaintDirtyRect() != paintingInfo.paintDirtyRect) {
    needsRepaint = true;
    shouldClearEmptyPaintPhaseFlags = true;
  }
  paintLayer.setPreviousPaintDirtyRect(paintingInfo.paintDirtyRect);

  // Repaint if scroll offset accumulation changes.
  if (paintingInfo.scrollOffsetAccumulation !=
      paintLayer.previousScrollOffsetAccumulationForPainting()) {
    needsRepaint = true;
    shouldClearEmptyPaintPhaseFlags = true;
  }
  paintLayer.setPreviousScrollOffsetAccumulationForPainting(
      paintingInfo.scrollOffsetAccumulation);

  return needsRepaint;
}

PaintResult PaintLayerPainter::paintLayerContents(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfoArg,
    PaintLayerFlags paintFlags,
    FragmentPolicy fragmentPolicy) {
  Optional<ScopedPaintChunkProperties> scopedPaintChunkProperties;
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      m_paintLayer.layoutObject() &&
      m_paintLayer.layoutObject()->isLayoutView()) {
    const auto* objectPaintProperties =
        m_paintLayer.layoutObject()->paintProperties();
    DCHECK(objectPaintProperties &&
           objectPaintProperties->localBorderBoxProperties());
    PaintChunkProperties properties(
        context.getPaintController().currentPaintChunkProperties());
    auto& localBorderBoxProperties =
        *objectPaintProperties->localBorderBoxProperties();
    properties.transform =
        localBorderBoxProperties.propertyTreeState.transform();
    properties.scroll = localBorderBoxProperties.propertyTreeState.scroll();
    properties.clip = localBorderBoxProperties.propertyTreeState.clip();
    properties.effect = localBorderBoxProperties.propertyTreeState.effect();
    properties.backfaceHidden =
        m_paintLayer.layoutObject()->hasHiddenBackface();
    scopedPaintChunkProperties.emplace(context.getPaintController(),
                                       m_paintLayer, properties);
  }

  DCHECK(m_paintLayer.isSelfPaintingLayer() ||
         m_paintLayer.hasSelfPaintingLayerDescendant());
  DCHECK(!(paintFlags & PaintLayerAppliedTransform));

  bool isSelfPaintingLayer = m_paintLayer.isSelfPaintingLayer();
  bool isPaintingOverlayScrollbars =
      paintFlags & PaintLayerPaintingOverlayScrollbars;
  bool isPaintingScrollingContent =
      paintFlags & PaintLayerPaintingCompositingScrollingPhase;
  bool isPaintingCompositedForeground =
      paintFlags & PaintLayerPaintingCompositingForegroundPhase;
  bool isPaintingCompositedBackground =
      paintFlags & PaintLayerPaintingCompositingBackgroundPhase;
  bool isPaintingOverflowContents =
      paintFlags & PaintLayerPaintingOverflowContents;
  // Outline always needs to be painted even if we have no visible content.
  // Also, the outline is painted in the background phase during composited
  // scrolling.  If it were painted in the foreground phase, it would move with
  // the scrolled content. When not composited scrolling, the outline is painted
  // in the foreground phase. Since scrolled contents are moved by paint
  // invalidation in this case, the outline won't get 'dragged along'.
  bool shouldPaintSelfOutline =
      isSelfPaintingLayer && !isPaintingOverlayScrollbars &&
      ((isPaintingScrollingContent && isPaintingCompositedBackground) ||
       (!isPaintingScrollingContent && isPaintingCompositedForeground)) &&
      m_paintLayer.layoutObject()->styleRef().hasOutline();
  bool shouldPaintContent = m_paintLayer.hasVisibleContent() &&
                            isSelfPaintingLayer && !isPaintingOverlayScrollbars;

  PaintResult result = FullyPainted;

  if (paintFlags & PaintLayerPaintingRootBackgroundOnly &&
      !m_paintLayer.layoutObject()->isLayoutView())
    return result;

  if (m_paintLayer.layoutObject()->view()->frame() &&
      m_paintLayer.layoutObject()->view()->frame()->shouldThrottleRendering())
    return result;

  // Ensure our lists are up to date.
  m_paintLayer.stackingNode()->updateLayerListsIfNeeded();

  LayoutSize subpixelAccumulation =
      m_paintLayer.compositingState() == PaintsIntoOwnBacking
          ? m_paintLayer.subpixelAccumulation()
          : paintingInfoArg.subPixelAccumulation;
  ShouldRespectOverflowClipType respectOverflowClip =
      shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject());

  Optional<SubsequenceRecorder> subsequenceRecorder;
  bool shouldClearEmptyPaintPhaseFlags = false;
  if (shouldCreateSubsequence(m_paintLayer, context, paintingInfoArg,
                              paintFlags)) {
    if (!shouldRepaintSubsequence(m_paintLayer, paintingInfoArg,
                                  respectOverflowClip, subpixelAccumulation,
                                  shouldClearEmptyPaintPhaseFlags) &&
        SubsequenceRecorder::useCachedSubsequenceIfPossible(context,
                                                            m_paintLayer))
      return result;
    subsequenceRecorder.emplace(context, m_paintLayer);
  } else {
    shouldClearEmptyPaintPhaseFlags = true;
  }

  if (shouldClearEmptyPaintPhaseFlags) {
    m_paintLayer.setPreviousPaintPhaseDescendantOutlinesEmpty(false);
    m_paintLayer.setPreviousPaintPhaseFloatEmpty(false);
    m_paintLayer.setPreviousPaintPhaseDescendantBlockBackgroundsEmpty(false);
  }

  PaintLayerPaintingInfo paintingInfo = paintingInfoArg;

  LayoutPoint offsetFromRoot;
  m_paintLayer.convertToLayerCoords(paintingInfo.rootLayer, offsetFromRoot);
  offsetFromRoot.move(subpixelAccumulation);

  LayoutRect bounds = m_paintLayer.physicalBoundingBox(offsetFromRoot);
  if (!paintingInfo.paintDirtyRect.contains(bounds))
    result = MayBeClippedByPaintDirtyRect;

  if (paintingInfo.ancestorHasClipPathClipping &&
      m_paintLayer.layoutObject()->isPositioned())
    UseCounter::count(m_paintLayer.layoutObject()->document(),
                      UseCounter::ClipPathOfPositionedElement);

  // These helpers output clip and compositing operations using a RAII pattern.
  // Stack-allocated-varibles are destructed in the reverse order of
  // construction, so they are nested properly.
  Optional<ClipPathClipper> clipPathClipper;
  // Clip-path, like border radius, must not be applied to the contents of a
  // composited-scrolling container.  It must, however, still be applied to the
  // mask layer, so that the compositor can properly mask the
  // scrolling contents and scrollbars.
  if (m_paintLayer.layoutObject()->hasClipPath() &&
      (!m_paintLayer.needsCompositedScrolling() ||
       (paintFlags & PaintLayerPaintingChildClippingMaskPhase))) {
    paintingInfo.ancestorHasClipPathClipping = true;

    LayoutRect referenceBox(m_paintLayer.boxForClipPath());
    // Note that this isn't going to work correctly if crossing a column
    // boundary. The reference box should be determined per-fragment, and hence
    // this ought to be performed after fragmentation.
    if (m_paintLayer.enclosingPaginationLayer())
      m_paintLayer.convertFromFlowThreadToVisualBoundingBoxInAncestor(
          paintingInfo.rootLayer, referenceBox);
    else
      referenceBox.moveBy(offsetFromRoot);
    clipPathClipper.emplace(
        context, *m_paintLayer.layoutObject()->styleRef().clipPath(),
        *m_paintLayer.layoutObject(), FloatRect(referenceBox),
        FloatPoint(referenceBox.location()));
  }

  Optional<CompositingRecorder> compositingRecorder;
  // Blending operations must be performed only with the nearest ancestor
  // stacking context.  Note that there is no need to composite if we're
  // painting the root.
  // FIXME: this should be unified further into
  // PaintLayer::paintsWithTransparency().
  bool shouldCompositeForBlendMode =
      (!m_paintLayer.layoutObject()->isDocumentElement() ||
       m_paintLayer.layoutObject()->isSVGRoot()) &&
      m_paintLayer.stackingNode()->isStackingContext() &&
      m_paintLayer.hasNonIsolatedDescendantWithBlendMode();
  if (shouldCompositeForBlendMode ||
      m_paintLayer.paintsWithTransparency(paintingInfo.getGlobalPaintFlags())) {
    FloatRect compositingBounds = FloatRect(m_paintLayer.paintingExtent(
        paintingInfo.rootLayer, paintingInfo.subPixelAccumulation,
        paintingInfo.getGlobalPaintFlags()));
    compositingRecorder.emplace(
        context, *m_paintLayer.layoutObject(),
        WebCoreCompositeToSkiaComposite(
            CompositeSourceOver,
            m_paintLayer.layoutObject()->style()->blendMode()),
        m_paintLayer.layoutObject()->opacity(), &compositingBounds);
  }

  PaintLayerPaintingInfo localPaintingInfo(paintingInfo);
  localPaintingInfo.subPixelAccumulation = subpixelAccumulation;

  PaintLayerFragments layerFragments;
  if (shouldPaintContent || shouldPaintSelfOutline ||
      isPaintingOverlayScrollbars) {
    // Collect the fragments. This will compute the clip rectangles and paint
    // offsets for each layer fragment.
    ClipRectsCacheSlot cacheSlot = (paintFlags & PaintLayerUncachedClipRects)
                                       ? UncachedClipRects
                                       : PaintingClipRects;
    // TODO(trchen): We haven't decided how to handle visual fragmentation with
    // SPv2.  Related thread
    // https://groups.google.com/a/chromium.org/forum/#!topic/graphics-dev/81XuWFf-mxM
    if (fragmentPolicy == ForceSingleFragment ||
        RuntimeEnabledFeatures::slimmingPaintV2Enabled())
      m_paintLayer.appendSingleFragmentIgnoringPagination(
          layerFragments, localPaintingInfo.rootLayer,
          localPaintingInfo.paintDirtyRect, cacheSlot,
          IgnoreOverlayScrollbarSize, respectOverflowClip, &offsetFromRoot,
          localPaintingInfo.subPixelAccumulation);
    else
      m_paintLayer.collectFragments(layerFragments, localPaintingInfo.rootLayer,
                                    localPaintingInfo.paintDirtyRect, cacheSlot,
                                    IgnoreOverlayScrollbarSize,
                                    respectOverflowClip, &offsetFromRoot,
                                    localPaintingInfo.subPixelAccumulation);

    if (shouldPaintContent) {
      // TODO(wangxianzhu): This is for old slow scrolling. Implement similar
      // optimization for slimming paint v2.
      shouldPaintContent = atLeastOneFragmentIntersectsDamageRect(
          layerFragments, localPaintingInfo, paintFlags, offsetFromRoot);
      if (!shouldPaintContent)
        result = MayBeClippedByPaintDirtyRect;
    }
  }

  bool selectionOnly =
      localPaintingInfo.getGlobalPaintFlags() & GlobalPaintSelectionOnly;

  {  // Begin block for the lifetime of any filter.
    FilterPainter filterPainter(m_paintLayer, context, offsetFromRoot,
                                layerFragments.isEmpty()
                                    ? ClipRect()
                                    : layerFragments[0].backgroundRect,
                                localPaintingInfo, paintFlags);

    Optional<ScopedPaintChunkProperties> contentScopedPaintChunkProperties;
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
        !scopedPaintChunkProperties.has_value()) {
      // If layoutObject() is a LayoutView and root layer scrolling is enabled,
      // the LayoutView's paint properties will already have been applied at
      // the top of this method, in scopedPaintChunkProperties.
      DCHECK(!(RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
               m_paintLayer.layoutObject() &&
               m_paintLayer.layoutObject()->isLayoutView()));
      const auto* objectPaintProperties =
          m_paintLayer.layoutObject()->paintProperties();
      DCHECK(objectPaintProperties &&
             objectPaintProperties->localBorderBoxProperties());
      PaintChunkProperties properties(
          context.getPaintController().currentPaintChunkProperties());
      auto& localBorderBoxProperties =
          *objectPaintProperties->localBorderBoxProperties();
      properties.transform =
          localBorderBoxProperties.propertyTreeState.transform();
      properties.scroll = localBorderBoxProperties.propertyTreeState.scroll();
      properties.clip = localBorderBoxProperties.propertyTreeState.clip();
      properties.effect = localBorderBoxProperties.propertyTreeState.effect();
      properties.backfaceHidden =
          m_paintLayer.layoutObject()->hasHiddenBackface();
      contentScopedPaintChunkProperties.emplace(context.getPaintController(),
                                                m_paintLayer, properties);
    }

    bool isPaintingRootLayer = (&m_paintLayer) == paintingInfo.rootLayer;
    bool shouldPaintBackground =
        shouldPaintContent && !selectionOnly &&
        (isPaintingCompositedBackground ||
         (isPaintingRootLayer &&
          !(paintFlags & PaintLayerPaintingSkipRootBackground)));
    bool shouldPaintNegZOrderList =
        (isPaintingScrollingContent && isPaintingOverflowContents) ||
        (!isPaintingScrollingContent && isPaintingCompositedBackground);
    bool shouldPaintOwnContents =
        isPaintingCompositedForeground && shouldPaintContent;
    bool shouldPaintNormalFlowAndPosZOrderLists =
        isPaintingCompositedForeground;
    bool shouldPaintOverlayScrollbars = isPaintingOverlayScrollbars;

    if (shouldPaintBackground) {
      paintBackgroundForFragments(layerFragments, context,
                                  paintingInfo.paintDirtyRect,
                                  localPaintingInfo, paintFlags);
    }

    if (shouldPaintNegZOrderList) {
      if (paintChildren(NegativeZOrderChildren, context, paintingInfo,
                        paintFlags) == MayBeClippedByPaintDirtyRect)
        result = MayBeClippedByPaintDirtyRect;
    }

    if (shouldPaintOwnContents) {
      paintForegroundForFragments(layerFragments, context,
                                  paintingInfo.paintDirtyRect,
                                  localPaintingInfo, selectionOnly, paintFlags);
    }

    if (shouldPaintSelfOutline)
      paintSelfOutlineForFragments(layerFragments, context, localPaintingInfo,
                                   paintFlags);

    if (shouldPaintNormalFlowAndPosZOrderLists) {
      if (paintChildren(NormalFlowChildren | PositiveZOrderChildren, context,
                        paintingInfo,
                        paintFlags) == MayBeClippedByPaintDirtyRect)
        result = MayBeClippedByPaintDirtyRect;
    }

    if (shouldPaintOverlayScrollbars)
      paintOverflowControlsForFragments(layerFragments, context,
                                        localPaintingInfo, paintFlags);
  }  // FilterPainter block

  bool shouldPaintMask =
      (paintFlags & PaintLayerPaintingCompositingMaskPhase) &&
      shouldPaintContent && m_paintLayer.layoutObject()->hasMask() &&
      !selectionOnly;
  bool shouldPaintClippingMask =
      (paintFlags & PaintLayerPaintingChildClippingMaskPhase) &&
      shouldPaintContent && !selectionOnly;

  if (shouldPaintMask)
    paintMaskForFragments(layerFragments, context, localPaintingInfo,
                          paintFlags);
  if (shouldPaintClippingMask) {
    // Paint the border radius mask for the fragments.
    paintChildClippingMaskForFragments(layerFragments, context,
                                       localPaintingInfo, paintFlags);
  }

  if (subsequenceRecorder)
    m_paintLayer.setPreviousPaintResult(result);
  return result;
}

bool PaintLayerPainter::needsToClip(
    const PaintLayerPaintingInfo& localPaintingInfo,
    const ClipRect& clipRect) {
  // Clipping will be applied by property nodes directly for SPv2.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return false;

  return clipRect.rect() != localPaintingInfo.paintDirtyRect ||
         clipRect.hasRadius();
}

bool PaintLayerPainter::atLeastOneFragmentIntersectsDamageRect(
    PaintLayerFragments& fragments,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags localPaintFlags,
    const LayoutPoint& offsetFromRoot) {
  if (m_paintLayer.enclosingPaginationLayer())
    return true;  // The fragments created have already been found to intersect
                  // with the damage rect.

  if (&m_paintLayer == localPaintingInfo.rootLayer &&
      (localPaintFlags & PaintLayerPaintingOverflowContents))
    return true;

  for (PaintLayerFragment& fragment : fragments) {
    LayoutPoint newOffsetFromRoot = offsetFromRoot + fragment.paginationOffset;
    // Note that this really only works reliably on the first fragment. If the
    // layer has visible overflow and a subsequent fragment doesn't intersect
    // with the border box of the layer (i.e. only contains an overflow portion
    // of the layer), intersection will fail. The reason for this is that
    // fragment.layerBounds is set to the border box, not the bounding box, of
    // the layer.
    if (m_paintLayer.intersectsDamageRect(fragment.layerBounds,
                                          fragment.backgroundRect.rect(),
                                          newOffsetFromRoot))
      return true;
  }
  return false;
}

PaintResult PaintLayerPainter::paintLayerWithTransform(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags) {
  TransformationMatrix layerTransform =
      m_paintLayer.renderableTransform(paintingInfo.getGlobalPaintFlags());
  // If the transform can't be inverted, then don't paint anything.
  if (!layerTransform.isInvertible())
    return FullyPainted;

  // FIXME: We should make sure that we don't walk past paintingInfo.rootLayer
  // here.  m_paintLayer may be the "root", and then we should avoid looking at
  // its parent.
  PaintLayer* parentLayer = m_paintLayer.parent();

  LayoutObject* object = m_paintLayer.layoutObject();
  LayoutView* view = object->view();
  bool isFixedPosObjectInPagedMedia =
      object->style()->position() == FixedPosition &&
      object->container() == view && view->pageLogicalHeight();
  PaintLayer* paginationLayer = m_paintLayer.enclosingPaginationLayer();
  PaintLayerFragments fragments;
  // TODO(crbug.com/619094): Figure out the correct behaviour for fixed position
  // objects in paged media with vertical writing modes.
  if (isFixedPosObjectInPagedMedia && view->isHorizontalWritingMode()) {
    // "For paged media, boxes with fixed positions are repeated on every page."
    // https://www.w3.org/TR/2011/REC-CSS2-20110607/visuren.html#fixed-positioning
    unsigned pages =
        ceilf(view->documentRect().height() / view->pageLogicalHeight());
    LayoutPoint paginationOffset;

    // The fixed position object is offset from the top of the page, so remove
    // any scroll offset.
    LayoutPoint offsetFromRoot;
    m_paintLayer.convertToLayerCoords(paintingInfo.rootLayer, offsetFromRoot);
    paginationOffset -= offsetFromRoot - m_paintLayer.location();

    for (unsigned i = 0; i < pages; i++) {
      PaintLayerFragment fragment;
      fragment.backgroundRect = paintingInfo.paintDirtyRect;
      fragment.paginationOffset = paginationOffset;
      fragments.append(fragment);
      paginationOffset += LayoutPoint(LayoutUnit(), view->pageLogicalHeight());
    }
  } else if (paginationLayer) {
    // FIXME: This is a mess. Look closely at this code and the code in Layer
    // and fix any issues in it & refactor to make it obvious from code
    // structure what it does and that it's correct.
    ClipRectsCacheSlot cacheSlot = (paintFlags & PaintLayerUncachedClipRects)
                                       ? UncachedClipRects
                                       : PaintingClipRects;
    ShouldRespectOverflowClipType respectOverflowClip =
        shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject());
    // Calculate the transformed bounding box in the current coordinate space,
    // to figure out which fragmentainers (e.g. columns) we need to visit.
    LayoutRect transformedExtent = PaintLayer::transparencyClipBox(
        &m_paintLayer, paginationLayer, PaintLayer::PaintingTransparencyClipBox,
        PaintLayer::RootOfTransparencyClipBox,
        paintingInfo.subPixelAccumulation, paintingInfo.getGlobalPaintFlags());
    // FIXME: we don't check if paginationLayer is within paintingInfo.rootLayer
    // here.
    paginationLayer->collectFragments(
        fragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect,
        cacheSlot, IgnoreOverlayScrollbarSize, respectOverflowClip, 0,
        paintingInfo.subPixelAccumulation, &transformedExtent);
  } else {
    // We don't need to collect any fragments in the regular way here. We have
    // already calculated a clip rectangle for the ancestry if it was needed,
    // and clipping this layer is something that can be done further down the
    // path, when the transform has been applied.
    PaintLayerFragment fragment;
    fragment.backgroundRect = paintingInfo.paintDirtyRect;
    fragments.append(fragment);
  }

  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (fragments.size() > 1)
    cacheSkipper.emplace(context);

  ClipRect ancestorBackgroundClipRect;
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (parentLayer) {
      // Calculate the clip rectangle that the ancestors establish.
      ClipRectsContext clipRectsContext(
          paintingInfo.rootLayer,
          (paintFlags & PaintLayerUncachedClipRects) ? UncachedClipRects
                                                     : PaintingClipRects,
          IgnoreOverlayScrollbarSize);
      if (shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject()) ==
          IgnoreOverflowClip)
        clipRectsContext.setIgnoreOverflowClip();
      ancestorBackgroundClipRect =
          m_paintLayer.clipper().backgroundClipRect(clipRectsContext);
    }
  }

  PaintResult result = FullyPainted;
  for (const auto& fragment : fragments) {
    Optional<LayerClipRecorder> clipRecorder;
    if (parentLayer && !RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      ClipRect clipRectForFragment(ancestorBackgroundClipRect);
      // A fixed-position object is repeated on every page, but if it is clipped
      // by an ancestor layer then the repetitions are clipped out.
      if (!isFixedPosObjectInPagedMedia)
        clipRectForFragment.moveBy(fragment.paginationOffset);
      clipRectForFragment.intersect(fragment.backgroundRect);
      if (clipRectForFragment.isEmpty())
        continue;
      if (needsToClip(paintingInfo, clipRectForFragment)) {
        if (m_paintLayer.layoutObject()->isPositioned() &&
            clipRectForFragment.isClippedByClipCss())
          UseCounter::count(m_paintLayer.layoutObject()->document(),
                            UseCounter::ClipCssOfPositionedElement);
        if (m_paintLayer.layoutObject()->isFixedPositioned())
          UseCounter::count(m_paintLayer.layoutObject()->document(),
                            UseCounter::ClipCssOfFixedPositionElement);
        clipRecorder.emplace(context, *parentLayer->layoutObject(),
                             DisplayItem::kClipLayerParent, clipRectForFragment,
                             &paintingInfo, fragment.paginationOffset,
                             paintFlags);
      }
    }
    if (paintFragmentByApplyingTransform(context, paintingInfo, paintFlags,
                                         fragment.paginationOffset) ==
        MayBeClippedByPaintDirtyRect)
      result = MayBeClippedByPaintDirtyRect;
  }
  return result;
}

PaintResult PaintLayerPainter::paintFragmentByApplyingTransform(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags,
    const LayoutPoint& fragmentTranslation) {
  // This involves subtracting out the position of the layer in our current
  // coordinate space, but preserving the accumulated error for sub-pixel
  // layout.
  LayoutPoint delta;
  m_paintLayer.convertToLayerCoords(paintingInfo.rootLayer, delta);
  delta.moveBy(fragmentTranslation);
  TransformationMatrix transform(
      m_paintLayer.renderableTransform(paintingInfo.getGlobalPaintFlags()));
  IntPoint roundedDelta = roundedIntPoint(delta);
  transform.translateRight(roundedDelta.x(), roundedDelta.y());
  LayoutSize adjustedSubPixelAccumulation =
      paintingInfo.subPixelAccumulation + (delta - roundedDelta);

  // TODO(jbroman): Put the real transform origin here, instead of using a
  // matrix with the origin baked in.
  FloatPoint3D transformOrigin;
  Transform3DRecorder transform3DRecorder(
      context, *m_paintLayer.layoutObject(),
      DisplayItem::kTransform3DElementTransform, transform, transformOrigin);

  // Now do a paint with the root layer shifted to be us.
  PaintLayerPaintingInfo transformedPaintingInfo(
      &m_paintLayer, LayoutRect(enclosingIntRect(transform.inverse().mapRect(
                         paintingInfo.paintDirtyRect))),
      paintingInfo.getGlobalPaintFlags(), adjustedSubPixelAccumulation);
  transformedPaintingInfo.ancestorHasClipPathClipping =
      paintingInfo.ancestorHasClipPathClipping;

  // Remove skip root background flag when we're painting with a new root.
  if (&m_paintLayer != paintingInfo.rootLayer)
    paintFlags &= ~PaintLayerPaintingSkipRootBackground;

  return paintLayerContentsCompositingAllPhases(
      context, transformedPaintingInfo, paintFlags, ForceSingleFragment);
}

PaintResult PaintLayerPainter::paintChildren(
    unsigned childrenToVisit,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags) {
  PaintResult result = FullyPainted;
  if (!m_paintLayer.hasSelfPaintingLayerDescendant())
    return result;

#if ENABLE(ASSERT)
  LayerListMutationDetector mutationChecker(m_paintLayer.stackingNode());
#endif

  PaintLayerStackingNodeIterator iterator(*m_paintLayer.stackingNode(),
                                          childrenToVisit);
  PaintLayerStackingNode* child = iterator.next();
  if (!child)
    return result;

  IntSize scrollOffsetAccumulationForChildren =
      paintingInfo.scrollOffsetAccumulation;
  if (m_paintLayer.layoutObject()->hasOverflowClip())
    scrollOffsetAccumulationForChildren +=
        m_paintLayer.layoutBox()->scrolledContentOffset();

  for (; child; child = iterator.next()) {
    PaintLayerPainter childPainter(*child->layer());
    // If this Layer should paint into its own backing or a grouped backing,
    // that will be done via CompositedLayerMapping::paintContents() and
    // CompositedLayerMapping::doPaintTask().
    if (!childPainter.shouldPaintLayerInSoftwareMode(
            paintingInfo.getGlobalPaintFlags(), paintFlags))
      continue;

    PaintLayerPaintingInfo childPaintingInfo = paintingInfo;
    childPaintingInfo.scrollOffsetAccumulation =
        scrollOffsetAccumulationForChildren;
    // Rare case: accumulate scroll offset of non-stacking-context ancestors up
    // to m_paintLayer.
    for (PaintLayer* parentLayer = child->layer()->parent();
         parentLayer != &m_paintLayer; parentLayer = parentLayer->parent()) {
      if (parentLayer->layoutObject()->hasOverflowClip())
        childPaintingInfo.scrollOffsetAccumulation +=
            parentLayer->layoutBox()->scrolledContentOffset();
    }

    if (childPainter.paintLayer(context, childPaintingInfo, paintFlags) ==
        MayBeClippedByPaintDirtyRect)
      result = MayBeClippedByPaintDirtyRect;
  }

  return result;
}

bool PaintLayerPainter::shouldPaintLayerInSoftwareMode(
    const GlobalPaintFlags globalPaintFlags,
    PaintLayerFlags paintFlags) {
  DisableCompositingQueryAsserts disabler;

  return m_paintLayer.compositingState() == NotComposited ||
         (globalPaintFlags & GlobalPaintFlattenCompositingLayers);
}

void PaintLayerPainter::paintOverflowControlsForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags) {
  PaintLayerScrollableArea* scrollableArea = m_paintLayer.getScrollableArea();
  if (!scrollableArea)
    return;

  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments) {
    // We need to apply the same clips and transforms that
    // paintFragmentWithPhase would have.
    LayoutRect cullRect = fragment.backgroundRect.rect();

    Optional<LayerClipRecorder> clipRecorder;
    if (needsToClip(localPaintingInfo, fragment.backgroundRect)) {
      clipRecorder.emplace(context, *m_paintLayer.layoutObject(),
                           DisplayItem::kClipLayerOverflowControls,
                           fragment.backgroundRect, &localPaintingInfo,
                           fragment.paginationOffset, paintFlags);
    }

    Optional<ScrollRecorder> scrollRecorder;
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
        !localPaintingInfo.scrollOffsetAccumulation.isZero()) {
      cullRect.move(localPaintingInfo.scrollOffsetAccumulation);
      scrollRecorder.emplace(context, *m_paintLayer.layoutObject(),
                             DisplayItem::kScrollOverflowControls,
                             localPaintingInfo.scrollOffsetAccumulation);
    }

    // We pass IntPoint() as the paint offset here, because
    // ScrollableArea::paintOverflowControls just ignores it and uses the
    // offset found in a previous pass.
    CullRect snappedCullRect(pixelSnappedIntRect(cullRect));
    ScrollableAreaPainter(*scrollableArea)
        .paintOverflowControls(context, IntPoint(), snappedCullRect, true);
  }
}

void PaintLayerPainter::paintFragmentWithPhase(
    PaintPhase phase,
    const PaintLayerFragment& fragment,
    GraphicsContext& context,
    const ClipRect& clipRect,
    const PaintLayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags,
    ClipState clipState) {
  DCHECK(m_paintLayer.isSelfPaintingLayer());

  Optional<LayerClipRecorder> clipRecorder;
  if (clipState != HasClipped && paintingInfo.clipToDirtyRect &&
      needsToClip(paintingInfo, clipRect)) {
    DisplayItem::Type clipType =
        DisplayItem::paintPhaseToClipLayerFragmentType(phase);
    LayerClipRecorder::BorderRadiusClippingRule clippingRule;
    switch (phase) {
      case PaintPhaseSelfBlockBackgroundOnly:  // Background painting will
                                               // handle clipping to self.
      case PaintPhaseSelfOutlineOnly:
      case PaintPhaseMask:  // Mask painting will handle clipping to self.
        clippingRule = LayerClipRecorder::DoNotIncludeSelfForBorderRadius;
        break;
      default:
        clippingRule = LayerClipRecorder::IncludeSelfForBorderRadius;
        break;
    }

    clipRecorder.emplace(context, *m_paintLayer.layoutObject(), clipType,
                         clipRect, &paintingInfo, fragment.paginationOffset,
                         paintFlags, clippingRule);
  }

  LayoutRect newCullRect(clipRect.rect());
  Optional<ScrollRecorder> scrollRecorder;
  LayoutPoint paintOffset = -m_paintLayer.layoutBoxLocation();
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    const auto* objectPaintProperties =
        m_paintLayer.layoutObject()->paintProperties();
    DCHECK(objectPaintProperties &&
           objectPaintProperties->localBorderBoxProperties());
    paintOffset +=
        toSize(objectPaintProperties->localBorderBoxProperties()->paintOffset);
    newCullRect.move(paintingInfo.scrollOffsetAccumulation);
  } else {
    paintOffset += toSize(fragment.layerBounds.location());
    if (!paintingInfo.scrollOffsetAccumulation.isZero()) {
      // As a descendant of the root layer, m_paintLayer's painting is not
      // controlled by the ScrollRecorders created by BlockPainter of the
      // ancestor layers up to the root layer, so we need to issue
      // ScrollRecorder for this layer seperately, with the scroll offset
      // accumulated from the root layer to the parent of this layer, to get the
      // same result as ScrollRecorder in BlockPainter.
      paintOffset += paintingInfo.scrollOffsetAccumulation;

      newCullRect.move(paintingInfo.scrollOffsetAccumulation);
      scrollRecorder.emplace(context, *m_paintLayer.layoutObject(), phase,
                             paintingInfo.scrollOffsetAccumulation);
    }
  }
  PaintInfo paintInfo(context, pixelSnappedIntRect(newCullRect), phase,
                      paintingInfo.getGlobalPaintFlags(), paintFlags,
                      paintingInfo.rootLayer->layoutObject());

  m_paintLayer.layoutObject()->paint(paintInfo, paintOffset);
}

void PaintLayerPainter::paintBackgroundForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const LayoutRect& transparencyPaintDirtyRect,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags) {
  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments)
    paintFragmentWithPhase(PaintPhaseSelfBlockBackgroundOnly, fragment, context,
                           fragment.backgroundRect, localPaintingInfo,
                           paintFlags, HasNotClipped);
}

void PaintLayerPainter::paintForegroundForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const LayoutRect& transparencyPaintDirtyRect,
    const PaintLayerPaintingInfo& localPaintingInfo,
    bool selectionOnly,
    PaintLayerFlags paintFlags) {
  DCHECK(!(paintFlags & PaintLayerPaintingRootBackgroundOnly));

  // Optimize clipping for the single fragment case.
  bool shouldClip = localPaintingInfo.clipToDirtyRect &&
                    layerFragments.size() == 1 &&
                    !layerFragments[0].foregroundRect.isEmpty();
  ClipState clipState = HasNotClipped;
  Optional<LayerClipRecorder> clipRecorder;
  if (shouldClip &&
      needsToClip(localPaintingInfo, layerFragments[0].foregroundRect)) {
    clipRecorder.emplace(context, *m_paintLayer.layoutObject(),
                         DisplayItem::kClipLayerForeground,
                         layerFragments[0].foregroundRect, &localPaintingInfo,
                         layerFragments[0].paginationOffset, paintFlags);
    clipState = HasClipped;
  }

  // We have to loop through every fragment multiple times, since we have to
  // issue paint invalidations in each specific phase in order for interleaving
  // of the fragments to work properly.
  if (selectionOnly) {
    paintForegroundForFragmentsWithPhase(PaintPhaseSelection, layerFragments,
                                         context, localPaintingInfo, paintFlags,
                                         clipState);
  } else {
    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() ||
        m_paintLayer.needsPaintPhaseDescendantBlockBackgrounds()) {
      size_t sizeBefore =
          context.getPaintController().newDisplayItemList().size();
      paintForegroundForFragmentsWithPhase(
          PaintPhaseDescendantBlockBackgroundsOnly, layerFragments, context,
          localPaintingInfo, paintFlags, clipState);
      // Don't set the empty flag if we are not painting the whole background.
      if (!(paintFlags & PaintLayerPaintingSkipRootBackground)) {
        bool phaseIsEmpty =
            context.getPaintController().newDisplayItemList().size() ==
            sizeBefore;
        DCHECK(phaseIsEmpty ||
               m_paintLayer.needsPaintPhaseDescendantBlockBackgrounds());
        m_paintLayer.setPreviousPaintPhaseDescendantBlockBackgroundsEmpty(
            phaseIsEmpty);
      }
    }

    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() ||
        m_paintLayer.needsPaintPhaseFloat()) {
      size_t sizeBefore =
          context.getPaintController().newDisplayItemList().size();
      paintForegroundForFragmentsWithPhase(PaintPhaseFloat, layerFragments,
                                           context, localPaintingInfo,
                                           paintFlags, clipState);
      bool phaseIsEmpty =
          context.getPaintController().newDisplayItemList().size() ==
          sizeBefore;
      DCHECK(phaseIsEmpty || m_paintLayer.needsPaintPhaseFloat());
      m_paintLayer.setPreviousPaintPhaseFloatEmpty(phaseIsEmpty);
    }

    paintForegroundForFragmentsWithPhase(PaintPhaseForeground, layerFragments,
                                         context, localPaintingInfo, paintFlags,
                                         clipState);

    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() ||
        m_paintLayer.needsPaintPhaseDescendantOutlines()) {
      size_t sizeBefore =
          context.getPaintController().newDisplayItemList().size();
      paintForegroundForFragmentsWithPhase(
          PaintPhaseDescendantOutlinesOnly, layerFragments, context,
          localPaintingInfo, paintFlags, clipState);
      bool phaseIsEmpty =
          context.getPaintController().newDisplayItemList().size() ==
          sizeBefore;
      DCHECK(phaseIsEmpty || m_paintLayer.needsPaintPhaseDescendantOutlines());
      m_paintLayer.setPreviousPaintPhaseDescendantOutlinesEmpty(phaseIsEmpty);
    }
  }
}

void PaintLayerPainter::paintForegroundForFragmentsWithPhase(
    PaintPhase phase,
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags,
    ClipState clipState) {
  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments) {
    if (!fragment.foregroundRect.isEmpty())
      paintFragmentWithPhase(phase, fragment, context, fragment.foregroundRect,
                             localPaintingInfo, paintFlags, clipState);
  }
}

void PaintLayerPainter::paintSelfOutlineForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags) {
  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments) {
    if (!fragment.backgroundRect.isEmpty())
      paintFragmentWithPhase(PaintPhaseSelfOutlineOnly, fragment, context,
                             fragment.backgroundRect, localPaintingInfo,
                             paintFlags, HasNotClipped);
  }
}

void PaintLayerPainter::paintMaskForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags) {
  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments)
    paintFragmentWithPhase(PaintPhaseMask, fragment, context,
                           fragment.backgroundRect, localPaintingInfo,
                           paintFlags, HasNotClipped);
}

void PaintLayerPainter::paintChildClippingMaskForFragments(
    const PaintLayerFragments& layerFragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo,
    PaintLayerFlags paintFlags) {
  Optional<DisplayItemCacheSkipper> cacheSkipper;
  if (layerFragments.size() > 1)
    cacheSkipper.emplace(context);

  for (auto& fragment : layerFragments)
    paintFragmentWithPhase(PaintPhaseClippingMask, fragment, context,
                           fragment.foregroundRect, localPaintingInfo,
                           paintFlags, HasNotClipped);
}

void PaintLayerPainter::paintOverlayScrollbars(
    GraphicsContext& context,
    const LayoutRect& damageRect,
    const GlobalPaintFlags paintFlags) {
  if (!m_paintLayer.containsDirtyOverlayScrollbars())
    return;

  PaintLayerPaintingInfo paintingInfo(&m_paintLayer,
                                      LayoutRect(enclosingIntRect(damageRect)),
                                      paintFlags, LayoutSize());
  paintLayer(context, paintingInfo, PaintLayerPaintingOverlayScrollbars);

  m_paintLayer.setContainsDirtyOverlayScrollbars(false);
}

}  // namespace blink
