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

#ifndef LayoutSVGRoot_h
#define LayoutSVGRoot_h

#include "core/layout/LayoutReplaced.h"

namespace blink {

class SVGElement;

class CORE_EXPORT LayoutSVGRoot final : public LayoutReplaced {
public:
    explicit LayoutSVGRoot(SVGElement*);
    ~LayoutSVGRoot() override;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    void computeIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;

    // If you have a LayoutSVGRoot, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didScreenScaleFactorChange() const { return m_didScreenScaleFactorChange; }
    void setNeedsBoundariesUpdate() override { m_needsBoundariesOrTransformUpdate = true; }
    void setNeedsTransformUpdate() override { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize)
    {
        // SVGImage::draw() does a view layout prior to painting,
        // and we need that layout to know of the new size otherwise
        // the layout may be incorrectly using the old size.
        if (m_containerSize != containerSize)
            setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SizeChanged);
        m_containerSize = containerSize;
    }

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }
    bool shouldApplyViewportClip() const;

    LayoutRect visualOverflowRect() const override;

    bool hasNonIsolatedBlendingDescendants() const final;

    const char* name() const override { return "LayoutSVGRoot"; }

private:
    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    LayoutObjectChildList* virtualChildren() override { return children(); }
    const LayoutObjectChildList* virtualChildren() const override { return children(); }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || type == LayoutObjectSVGRoot || LayoutReplaced::isOfType(type); }

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight(LayoutUnit estimatedUsedWidth = LayoutUnit()) const override;
    void layout() override;
    void paintReplaced(const PaintInfo&, const LayoutPoint&) const override;

    void willBeDestroyed() override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;
    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    const AffineTransform& localToSVGParentTransform() const override;

    FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const override { return m_strokeBoundingBox; }
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const override { return m_paintInvalidationBoundingBox; }

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    LayoutRect localOverflowRectForPaintInvalidation() const override;

    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const override;
    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const override;

    bool canBeSelectionLeaf() const override { return false; }
    bool canHaveChildren() const override { return true; }

    void descendantIsolationRequirementsChanged(DescendantIsolationState) final;

    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    PositionWithAffinity positionForPoint(const LayoutPoint&) final;

    LayoutObjectChildList m_children;
    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_paintInvalidationBoundingBox;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    bool m_isLayoutSizeChanged : 1;
    bool m_didScreenScaleFactorChange : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_hasBoxDecorationBackground : 1;
    mutable bool m_hasNonIsolatedBlendingDescendants : 1;
    mutable bool m_hasNonIsolatedBlendingDescendantsDirty : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGRoot, isSVGRoot());

} // namespace blink

#endif // LayoutSVGRoot_h
