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

#ifndef PaintLayerCompositor_h
#define PaintLayerCompositor_h

#include "core/CoreExport.h"
#include "core/layout/compositing/CompositingReasonFinder.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "wtf/HashMap.h"
#include <memory>

namespace blink {

class PaintLayer;
class DocumentLifecycle;
class GraphicsLayer;
class IntPoint;
class JSONObject;
class Page;
class LayoutPart;
class Scrollbar;
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

// PaintLayerCompositor manages the hierarchy of
// composited Layers. It determines which Layers
// become compositing, and creates and maintains a hierarchy of
// GraphicsLayers based on the Layer painting order.
//
// There is one PaintLayerCompositor per LayoutView.
//
// In Slimming Paint v2, PaintLayerCompositor will be eventually replaced by
// PaintArtifactCompositor.

class CORE_EXPORT PaintLayerCompositor final : public GraphicsLayerClient {
    USING_FAST_MALLOC(PaintLayerCompositor);
public:
    explicit PaintLayerCompositor(LayoutView&);
    ~PaintLayerCompositor() override;

    void updateIfNeededRecursive();

    // Return true if this LayoutView is in "compositing mode" (i.e. has one or more
    // composited Layers)
    bool inCompositingMode() const;
    // FIXME: Replace all callers with inCompositingMode and remove this function.
    bool staleInCompositingMode() const;
    // This will make a compositing layer at the root automatically, and hook up to
    // the native view/window system.
    void setCompositingModeEnabled(bool);

    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const { return m_hasAcceleratedCompositing; }

    bool preferCompositingToLCDTextEnabled() const;

    bool rootShouldAlwaysComposite() const;

    // Copy the accelerated compositing related flags from Settings
    void updateAcceleratedCompositingSettings();

    // Used to indicate that a compositing update will be needed for the next frame that gets drawn.
    void setNeedsCompositingUpdate(CompositingUpdateType);

    void didLayout();

    // Whether layer's compositedLayerMapping needs a GraphicsLayer to clip z-order children of the given Layer.
    bool clipsCompositingDescendants(const PaintLayer*) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const PaintLayer*) const;

    bool supportsFixedRootBackgroundCompositing() const;
    bool needsFixedRootBackgroundLayer(const PaintLayer*) const;
    GraphicsLayer* fixedRootBackgroundLayer() const;
    void setNeedsUpdateFixedBackground() { m_needsUpdateFixedBackground = true; }

    // Issue paint invalidations of the appropriate layers when the given Layer starts or stops being composited.
    void paintInvalidationOnCompositingChange(PaintLayer*);

    void fullyInvalidatePaint();

    PaintLayer* rootLayer() const;
    GraphicsLayer* rootGraphicsLayer() const;
    GraphicsLayer* frameScrollLayer() const;
    GraphicsLayer* scrollLayer() const;
    GraphicsLayer* containerLayer() const;

    enum RootLayerAttachment {
        RootLayerUnattached,
        RootLayerAttachedViaChromeClient,
        RootLayerAttachedViaEnclosingFrame
    };

    RootLayerAttachment getRootLayerAttachment() const { return m_rootLayerAttachment; }
    void updateRootLayerAttachment();
    void updateRootLayerPosition();

    void setIsInWindow(bool);

    static PaintLayerCompositor* frameContentsCompositor(LayoutPart*);
    // Return true if the layers changed.
    static bool attachFrameContentLayersToIframeLayer(LayoutPart*);

    // Update the geometry of the layers used for clipping and scrolling in frames.
    void frameViewDidChangeLocation(const IntPoint& contentsOffset);
    void frameViewDidChangeSize();
    void frameViewDidScroll();
    void frameViewScrollbarsExistenceDidChange();
    void rootFixedBackgroundsChanged();

    bool scrollingLayerDidChange(PaintLayer*);

    PassRefPtr<JSONObject> layerTreeAsJSON(LayerTreeFlags) const;

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    void setTracksPaintInvalidations(bool);

    String debugName(const GraphicsLayer*) const override;
    DocumentLifecycle& lifecycle() const;

    bool needsUpdateDescendantDependentFlags() const { return m_needsUpdateDescendantDependentFlags; }
    void setNeedsUpdateDescendantDependentFlags() { m_needsUpdateDescendantDependentFlags = true; }

    void updatePotentialCompositingReasonsFromStyle(PaintLayer*);

    // Whether the layer could ever be composited.
    bool canBeComposited(const PaintLayer*) const;

    // FIXME: Move allocateOrClearCompositedLayerMapping to CompositingLayerAssigner once we've fixed
    // the compositing chicken/egg issues.
    bool allocateOrClearCompositedLayerMapping(PaintLayer*, CompositingStateTransitionType compositedLayerUpdate);

    void updateDirectCompositingReasons(PaintLayer*);

    bool inOverlayFullscreenVideo() const { return m_inOverlayFullscreenVideo; }

private:
#if ENABLE(ASSERT)
    void assertNoUnresolvedDirtyBits();
#endif

    void updateIfNeededRecursiveInternal();

    // GraphicsLayerClient implementation
    bool needsRepaint(const GraphicsLayer&) const { return true; }
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& interestRect) const override;

    bool isTrackingPaintInvalidations() const override;

    void updateWithoutAcceleratedCompositing(CompositingUpdateType);
    void updateIfNeeded();

    void ensureRootLayer();
    void destroyRootLayer();

    void attachRootLayer(RootLayerAttachment);
    void detachRootLayer();

    void attachCompositorTimeline();
    void detachCompositorTimeline();

    void updateOverflowControlsLayers();

    Page* page() const;

    ScrollingCoordinator* scrollingCoordinator() const;

    void enableCompositingModeIfNeeded();

    bool requiresHorizontalScrollbarLayer() const;
    bool requiresVerticalScrollbarLayer() const;
    bool requiresScrollCornerLayer() const;

    void applyOverlayFullscreenVideoAdjustmentIfNeeded();

    // Checks the given graphics layer against the compositor's horizontal and vertical scrollbar
    // graphics layers, returning the associated Scrollbar instance if any, else nullptr.
    Scrollbar* graphicsLayerToScrollbar(const GraphicsLayer*) const;

    LayoutView& m_layoutView;
    std::unique_ptr<GraphicsLayer> m_rootContentLayer;

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
    bool m_isTrackingPaintInvalidations; // Used for testing.
    bool m_inOverlayFullscreenVideo;
    bool m_needsUpdateDescendantDependentFlags;

    RootLayerAttachment m_rootLayerAttachment;

    // Enclosing container layer, which clips for iframe content
    std::unique_ptr<GraphicsLayer> m_containerLayer;
    std::unique_ptr<GraphicsLayer> m_scrollLayer;

    // Enclosing layer for overflow controls and the clipping layer
    std::unique_ptr<GraphicsLayer> m_overflowControlsHostLayer;

    // Layers for overflow controls
    std::unique_ptr<GraphicsLayer> m_layerForHorizontalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForVerticalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForScrollCorner;
};

} // namespace blink

#endif // PaintLayerCompositor_h
