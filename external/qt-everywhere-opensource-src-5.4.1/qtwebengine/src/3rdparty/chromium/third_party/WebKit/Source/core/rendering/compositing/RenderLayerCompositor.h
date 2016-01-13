/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderLayerCompositor_h
#define RenderLayerCompositor_h

#include "core/page/ChromeClient.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositingReasonFinder.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "wtf/HashMap.h"

namespace WebCore {

class GraphicsLayer;
class RenderPart;
class ScrollingCoordinator;

enum CompositingUpdateType {
    CompositingUpdateNone,
    CompositingUpdateAfterGeometryChange,
    CompositingUpdateAfterCompositingInputChange,
    CompositingUpdateRebuildTree,
};

enum CompositingStateTransitionType {
    NoCompositingStateChange,
    AllocateOwnCompositedLayerMapping,
    RemoveOwnCompositedLayerMapping,
    PutInSquashingLayer,
    RemoveFromSquashingLayer
};

// RenderLayerCompositor manages the hierarchy of
// composited RenderLayers. It determines which RenderLayers
// become compositing, and creates and maintains a hierarchy of
// GraphicsLayers based on the RenderLayer painting order.
//
// There is one RenderLayerCompositor per RenderView.

class RenderLayerCompositor FINAL : public GraphicsLayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerCompositor(RenderView&);
    virtual ~RenderLayerCompositor();

    void updateIfNeededRecursive();

    // Return true if this RenderView is in "compositing mode" (i.e. has one or more
    // composited RenderLayers)
    bool inCompositingMode() const;
    // FIXME: Replace all callers with inCompositingMdoe and remove this function.
    bool staleInCompositingMode() const;
    // This will make a compositing layer at the root automatically, and hook up to
    // the native view/window system.
    void setCompositingModeEnabled(bool);

    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const { return m_hasAcceleratedCompositing; }
    bool layerSquashingEnabled() const;

    bool acceleratedCompositingForOverflowScrollEnabled() const;

    bool rootShouldAlwaysComposite() const;

    // Copy the accelerated compositing related flags from Settings
    void updateAcceleratedCompositingSettings();

    // Used to indicate that a compositing update will be needed for the next frame that gets drawn.
    void setNeedsCompositingUpdate(CompositingUpdateType);

    void didLayout();

    enum UpdateLayerCompositingStateOptions {
        Normal,
        UseChickenEggHacks, // Use this to trigger temporary chicken-egg hacks. See crbug.com/339892.
    };

    // Update the compositing dirty bits, based on the compositing-impacting properties of the layer.
    void updateLayerCompositingState(RenderLayer*, UpdateLayerCompositingStateOptions = Normal);

    // Returns whether this layer is clipped by another layer that is not an ancestor of the given layer in the stacking context hierarchy.
    bool clippedByNonAncestorInStackingTree(const RenderLayer*) const;
    // Whether layer's compositedLayerMapping needs a GraphicsLayer to clip z-order children of the given RenderLayer.
    bool clipsCompositingDescendants(const RenderLayer*) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const RenderLayer*) const;

    bool supportsFixedRootBackgroundCompositing() const;
    bool needsFixedRootBackgroundLayer(const RenderLayer*) const;
    GraphicsLayer* fixedRootBackgroundLayer() const;
    void setNeedsUpdateFixedBackground() { m_needsUpdateFixedBackground = true; }

    // Repaint the appropriate layers when the given RenderLayer starts or stops being composited.
    void repaintOnCompositingChange(RenderLayer*);

    void repaintInCompositedAncestor(RenderLayer*, const LayoutRect&);
    void repaintCompositedLayers();

    RenderLayer* rootRenderLayer() const;
    GraphicsLayer* rootGraphicsLayer() const;
    GraphicsLayer* scrollLayer() const;
    GraphicsLayer* containerLayer() const;

    // We don't always have a root transform layer. This function lazily allocates one
    // and returns it as required.
    GraphicsLayer* ensureRootTransformLayer();

    enum RootLayerAttachment {
        RootLayerUnattached,
        RootLayerAttachedViaChromeClient,
        RootLayerAttachedViaEnclosingFrame
    };

    RootLayerAttachment rootLayerAttachment() const { return m_rootLayerAttachment; }
    void updateRootLayerAttachment();
    void updateRootLayerPosition();

    void setIsInWindow(bool);

    static RenderLayerCompositor* frameContentsCompositor(RenderPart*);
    // Return true if the layers changed.
    static bool parentFrameContentLayers(RenderPart*);

    // Update the geometry of the layers used for clipping and scrolling in frames.
    void frameViewDidChangeLocation(const IntPoint& contentsOffset);
    void frameViewDidChangeSize();
    void frameViewDidScroll();
    void frameViewScrollbarsExistenceDidChange();
    void rootFixedBackgroundsChanged();

    bool scrollingLayerDidChange(RenderLayer*);

    String layerTreeAsText(LayerTreeFlags);

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    void resetTrackedRepaintRects();
    void setTracksRepaints(bool);

    virtual String debugName(const GraphicsLayer*) OVERRIDE;

    void updateStyleDeterminedCompositingReasons(RenderLayer*);

    // Whether the layer could ever be composited.
    bool canBeComposited(const RenderLayer*) const;

    // FIXME: Move allocateOrClearCompositedLayerMapping to CompositingLayerAssigner once we've fixed
    // the compositing chicken/egg issues.
    bool allocateOrClearCompositedLayerMapping(RenderLayer*, CompositingStateTransitionType compositedLayerUpdate);

    void updateDirectCompositingReasons(RenderLayer*);

    void setOverlayLayer(GraphicsLayer*);

    bool inOverlayFullscreenVideo() const { return m_inOverlayFullscreenVideo; }

private:
    class OverlapMap;

#if ASSERT_ENABLED
    void assertNoUnresolvedDirtyBits();
#endif

    // Make updates to the layer based on viewport-constrained properties such as position:fixed. This can in turn affect
    // compositing.
    bool updateLayerIfViewportConstrained(RenderLayer*);

    // GraphicsLayerClient implementation
    virtual void notifyAnimationStarted(const GraphicsLayer*, double) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) OVERRIDE;

    virtual bool isTrackingRepaints() const OVERRIDE;

    // Whether the given RL needs to paint into its own separate backing (and hence would need its own CompositedLayerMapping).
    bool needsOwnBacking(const RenderLayer*) const;

    void updateIfNeeded();

    void recursiveRepaintLayer(RenderLayer*);

    void computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer*, OverlapMap&, struct CompositingRecursionData&, bool& descendantHas3DTransform, Vector<RenderLayer*>& unclippedDescendants, IntRect& absoluteDecendantBoundingBox);

    bool hasAnyAdditionalCompositedLayers(const RenderLayer* rootLayer) const;

    void ensureRootLayer();
    void destroyRootLayer();

    void attachRootLayer(RootLayerAttachment);
    void detachRootLayer();

    void updateOverflowControlsLayers();

    Page* page() const;

    GraphicsLayerFactory* graphicsLayerFactory() const;
    ScrollingCoordinator* scrollingCoordinator() const;

    void enableCompositingModeIfNeeded();

    bool requiresHorizontalScrollbarLayer() const;
    bool requiresVerticalScrollbarLayer() const;
    bool requiresScrollCornerLayer() const;

    void applyUpdateLayerCompositingStateChickenEggHacks(RenderLayer*, CompositingStateTransitionType compositedLayerUpdate);

    DocumentLifecycle& lifecycle() const;

    void applyOverlayFullscreenVideoAdjustment();

    RenderView& m_renderView;
    OwnPtr<GraphicsLayer> m_rootContentLayer;
    OwnPtr<GraphicsLayer> m_rootTransformLayer;

    CompositingReasonFinder m_compositingReasonFinder;

    CompositingUpdateType m_pendingUpdateType;

    bool m_hasAcceleratedCompositing;
    bool m_compositing;

    // The root layer doesn't composite if it's a non-scrollable frame.
    // So, after a layout we set this dirty bit to know that we need
    // to recompute whether the root layer should composite even if
    // none of its descendants composite.
    // FIXME: Get rid of all the callers of setCompositingModeEnabled
    // except the one in updateIfNeeded, then rename this to
    // m_compositingDirty.
    bool m_rootShouldAlwaysCompositeDirty;
    bool m_needsUpdateFixedBackground;
    bool m_isTrackingRepaints; // Used for testing.

    RootLayerAttachment m_rootLayerAttachment;

    // Enclosing container layer, which clips for iframe content
    OwnPtr<GraphicsLayer> m_containerLayer;
    OwnPtr<GraphicsLayer> m_scrollLayer;

    // Enclosing layer for overflow controls and the clipping layer
    OwnPtr<GraphicsLayer> m_overflowControlsHostLayer;

    // Layers for overflow controls
    OwnPtr<GraphicsLayer> m_layerForHorizontalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForVerticalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForScrollCorner;
#if USE(RUBBER_BANDING)
    OwnPtr<GraphicsLayer> m_layerForOverhangShadow;
#endif

    bool m_inOverlayFullscreenVideo;
};

} // namespace WebCore

#endif // RenderLayerCompositor_h
