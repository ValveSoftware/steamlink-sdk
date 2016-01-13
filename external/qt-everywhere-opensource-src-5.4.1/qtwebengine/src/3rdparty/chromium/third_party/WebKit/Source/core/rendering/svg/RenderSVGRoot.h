/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#include "core/rendering/RenderReplaced.h"
#include "platform/geometry/FloatRect.h"

namespace WebCore {

class SVGElement;

class RenderSVGRoot FINAL : public RenderReplaced {
public:
    explicit RenderSVGRoot(SVGElement*);
    virtual ~RenderSVGRoot();

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const OVERRIDE;

    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a RenderSVGRoot, use firstChild or lastChild instead.
    void slowFirstChild() const WTF_DELETED_FUNCTION;
    void slowLastChild() const WTF_DELETED_FUNCTION;

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    virtual void setNeedsBoundariesUpdate() OVERRIDE { m_needsBoundariesOrTransformUpdate = true; }
    virtual void setNeedsTransformUpdate() OVERRIDE { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize)
    {
        // SVGImage::draw() does a view layout prior to painting,
        // and we need that layout to know of the new size otherwise
        // the rendering may be incorrectly using the old size.
        if (m_containerSize != containerSize)
            setNeedsLayoutAndFullPaintInvalidation();
        m_containerSize = containerSize;
    }

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }

private:
    virtual RenderObjectChildList* virtualChildren() OVERRIDE { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const OVERRIDE { return children(); }

    virtual const char* renderName() const OVERRIDE { return "RenderSVGRoot"; }
    virtual bool isSVGRoot() const OVERRIDE { return true; }
    virtual bool isSVG() const OVERRIDE { return true; }

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const OVERRIDE;
    virtual LayoutUnit computeReplacedLogicalHeight() const OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) OVERRIDE;

    virtual void willBeDestroyed() OVERRIDE;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;
    virtual bool canHaveWhitespaceChildren() const OVERRIDE { return false; }

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    virtual const AffineTransform& localToParentTransform() const OVERRIDE;

    virtual FloatRect objectBoundingBox() const OVERRIDE { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const OVERRIDE { return m_strokeBoundingBox; }
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const OVERRIDE { return m_repaintBoundingBox; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const OVERRIDE;
    virtual void computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect& paintInvalidationRect, bool fixed) const OVERRIDE;

    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE;

    virtual bool canBeSelectionLeaf() const OVERRIDE { return false; }
    virtual bool canHaveChildren() const OVERRIDE { return true; }

    bool shouldApplyViewportClip() const;
    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    RenderObjectChildList m_children;
    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_hasBoxDecorations : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGRoot, isSVGRoot());

} // namespace WebCore

#endif // RenderSVGRoot_h
