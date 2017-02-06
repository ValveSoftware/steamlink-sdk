/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef CompositedLayerMapping_h
#define CompositedLayerMapping_h

#include "core/layout/compositing/GraphicsLayerUpdater.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class PaintLayerCompositor;

// A GraphicsLayerPaintInfo contains all the info needed to paint a partial subtree of Layers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    PaintLayer* paintLayer;

    LayoutRect compositedBounds;

    // The clip rect to apply, in the local coordinate space of the squashed layer, when painting it.
    IntRect localClipRectForSquashedLayer;

    // Offset describing where this squashed Layer paints into the shared GraphicsLayer backing.
    IntSize offsetFromLayoutObject;
    bool offsetFromLayoutObjectSet;

    GraphicsLayerPaintInfo() : paintLayer(nullptr), offsetFromLayoutObjectSet(false) { }
};

enum GraphicsLayerUpdateScope {
    GraphicsLayerUpdateNone,
    GraphicsLayerUpdateLocal,
    GraphicsLayerUpdateSubtree,
};

// CompositedLayerMapping keeps track of how PaintLayers correspond to
// GraphicsLayers of the composited layer tree. Each instance of CompositedLayerMapping
// manages a small cluster of GraphicsLayers and the references to which Layers
// and paint phases contribute to each GraphicsLayer.
//
// - If a PaintLayer is composited,
//   - if it paints into its own backings (GraphicsLayers), it owns a
//     CompositedLayerMapping (PaintLayer::compositedLayerMapping()) to keep
//     track the backings;
//   - if it paints into grouped backing (i.e. it's squashed), it has a pointer
//     (PaintLayer::groupedMapping()) to the CompositedLayerMapping into which
//     the PaintLayer is squashed;
// - Otherwise the PaintLayer doesn't own or directly reference any CompositedLayerMapping.
class CORE_EXPORT CompositedLayerMapping final : public GraphicsLayerClient {

    WTF_MAKE_NONCOPYABLE(CompositedLayerMapping); USING_FAST_MALLOC(CompositedLayerMapping);
public:
    explicit CompositedLayerMapping(PaintLayer&);
    ~CompositedLayerMapping() override;

    PaintLayer& owningLayer() const { return m_owningLayer; }

    bool updateGraphicsLayerConfiguration();
    void updateGraphicsLayerGeometry(const PaintLayer* compositingContainer, const PaintLayer* compositingStackingContext, Vector<PaintLayer*>& layersNeedingPaintInvalidation);

    // Update whether layer needs blending.
    void updateContentsOpaque();

    GraphicsLayer* mainGraphicsLayer() const { return m_graphicsLayer.get(); }

    // Layer to clip children
    bool hasClippingLayer() const { return m_childContainmentLayer.get(); }
    GraphicsLayer* clippingLayer() const { return m_childContainmentLayer.get(); }

    // Layer to get clipped by ancestor
    bool hasAncestorClippingLayer() const { return m_ancestorClippingLayer.get(); }
    GraphicsLayer* ancestorClippingLayer() const { return m_ancestorClippingLayer.get(); }

    GraphicsLayer* foregroundLayer() const { return m_foregroundLayer.get(); }

    GraphicsLayer* backgroundLayer() const { return m_backgroundLayer.get(); }
    bool backgroundLayerPaintsFixedRootBackground() const { return m_backgroundLayerPaintsFixedRootBackground; }

    bool hasScrollingLayer() const { return m_scrollingLayer.get(); }
    GraphicsLayer* scrollingLayer() const { return m_scrollingLayer.get(); }
    GraphicsLayer* scrollingContentsLayer() const { return m_scrollingContentsLayer.get(); }

    bool hasMaskLayer() const { return m_maskLayer.get(); }
    GraphicsLayer* maskLayer() const { return m_maskLayer.get(); }

    bool hasChildClippingMaskLayer() const { return m_childClippingMaskLayer.get(); }
    GraphicsLayer* childClippingMaskLayer() const { return m_childClippingMaskLayer.get(); }

    GraphicsLayer* parentForSublayers() const;
    GraphicsLayer* childForSuperlayers() const;
    void setSublayers(const GraphicsLayerVector&);

    bool hasChildTransformLayer() const { return m_childTransformLayer.get(); }
    GraphicsLayer* childTransformLayer() const { return m_childTransformLayer.get(); }

    GraphicsLayer* squashingContainmentLayer() const { return m_squashingContainmentLayer.get(); }
    GraphicsLayer* squashingLayer() const { return m_squashingLayer.get(); }

    void setSquashingContentsNeedDisplay();
    void setContentsNeedDisplay();
    // LayoutRect is in the coordinate space of the layer's layout object.
    void setContentsNeedDisplayInRect(const LayoutRect&, PaintInvalidationReason, const DisplayItemClient&);
    // Invalidates just the non-scrolling content layers.
    void setNonScrollingContentsNeedDisplayInRect(const LayoutRect&, PaintInvalidationReason, const DisplayItemClient&);
    // Invalidates just scrolling content layers.
    void setScrollingContentsNeedDisplayInRect(const LayoutRect&, PaintInvalidationReason, const DisplayItemClient&);

    // Notification from the layoutObject that its content changed.
    void contentChanged(ContentChangeType);

    LayoutRect compositedBounds() const { return m_compositedBounds; }
    IntRect pixelSnappedCompositedBounds() const;

    void positionOverflowControlsLayers();

    // Returns true if the assignment actually changed the assigned squashing layer.
    bool updateSquashingLayerAssignment(PaintLayer* squashedLayer, size_t nextSquashedLayerIndex);
    void removeLayerFromSquashingGraphicsLayer(const PaintLayer*);
#if ENABLE(ASSERT)
    bool verifyLayerInSquashingVector(const PaintLayer*);
#endif

    void finishAccumulatingSquashingLayers(size_t nextSquashedLayerIndex, Vector<PaintLayer*>& layersNeedingPaintInvalidation);
    void updateRenderingContext();
    void updateShouldFlattenTransform();
    void updateElementIdAndCompositorMutableProperties();

    // GraphicsLayerClient interface
    void notifyFirstPaint() override;
    void notifyFirstTextPaint() override;
    void notifyFirstImagePaint() override;

    IntRect computeInterestRect(const GraphicsLayer*, const IntRect& previousInterestRect) const override;
    LayoutSize subpixelAccumulation() const final;
    bool needsRepaint(const GraphicsLayer&) const override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& interestRect) const override;

    bool isTrackingPaintInvalidations() const override;

#if ENABLE(ASSERT)
    void verifyNotPainting() override;
#endif

    LayoutRect contentsBox() const;

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    // Returns true if the overflow controls cannot be positioned within this
    // CLM's internal hierarchy without incorrectly stacking under some
    // scrolling content. If this returns true, these controls must be
    // repositioned in the graphics layer tree to ensure that they stack above
    // scrolling content.
    bool needsToReparentOverflowControls() const;

    // Removes the overflow controls host layer from its parent and positions it
    // so that it can be inserted as a sibling to this CLM without changing
    // position.
    GraphicsLayer* detachLayerForOverflowControls(const PaintLayer& enclosingLayer);

    void updateFilters(const ComputedStyle&);
    void updateBackdropFilters(const ComputedStyle&);

    void setBlendMode(WebBlendMode);

    bool needsGraphicsLayerUpdate() { return m_pendingUpdateScope > GraphicsLayerUpdateNone; }
    void setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateScope scope) { m_pendingUpdateScope = std::max(static_cast<GraphicsLayerUpdateScope>(m_pendingUpdateScope), scope); }
    void clearNeedsGraphicsLayerUpdate() { m_pendingUpdateScope = GraphicsLayerUpdateNone; }

    GraphicsLayerUpdater::UpdateType updateTypeForChildren(GraphicsLayerUpdater::UpdateType) const;

#if ENABLE(ASSERT)
    void assertNeedsToUpdateGraphicsLayerBitsCleared() {  ASSERT(m_pendingUpdateScope == GraphicsLayerUpdateNone); }
#endif

    String debugName(const GraphicsLayer*) const override;

    LayoutSize contentOffsetInCompositingLayer() const;

    LayoutPoint squashingOffsetFromTransformedAncestor()
    {
        return m_squashingLayerOffsetFromTransformedAncestor;
    }

    // If there is a squashed layer painting into this CLM that is an ancestor of the given LayoutObject, return it. Otherwise return nullptr.
    const GraphicsLayerPaintInfo* containingSquashedLayer(const LayoutObject*, unsigned maxSquashedLayerIndex);

    void updateScrollingBlockSelection();

    void adjustForCompositedScrolling(const GraphicsLayer*, IntSize& offset) const;

private:
    IntRect recomputeInterestRect(const GraphicsLayer*) const;
    static bool interestRectChangedEnoughToRepaint(const IntRect& previousInterestRect, const IntRect& newInterestRect, const IntSize& layerSize);

    static const GraphicsLayerPaintInfo* containingSquashedLayer(const LayoutObject*,  const Vector<GraphicsLayerPaintInfo>& layers, unsigned maxSquashedLayerIndex);

    // Paints the scrollbar part associated with the given graphics layer into the given context.
    void paintScrollableArea(const GraphicsLayer*, GraphicsContext&, const IntRect& interestRect) const;
    // Returns whether the given layer is part of the scrollable area, if any, associated with this mapping.
    bool isScrollableAreaLayer(const GraphicsLayer*) const;

    // Helper methods to updateGraphicsLayerGeometry:
    void computeGraphicsLayerParentLocation(const PaintLayer* compositingContainer, const IntRect& ancestorCompositingBounds, IntPoint& graphicsLayerParentLocation);
    void updateSquashingLayerGeometry(const IntPoint& graphicsLayerParentLocation, const PaintLayer* compositingContainer, Vector<GraphicsLayerPaintInfo>& layers, GraphicsLayer*, LayoutPoint* offsetFromTransformedAncestor, Vector<PaintLayer*>& layersNeedingPaintInvalidation);
    void updateMainGraphicsLayerGeometry(const IntRect& relativeCompositingBounds, const IntRect& localCompositingBounds, const IntPoint& graphicsLayerParentLocation);
    void updateAncestorClippingLayerGeometry(const PaintLayer* compositingContainer, const IntPoint& snappedOffsetFromCompositedAncestor, IntPoint& graphicsLayerParentLocation);
    void updateOverflowControlsHostLayerGeometry(const PaintLayer* compositingStackingContext, const PaintLayer* compositingContainer);
    void updateChildContainmentLayerGeometry(const IntRect& clippingBox, const IntRect& localCompositingBounds);
    void updateChildTransformLayerGeometry();
    void updateMaskLayerGeometry();
    void updateTransformGeometry(const IntPoint& snappedOffsetFromCompositedAncestor, const IntRect& relativeCompositingBounds);
    void updateForegroundLayerGeometry(const FloatSize& relativeCompositingBoundsSize, const IntRect& clippingBox);
    void updateBackgroundLayerGeometry(const FloatSize& relativeCompositingBoundsSize);
    void updateReflectionLayerGeometry(Vector<PaintLayer*>& layersNeedingPaintInvalidation);
    void updateScrollingLayerGeometry(const IntRect& localCompositingBounds);
    void updateChildClippingMaskLayerGeometry();

    void createPrimaryGraphicsLayer();
    void destroyGraphicsLayers();

    std::unique_ptr<GraphicsLayer> createGraphicsLayer(CompositingReasons, SquashingDisallowedReasons = SquashingDisallowedReasonsNone);
    bool toggleScrollbarLayerIfNeeded(std::unique_ptr<GraphicsLayer>&, bool needsLayer, CompositingReasons);

    LayoutBoxModelObject* layoutObject() const { return m_owningLayer.layoutObject(); }
    PaintLayerCompositor* compositor() const { return m_owningLayer.compositor(); }

    void updateInternalHierarchy();
    void updatePaintingPhases();
    bool updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip);
    bool updateChildTransformLayer(bool needsChildTransformLayer);
    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer, bool needsAncestorClip);
    bool updateForegroundLayer(bool needsForegroundLayer);
    bool updateBackgroundLayer(bool needsBackgroundLayer);
    bool updateMaskLayer(bool needsMaskLayer);
    void updateChildClippingMaskLayer(bool needsChildClippingMaskLayer);
    bool requiresHorizontalScrollbarLayer() const { return m_owningLayer.getScrollableArea() && m_owningLayer.getScrollableArea()->horizontalScrollbar(); }
    bool requiresVerticalScrollbarLayer() const { return m_owningLayer.getScrollableArea() && m_owningLayer.getScrollableArea()->verticalScrollbar(); }
    bool requiresScrollCornerLayer() const { return m_owningLayer.getScrollableArea() && !m_owningLayer.getScrollableArea()->scrollCornerAndResizerRect().isEmpty(); }
    bool updateScrollingLayers(bool scrollingLayers);
    void updateScrollParent(const PaintLayer*);
    void updateClipParent(const PaintLayer* scrollParent);
    bool updateSquashingLayers(bool needsSquashingLayers);
    void updateDrawsContent();
    void updateChildrenTransform();
    void updateCompositedBounds();
    void registerScrollingLayers();

    // Also sets subpixelAccumulation on the layer.
    void computeBoundsOfOwningLayer(const PaintLayer* compositedAncestor, IntRect& localCompositingBounds, IntRect& compositingBoundsRelativeToCompositedAncestor, LayoutPoint& offsetFromCompositedAncestor, IntPoint& snappedOffsetFromCompositedAncestor);

    void setBackgroundLayerPaintsFixedRootBackground(bool);

    GraphicsLayerPaintingPhase paintingPhaseForPrimaryLayer() const;

    // Result is transform origin in pixels.
    FloatPoint3D computeTransformOrigin(const IntRect& borderBox) const;

    void updateOpacity(const ComputedStyle&);
    void updateTransform(const ComputedStyle&);
    void updateLayerBlendMode(const ComputedStyle&);
    void updateIsRootForIsolatedGroup();
    // Return the opacity value that this layer should use for compositing.
    float compositingOpacity(float layoutObjectOpacity) const;

    bool paintsChildren() const;

    // Returns true if this layer has content that needs to be displayed by painting into the backing store.
    bool containsPaintedContent() const;
    // Returns true if the Layer just contains an image that we can composite directly.
    bool isDirectlyCompositedImage() const;
    void updateImageContents();

    Color layoutObjectBackgroundColor() const;
    void updateBackgroundColor();
    void updateContentsRect();
    void updateContentsOffsetInCompositingLayer(const IntPoint& snappedOffsetFromCompositedAncestor, const IntPoint& graphicsLayerParentLocation);
    void updateAfterPartResize();
    void updateCompositingReasons();

    static bool hasVisibleNonCompositingDescendant(PaintLayer* parent);

    void doPaintTask(const GraphicsLayerPaintInfo&, const GraphicsLayer&, const PaintLayerFlags&, GraphicsContext&, const IntRect& clip) const;

    // Computes the background clip rect for the given squashed layer, up to any containing layer that is squashed into the
    // same squashing layer and contains this squashed layer's clipping ancestor.
    // The clip rect is returned in the coordinate space of the given squashed layer.
    // If there is no such containing layer, returns the infinite rect.
    // FIXME: unify this code with the code that sets up m_ancestorClippingLayer. They are doing very similar things.
    static IntRect localClipRectForSquashedLayer(const PaintLayer& referenceLayer, const GraphicsLayerPaintInfo&,  const Vector<GraphicsLayerPaintInfo>& layers);

    // Return true if |m_owningLayer|'s compositing ancestor is not a descendant (inclusive) of the
    // clipping container for |m_owningLayer|.
    bool owningLayerClippedByLayerNotAboveCompositedAncestor(const PaintLayer* scrollParent);

    const PaintLayer* scrollParent();

    // Clear the groupedMapping entry on the layer at the given index, only if that layer does
    // not appear earlier in the set of layers for this object.
    bool invalidateLayerIfNoPrecedingEntry(size_t);

    PaintLayer& m_owningLayer;

    // The hierarchy of layers that is maintained by the CompositedLayerMapping looks like this:
    //
    //  + m_ancestorClippingLayer [OPTIONAL]
    //    + m_graphicsLayer
    //      + m_childTransformLayer [OPTIONAL]
    //      | + m_childContainmentLayer [OPTIONAL] <-OR-> m_scrollingLayer [OPTIONAL]
    //      |                                             + m_scrollingContentsLayer [Present iff m_scrollingLayer is present]
    //      + m_overflowControlsAncestorClippingLayer [OPTIONAL] // *The overflow controls may need to be repositioned in the
    //        + m_overflowControlsHostLayer [OPTIONAL]           //  graphics layer tree by the RLC to ensure that they stack
    //          + m_layerForVerticalScrollbar [OPTIONAL]         //  above scrolling content.
    //          + m_layerForHorizontalScrollbar [OPTIONAL]
    //          + m_layerForScrollCorner [OPTIONAL]
    //
    // We need an ancestor clipping layer if our clipping ancestor is not our ancestor in the
    // clipping tree. Here's what that might look like.
    //
    // Let A = the clipping ancestor,
    //     B = the clip descendant, and
    //     SC = the stacking context that is the ancestor of A and B in the stacking tree.
    //
    // SC
    //  + A = m_graphicsLayer
    //  |  + m_childContainmentLayer
    //  |     + ...
    //  ...
    //  |
    //  + B = m_ancestorClippingLayer [+]
    //     + m_graphicsLayer
    //        + ...
    //
    // In this case B is clipped by another layer that doesn't happen to be its ancestor: A.
    // So we create an ancestor clipping layer for B, [+], which ensures that B is clipped
    // as if it had been A's descendant.
    std::unique_ptr<GraphicsLayer> m_ancestorClippingLayer; // Only used if we are clipped by an ancestor which is not a stacking context.
    std::unique_ptr<GraphicsLayer> m_graphicsLayer;
    std::unique_ptr<GraphicsLayer> m_childContainmentLayer; // Only used if we have clipping on a stacking context with compositing children.
    std::unique_ptr<GraphicsLayer> m_childTransformLayer; // Only used if we have perspective.
    std::unique_ptr<GraphicsLayer> m_scrollingLayer; // Only used if the layer is using composited scrolling.
    std::unique_ptr<GraphicsLayer> m_scrollingContentsLayer; // Only used if the layer is using composited scrolling.

    // This layer is also added to the hierarchy by the RLB, but in a different way than
    // the layers above. It's added to m_graphicsLayer as its mask layer (naturally) if
    // we have a mask, and isn't part of the typical hierarchy (it has no children).
    std::unique_ptr<GraphicsLayer> m_maskLayer; // Only used if we have a mask.
    std::unique_ptr<GraphicsLayer> m_childClippingMaskLayer; // Only used if we have to clip child layers or accelerated contents with border radius or clip-path.

    // There are two other (optional) layers whose painting is managed by the CompositedLayerMapping,
    // but whose position in the hierarchy is maintained by the PaintLayerCompositor. These
    // are the foreground and background layers. The foreground layer exists if we have composited
    // descendants with negative z-order. We need the extra layer in this case because the layer
    // needs to draw both below (for the background, say) and above (for the normal flow content, say)
    // the negative z-order descendants and this is impossible with a single layer. The RLC handles
    // inserting m_foregroundLayer in the correct position in our descendant list for us (right after
    // the neg z-order dsecendants).
    //
    // The background layer is only created if this is the root layer and our background is entirely
    // fixed. In this case we want to put the background in a separate composited layer so that when
    // we scroll, we don't have to re-raster the background into position. This layer is also inserted
    // into the tree by the RLC as it gets a special home. This layer becomes a descendant of the
    // frame clipping layer. That is:
    //   ...
    //     + frame clipping layer
    //       + m_backgroundLayer
    //       + frame scrolling layer
    //         + root content layer
    //
    // With the hierarchy set up like this, the root content layer is able to scroll without affecting
    // the background layer (or paint invalidation).
    std::unique_ptr<GraphicsLayer> m_foregroundLayer; // Only used in cases where we need to draw the foreground separately.
    std::unique_ptr<GraphicsLayer> m_backgroundLayer; // Only used in cases where we need to draw the background separately.

    std::unique_ptr<GraphicsLayer> m_layerForHorizontalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForVerticalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForScrollCorner;

    // This layer contains the scrollbar and scroll corner layers and clips them to the border box
    // bounds of our LayoutObject. It is usually added to m_graphicsLayer, but may be reparented by
    // GraphicsLayerTreeBuilder to ensure that scrollbars appear above scrolling content.
    std::unique_ptr<GraphicsLayer> m_overflowControlsHostLayer;

    // The reparented overflow controls sometimes need to be clipped by a non-ancestor. In just the same
    // way we need an ancestor clipping layer to clip this CLM's internal hierarchy, we add another layer
    // to clip the overflow controls. We could combine this with m_overflowControlsHostLayer, but that
    // would require manually intersecting their clips, and shifting the overflow controls to compensate
    // for this clip's offset. By using a separate layer, the overflow controls can remain ignorant of
    // ancestor clipping.
    std::unique_ptr<GraphicsLayer> m_overflowControlsAncestorClippingLayer;

    // A squashing CLM has two possible squashing-related structures.
    //
    // If m_ancestorClippingLayer is present:
    //
    // m_ancestorClippingLayer
    //   + m_graphicsLayer
    //   + m_squashingLayer
    //
    // If not:
    //
    // m_squashingContainmentLayer
    //   + m_graphicsLayer
    //   + m_squashingLayer
    //
    // Stacking children of a squashed layer receive graphics layers that are parented to the compositd ancestor of the
    // squashed layer (i.e. nearest enclosing composited layer that is not squashed).
    std::unique_ptr<GraphicsLayer> m_squashingContainmentLayer; // Only used if any squashed layers exist and m_squashingContainmentLayer is not present, to contain the squashed layers as siblings to the rest of the GraphicsLayer tree chunk.
    std::unique_ptr<GraphicsLayer> m_squashingLayer; // Only used if any squashed layers exist, this is the backing that squashed layers paint into.
    Vector<GraphicsLayerPaintInfo> m_squashedLayers;
    LayoutPoint m_squashingLayerOffsetFromTransformedAncestor;

    LayoutRect m_compositedBounds;

    LayoutSize m_contentOffsetInCompositingLayer;

    // We keep track of the scrolling contents offset, so that when it changes we can notify the ScrollingCoordinator, which
    // passes on main-thread scrolling updates to the compositor.
    DoubleSize m_scrollingContentsOffset;

    unsigned m_contentOffsetInCompositingLayerDirty : 1;

    unsigned m_pendingUpdateScope : 2;
    unsigned m_isMainFrameLayoutViewLayer : 1;

    unsigned m_backgroundLayerPaintsFixedRootBackground : 1;
    unsigned m_scrollingContentsAreEmpty : 1;

    friend class CompositedLayerMappingTest;
};

} // namespace blink

#endif // CompositedLayerMapping_h
