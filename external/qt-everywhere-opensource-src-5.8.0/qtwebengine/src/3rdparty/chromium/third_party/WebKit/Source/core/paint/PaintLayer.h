/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef PaintLayer_h
#define PaintLayer_h

#include "core/CoreExport.h"
#include "core/layout/ClipRectsCache.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintLayerClipper.h"
#include "core/paint/PaintLayerFilterInfo.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/paint/PaintLayerReflectionInfo.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/PaintLayerStackingNode.h"
#include "core/paint/PaintLayerStackingNodeIterator.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/SquashingDisallowedReasons.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CompositedLayerMapping;
class ComputedStyle;
class FilterEffectBuilder;
class FilterOperations;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class PaintLayerCompositor;
class PaintTiming;
class TransformationMatrix;

enum IncludeSelfOrNot { IncludeSelf, ExcludeSelf };

enum CompositingQueryMode {
    CompositingQueriesAreAllowed,
    CompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases
};

// FIXME: remove this once the compositing query ASSERTS are no longer hit.
class CORE_EXPORT DisableCompositingQueryAsserts {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(DisableCompositingQueryAsserts);
public:
    DisableCompositingQueryAsserts();
private:
    TemporaryChange<CompositingQueryMode> m_disabler;
};

struct PaintLayerRareData {
    PaintLayerRareData();
    ~PaintLayerRareData();

    // Our current relative position offset.
    LayoutSize offsetForInFlowPosition;

    std::unique_ptr<TransformationMatrix> transform;

    // Pointer to the enclosing Layer that caused us to be paginated. It is 0 if we are not paginated.
    //
    // See LayoutMultiColumnFlowThread and
    // https://sites.google.com/a/chromium.org/dev/developers/design-documents/multi-column-layout
    // for more information about the multicol implementation. It's important to understand the
    // difference between flow thread coordinates and visual coordinates when working with multicol
    // in Layer, since Layer is one of the few places where we have to worry about the
    // visual ones. Internally we try to use flow-thread coordinates whenever possible.
    PaintLayer* enclosingPaginationLayer;

    // These compositing reasons are updated whenever style changes, not while updating compositing layers.
    // They should not be used to infer the compositing state of this layer.
    CompositingReasons potentialCompositingReasonsFromStyle;

    // Once computed, indicates all that a layer needs to become composited using the CompositingReasons enum bitfield.
    CompositingReasons compositingReasons;

    // This captures reasons why a paint layer might be forced to be separately
    // composited rather than sharing a backing with another layer.
    SquashingDisallowedReasons squashingDisallowedReasons;

    // If the layer paints into its own backings, this keeps track of the backings.
    // It's nullptr if the layer is not composited or paints into grouped backing.
    std::unique_ptr<CompositedLayerMapping> compositedLayerMapping;

    // If the layer paints into grouped backing (i.e. squashed), this points to the
    // grouped CompositedLayerMapping. It's null if the layer is not composited or
    // paints into its own backing.
    CompositedLayerMapping* groupedMapping;

    std::unique_ptr<PaintLayerReflectionInfo> reflectionInfo;

    Persistent<PaintLayerFilterInfo> filterInfo;

    // The accumulated subpixel offset of a composited layer's composited bounds compared to absolute coordinates.
    LayoutSize subpixelAccumulation;
};

// PaintLayer is an old object that handles lots of unrelated operations.
//
// We want it to die at some point and be replaced by more focused objects,
// which would remove (or at least compartimentalize) a lot of complexity.
// See the STATUS OF PAINTLAYER section below.
//
// The class is central to painting and hit-testing. That's because it handles
// a lot of tasks (we included ones done by associated satellite objects for
// historical reasons):
// - Complex painting operations (opacity, clipping, filters, reflections, ...).
// - hardware acceleration (through PaintLayerCompositor).
// - scrolling (through PaintLayerScrollableArea).
// - some performance optimizations.
//
// The compositing code is also based on PaintLayer. The entry to it is the
// PaintLayerCompositor, which fills |m_compositedLayerMapping| for hardware
// accelerated layers.
//
// TODO(jchaffraix): Expand the documentation about hardware acceleration.
//
//
// ***** SELF-PAINTING LAYER *****
// One important concept about PaintLayer is "self-painting"
// (m_isSelfPaintingLayer).
// PaintLayer started as the implementation of a stacking context. This meant
// that we had to use PaintLayer's painting order (the code is now in
// PaintLayerPainter and PaintLayerStackingNode) instead of the LayoutObject's
// children order. Over the years, as more operations were handled by
// PaintLayer, some LayoutObjects that were not stacking context needed to have
// a PaintLayer for bookkeeping reasons. One such example is the overflow hidden
// case that wanted hardware acceleration and thus had to allocate a PaintLayer
// to get it. However overflow hidden is something LayoutObject can paint
// without a PaintLayer, which includes a lot of painting overhead. Thus the
// self-painting flag was introduced. The flag is a band-aid solution done for
// performance reason only. It just brush over the underlying problem, which is
// that its design doesn't match the system's requirements anymore.
//
// Note that the self-painting flag determines how we paint a LayoutObject:
// - If the flag is true, the LayoutObject is painted through its PaintLayer,
//   which is required to apply complex paint operations. The paint order is
//   handled by PaintLayerPainter::paintChildren, where we look at children
//   PaintLayers.
// - If the flag is false, the LayoutObject is painted like normal children (ie
//   as if it didn't have a PaintLayer). The paint order is handled by
//   BlockPainter::paintChild that looks at children LayoutObjects.
// This means that the self-painting flag changes the painting order in a subtle
// way, which can potentially have visible consequences. Those bugs are called
// painting inversion as we invert the order of painting for 2 elements
// (painting one wrongly in front of the other).
// See https://crbug.com/370604 for an example.
//
//
// ***** STATUS OF PAINTLAYER *****
// We would like to remove this class in the future. The reasons for the removal
// are:
// - it has been a dumping ground for features for too long.
// - it is the wrong level of abstraction, bearing no correspondence to any CSS
//   concept.
//
// Its features need to be migrated to helper objects. This was started with the
// introduction of satellite objects: PaintLayer*. Those helper objects then
// need to be moved to the appropriate LayoutObject class, probably to a rare
// data field to avoid growing all the LayoutObjects.
//
// A good example of this is PaintLayerScrollableArea, which can only happen
// be instanciated for LayoutBoxes. With the current design, it's hard to know
// that by reading the code.
class CORE_EXPORT PaintLayer : public DisplayItemClient {
    WTF_MAKE_NONCOPYABLE(PaintLayer);
public:
    PaintLayer(LayoutBoxModelObject*);
    ~PaintLayer() override;

    // DisplayItemClient methods
    String debugName() const final;
    LayoutRect visualRect() const final;

    LayoutBoxModelObject* layoutObject() const { return m_layoutObject; }
    LayoutBox* layoutBox() const { return m_layoutObject && m_layoutObject->isBox() ? toLayoutBox(m_layoutObject) : 0; }
    PaintLayer* parent() const { return m_parent; }
    PaintLayer* previousSibling() const { return m_previous; }
    PaintLayer* nextSibling() const { return m_next; }
    PaintLayer* firstChild() const { return m_first; }
    PaintLayer* lastChild() const { return m_last; }

    // TODO(wangxianzhu): Find a better name for it. 'paintContainer' might be good but
    // we can't use it for now because it conflicts with PaintInfo::paintContainer.
    PaintLayer* compositingContainer() const;

    void addChild(PaintLayer* newChild, PaintLayer* beforeChild = 0);
    PaintLayer* removeChild(PaintLayer*);

    void removeOnlyThisLayerAfterStyleChange();
    void insertOnlyThisLayerAfterStyleChange();

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle);

    // FIXME: Many people call this function while it has out-of-date information.
    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }

    // PaintLayers which represent LayoutParts may become self-painting due to being composited.
    // If this is the case, this method returns true.
    bool isSelfPaintingOnlyBecauseIsCompositedPart() const;

    bool isTransparent() const { return layoutObject()->isTransparent() || layoutObject()->style()->hasBlendMode() || layoutObject()->hasMask(); }

    bool isReflection() const { return layoutObject()->isReplica(); }
    PaintLayerReflectionInfo* reflectionInfo() { return m_rareData ? m_rareData->reflectionInfo.get() : nullptr; }
    const PaintLayerReflectionInfo* reflectionInfo() const { return const_cast<PaintLayer*>(this)->reflectionInfo(); }

    const PaintLayer* root() const
    {
        const PaintLayer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }

    const LayoutPoint& location() const { ASSERT(!m_needsPositionUpdate); return m_location; }
    // FIXME: size() should ASSERT(!m_needsPositionUpdate) as well, but that fails in some tests,
    // for example, fast/repaint/clipped-relative.html.
    const IntSize& size() const { return m_size; }
    void setSizeHackForLayoutTreeAsText(const IntSize& size) { m_size = size; }

    LayoutRect rect() const { return LayoutRect(location(), LayoutSize(size())); }

    bool isRootLayer() const { return m_isRootLayer; }

    PaintLayerCompositor* compositor() const;

    // Notification from the layoutObject that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without invalidating paint.
    void contentChanged(ContentChangeType);

    void updateLayerPosition();

    void updateLayerPositionsAfterLayout();
    void updateLayerPositionsAfterOverflowScroll(const DoubleSize& scrollDelta);

    PaintLayer* enclosingPaginationLayer() const { return m_rareData ? m_rareData->enclosingPaginationLayer : nullptr; }

    void updateTransformationMatrix();
    PaintLayer* renderingContextRoot();
    const PaintLayer* renderingContextRoot() const;

    LayoutSize offsetForInFlowPosition() const { return m_rareData ? m_rareData->offsetForInFlowPosition : LayoutSize(); }

    PaintLayerStackingNode* stackingNode() { return m_stackingNode.get(); }
    const PaintLayerStackingNode* stackingNode() const { return m_stackingNode.get(); }

    bool subtreeIsInvisible() const { return !hasVisibleContent() && !hasVisibleDescendant(); }

    // FIXME: hasVisibleContent() should call updateDescendantDependentFlags() if m_visibleContentStatusDirty.
    bool hasVisibleContent() const { ASSERT(!m_visibleContentStatusDirty); return m_hasVisibleContent; }

    // FIXME: hasVisibleDescendant() should call updateDescendantDependentFlags() if m_visibleDescendantStatusDirty.
    bool hasVisibleDescendant() const { ASSERT(!m_visibleDescendantStatusDirty); return m_hasVisibleDescendant; }

    void dirtyVisibleContentStatus();
    void potentiallyDirtyVisibleContentStatus(EVisibility);

    bool hasBoxDecorationsOrBackground() const;
    bool hasVisibleBoxDecorations() const;
    // True if this layer container layoutObjects that paint.
    bool hasNonEmptyChildLayoutObjects() const;

    // Will ensure that isAllScrollingContentComposited() is up to date.
    void updateScrollingStateAfterCompositingChange();
    bool isAllScrollingContentComposited() const { return m_isAllScrollingContentComposited; }

    // Gets the ancestor layer that serves as the containing block of this layer. This is either
    // another out of flow positioned layer, or one that contains paint.
    // If |ancestor| is specified, |*skippedAncestor| will be set to true if |ancestor| is found in
    // the ancestry chain between this layer and the containing block layer; if not found, it will
    // be set to false. Either both |ancestor| and |skippedAncestor| should be nullptr, or none of
    // them should.
    PaintLayer* containingLayerForOutOfFlowPositioned(const PaintLayer* ancestor = nullptr, bool* skippedAncestor = nullptr) const;

    bool isPaintInvalidationContainer() const;

    // Do *not* call this method unless you know what you are dooing. You probably want to call enclosingCompositingLayerForPaintInvalidation() instead.
    // If includeSelf is true, may return this.
    PaintLayer* enclosingLayerWithCompositedLayerMapping(IncludeSelfOrNot) const;

    // Returns the enclosing layer root into which this layer paints, inclusive of this one. Note that the enclosing layer may or may not have its own
    // GraphicsLayer backing, but is nevertheless the root for a call to the Layer::paint*() methods.
    PaintLayer* enclosingLayerForPaintInvalidation() const;

    PaintLayer* enclosingLayerForPaintInvalidationCrossingFrameBoundaries() const;

    bool hasAncestorWithFilterThatMovesPixels() const;

    bool canUseConvertToLayerCoords() const
    {
        // These LayoutObjects have an impact on their layers without the layoutObjects knowing about it.
        return !layoutObject()->hasTransformRelatedProperty() && !layoutObject()->isSVGRoot();
    }

    void convertToLayerCoords(const PaintLayer* ancestorLayer, LayoutPoint&) const;
    void convertToLayerCoords(const PaintLayer* ancestorLayer, LayoutRect&) const;

    // Does the same as convertToLayerCoords() when not in multicol. For multicol, however,
    // convertToLayerCoords() calculates the offset in flow-thread coordinates (what the layout
    // engine uses internally), while this method calculates the visual coordinates; i.e. it figures
    // out which column the layer starts in and adds in the offset. See
    // http://www.chromium.org/developers/design-documents/multi-column-layout for more info.
    LayoutPoint visualOffsetFromAncestor(const PaintLayer* ancestorLayer) const;

    // Convert a bounding box from flow thread coordinates, relative to |this|, to visual coordinates, relative to |ancestorLayer|.
    // See http://www.chromium.org/developers/design-documents/multi-column-layout for more info on these coordinate types.
    // This method requires this layer to be paginated; i.e. it must have an enclosingPaginationLayer().
    void convertFromFlowThreadToVisualBoundingBoxInAncestor(const PaintLayer* ancestorLayer, LayoutRect&) const;

    // The hitTest() method looks for mouse events by walking layers that intersect the point from front to back.
    bool hitTest(HitTestResult&);

    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const LayoutPoint& offsetFromRoot) const;

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect physicalBoundingBox(const LayoutPoint& offsetFromRoot) const;
    LayoutRect physicalBoundingBox(const PaintLayer* ancestorLayer) const;
    LayoutRect physicalBoundingBoxIncludingReflectionAndStackingChildren(const LayoutPoint& offsetFromRoot) const;
    LayoutRect fragmentsBoundingBox(const PaintLayer* ancestorLayer) const;

    LayoutRect boundingBoxForCompositingOverlapTest() const;

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const;

    // MaybeIncludeTransformForAncestorLayer means that a transform on |ancestorLayer| may be applied to the bounding box,
    // in particular if paintsWithTransform() is true.
    enum CalculateBoundsOptions {
        MaybeIncludeTransformForAncestorLayer,
        NeverIncludeTransformForAncestorLayer,
    };
    LayoutRect boundingBoxForCompositing(const PaintLayer* ancestorLayer = 0, CalculateBoundsOptions = MaybeIncludeTransformForAncestorLayer) const;

    LayoutUnit staticInlinePosition() const { return m_staticInlinePosition; }
    LayoutUnit staticBlockPosition() const { return m_staticBlockPosition; }

    void setStaticInlinePosition(LayoutUnit position) { m_staticInlinePosition = position; }
    void setStaticBlockPosition(LayoutUnit position) { m_staticBlockPosition = position; }

    LayoutSize subpixelAccumulation() const;
    void setSubpixelAccumulation(const LayoutSize&);

    bool hasTransformRelatedProperty() const { return layoutObject()->hasTransformRelatedProperty(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_rareData ? m_rareData->transform.get() : nullptr; }

    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in. If the layer does not have a transform,
    // returns the identity matrix.
    TransformationMatrix currentTransform() const;
    TransformationMatrix renderableTransform(GlobalPaintFlags) const;

    // Get the perspective transform, which is applied to transformed sublayers.
    // Returns true if the layer has a -webkit-perspective.
    // Note that this transform does not have the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    bool preserves3D() const { return layoutObject()->style()->preserves3D(); }
    bool has3DTransform() const { return m_rareData && m_rareData->transform && !m_rareData->transform->isAffine(); }

    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool shouldPreserve3D() const { return !layoutObject()->hasReflection() && layoutObject()->style()->preserves3D(); }

    void filterNeedsPaintInvalidation();

    // Returns |true| if any property that renders using filter operations is
    // used (including, but not limited to, 'filter').
    bool hasFilterInducingProperty() const { return layoutObject()->hasFilterInducingProperty(); }

    void* operator new(size_t);
    // Only safe to call from LayoutBoxModelObject::destroyLayer()
    void operator delete(void*);

    CompositingState compositingState() const;

    // This returns true if our document is in a phase of its lifestyle during which
    // compositing state may legally be read.
    bool isAllowedToQueryCompositingState() const;

    // Don't null check this.
    // FIXME: Rename.
    CompositedLayerMapping* compositedLayerMapping() const;
    GraphicsLayer* graphicsLayerBacking() const;
    GraphicsLayer* graphicsLayerBackingForScrolling() const;
    // NOTE: If you are using hasCompositedLayerMapping to determine the state of compositing for this layer,
    // (and not just to do bookkeeping related to the mapping like, say, allocating or deallocating a mapping),
    // then you may have incorrect logic. Use compositingState() instead.
    // FIXME: This is identical to null checking compositedLayerMapping(), why not just call that?
    bool hasCompositedLayerMapping() const { return m_rareData && m_rareData->compositedLayerMapping; }
    void ensureCompositedLayerMapping();
    void clearCompositedLayerMapping(bool layerBeingDestroyed = false);
    CompositedLayerMapping* groupedMapping() const { return m_rareData ? m_rareData->groupedMapping : nullptr; }
    enum SetGroupMappingOptions {
        InvalidateLayerAndRemoveFromMapping,
        DoNotInvalidateLayerAndRemoveFromMapping
    };
    void setGroupedMapping(CompositedLayerMapping*, SetGroupMappingOptions);

    bool hasCompositedMask() const;
    bool hasCompositedClippingMask() const;
    bool needsCompositedScrolling() const { return m_scrollableArea && m_scrollableArea->needsCompositedScrolling(); }

    static void mapPointInPaintInvalidationContainerToBacking(const LayoutBoxModelObject& paintInvalidationContainer, FloatPoint&);
    static void mapRectInPaintInvalidationContainerToBacking(const LayoutBoxModelObject& paintInvalidationContainer, LayoutRect&);

    // Adjusts the given rect (in the coordinate space of the LayoutObject) to the coordinate space of |paintInvalidationContainer|'s GraphicsLayer backing.
    // Should use PaintInvalidationState::mapRectToPaintInvalidationBacking() instead if PaintInvalidationState is available.
    static void mapRectToPaintInvalidationBacking(const LayoutObject&, const LayoutBoxModelObject& paintInvalidationContainer, LayoutRect&);

    bool paintsWithTransparency(GlobalPaintFlags globalPaintFlags) const
    {
        return isTransparent() && ((globalPaintFlags & GlobalPaintFlattenCompositingLayers) || compositingState() != PaintsIntoOwnBacking);
    }

    bool paintsWithTransform(GlobalPaintFlags) const;

    // Returns true if background phase is painted opaque in the given rect.
    // The query rect is given in local coordinates.
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool containsDirtyOverlayScrollbars() const { return m_containsDirtyOverlayScrollbars; }
    void setContainsDirtyOverlayScrollbars(bool dirtyScrollbars) { m_containsDirtyOverlayScrollbars = dirtyScrollbars; }

    FilterOperations computeFilterOperations(const ComputedStyle&) const;
    FilterOperations computeBackdropFilterOperations(const ComputedStyle&) const;
    bool paintsWithFilters() const;
    bool paintsWithBackdropFilters() const;
    FilterEffect* lastFilterEffect() const;

    // Maps "forward" to determine which pixels in a destination rect are
    // affected by pixels in the source rect.
    // See also FilterEffect::mapRect.
    FloatRect mapRectForFilter(const FloatRect&) const;

    // Calls the above, rounding outwards.
    LayoutRect mapLayoutRectForFilter(const LayoutRect&) const;

    bool hasFilterThatMovesPixels() const;

    PaintLayerFilterInfo* filterInfo() const { return m_rareData ? m_rareData->filterInfo.get() : nullptr; }
    PaintLayerFilterInfo& ensureFilterInfo();

    void updateFilters(const ComputedStyle* oldStyle, const ComputedStyle& newStyle);

    Node* enclosingNode() const;

    bool isInTopLayer() const;

    bool scrollsWithViewport() const;
    bool scrollsWithRespectTo(const PaintLayer*) const;

    void addLayerHitTestRects(LayerHitTestRects&) const;

    // Compute rects only for this layer
    void computeSelfHitTestRects(LayerHitTestRects&) const;

    // FIXME: This should probably return a ScrollableArea but a lot of internal methods are mistakenly exposed.
    PaintLayerScrollableArea* getScrollableArea() const { return m_scrollableArea.get(); }

    PaintLayerClipper clipper() const { return PaintLayerClipper(*this); }

    bool scrollsOverflow() const;

    CompositingReasons potentialCompositingReasonsFromStyle() const { return m_rareData ? m_rareData->potentialCompositingReasonsFromStyle : CompositingReasonNone; }
    void setPotentialCompositingReasonsFromStyle(CompositingReasons reasons)
    {
        ASSERT(reasons == (reasons & CompositingReasonComboAllStyleDeterminedReasons));
        if (m_rareData || reasons != CompositingReasonNone)
            ensureRareData().potentialCompositingReasonsFromStyle = reasons;
    }

    bool hasStyleDeterminedDirectCompositingReasons() const { return potentialCompositingReasonsFromStyle() & CompositingReasonComboAllDirectStyleDeterminedReasons; }

    class AncestorDependentCompositingInputs {
        DISALLOW_NEW();
    public:
        AncestorDependentCompositingInputs()
            : clippingContainer(nullptr)
        { }

        IntRect clippedAbsoluteBoundingBox;
        const LayoutObject* clippingContainer;
    };

    class RareAncestorDependentCompositingInputs {
    public:
        RareAncestorDependentCompositingInputs()
            : opacityAncestor(nullptr)
            , transformAncestor(nullptr)
            , filterAncestor(nullptr)
            , ancestorScrollingLayer(nullptr)
            , nearestFixedPositionLayer(nullptr)
            , scrollParent(nullptr)
            , clipParent(nullptr)
        { }

        bool isDefault() const { return !opacityAncestor && !transformAncestor && !filterAncestor && !ancestorScrollingLayer && !nearestFixedPositionLayer && !scrollParent && !clipParent; }

        const PaintLayer* opacityAncestor;
        const PaintLayer* transformAncestor;
        const PaintLayer* filterAncestor;

        // The fist ancestor which can scroll. This is a subset of the
        // ancestorOverflowLayer chain where the scrolling layer is visible and
        // has a larger scroll content than its bounds.
        const PaintLayer* ancestorScrollingLayer;
        const PaintLayer* nearestFixedPositionLayer;

        // A scroll parent is a compositor concept. It's only needed in blink
        // because we need to use it as a promotion trigger. A layer has a
        // scroll parent if neither its compositor scrolling ancestor, nor any
        // other layer scrolled by this ancestor, is a stacking ancestor of this
        // layer. Layers with scroll parents must be scrolled with the main
        // scrolling layer by the compositor.
        const PaintLayer* scrollParent;

        // A clip parent is another compositor concept that has leaked into
        // blink so that it may be used as a promotion trigger. Layers with clip
        // parents escape the clip of a stacking tree ancestor. The compositor
        // needs to know about clip parents in order to circumvent its normal
        // clipping logic.
        const PaintLayer* clipParent;
    };

    void setNeedsCompositingInputsUpdate();
    bool childNeedsCompositingInputsUpdate() const { return m_childNeedsCompositingInputsUpdate; }
    bool needsCompositingInputsUpdate() const
    {
        // While we're updating the compositing inputs, these values may differ.
        // We should never be asking for this value when that is the case.
        ASSERT(m_needsDescendantDependentCompositingInputsUpdate == m_needsAncestorDependentCompositingInputsUpdate);
        return m_needsDescendantDependentCompositingInputsUpdate;
    }

    void updateAncestorOverflowLayer(const PaintLayer* ancestorOverflowLayer) { m_ancestorOverflowLayer = ancestorOverflowLayer; }
    void updateAncestorDependentCompositingInputs(const AncestorDependentCompositingInputs&, const RareAncestorDependentCompositingInputs&, bool hasAncestorWithClipPath);
    void updateDescendantDependentCompositingInputs(bool hasDescendantWithClipPath, bool hasNonIsolatedDescendantWithBlendMode);
    void didUpdateCompositingInputs();

    IntRect clippedAbsoluteBoundingBox() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_ancestorDependentCompositingInputs.clippedAbsoluteBoundingBox; }
    const PaintLayer* opacityAncestor() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->opacityAncestor : nullptr; }
    const PaintLayer* transformAncestor() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->transformAncestor : nullptr; }
    const PaintLayer* filterAncestor() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->filterAncestor : nullptr; }
    const LayoutObject* clippingContainer() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_ancestorDependentCompositingInputs.clippingContainer; }
    const PaintLayer* ancestorOverflowLayer() const { return m_ancestorOverflowLayer; }
    const PaintLayer* ancestorScrollingLayer() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->ancestorScrollingLayer : nullptr; }
    const PaintLayer* nearestFixedPositionLayer() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->nearestFixedPositionLayer : nullptr; }
    const PaintLayer* scrollParent() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->scrollParent : nullptr; }
    const PaintLayer* clipParent() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_rareAncestorDependentCompositingInputs ? m_rareAncestorDependentCompositingInputs->clipParent : nullptr; }
    bool hasAncestorWithClipPath() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_hasAncestorWithClipPath; }
    bool hasDescendantWithClipPath() const { ASSERT(!m_needsDescendantDependentCompositingInputsUpdate); return m_hasDescendantWithClipPath; }
    bool hasNonIsolatedDescendantWithBlendMode() const;

    bool lostGroupedMapping() const { ASSERT(isAllowedToQueryCompositingState()); return m_lostGroupedMapping; }
    void setLostGroupedMapping(bool b) { m_lostGroupedMapping = b; }

    CompositingReasons getCompositingReasons() const { ASSERT(isAllowedToQueryCompositingState()); return m_rareData ? m_rareData->compositingReasons : CompositingReasonNone; }
    void setCompositingReasons(CompositingReasons, CompositingReasons mask = CompositingReasonAll);

    SquashingDisallowedReasons getSquashingDisallowedReasons() const { ASSERT(isAllowedToQueryCompositingState()); return m_rareData ? m_rareData->squashingDisallowedReasons : SquashingDisallowedReasonsNone; }
    void setSquashingDisallowedReasons(SquashingDisallowedReasons);

    bool hasCompositingDescendant() const { ASSERT(isAllowedToQueryCompositingState()); return m_hasCompositingDescendant; }
    void setHasCompositingDescendant(bool);

    bool shouldIsolateCompositedDescendants() const { ASSERT(isAllowedToQueryCompositingState()); return m_shouldIsolateCompositedDescendants; }
    void setShouldIsolateCompositedDescendants(bool);

    void updateDescendantDependentFlags();

    void updateOrRemoveFilterEffectBuilder();

    void updateSelfPaintingLayer();
    // This is O(depth) so avoid calling this in loops. Instead use optimizations like
    // those in PaintInvalidationState.
    PaintLayer* enclosingSelfPaintingLayer();

    PaintLayer* enclosingTransformedAncestor() const;
    LayoutPoint computeOffsetFromTransformedAncestor() const;

    void didUpdateNeedsCompositedScrolling();

    bool hasSelfPaintingLayerDescendant() const
    {
        if (m_hasSelfPaintingLayerDescendantDirty)
            updateHasSelfPaintingLayerDescendant();
        ASSERT(!m_hasSelfPaintingLayerDescendantDirty);
        return m_hasSelfPaintingLayerDescendant;
    }
    LayoutRect paintingExtent(const PaintLayer* rootLayer, const LayoutSize& subPixelAccumulation, GlobalPaintFlags);
    void appendSingleFragmentIgnoringPagination(PaintLayerFragments&, const PaintLayer* rootLayer, const LayoutRect& dirtyRect, ClipRectsCacheSlot, OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize, ShouldRespectOverflowClipType = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0, const LayoutSize& subPixelAccumulation = LayoutSize());
    void collectFragments(PaintLayerFragments&, const PaintLayer* rootLayer, const LayoutRect& dirtyRect,
        ClipRectsCacheSlot, OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize,
        ShouldRespectOverflowClipType = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0,
        const LayoutSize& subPixelAccumulation = LayoutSize(), const LayoutRect* layerBoundingBox = 0);

    LayoutPoint layoutBoxLocation() const { return layoutObject()->isBox() ? toLayoutBox(layoutObject())->location() : LayoutPoint(); }

    enum TransparencyClipBoxBehavior {
        PaintingTransparencyClipBox,
        HitTestingTransparencyClipBox
    };

    enum TransparencyClipBoxMode {
        DescendantsOfTransparencyClipBox,
        RootOfTransparencyClipBox
    };

    static LayoutRect transparencyClipBox(const PaintLayer*, const PaintLayer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
        TransparencyClipBoxMode transparencyMode, const LayoutSize& subPixelAccumulation, GlobalPaintFlags = GlobalPaintNormalPhase);

    bool needsRepaint() const { return m_needsRepaint; }
    void setNeedsRepaint();
    void clearNeedsRepaintRecursively();

    // These previousXXX() functions are for subsequence caching. They save the painting status of the layer
    // during the previous painting with subsequence. A painting without subsequence [1] doesn't change this status.
    // [1] See shouldCreateSubsequence() in PaintLayerPainter.cpp for the cases we use subsequence when painting a PaintLayer.

    IntSize previousScrollOffsetAccumulationForPainting() const { return m_previousScrollOffsetAccumulationForPainting; }
    void setPreviousScrollOffsetAccumulationForPainting(const IntSize& s) { m_previousScrollOffsetAccumulationForPainting = s; }

    ClipRects* previousPaintingClipRects() const { return m_previousPaintingClipRects.get(); }
    void setPreviousPaintingClipRects(ClipRects& clipRects) { m_previousPaintingClipRects = &clipRects; }

    LayoutRect previousPaintDirtyRect() const { return m_previousPaintDirtyRect; }
    void setPreviousPaintDirtyRect(const LayoutRect& rect) { m_previousPaintDirtyRect = rect; }

    PaintLayerPainter::PaintResult previousPaintResult() const { return static_cast<PaintLayerPainter::PaintResult>(m_previousPaintResult); }
    void setPreviousPaintResult(PaintLayerPainter::PaintResult result) { m_previousPaintResult = static_cast<unsigned>(result); ASSERT(m_previousPaintResult == static_cast<unsigned>(result)); }

    // Used to skip PaintPhaseDescendantOutlinesOnly for layers that have never had descendant outlines.
    // Once it's set we never clear it because it's not easy to track if all outlines have been removed.
    // For more details, see core/paint/REAME.md#Empty paint phase optimization.
    bool needsPaintPhaseDescendantOutlines() const { return m_needsPaintPhaseDescendantOutlines; }
    void setNeedsPaintPhaseDescendantOutlines() { ASSERT(isSelfPaintingLayer()); m_needsPaintPhaseDescendantOutlines = true; }

    // Similar to above, but for PaintPhaseFloat.
    bool needsPaintPhaseFloat() const { return m_needsPaintPhaseFloat; }
    void setNeedsPaintPhaseFloat() { ASSERT(isSelfPaintingLayer()); m_needsPaintPhaseFloat = true; }

    bool needsPaintPhaseDescendantBlockBackgrounds() const { return m_needsPaintPhaseDescendantBlockBackgrounds; }
    void setNeedsPaintPhaseDescendantBlockBackgrounds() { ASSERT(isSelfPaintingLayer()); m_needsPaintPhaseDescendantBlockBackgrounds = true; }

    PaintTiming* paintTiming();

    ClipRectsCache* clipRectsCache() const { return m_clipRectsCache.get(); }
    ClipRectsCache& ensureClipRectsCache() const
    {
        if (!m_clipRectsCache)
            m_clipRectsCache = wrapUnique(new ClipRectsCache);
        return *m_clipRectsCache;
    }
    void clearClipRectsCache() const { m_clipRectsCache.reset(); }

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();
    bool has3DTransformedDescendant() const { DCHECK(!m_3DTransformedDescendantStatusDirty); return m_has3DTransformedDescendant; }

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    void endShouldKeepAliveAllClientsRecursive();
#endif

private:
    // Bounding box in the coordinates of this layer.
    LayoutRect logicalBoundingBox() const;

    bool hasOverflowControls() const;

    void dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    void updateLayerPositionRecursive();
    void updateLayerPositionsAfterScrollRecursive(const DoubleSize& scrollDelta, bool paintInvalidationContainerWasScrolled);

    void setNextSibling(PaintLayer* next) { m_next = next; }
    void setPreviousSibling(PaintLayer* prev) { m_previous = prev; }
    void setFirstChild(PaintLayer* first) { m_first = first; }
    void setLastChild(PaintLayer* last) { m_last = last; }

    void updateHasSelfPaintingLayerDescendant() const;
    PaintLayer* hitTestLayer(PaintLayer* rootLayer, PaintLayer* containerLayer, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, bool appliedTransform,
        const HitTestingTransformState* = 0, double* zOffset = 0);
    PaintLayer* hitTestLayerByApplyingTransform(PaintLayer* rootLayer, PaintLayer* containerLayer, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = 0, double* zOffset = 0,
        const LayoutPoint& translationOffset = LayoutPoint());
    PaintLayer* hitTestChildren(ChildrenIteration, PaintLayer* rootLayer, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState*, double* zOffsetForDescendants, double* zOffset,
        const HitTestingTransformState* unflattenedTransformState, bool depthSortDescendants);

    PassRefPtr<HitTestingTransformState> createLocalTransformState(PaintLayer* rootLayer, PaintLayer* containerLayer,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState* containerTransformState,
        const LayoutPoint& translationOffset = LayoutPoint()) const;

    bool hitTestContents(HitTestResult&, const LayoutRect& layerBounds, const HitTestLocation&, HitTestFilter) const;
    bool hitTestContentsForFragments(const PaintLayerFragments&, HitTestResult&, const HitTestLocation&, HitTestFilter, bool& insideClipRect) const;
    PaintLayer* hitTestTransformedLayerInFragments(PaintLayer* rootLayer, PaintLayer* containerLayer, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffset, ClipRectsCacheSlot);
    bool hitTestClippedOutByClipPath(PaintLayer* rootLayer, const HitTestLocation&) const;

    bool childBackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool shouldBeSelfPaintingLayer() const;

    // FIXME: We should only create the stacking node if needed.
    bool requiresStackingNode() const { return true; }
    void updateStackingNode();

    void updateReflectionInfo(const ComputedStyle*);
    FilterEffectBuilder* updateFilterEffectBuilder() const;

    // FIXME: We could lazily allocate our ScrollableArea based on style properties ('overflow', ...)
    // but for now, we are always allocating it for LayoutBox as it's safer. crbug.com/467721.
    bool requiresScrollableArea() const { return layoutBox(); }
    void updateScrollableArea();

    void dirtyAncestorChainVisibleDescendantStatus();

    bool attemptDirectCompositingUpdate(StyleDifference, const ComputedStyle* oldStyle);
    void updateTransform(const ComputedStyle* oldStyle, const ComputedStyle& newStyle);

    void removeAncestorOverflowLayer(const PaintLayer* removedLayer);

    void updateOrRemoveFilterClients();

    void updatePaginationRecursive(bool needsPaginationUpdate = false);
    void clearPaginationRecursive();

    void setNeedsRepaintInternal();
    void markCompositingContainerChainForNeedsRepaint();

    PaintLayerRareData& ensureRareData()
    {
        if (!m_rareData)
            m_rareData = wrapUnique(new PaintLayerRareData);
        return *m_rareData;
    }

    void mergeNeedsPaintPhaseFlagsFrom(const PaintLayer& layer)
    {
        m_needsPaintPhaseDescendantOutlines |= layer.m_needsPaintPhaseDescendantOutlines;
        m_needsPaintPhaseFloat |= layer.m_needsPaintPhaseFloat;
        m_needsPaintPhaseDescendantBlockBackgrounds |= layer.m_needsPaintPhaseDescendantBlockBackgrounds;
    }

    bool isSelfPaintingLayerForIntrinsicOrScrollingReasons() const;

    bool shouldFragmentCompositedBounds(const PaintLayer* compositingLayer) const;

    // Self-painting layer is an optimization where we avoid the heavy Layer painting
    // machinery for a Layer allocated only to handle the overflow clip case.
    // FIXME(crbug.com/332791): Self-painting layer should be merged into the overflow-only concept.
    unsigned m_isSelfPaintingLayer : 1;

    // If have no self-painting descendants, we don't have to walk our children during painting. This can lead to
    // significant savings, especially if the tree has lots of non-self-painting layers grouped together (e.g. table cells).
    mutable unsigned m_hasSelfPaintingLayerDescendant : 1;
    mutable unsigned m_hasSelfPaintingLayerDescendantDirty : 1;

    const unsigned m_isRootLayer : 1;

    unsigned m_visibleContentStatusDirty : 1;
    unsigned m_hasVisibleContent : 1;
    unsigned m_visibleDescendantStatusDirty : 1;
    unsigned m_hasVisibleDescendant : 1;

#if ENABLE(ASSERT)
    unsigned m_needsPositionUpdate : 1;
#endif

    unsigned m_3DTransformedDescendantStatusDirty : 1;
    // Set on a stacking context layer that has 3D descendants anywhere
    // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
    unsigned m_has3DTransformedDescendant : 1;

    unsigned m_containsDirtyOverlayScrollbars : 1;

    unsigned m_needsAncestorDependentCompositingInputsUpdate : 1;
    unsigned m_needsDescendantDependentCompositingInputsUpdate : 1;
    unsigned m_childNeedsCompositingInputsUpdate : 1;

    // Used only while determining what layers should be composited. Applies to the tree of z-order lists.
    unsigned m_hasCompositingDescendant : 1;

    // True iff we have scrollable overflow and all children of m_layoutObject are known to paint
    // exclusively into their own composited layers.  Set by updateScrollingStateAfterCompositingChange().
    unsigned m_isAllScrollingContentComposited : 1;

    // Should be for stacking contexts having unisolated blending descendants.
    unsigned m_shouldIsolateCompositedDescendants : 1;

    // True if this layout layer just lost its grouped mapping due to the CompositedLayerMapping being destroyed,
    // and we don't yet know to what graphics layer this Layer will be assigned.
    unsigned m_lostGroupedMapping : 1;

    unsigned m_needsRepaint : 1;
    unsigned m_previousPaintResult : 1; // PaintLayerPainter::PaintResult

    unsigned m_needsPaintPhaseDescendantOutlines : 1;
    unsigned m_needsPaintPhaseFloat : 1;
    unsigned m_needsPaintPhaseDescendantBlockBackgrounds : 1;

    // These bitfields are part of ancestor/descendant dependent compositing inputs.
    unsigned m_hasDescendantWithClipPath : 1;
    unsigned m_hasNonIsolatedDescendantWithBlendMode : 1;
    unsigned m_hasAncestorWithClipPath : 1;

    LayoutBoxModelObject* m_layoutObject;

    PaintLayer* m_parent;
    PaintLayer* m_previous;
    PaintLayer* m_next;
    PaintLayer* m_first;
    PaintLayer* m_last;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    LayoutPoint m_location;

    // The layer's size.
    //
    // If the associated LayoutBoxModelObject is a LayoutBox, it's its border
    // box. Otherwise, this is the LayoutInline's lines' bounding box.
    IntSize m_size;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    LayoutUnit m_staticInlinePosition;
    LayoutUnit m_staticBlockPosition;

    // The first ancestor having a non visible overflow.
    const PaintLayer* m_ancestorOverflowLayer;

    AncestorDependentCompositingInputs m_ancestorDependentCompositingInputs;
    std::unique_ptr<RareAncestorDependentCompositingInputs> m_rareAncestorDependentCompositingInputs;

    Persistent<PaintLayerScrollableArea> m_scrollableArea;

    mutable std::unique_ptr<ClipRectsCache> m_clipRectsCache;

    std::unique_ptr<PaintLayerStackingNode> m_stackingNode;

    IntSize m_previousScrollOffsetAccumulationForPainting;
    RefPtr<ClipRects> m_previousPaintingClipRects;
    LayoutRect m_previousPaintDirtyRect;

    std::unique_ptr<PaintLayerRareData> m_rareData;
};

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showLayerTree(const blink::PaintLayer*);
void showLayerTree(const blink::LayoutObject*);
#endif

#endif // Layer_h
