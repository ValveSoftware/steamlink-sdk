/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/layout/svg/LayoutSVGBlock.h"

#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/style/ShadowList.h"
#include "core/svg/SVGElement.h"

namespace blink {

LayoutSVGBlock::LayoutSVGBlock(SVGElement* element)
    : LayoutBlockFlow(element)
{
}

bool LayoutSVGBlock::allowsOverflowClip() const
{
    // LayoutSVGBlock, used by Layout(SVGText|ForeignObject), is not allowed to have overflow clip.
    // LayoutBlock assumes a layer to be present when the overflow clip functionality is requested. Both
    // Layout(SVGText|ForeignObject) return 'NoPaintLayer' on 'layerTypeRequired'. Fine for LayoutSVGText.
    //
    // If we want to support overflow rules for <foreignObject> we can choose between two solutions:
    // a) make LayoutSVGForeignObject require layers and SVG layer aware
    // b) refactor overflow logic out of Layer (as suggested by dhyatt), which is a large task
    //
    // Until this is resolved, disable overflow support. Opera/FF don't support it as well at the moment (Feb 2010).
    //
    // Note: This does NOT affect overflow handling on outer/inner <svg> elements - this is handled
    // manually by LayoutSVGRoot - which owns the documents enclosing root layer and thus works fine.
    return false;
}

void LayoutSVGBlock::absoluteRects(Vector<IntRect>&, const LayoutPoint&) const
{
    // This code path should never be taken for SVG, as we're assuming useTransforms=true everywhere, absoluteQuads should be used.
    ASSERT_NOT_REACHED();
}

void LayoutSVGBlock::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    LayoutBlockFlow::willBeDestroyed();
}

void LayoutSVGBlock::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
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

    LayoutBlock::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, styleRef());
}

void LayoutSVGBlock::mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags) const
{
    SVGLayoutSupport::mapLocalToAncestor(this, ancestor, transformState);
}

void LayoutSVGBlock::mapAncestorToLocal(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags) const
{
    if (this == ancestor)
        return;
    SVGLayoutSupport::mapAncestorToLocal(*this, ancestor, transformState);
}

const LayoutObject* LayoutSVGBlock::pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap& geometryMap) const
{
    return SVGLayoutSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

LayoutRect LayoutSVGBlock::absoluteClippedOverflowRect() const
{
    return SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(*this, *view());
}

bool LayoutSVGBlock::mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect& rect, VisualRectFlags) const
{
    return SVGLayoutSupport::mapToVisualRectInAncestorSpace(*this, ancestor, FloatRect(rect), rect);
}

bool LayoutSVGBlock::nodeAtPoint(HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace blink
