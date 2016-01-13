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

#include "config.h"
#include "core/rendering/svg/RenderSVGModelObject.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGGraphicsElement.h"

namespace WebCore {

RenderSVGModelObject::RenderSVGModelObject(SVGElement* node)
    : RenderObject(node)
{
}

bool RenderSVGModelObject::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isSVG() && !(child->isSVGInline() || child->isSVGInlineText());
}

LayoutRect RenderSVGModelObject::clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, paintInvalidationContainer);
}

void RenderSVGModelObject::computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect& paintInvalidationRect, bool fixed) const
{
    SVGRenderSupport::computeFloatRectForRepaint(this, paintInvalidationContainer, paintInvalidationRect, fixed);
}

void RenderSVGModelObject::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags, bool* wasFixed) const
{
    SVGRenderSupport::mapLocalToContainer(this, repaintContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGModelObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGRenderSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

void RenderSVGModelObject::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    IntRect rect = enclosingIntRect(strokeBoundingBox());
    rect.moveBy(roundedIntPoint(accumulatedOffset));
    rects.append(rect);
}

void RenderSVGModelObject::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox(), 0 /* mode */, wasFixed));
}

void RenderSVGModelObject::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    RenderObject::willBeDestroyed();
}

void RenderSVGModelObject::computeLayerHitTestRects(LayerHitTestRects& rects) const
{
    // Using just the rect for the SVGRoot is good enough for now.
    SVGRenderSupport::findTreeRootObject(this)->computeLayerHitTestRects(rects);
}

void RenderSVGModelObject::addLayerHitTestRects(LayerHitTestRects&, const RenderLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    // We don't walk into SVG trees at all - just report their container.
}

void RenderSVGModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (diff.needsFullLayout()) {
        setNeedsBoundariesUpdate();
        if (style()->hasTransform())
            setNeedsTransformUpdate();
    }

    RenderObject::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, style());
}

bool RenderSVGModelObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

// The SVG addFocusRingRects() method adds rects in local coordinates so the default absoluteFocusRingQuads
// returns incorrect values for SVG objects. Overriding this method provides access to the absolute bounds.
void RenderSVGModelObject::absoluteFocusRingQuads(Vector<FloatQuad>& quads)
{
    quads.append(localToAbsoluteQuad(FloatQuad(paintInvalidationRectInLocalCoordinates())));
}

void RenderSVGModelObject::invalidateTreeAfterLayout(const RenderLayerModelObject& paintInvalidationContainer)
{
    // Note: This is a reduced version of RenderBox::invalidateTreeAfterLayout().
    // FIXME: Should share code with RenderBox::invalidateTreeAfterLayout().
    ASSERT(RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
    ASSERT(!needsLayout());

    if (!shouldCheckForPaintInvalidationAfterLayout())
        return;

    ForceHorriblySlowRectMapping slowRectMapping(*this);

    const LayoutRect oldPaintInvalidationRect = previousPaintInvalidationRect();
    const LayoutPoint oldPositionFromPaintInvalidationContainer = previousPositionFromPaintInvalidationContainer();
    const RenderLayerModelObject& newPaintInvalidationContainer = *containerForPaintInvalidation();
    setPreviousPaintInvalidationRect(clippedOverflowRectForPaintInvalidation(&newPaintInvalidationContainer));
    setPreviousPositionFromPaintInvalidationContainer(RenderLayer::positionFromPaintInvalidationContainer(this, &newPaintInvalidationContainer));

    // If an ancestor container had its transform changed, then we just
    // need to update the RenderSVGModelObject's repaint rect above. The invalidation
    // will be handled by the container where the transform changed. This essentially
    // means that we prune the entire branch for performance.
    if (!SVGRenderSupport::parentTransformDidChange(this))
        return;

    // If we are set to do a full paint invalidation that means the RenderView will be
    // issue paint invalidations. We can then skip issuing of paint invalidations for the child
    // renderers as they'll be covered by the RenderView.
    if (view()->doingFullRepaint()) {
        RenderObject::invalidateTreeAfterLayout(newPaintInvalidationContainer);
        return;
    }

    const LayoutRect& newPaintInvalidationRect = previousPaintInvalidationRect();
    const LayoutPoint& newPositionFromPaintInvalidationContainer = previousPositionFromPaintInvalidationContainer();
    invalidatePaintAfterLayoutIfNeeded(containerForPaintInvalidation(),
        shouldDoFullPaintInvalidationAfterLayout(), oldPaintInvalidationRect, oldPositionFromPaintInvalidationContainer,
        &newPaintInvalidationRect, &newPositionFromPaintInvalidationContainer);

    RenderObject::invalidateTreeAfterLayout(newPaintInvalidationContainer);
}

} // namespace WebCore
