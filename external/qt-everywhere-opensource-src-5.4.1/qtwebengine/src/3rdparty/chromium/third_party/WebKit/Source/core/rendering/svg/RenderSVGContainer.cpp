/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/rendering/svg/RenderSVGContainer.h"

#include "core/frame/Settings.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/LayoutRepainter.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "platform/graphics/GraphicsContextCullSaver.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace WebCore {

RenderSVGContainer::RenderSVGContainer(SVGElement* node)
    : RenderSVGModelObject(node)
    , m_objectBoundingBoxValid(false)
    , m_needsBoundariesUpdate(true)
{
}

RenderSVGContainer::~RenderSVGContainer()
{
}

void RenderSVGContainer::layout()
{
    ASSERT(needsLayout());

    // RenderSVGRoot disables layoutState for the SVG rendering tree.
    ASSERT(!view()->layoutStateCachedOffsetsEnabled());

    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(this) || selfWillPaint());

    // Allow RenderSVGViewportContainer to update its viewport.
    calcViewport();

    // Allow RenderSVGTransformableContainer to update its transform.
    bool updatedTransform = calculateLocalTransform();

    // RenderSVGViewportContainer needs to set the 'layout size changed' flag.
    determineIfLayoutSizeChanged();

    SVGRenderSupport::layoutChildren(this, selfNeedsLayout() || SVGRenderSupport::filtersForceContainerLayout(this));

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && needsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds.
    if (m_needsBoundariesUpdate || updatedTransform) {
        updateCachedBoundaries();
        m_needsBoundariesUpdate = false;

        // If our bounds changed, notify the parents.
        RenderSVGModelObject::setNeedsBoundariesUpdate();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGContainer::addChild(RenderObject* child, RenderObject* beforeChild)
{
    RenderSVGModelObject::addChild(child, beforeChild);
    SVGResourcesCache::clientWasAddedToTree(child, child->style());
}

void RenderSVGContainer::removeChild(RenderObject* child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    RenderSVGModelObject::removeChild(child);
}


bool RenderSVGContainer::selfWillPaint()
{
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    return resources && resources->filter();
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, this);

    if (paintInfo.context->paintingDisabled())
        return;

    // Spec: groups w/o children still may render filter content.
    if (!firstChild() && !selfWillPaint())
        return;

    FloatRect repaintRect = paintInvalidationRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsRepaintRect(repaintRect, localToParentTransform(), paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    {
        GraphicsContextStateSaver stateSaver(*childPaintInfo.context);

        // Let the RenderSVGViewportContainer subclass clip if necessary
        applyViewportClip(childPaintInfo);

        childPaintInfo.applyTransform(localToParentTransform());

        SVGRenderingContext renderingContext;
        GraphicsContextCullSaver cullSaver(*childPaintInfo.context);
        bool continueRendering = true;
        if (childPaintInfo.phase == PaintPhaseForeground) {
            renderingContext.prepareToRenderSVGContent(this, childPaintInfo);
            continueRendering = renderingContext.isRenderingPrepared();

            if (continueRendering && document().settings()->containerCullingEnabled())
                cullSaver.cull(paintInvalidationRectInLocalCoordinates());
        }

        if (continueRendering) {
            childPaintInfo.updatePaintingRootForChildren(this);
            for (RenderObject* child = firstChild(); child; child = child->nextSibling())
                child->paint(childPaintInfo, IntPoint());
        }
    }

    // FIXME: This really should be drawn from local coordinates, but currently we hack it
    // to avoid our clip killing our outline rect. Thus we translate our
    // outline rect into parent coords before drawing.
    // FIXME: This means our focus ring won't share our rotation like it should.
    // We should instead disable our clip during PaintPhaseOutline
    if (paintInfo.phase == PaintPhaseForeground && style()->outlineWidth() && style()->visibility() == VISIBLE) {
        IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRect));
        paintOutline(paintInfo, paintRectInParent);
    }
}

// addFocusRingRects is called from paintOutline and needs to be in the same coordinates as the paintOuline call
void RenderSVGContainer::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint&, const RenderLayerModelObject*)
{
    IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(paintInvalidationRectInLocalCoordinates()));
    if (!paintRectInParent.isEmpty())
        rects.append(paintRectInParent);
}

void RenderSVGContainer::updateCachedBoundaries()
{
    SVGRenderSupport::computeContainerBoundingBoxes(this, m_objectBoundingBox, m_objectBoundingBoxValid, m_strokeBoundingBox, m_repaintBoundingBox);
    SVGRenderSupport::intersectRepaintRectWithResources(this, m_repaintBoundingBox);
}

bool RenderSVGContainer::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Give RenderSVGViewportContainer a chance to apply its viewport clip
    if (!pointIsInsideViewportClip(pointInParent))
        return false;

    FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

    if (!SVGRenderSupport::pointInClippingArea(this, localPoint))
        return false;

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
            updateHitTestResult(result, roundedLayoutPoint(localPoint));
            return true;
        }
    }

    // pointer-events=boundingBox makes it possible for containers to be direct targets
    if (style()->pointerEvents() == PE_BOUNDINGBOX) {
        ASSERT(isObjectBoundingBoxValid());
        if (objectBoundingBox().contains(localPoint)) {
            updateHitTestResult(result, roundedLayoutPoint(localPoint));
            return true;
        }
    }
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}
