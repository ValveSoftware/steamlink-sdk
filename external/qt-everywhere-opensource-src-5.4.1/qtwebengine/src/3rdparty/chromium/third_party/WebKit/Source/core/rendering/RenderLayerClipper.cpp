/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/rendering/RenderLayerClipper.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

void RenderLayerClipper::updateClipRects(const ClipRectsContext& clipRectsContext)
{
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    ASSERT(clipRectsType < NumCachedClipRectsTypes);
    if (m_clipRectsCache
        && clipRectsContext.rootLayer == m_clipRectsCache->clipRectsRoot(clipRectsType)
        && m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip)) {
        // FIXME: We used to ASSERT that we always got a consistent root layer.
        // We should add a test that has an inconsistent root. See
        // http://crbug.com/366118 for an example.
        ASSERT(m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] == clipRectsContext.overlayScrollbarSizeRelevancy);

#ifdef CHECK_CACHED_CLIP_RECTS
        // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        ClipRects clipRects;
        calculateClipRects(tempContext, clipRects);
        ASSERT(clipRects == *m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip).get());
#endif
        return; // We have the correct cached value.
    }

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = !isClippingRootForContext(clipRectsContext) ? m_renderer.layer()->parent() : 0;
    if (parentLayer)
        parentLayer->clipper().updateClipRects(clipRectsContext);

    ClipRects clipRects;
    calculateClipRects(clipRectsContext, clipRects);

    if (!m_clipRectsCache)
        m_clipRectsCache = adoptPtr(new ClipRectsCache);

    if (parentLayer && parentLayer->clipper().clipRects(clipRectsContext) && clipRects == *parentLayer->clipper().clipRects(clipRectsContext))
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, parentLayer->clipper().clipRects(clipRectsContext), clipRectsContext.rootLayer);
    else
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, ClipRects::create(clipRects), clipRectsContext.rootLayer);

#ifndef NDEBUG
    m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] = clipRectsContext.overlayScrollbarSizeRelevancy;
#endif
}

void RenderLayerClipper::clearClipRectsIncludingDescendants(ClipRectsType typeToClear)
{
    // FIXME: it's not clear how this layer not having clip rects guarantees that no descendants have any.
    if (!m_clipRectsCache)
        return;

    clearClipRects(typeToClear);

    for (RenderLayer* layer = m_renderer.layer()->firstChild(); layer; layer = layer->nextSibling())
        layer->clipper().clearClipRectsIncludingDescendants(typeToClear);
}

void RenderLayerClipper::clearClipRects(ClipRectsType typeToClear)
{
    if (typeToClear == AllClipRectTypes) {
        m_clipRectsCache = nullptr;
        m_compositingClipRectsDirty = false;
    } else {
        if (typeToClear == CompositingClipRects)
            m_compositingClipRectsDirty = false;

        ASSERT(typeToClear < NumCachedClipRectsTypes);
        RefPtr<ClipRects> dummy;
        m_clipRectsCache->setClipRects(typeToClear, RespectOverflowClip, dummy, 0);
        m_clipRectsCache->setClipRects(typeToClear, IgnoreOverflowClip, dummy, 0);
    }
}

LayoutRect RenderLayerClipper::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderView* renderView = m_renderer.view();
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, TemporaryClipRects);
    // Need to use temporary clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    calculateRects(clipRectsContext, renderView->unscaledDocumentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return clippingRootLayer->renderer()->localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayerClipper::localClipRect() const
{
    // FIXME: border-radius not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, PaintingClipRects);
    calculateRects(clipRectsContext, PaintInfo::infiniteRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == PaintInfo::infiniteRect())
        return clipRect;

    LayoutPoint clippingRootOffset;
    m_renderer.layer()->convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void RenderLayerClipper::calculateRects(const ClipRectsContext& clipRectsContext, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot) const
{
    bool isClippingRoot = isClippingRootForContext(clipRectsContext);

    if (!isClippingRoot && m_renderer.layer()->parent()) {
        backgroundRect = backgroundClipRect(clipRectsContext);
        backgroundRect.move(roundedIntSize(clipRectsContext.subPixelAccumulation));
        backgroundRect.intersect(paintDirtyRect);
    } else {
        backgroundRect = paintDirtyRect;
    }

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;

    LayoutPoint offset;
    if (offsetFromRoot)
        offset = *offsetFromRoot;
    else
        m_renderer.layer()->convertToLayerCoords(clipRectsContext.rootLayer, offset);
    layerBounds = LayoutRect(offset, m_renderer.layer()->size());

    // Update the clip rects that will be passed to child layers.
    if (m_renderer.hasOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (!isClippingRoot || clipRectsContext.respectOverflowClip == RespectOverflowClip) {
            foregroundRect.intersect(toRenderBox(m_renderer).overflowClipRect(offset, clipRectsContext.overlayScrollbarSizeRelevancy));
            if (m_renderer.style()->hasBorderRadius())
                foregroundRect.setHasRadius(true);
        }

        // If we establish an overflow clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (toRenderBox(m_renderer).hasVisualOverflow()) {
            // FIXME: Perhaps we should be propagating the borderbox as the clip rect for children, even though
            //        we may need to inflate our clip specifically for shadows or outsets.
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = toRenderBox(m_renderer).visualOverflowRect();
            toRenderBox(m_renderer).flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.moveBy(offset);
            if (!isClippingRoot || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            LayoutRect bounds = toRenderBox(m_renderer).borderBoxRect();
            bounds.moveBy(offset);
            if (!isClippingRoot || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(bounds);
        }
    }

    // CSS clip (different than clipping due to overflow) can clip to any box, even if it falls outside of the border box.
    if (m_renderer.hasClip()) {
        // Clip applies to *us* as well, so go ahead and update the damageRect.
        LayoutRect newPosClip = toRenderBox(m_renderer).clipRect(offset);
        backgroundRect.intersect(newPosClip);
        foregroundRect.intersect(newPosClip);
        outlineRect.intersect(newPosClip);
    }
}

void RenderLayerClipper::calculateClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    if (!m_renderer.layer()->parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(PaintInfo::infiniteRect());
        return;
    }

    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    bool useCached = clipRectsType != TemporaryClipRects;

    bool isClippingRoot = isClippingRootForContext(clipRectsContext);

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = !isClippingRoot ? m_renderer.layer()->parent() : 0;

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipper().clipRects(clipRectsContext)) {
            clipRects = *parentLayer->clipper().clipRects(clipRectsContext);
        } else {
            ClipRectsContext parentContext(clipRectsContext);
            parentContext.overlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize; // FIXME: why?
            parentLayer->clipper().calculateClipRects(parentContext, clipRects);
        }
    } else {
        clipRects.reset(PaintInfo::infiniteRect());
    }

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (m_renderer.style()->position() == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (m_renderer.style()->hasInFlowPosition()) {
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    } else if (m_renderer.style()->position() == AbsolutePosition) {
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    }

    // Update the clip rects that will be passed to child layers.
    if ((m_renderer.hasOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || !isClippingRoot)) || m_renderer.hasClip()) {
        // This layer establishes a clip of some kind.

        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the RenderLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        LayoutPoint offset;
        offset = roundedLayoutPoint(m_renderer.localToContainerPoint(FloatPoint(), clipRectsContext.rootLayer->renderer()));
        RenderView* view = m_renderer.view();
        ASSERT(view);
        if (view && clipRects.fixed() && clipRectsContext.rootLayer->renderer() == view) {
            offset -= view->frameView()->scrollOffsetForFixedPosition();
        }

        if (m_renderer.hasOverflowClip()) {
            ClipRect newOverflowClip = toRenderBox(m_renderer).overflowClipRect(offset, clipRectsContext.overlayScrollbarSizeRelevancy);
            if (m_renderer.style()->hasBorderRadius())
                newOverflowClip.setHasRadius(true);
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (m_renderer.isPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (m_renderer.hasClip()) {
            LayoutRect newPosClip = toRenderBox(m_renderer).clipRect(offset);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

static inline ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

void RenderLayerClipper::setCompositingClipRectsDirty()
{
    m_compositingClipRectsDirty = true;
}

ClipRect RenderLayerClipper::backgroundClipRect(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(m_renderer.layer()->parent());

    if (clipRectsContext.clipRectsType == CompositingClipRects)
        const_cast<RenderLayerClipper*>(this)->clearClipRectsIncludingDescendants(CompositingClipRects);

    ClipRects parentRects;

    // If we cross into a different pagination context, then we can't rely on the cache.
    // Just switch over to using TemporaryClipRects.
    if (clipRectsContext.clipRectsType != TemporaryClipRects && m_renderer.layer()->parent()->enclosingPaginationLayer() != m_renderer.layer()->enclosingPaginationLayer()) {
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        parentClipRects(tempContext, parentRects);
    } else {
        parentClipRects(clipRectsContext, parentRects);
    }

    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, m_renderer.style()->position());
    RenderView* view = m_renderer.view();
    ASSERT(view);

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects.fixed() && clipRectsContext.rootLayer->renderer() == view && backgroundClipRect != PaintInfo::infiniteRect())
        backgroundClipRect.move(view->frameView()->scrollOffsetForFixedPosition());

    return backgroundClipRect;
}

bool RenderLayerClipper::isClippingRootForContext(const ClipRectsContext& clipRectsContext) const
{
    return clipRectsContext.rootLayer == m_renderer.layer();
}

void RenderLayerClipper::parentClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    // The root is not clipped.
    if (isClippingRootForContext(clipRectsContext)) {
        clipRects.reset(PaintInfo::infiniteRect());
        return;
    }

    ASSERT(m_renderer.layer()->parent());

    RenderLayerClipper& parentClipper = m_renderer.layer()->parent()->clipper();
    if (clipRectsContext.clipRectsType == TemporaryClipRects) {
        parentClipper.calculateClipRects(clipRectsContext, clipRects);
        return;
    }

    parentClipper.updateClipRects(clipRectsContext);
    clipRects = *parentClipper.clipRects(clipRectsContext);
}

RenderLayer* RenderLayerClipper::clippingRootForPainting() const
{
    if (m_renderer.hasCompositedLayerMapping() || m_renderer.groupedMapping())
        return const_cast<RenderLayer*>(m_renderer.layer());

    const RenderLayer* current = m_renderer.layer();
    while (current) {
        if (current->isRootLayer())
            return const_cast<RenderLayer*>(current);

        current = current->compositingContainer();
        ASSERT(current);
        if (current->transform() || (current->compositingState() == PaintsIntoOwnBacking) || current->groupedMapping())
            return const_cast<RenderLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
