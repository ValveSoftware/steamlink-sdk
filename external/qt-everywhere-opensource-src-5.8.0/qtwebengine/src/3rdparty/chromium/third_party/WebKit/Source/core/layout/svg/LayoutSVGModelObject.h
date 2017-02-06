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

#ifndef LayoutSVGModelObject_h
#define LayoutSVGModelObject_h

#include "core/layout/LayoutObject.h"
#include "core/svg/SVGElement.h"

namespace blink {

// Most layoutObjects in the SVG layout tree will inherit from this class
// but not all. (e.g. LayoutSVGForeignObject, LayoutSVGBlock) thus methods
// required by SVG layoutObjects need to be declared on LayoutObject, but shared
// logic can go in this class or in SVGLayoutSupport.

class LayoutSVGModelObject : public LayoutObject {
public:
    explicit LayoutSVGModelObject(SVGElement*);

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    LayoutRect absoluteClippedOverflowRect() const override;
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const override { return m_paintInvalidationBoundingBox; }

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&) const override;

    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const final;
    void mapAncestorToLocal(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const final;
    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const final;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void computeLayerHitTestRects(LayerHitTestRects&) const final;

    SVGElement* element() const { return toSVGElement(LayoutObject::node()); }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || LayoutObject::isOfType(type); }

protected:
    void addLayerHitTestRects(LayerHitTestRects&, const PaintLayer* currentCompositedLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const final;
    void willBeDestroyed() override;

    PaintInvalidationReason getPaintInvalidationReason(const PaintInvalidationState&, const LayoutRect&, const LayoutPoint&, const LayoutRect&, const LayoutPoint&) const final;
private:
    // LayoutSVGModelObject subclasses should use element() instead.
    void node() const = delete;

    // This method should never be called, SVG uses a different nodeAtPoint method
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;
    IntRect absoluteElementBoundingBoxRect() const final;

protected:
    FloatRect m_paintInvalidationBoundingBox;
};

} // namespace blink

#endif
