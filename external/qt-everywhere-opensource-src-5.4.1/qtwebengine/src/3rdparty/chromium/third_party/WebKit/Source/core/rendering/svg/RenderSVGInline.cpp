/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#include "core/rendering/svg/RenderSVGInline.h"

#include "core/rendering/svg/RenderSVGText.h"
#include "core/rendering/svg/SVGInlineFlowBox.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGAElement.h"

namespace WebCore {

bool RenderSVGInline::isChildAllowed(RenderObject* child, RenderStyle* style) const
{
    if (child->isText())
        return SVGRenderSupport::isRenderableTextNode(child);

    if (isSVGAElement(*node())) {
        // Disallow direct descendant 'a'.
        if (isSVGAElement(*child->node()))
            return false;
    }

    if (!child->isSVGInline() && !child->isSVGInlineText())
        return false;

    return RenderInline::isChildAllowed(child, style);
}

RenderSVGInline::RenderSVGInline(Element* element)
    : RenderInline(element)
{
    setAlwaysCreateLineBoxes();
}

InlineFlowBox* RenderSVGInline::createInlineFlowBox()
{
    InlineFlowBox* box = new SVGInlineFlowBox(*this);
    box->setHasVirtualLogicalHeight();
    return box;
}

FloatRect RenderSVGInline::objectBoundingBox() const
{
    if (const RenderObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->objectBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::strokeBoundingBox() const
{
    if (const RenderObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->strokeBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::paintInvalidationRectInLocalCoordinates() const
{
    if (const RenderObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->paintInvalidationRectInLocalCoordinates();

    return FloatRect();
}

LayoutRect RenderSVGInline::clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, paintInvalidationContainer);
}

void RenderSVGInline::computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect& paintInvalidationRect, bool fixed) const
{
    SVGRenderSupport::computeFloatRectForRepaint(this, paintInvalidationContainer, paintInvalidationRect, fixed);
}

void RenderSVGInline::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags, bool* wasFixed) const
{
    SVGRenderSupport::mapLocalToContainer(this, repaintContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGRenderSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    const RenderObject* object = RenderSVGText::locateRenderSVGTextAncestor(this);
    if (!object)
        return;

    FloatRect textBoundingBox = object->strokeBoundingBox();
    for (InlineFlowBox* box = firstLineBox(); box; box = box->nextLineBox())
        quads.append(localToAbsoluteQuad(FloatRect(textBoundingBox.x() + box->x(), textBoundingBox.y() + box->y(), box->logicalWidth(), box->logicalHeight()), false, wasFixed));
}

void RenderSVGInline::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    RenderInline::willBeDestroyed();
}

void RenderSVGInline::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (diff.needsFullLayout())
        setNeedsBoundariesUpdate();

    RenderInline::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, style());
}

void RenderSVGInline::addChild(RenderObject* child, RenderObject* beforeChild)
{
    RenderInline::addChild(child, beforeChild);
    SVGResourcesCache::clientWasAddedToTree(child, child->style());

    if (RenderSVGText* textRenderer = RenderSVGText::locateRenderSVGTextAncestor(this))
        textRenderer->subtreeChildWasAdded(child);
}

void RenderSVGInline::removeChild(RenderObject* child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);

    RenderSVGText* textRenderer = RenderSVGText::locateRenderSVGTextAncestor(this);
    if (!textRenderer) {
        RenderInline::removeChild(child);
        return;
    }
    Vector<SVGTextLayoutAttributes*, 2> affectedAttributes;
    textRenderer->subtreeChildWillBeRemoved(child, affectedAttributes);
    RenderInline::removeChild(child);
    textRenderer->subtreeChildWasRemoved(affectedAttributes);
}

}
