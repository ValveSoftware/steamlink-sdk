/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
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

#include "core/layout/svg/LayoutSVGShape.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGShapePainter.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGPathElement.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/StrokeData.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"

namespace blink {

LayoutSVGShape::LayoutSVGShape(SVGGeometryElement* node)
    : LayoutSVGModelObject(node)
    , m_needsBoundariesUpdate(false) // Default is false, the cached rects are empty from the beginning.
    , m_needsShapeUpdate(true) // Default is true, so we grab a Path object once from SVGGeometryElement.
    , m_needsTransformUpdate(true) // Default is true, so we grab a AffineTransform object once from SVGGeometryElement.
{
}

LayoutSVGShape::~LayoutSVGShape()
{
}

void LayoutSVGShape::createPath()
{
    if (!m_path)
        m_path = wrapUnique(new Path());
    *m_path = toSVGGeometryElement(element())->asPath();
    if (m_rareData.get())
        m_rareData->m_cachedNonScalingStrokePath.clear();
}

float LayoutSVGShape::dashScaleFactor() const
{
    if (!isSVGPathElement(element())
        || !styleRef().svgStyle().strokeDashArray()->size())
        return 1;
    return toSVGPathElement(*element()).pathLengthScaleFactor();
}

void LayoutSVGShape::updateShapeFromElement()
{
    createPath();

    m_fillBoundingBox = calculateObjectBoundingBox();
    m_strokeBoundingBox = calculateStrokeBoundingBox();
}

FloatRect LayoutSVGShape::hitTestStrokeBoundingBox() const
{
    if (style()->svgStyle().hasStroke())
        return m_strokeBoundingBox;

    // Implementation of http://dev.w3.org/fxtf/css-masking-1/#compute-stroke-bounding-box
    // for the <rect> / <ellipse> / <circle> case except that we ignore whether
    // the stroke is none.

    FloatRect box = m_fillBoundingBox;
    const float strokeWidth = this->strokeWidth();
    box.inflate(strokeWidth / 2);
    return box;
}

bool LayoutSVGShape::shapeDependentStrokeContains(const FloatPoint& point)
{
    ASSERT(m_path);
    StrokeData strokeData;
    SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, styleRef(), *this, dashScaleFactor());

    if (hasNonScalingStroke()) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);

        return usePath->strokeContains(nonScalingTransform.mapPoint(point), strokeData);
    }

    return m_path->strokeContains(point, strokeData);
}

bool LayoutSVGShape::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    return path().contains(point, fillRule);
}

bool LayoutSVGShape::fillContains(const FloatPoint& point, bool requiresFill, const WindRule fillRule)
{
    if (!m_fillBoundingBox.contains(point))
        return false;

    if (requiresFill && !SVGPaintServer::existsForLayoutObject(*this, styleRef(), ApplyToFillMode))
        return false;

    return shapeDependentFillContains(point, fillRule);
}

bool LayoutSVGShape::strokeContains(const FloatPoint& point, bool requiresStroke)
{
    if (requiresStroke) {
        if (!strokeBoundingBox().contains(point))
            return false;

        if (!SVGPaintServer::existsForLayoutObject(*this, styleRef(), ApplyToStrokeMode))
            return false;
    } else {
        if (!hitTestStrokeBoundingBox().contains(point))
            return false;
    }

    return shapeDependentStrokeContains(point);
}

void LayoutSVGShape::updateLocalTransform()
{
    SVGGraphicsElement* graphicsElement = toSVGGraphicsElement(element());
    if (graphicsElement->hasAnimatedLocalTransform()) {
        m_localTransform.setTransform(graphicsElement->calculateAnimatedLocalTransform());
    } else {
        m_localTransform = AffineTransform();
    }
}

void LayoutSVGShape::layout()
{
    LayoutAnalyzer::Scope analyzer(*this);

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    bool updateParentBoundaries = false;
    // updateShapeFromElement() also updates the object & stroke bounds - which
    // feeds into the paint invalidation rect - so we need to call it for both
    // the shape-update and the bounds-update flag, since .
    if (m_needsShapeUpdate || m_needsBoundariesUpdate) {
        updateShapeFromElement();
        m_needsShapeUpdate = false;

        m_paintInvalidationBoundingBox = strokeBoundingBox();
        SVGLayoutSupport::intersectPaintInvalidationRectWithResources(this, m_paintInvalidationBoundingBox);
        m_needsBoundariesUpdate = false;

        updateParentBoundaries = true;
    }

    if (m_needsTransformUpdate) {
        updateLocalTransform();
        m_needsTransformUpdate = false;
        updateParentBoundaries = true;
    }

    // If our bounds changed, notify the parents.
    if (updateParentBoundaries)
        LayoutSVGModelObject::setNeedsBoundariesUpdate();

    ASSERT(!m_needsShapeUpdate);
    ASSERT(!m_needsBoundariesUpdate);
    ASSERT(!m_needsTransformUpdate);
    clearNeedsLayout();
}

Path* LayoutSVGShape::nonScalingStrokePath(const Path* path, const AffineTransform& strokeTransform) const
{
    LayoutSVGShapeRareData& rareData = ensureRareData();
    if (!rareData.m_cachedNonScalingStrokePath.isEmpty() && strokeTransform == rareData.m_cachedNonScalingStrokeTransform)
        return &rareData.m_cachedNonScalingStrokePath;

    rareData.m_cachedNonScalingStrokePath = *path;
    rareData.m_cachedNonScalingStrokePath.transform(strokeTransform);
    rareData.m_cachedNonScalingStrokeTransform = strokeTransform;
    return &rareData.m_cachedNonScalingStrokePath;
}

AffineTransform LayoutSVGShape::nonScalingStrokeTransform() const
{
    AffineTransform t = toSVGGraphicsElement(element())->getScreenCTM(SVGGraphicsElement::DisallowStyleUpdate);
    // Width of non-scaling stroke is independent of translation, so zero it out here.
    t.setE(0);
    t.setF(0);
    return t;
}

void LayoutSVGShape::paint(const PaintInfo& paintInfo, const LayoutPoint&) const
{
    SVGShapePainter(*this).paint(paintInfo);
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void LayoutSVGShape::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint&, IncludeBlockVisualOverflowOrNot) const
{
    rects.append(LayoutRect(paintInvalidationRectInLocalSVGCoordinates()));
}

bool LayoutSVGShape::nodeAtFloatPoint(HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the foreground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint;
    if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(this, localToSVGParentTransform(), pointInParent, localPoint))
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING, result.hitTestRequest(), style()->pointerEvents());
    if (nodeAtFloatPointInternal(result.hitTestRequest(), localPoint, hitRules)) {
        const LayoutPoint& localLayoutPoint = roundedLayoutPoint(localPoint);
        updateHitTestResult(result, localLayoutPoint);
        if (result.addNodeToListBasedTestResult(element(), localLayoutPoint) == StopHitTesting)
            return true;
    }

    return false;
}

bool LayoutSVGShape::nodeAtFloatPointInternal(const HitTestRequest& request, const FloatPoint& localPoint, PointerEventsHitRules hitRules)
{
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        const SVGComputedStyle& svgStyle = style()->svgStyle();
        WindRule fillRule = svgStyle.fillRule();
        if (request.svgClipContent())
            fillRule = svgStyle.clipRule();
        if ((hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint))
            || (hitRules.canHitStroke && (svgStyle.hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (svgStyle.hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill, fillRule)))
            return true;
    }
    return false;
}

FloatRect LayoutSVGShape::calculateObjectBoundingBox() const
{
    return path().boundingRect();
}

FloatRect LayoutSVGShape::calculateStrokeBoundingBox() const
{
    ASSERT(m_path);
    FloatRect strokeBoundingBox = m_fillBoundingBox;

    if (style()->svgStyle().hasStroke()) {
        StrokeData strokeData;
        SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, styleRef(), *this, dashScaleFactor());
        if (hasNonScalingStroke()) {
            AffineTransform nonScalingTransform = nonScalingStrokeTransform();
            if (nonScalingTransform.isInvertible()) {
                Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);
                FloatRect strokeBoundingRect = usePath->strokeBoundingRect(strokeData);
                strokeBoundingRect = nonScalingTransform.inverse().mapRect(strokeBoundingRect);
                strokeBoundingBox.unite(strokeBoundingRect);
            }
        } else {
            strokeBoundingBox.unite(path().strokeBoundingRect(strokeData));
        }
    }

    return strokeBoundingBox;
}

float LayoutSVGShape::strokeWidth() const
{
    SVGLengthContext lengthContext(element());
    return lengthContext.valueForLength(style()->svgStyle().strokeWidth());
}

LayoutSVGShapeRareData& LayoutSVGShape::ensureRareData() const
{
    if (!m_rareData)
        m_rareData = wrapUnique(new LayoutSVGShapeRareData());
    return *m_rareData.get();
}

} // namespace blink
