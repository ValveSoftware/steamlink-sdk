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

#ifndef LayoutSVGText_h
#define LayoutSVGText_h

#include "core/layout/svg/LayoutSVGBlock.h"

namespace blink {

class LayoutSVGInlineText;
class SVGTextElement;

class LayoutSVGText final : public LayoutSVGBlock {
public:
    explicit LayoutSVGText(SVGTextElement*);
    ~LayoutSVGText() override;

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    void setNeedsPositioningValuesUpdate() { m_needsPositioningValuesUpdate = true; }
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }
    void setNeedsTextMetricsUpdate() { m_needsTextMetricsUpdate = true; }
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const override;
    FloatRect objectBoundingBox() const override { return FloatRect(frameRect()); }
    FloatRect strokeBoundingBox() const override;
    bool isObjectBoundingBoxValid() const;

    static LayoutSVGText* locateLayoutSVGTextAncestor(LayoutObject*);
    static const LayoutSVGText* locateLayoutSVGTextAncestor(const LayoutObject*);

    bool needsReordering() const { return m_needsReordering; }
    const Vector<LayoutSVGInlineText*>& descendantTextNodes() const { return m_descendantTextNodes; }

    void subtreeChildWasAdded();
    void subtreeChildWillBeRemoved();
    void subtreeTextDidChange();

    const AffineTransform& localToSVGParentTransform() const override { return m_localTransform; }

    const char* name() const override { return "LayoutSVGText"; }

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGText || LayoutSVGBlock::isOfType(type); }

    void paint(const PaintInfo&, const LayoutPoint&) const override;
    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    void layout() override;

    void absoluteQuads(Vector<FloatQuad>&) const override;

    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;
    void willBeDestroyed() override;

    void invalidateTreeIfNeeded(const PaintInvalidationState&) override;

    RootInlineBox* createRootInlineBox() override;

    void invalidatePositioningValues(LayoutInvalidationReasonForTracing);

    bool m_needsReordering : 1;
    bool m_needsPositioningValuesUpdate : 1;
    bool m_needsTransformUpdate : 1;
    bool m_needsTextMetricsUpdate : 1;
    Vector<LayoutSVGInlineText*> m_descendantTextNodes;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGText, isSVGText());

} // namespace blink

#endif
