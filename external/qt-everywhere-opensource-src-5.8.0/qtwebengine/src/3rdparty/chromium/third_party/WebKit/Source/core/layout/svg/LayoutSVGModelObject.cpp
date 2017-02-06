/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/svg/LayoutSVGModelObject.h"

#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGContainer.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGGraphicsElement.h"

namespace blink {

LayoutSVGModelObject::LayoutSVGModelObject(SVGElement* node)
    : LayoutObject(node)
{
}

bool LayoutSVGModelObject::isChildAllowed(LayoutObject* child, const ComputedStyle&) const
{
    return child->isSVG() && !(child->isSVGInline() || child->isSVGInlineText() || child->isSVGGradientStop());
}

void LayoutSVGModelObject::mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags) const
{
    SVGLayoutSupport::mapLocalToAncestor(this, ancestor, transformState);
}

LayoutRect LayoutSVGModelObject::absoluteClippedOverflowRect() const
{
    return SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(*this, *view());
}

void LayoutSVGModelObject::mapAncestorToLocal(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags) const
{
    SVGLayoutSupport::mapAncestorToLocal(*this, ancestor, transformState);
}

const LayoutObject* LayoutSVGModelObject::pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap& geometryMap) const
{
    return SVGLayoutSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

void LayoutSVGModelObject::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    IntRect rect = enclosingIntRect(strokeBoundingBox());
    rect.moveBy(roundedIntPoint(accumulatedOffset));
    rects.append(rect);
}

void LayoutSVGModelObject::absoluteQuads(Vector<FloatQuad>& quads) const
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox()));
}

void LayoutSVGModelObject::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    LayoutObject::willBeDestroyed();
}

PaintInvalidationReason LayoutSVGModelObject::getPaintInvalidationReason(const PaintInvalidationState& paintInvalidationState,
    const LayoutRect& oldBounds, const LayoutPoint& oldLocation, const LayoutRect& newBounds, const LayoutPoint& newLocation) const
{
    PaintInvalidationReason invalidationReason = LayoutObject::getPaintInvalidationReason(paintInvalidationState, oldBounds, oldLocation, newBounds, newLocation);

    // Disable incremental invalidation for SVG objects to prevent under-
    // invalidation. Unlike boxes, it is non-trivial (and rare) for SVG objects
    // to be able to be incrementally invalidated (e.g., on height changes).
    if (invalidationReason == PaintInvalidationIncremental)
        return PaintInvalidationFull;

    return invalidationReason;
}

void LayoutSVGModelObject::computeLayerHitTestRects(LayerHitTestRects& rects) const
{
    // Using just the rect for the SVGRoot is good enough for now.
    SVGLayoutSupport::findTreeRootObject(this)->computeLayerHitTestRects(rects);
}

void LayoutSVGModelObject::addLayerHitTestRects(LayerHitTestRects&, const PaintLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    // We don't walk into SVG trees at all - just report their container.
}

void LayoutSVGModelObject::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    if (diff.needsFullLayout()) {
        setNeedsBoundariesUpdate();
        if (diff.transformChanged())
            setNeedsTransformUpdate();
    }

    if (isBlendingAllowed()) {
        bool hasBlendModeChanged = (oldStyle && oldStyle->hasBlendMode()) == !style()->hasBlendMode();
        if (parent() && hasBlendModeChanged)
            parent()->descendantIsolationRequirementsChanged(style()->hasBlendMode() ? DescendantIsolationRequired : DescendantIsolationNeedsUpdate);
    }

    LayoutObject::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, styleRef());
}

bool LayoutSVGModelObject::nodeAtPoint(HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

// The SVG addOutlineRects() method adds rects in local coordinates so the default absoluteElementBoundingBoxRect()
// returns incorrect values for SVG objects. Overriding this method provides access to the absolute bounds.
IntRect LayoutSVGModelObject::absoluteElementBoundingBoxRect() const
{
    return localToAbsoluteQuad(FloatQuad(paintInvalidationRectInLocalSVGCoordinates())).enclosingBoundingBox();
}

} // namespace blink
