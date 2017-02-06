/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
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

#ifndef LayoutSVGInline_h
#define LayoutSVGInline_h

#include "core/layout/LayoutInline.h"

namespace blink {

class LayoutSVGInline : public LayoutInline {
public:
    explicit LayoutSVGInline(Element*);

    const char* name() const override { return "LayoutSVGInline"; }
    PaintLayerType layerTypeRequired() const final { return NoPaintLayer; }
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || type == LayoutObjectSVGInline || LayoutInline::isOfType(type); }

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    // Chapter 10.4 of the SVG Specification say that we should use the
    // object bounding box of the parent text element.
    // We search for the root text element and take its bounding box.
    // It is also necessary to take the stroke and paint invalidation rect of
    // this element, since we need it for filters.
    FloatRect objectBoundingBox() const final;
    FloatRect strokeBoundingBox() const final;
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const final;

    LayoutRect absoluteClippedOverflowRect() const final;
    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const final;
    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const final;
    void absoluteQuads(Vector<FloatQuad>&) const final;

private:
    InlineFlowBox* createInlineFlowBox() final;

    void invalidateTreeIfNeeded(const PaintInvalidationState&) final;

    void willBeDestroyed() final;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) final;

    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) final;
    void removeChild(LayoutObject*) final;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGInline, isSVGInline());

} // namespace blink

#endif // LayoutSVGInline_H
