/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef RenderSVGText_h
#define RenderSVGText_h

#include "core/rendering/svg/RenderSVGBlock.h"
#include "core/rendering/svg/SVGTextLayoutAttributesBuilder.h"
#include "platform/transforms/AffineTransform.h"

namespace WebCore {

class RenderSVGInlineText;
class SVGTextElement;
class RenderSVGInlineText;

class RenderSVGText FINAL : public RenderSVGBlock {
public:
    explicit RenderSVGText(SVGTextElement*);
    virtual ~RenderSVGText();

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    void setNeedsPositioningValuesUpdate() { m_needsPositioningValuesUpdate = true; }
    virtual void setNeedsTransformUpdate() OVERRIDE { m_needsTransformUpdate = true; }
    void setNeedsTextMetricsUpdate() { m_needsTextMetricsUpdate = true; }
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const OVERRIDE;

    static RenderSVGText* locateRenderSVGTextAncestor(RenderObject*);
    static const RenderSVGText* locateRenderSVGTextAncestor(const RenderObject*);

    bool needsReordering() const { return m_needsReordering; }
    Vector<SVGTextLayoutAttributes*>& layoutAttributes() { return m_layoutAttributes; }

    void subtreeChildWasAdded(RenderObject*);
    void subtreeChildWillBeRemoved(RenderObject*, Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes);
    void subtreeChildWasRemoved(const Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes);
    void subtreeStyleDidChange();
    void subtreeTextDidChange(RenderSVGInlineText*);

private:
    virtual const char* renderName() const OVERRIDE { return "RenderSVGText"; }
    virtual bool isSVGText() const OVERRIDE { return true; }

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) OVERRIDE;
    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) OVERRIDE;

    virtual void layout() OVERRIDE;

    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const OVERRIDE;

    virtual void mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer, LayoutRect&, bool fixed = false) const OVERRIDE;

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;
    virtual void willBeDestroyed() OVERRIDE;

    virtual FloatRect objectBoundingBox() const OVERRIDE { return frameRect(); }
    virtual FloatRect strokeBoundingBox() const OVERRIDE;

    virtual const AffineTransform& localToParentTransform() const OVERRIDE { return m_localTransform; }
    virtual RootInlineBox* createRootInlineBox() OVERRIDE;

    virtual RenderBlock* firstLineBlock() const OVERRIDE;
    virtual void updateFirstLetter() OVERRIDE;

    bool shouldHandleSubtreeMutations() const;

    bool m_needsReordering : 1;
    bool m_needsPositioningValuesUpdate : 1;
    bool m_needsTransformUpdate : 1;
    bool m_needsTextMetricsUpdate : 1;
    SVGTextLayoutAttributesBuilder m_layoutAttributesBuilder;
    Vector<SVGTextLayoutAttributes*> m_layoutAttributes;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGText, isSVGText());

}

#endif
