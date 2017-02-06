/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
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

#ifndef LayoutSVGForeignObject_h
#define LayoutSVGForeignObject_h

#include "core/layout/svg/LayoutSVGBlock.h"

namespace blink {

class SVGForeignObjectElement;

// LayoutSVGForeignObject is the LayoutObject associated with <foreignobject>.
// http://www.w3.org/TR/SVG/extend.html#ForeignObjectElement
//
// Foreign object is a way of inserting arbitrary non-SVG content into SVG.
// A good example of this is HTML in SVG. Because of this, CSS content has to
// be aware of SVG: e.g. when determining containing blocks we stop at the
// enclosing foreign object (see LayoutObject::canContainFixedPositionObjects).
//
// Note that SVG is also allowed in HTML with the HTML5 parsing rules so SVG
// content also has to be aware of CSS objects.
// See http://www.w3.org/TR/html5/syntax.html#elements-0 with the rules for
// 'foreign elements'. TODO(jchaffraix): Find a better place for this paragraph.
class LayoutSVGForeignObject final : public LayoutSVGBlock {
public:
    explicit LayoutSVGForeignObject(SVGForeignObjectElement*);
    ~LayoutSVGForeignObject() override;

    const char* name() const override { return "LayoutSVGForeignObject"; }

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    void paint(const PaintInfo&, const LayoutPoint&) const override;

    void layout() override;

    FloatRect objectBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect strokeBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const override { return FloatRect(FloatPoint(), m_viewport.size()); }

    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGForeignObject || LayoutSVGBlock::isOfType(type); }

    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    FloatRect viewportRect() const { return m_viewport; }

private:
    void updateLogicalWidth() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    const AffineTransform& localToSVGParentTransform() const override;

    bool m_needsTransformUpdate : 1;
    FloatRect m_viewport;
    mutable AffineTransform m_localToParentTransform;
};

} // namespace blink

#endif
