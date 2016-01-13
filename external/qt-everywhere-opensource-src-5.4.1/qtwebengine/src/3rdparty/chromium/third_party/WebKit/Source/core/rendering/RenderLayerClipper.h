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

#ifndef RenderLayerClipper_h
#define RenderLayerClipper_h

#include "core/rendering/ClipRect.h"
#include "core/rendering/RenderBox.h" // For OverlayScrollbarSizeRelevancy.

namespace WebCore {

class RenderLayer;

struct ClipRectsContext {
    ClipRectsContext(const RenderLayer* inRootLayer, ClipRectsType inClipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, ShouldRespectOverflowClip inRespectOverflowClip = RespectOverflowClip, const LayoutSize& inSubPixelAccumulation = LayoutSize())
        : rootLayer(inRootLayer)
        , clipRectsType(inClipRectsType)
        , overlayScrollbarSizeRelevancy(inOverlayScrollbarSizeRelevancy)
        , respectOverflowClip(inRespectOverflowClip)
        , subPixelAccumulation(inSubPixelAccumulation)
    { }
    const RenderLayer* rootLayer;
    ClipRectsType clipRectsType;
    OverlayScrollbarSizeRelevancy overlayScrollbarSizeRelevancy;
    ShouldRespectOverflowClip respectOverflowClip;
    LayoutSize subPixelAccumulation;
};

class RenderLayerClipper FINAL {
    WTF_MAKE_NONCOPYABLE(RenderLayerClipper);
public:
    explicit RenderLayerClipper(RenderLayerModelObject& renderer)
        : m_renderer(renderer)
        , m_compositingClipRectsDirty(false)
    {
    }

    ClipRects* clipRects(const ClipRectsContext& context) const
    {
        ASSERT(context.clipRectsType < NumCachedClipRectsTypes);
        return m_clipRectsCache ? m_clipRectsCache->getClipRects(context.clipRectsType, context.respectOverflowClip).get() : 0;
    }

    // Compute and cache clip rects computed with the given layer as the root
    void updateClipRects(const ClipRectsContext&);

    void clearClipRectsIncludingDescendants(ClipRectsType typeToClear = AllClipRectTypes);
    void clearClipRects(ClipRectsType typeToClear = AllClipRectTypes);

    void setCompositingClipRectsDirty();

    LayoutRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    LayoutRect localClipRect() const; // Returns the background clip rect of the layer in the local coordinate space.

    ClipRect backgroundClipRect(const ClipRectsContext&) const;

    // FIXME: The following functions should be private.

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer}. It also computes our background and foreground clip rects
    // for painting/event handling.
    // Pass offsetFromRoot if known.
    void calculateRects(const ClipRectsContext&, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
        ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot = 0) const;

    // Compute and return the clip rects. If useCached is true, will used previously computed clip rects on ancestors
    // (rather than computing them all from scratch up the parent chain).
    void calculateClipRects(const ClipRectsContext&, ClipRects&) const;

private:
    void parentClipRects(const ClipRectsContext&, ClipRects&) const;

    // The layer relative to which clipping rects for this layer are computed.
    RenderLayer* clippingRootForPainting() const;

    bool isClippingRootForContext(const ClipRectsContext&) const;

    // FIXME: Could this be a RenderBox?
    RenderLayerModelObject& m_renderer;
    OwnPtr<ClipRectsCache> m_clipRectsCache;
    unsigned m_compositingClipRectsDirty : 1;
};

} // namespace WebCore

#endif // RenderLayerClipper_h
