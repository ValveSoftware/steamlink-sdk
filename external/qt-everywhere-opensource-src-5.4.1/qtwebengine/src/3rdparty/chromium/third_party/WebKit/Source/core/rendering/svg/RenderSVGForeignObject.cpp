/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/rendering/svg/RenderSVGForeignObject.h"

#include "core/rendering/HitTestResult.h"
#include "core/rendering/LayoutRepainter.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGForeignObjectElement.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace WebCore {

RenderSVGForeignObject::RenderSVGForeignObject(SVGForeignObjectElement* node)
    : RenderSVGBlock(node)
    , m_needsTransformUpdate(true)
{
}

RenderSVGForeignObject::~RenderSVGForeignObject()
{
}

bool RenderSVGForeignObject::isChildAllowed(RenderObject* child, RenderStyle* style) const
{
    // Disallow arbitary SVG content. Only allow proper <svg xmlns="svgNS"> subdocuments.
    return !child->isSVG() || child->isSVGRoot();
}

void RenderSVGForeignObject::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context->paintingDisabled()
        || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*childPaintInfo.context);
    childPaintInfo.applyTransform(localTransform());

    if (SVGRenderSupport::isOverflowHidden(this))
        childPaintInfo.context->clip(m_viewport);

    SVGRenderingContext renderingContext;
    bool continueRendering = true;
    if (paintInfo.phase == PaintPhaseForeground) {
        renderingContext.prepareToRenderSVGContent(this, childPaintInfo);
        continueRendering = renderingContext.isRenderingPrepared();
    }

    if (continueRendering) {
        // Paint all phases of FO elements atomically, as though the FO element established its
        // own stacking context.
        bool preservePhase = paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip;
        LayoutPoint childPoint = IntPoint();
        childPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
        RenderBlock::paint(childPaintInfo, IntPoint());
        if (!preservePhase) {
            childPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseFloat;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseForeground;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseOutline;
            RenderBlock::paint(childPaintInfo, childPoint);
        }
    }
}

const AffineTransform& RenderSVGForeignObject::localToParentTransform() const
{
    m_localToParentTransform = localTransform();
    m_localToParentTransform.translate(m_viewport.x(), m_viewport.y());
    return m_localToParentTransform;
}

void RenderSVGForeignObject::updateLogicalWidth()
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    setWidth(static_cast<int>(roundf(m_viewport.width())));
}

void RenderSVGForeignObject::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    // FIXME: Is this correct for vertical writing mode?
    computedValues.m_extent = static_cast<int>(roundf(m_viewport.height()));
    computedValues.m_position = logicalTop;
}

void RenderSVGForeignObject::layout()
{
    ASSERT(needsLayout());
    ASSERT(!view()->layoutStateCachedOffsetsEnabled()); // RenderSVGRoot disables layoutState for the SVG rendering tree.

    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(this));
    SVGForeignObjectElement* foreign = toSVGForeignObjectElement(node());

    bool updateCachedBoundariesInParents = false;
    if (m_needsTransformUpdate) {
        m_localTransform = foreign->animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    FloatRect oldViewport = m_viewport;

    // Cache viewport boundaries
    SVGLengthContext lengthContext(foreign);
    FloatPoint viewportLocation(foreign->x()->currentValue()->value(lengthContext), foreign->y()->currentValue()->value(lengthContext));
    m_viewport = FloatRect(viewportLocation, FloatSize(foreign->width()->currentValue()->value(lengthContext), foreign->height()->currentValue()->value(lengthContext)));
    if (!updateCachedBoundariesInParents)
        updateCachedBoundariesInParents = oldViewport != m_viewport;

    // Set box origin to the foreignObject x/y translation, so positioned objects in XHTML content get correct
    // positions. A regular RenderBoxModelObject would pull this information from RenderStyle - in SVG those
    // properties are ignored for non <svg> elements, so we mimic what happens when specifying them through CSS.

    // FIXME: Investigate in location rounding issues - only affects RenderSVGForeignObject & RenderSVGText
    setLocation(roundedIntPoint(viewportLocation));

    bool layoutChanged = everHadLayout() && selfNeedsLayout();
    RenderBlock::layout();
    ASSERT(!needsLayout());

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderSVGBlock::setNeedsBoundariesUpdate();

    // Invalidate all resources of this client if our layout changed.
    if (layoutChanged)
        SVGResourcesCache::clientLayoutChanged(this);

    repainter.repaintAfterLayout();
}

void RenderSVGForeignObject::mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer,
    LayoutRect& rect, bool fixed) const
{
    FloatRect r(rect);
    SVGRenderSupport::computeFloatRectForRepaint(this, paintInvalidationContainer, r, fixed);
    rect = enclosingLayoutRect(r);
}

bool RenderSVGForeignObject::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Embedded content is drawn in the foreground phase.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = localTransform().inverse().mapPoint(pointInParent);

    // Early exit if local point is not contained in clipped viewport area
    if (SVGRenderSupport::isOverflowHidden(this) && !m_viewport.contains(localPoint))
        return false;

    // FOs establish a stacking context, so we need to hit-test all layers.
    HitTestLocation hitTestLocation(roundedLayoutPoint(localPoint));
    return RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestForeground)
        || RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestFloat)
        || RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestChildBlockBackgrounds);
}

}
