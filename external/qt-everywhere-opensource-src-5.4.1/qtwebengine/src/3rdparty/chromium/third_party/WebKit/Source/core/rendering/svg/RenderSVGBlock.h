/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef RenderSVGBlock_h
#define RenderSVGBlock_h

#include "core/rendering/RenderBlockFlow.h"

namespace WebCore {

class SVGElement;

class RenderSVGBlock : public RenderBlockFlow {
public:
    explicit RenderSVGBlock(SVGElement*);

    virtual LayoutRect visualOverflowRect() const OVERRIDE FINAL;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const OVERRIDE FINAL;
    virtual void computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect&, bool fixed = false) const OVERRIDE FINAL;

    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE FINAL;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE FINAL;

    virtual AffineTransform localTransform() const OVERRIDE FINAL { return m_localTransform; }

    virtual LayerType layerTypeRequired() const OVERRIDE FINAL { return NoLayer; }

    virtual void invalidateTreeAfterLayout(const RenderLayerModelObject&) OVERRIDE;

protected:
    virtual void willBeDestroyed() OVERRIDE;

    AffineTransform m_localTransform;

private:
    virtual void updateFromStyle() OVERRIDE FINAL;

    virtual bool isSVG() const OVERRIDE FINAL { return true; }

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE FINAL;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE FINAL;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;
};

}
#endif
