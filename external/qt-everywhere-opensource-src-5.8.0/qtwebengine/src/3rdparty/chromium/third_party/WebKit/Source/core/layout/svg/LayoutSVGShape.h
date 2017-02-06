/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Computer, Inc
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#ifndef LayoutSVGShape_h
#define LayoutSVGShape_h

#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/layout/svg/SVGMarkerData.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class FloatPoint;
class PointerEventsHitRules;
class SVGGeometryElement;

enum ShapeGeometryCodePath {
    PathGeometry,
    RectGeometryFastPath,
    EllipseGeometryFastPath
};

struct LayoutSVGShapeRareData {
    WTF_MAKE_NONCOPYABLE(LayoutSVGShapeRareData);
    USING_FAST_MALLOC(LayoutSVGShapeRareData);
public:
    LayoutSVGShapeRareData() {}
    Path m_cachedNonScalingStrokePath;
    AffineTransform m_cachedNonScalingStrokeTransform;
};

class LayoutSVGShape : public LayoutSVGModelObject {
public:
    explicit LayoutSVGShape(SVGGeometryElement*);
    ~LayoutSVGShape() override;

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    void setNeedsBoundariesUpdate() final { m_needsBoundariesUpdate = true; }
    void setNeedsTransformUpdate() final { m_needsTransformUpdate = true; }

    bool nodeAtFloatPointInternal(const HitTestRequest&, const FloatPoint&, PointerEventsHitRules);

    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }
    bool hasPath() const { return m_path.get(); }
    float dashScaleFactor() const;

    virtual bool isShapeEmpty() const { return path().isEmpty(); }

    bool hasNonScalingStroke() const { return style()->svgStyle().vectorEffect() == VE_NON_SCALING_STROKE; }
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;
    AffineTransform nonScalingStrokeTransform() const;
    AffineTransform localSVGTransform() const final { return m_localTransform; }

    virtual const Vector<MarkerPosition>* markerPositions() const { return nullptr; }

    float strokeWidth() const;

    virtual ShapeGeometryCodePath geometryCodePath() const { return PathGeometry; }

    FloatRect objectBoundingBox() const final { return m_fillBoundingBox; }

    const char* name() const override { return "LayoutSVGShape"; }

protected:
    void clearPath() { m_path.reset(); }
    void createPath();

    virtual void updateShapeFromElement();
    // Calculates an inclusive bounding box of this shape as if this shape has
    // a stroke. If this shape has a stroke, then m_strokeBoundingBox is returned;
    // otherwise, estimates a bounding box (not necessarily tight) that would
    // include this shape's stroke bounding box if it had a stroke.
    virtual FloatRect hitTestStrokeBoundingBox() const;
    virtual bool shapeDependentStrokeContains(const FloatPoint&);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;

    FloatRect m_fillBoundingBox;
    FloatRect m_strokeBoundingBox;
    LayoutSVGShapeRareData& ensureRareData() const;

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = RULE_NONZERO);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    const AffineTransform& localToSVGParentTransform() const final { return m_localTransform; }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGShape || LayoutSVGModelObject::isOfType(type); }
    void layout() final;
    void paint(const PaintInfo&, const LayoutPoint&) const final;
    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const final;

    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) final;

    FloatRect strokeBoundingBox() const final { return m_strokeBoundingBox; }
    FloatRect calculateObjectBoundingBox() const;
    FloatRect calculateStrokeBoundingBox() const;
    void updateLocalTransform();

private:
    AffineTransform m_localTransform;
    // TODO(fmalita): the Path is now cached in SVGPath; while this additional cache is just a
    // shallow copy, it certainly has a complexity/state management cost (plus allocation & storage
    // overhead) - so we should look into removing it.
    std::unique_ptr<Path> m_path;
    mutable std::unique_ptr<LayoutSVGShapeRareData> m_rareData;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGShape, isSVGShape());

} // namespace blink

#endif
