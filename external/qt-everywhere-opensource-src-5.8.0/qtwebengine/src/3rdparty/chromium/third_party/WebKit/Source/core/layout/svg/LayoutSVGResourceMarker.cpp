/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/layout/svg/LayoutSVGResourceMarker.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "wtf/TemporaryChange.h"

namespace blink {

LayoutSVGResourceMarker::LayoutSVGResourceMarker(SVGMarkerElement* node)
    : LayoutSVGResourceContainer(node)
{
}

LayoutSVGResourceMarker::~LayoutSVGResourceMarker()
{
}

void LayoutSVGResourceMarker::layout()
{
    ASSERT(needsLayout());
    if (m_isInLayout)
        return;

    TemporaryChange<bool> inLayoutChange(m_isInLayout, true);

    // LayoutSVGHiddenContainer overwrites layout(). We need the
    // layouting of LayoutSVGContainer for calculating  local
    // transformations and paint invalidation.
    LayoutSVGContainer::layout();

    clearInvalidationMask();
}

void LayoutSVGResourceMarker::removeAllClientsFromCache(bool markForInvalidation)
{
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void LayoutSVGResourceMarker::removeClientFromCache(LayoutObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

FloatRect LayoutSVGResourceMarker::markerBoundaries(const AffineTransform& markerTransformation) const
{
    FloatRect coordinates = LayoutSVGContainer::paintInvalidationRectInLocalSVGCoordinates();

    // Map paint invalidation rect into parent coordinate space, in which the
    // marker boundaries have to be evaluated.
    coordinates = localToSVGParentTransform().mapRect(coordinates);

    return markerTransformation.mapRect(coordinates);
}

const AffineTransform& LayoutSVGResourceMarker::localToSVGParentTransform() const
{
    m_localToParentTransform = AffineTransform::translation(m_viewport.x(), m_viewport.y()) * viewportTransform();
    return m_localToParentTransform;
    // If this class were ever given a localTransform(), then the above would read:
    // return viewportTranslation * localTransform() * viewportTransform();
}

FloatPoint LayoutSVGResourceMarker::referencePoint() const
{
    SVGMarkerElement* marker = toSVGMarkerElement(element());
    ASSERT(marker);

    SVGLengthContext lengthContext(marker);
    return FloatPoint(marker->refX()->currentValue()->value(lengthContext), marker->refY()->currentValue()->value(lengthContext));
}

float LayoutSVGResourceMarker::angle() const
{
    return toSVGMarkerElement(element())->orientAngle()->currentValue()->value();
}

SVGMarkerUnitsType LayoutSVGResourceMarker::markerUnits() const
{
    return toSVGMarkerElement(element())->markerUnits()->currentValue()->enumValue();
}

SVGMarkerOrientType LayoutSVGResourceMarker::orientType() const
{
    return toSVGMarkerElement(element())->orientType()->currentValue()->enumValue();
}

AffineTransform LayoutSVGResourceMarker::markerTransformation(const FloatPoint& origin, float autoAngle, float strokeWidth) const
{
    // Apply scaling according to markerUnits ('strokeWidth' or 'userSpaceOnUse'.)
    float markerScale = markerUnits() == SVGMarkerUnitsStrokeWidth ? strokeWidth : 1;

    AffineTransform transform;
    transform.translate(origin.x(), origin.y());
    transform.rotate(orientType() == SVGMarkerOrientAngle ? angle() : autoAngle);
    transform.scale(markerScale);

    // The 'origin' coordinate maps to SVGs refX/refY, given in coordinates
    // relative to the viewport established by the marker.
    FloatPoint mappedReferencePoint = viewportTransform().mapPoint(referencePoint());
    transform.translate(-mappedReferencePoint.x(), -mappedReferencePoint.y());
    return transform;
}

bool LayoutSVGResourceMarker::shouldPaint() const
{
    // An empty viewBox disables rendering.
    SVGMarkerElement* marker = toSVGMarkerElement(element());
    ASSERT(marker);
    return !marker->viewBox()->isSpecified()
        || !marker->viewBox()->currentValue()->isValid()
        || !marker->viewBox()->currentValue()->value().isEmpty();
}

AffineTransform LayoutSVGResourceMarker::viewportTransform() const
{
    SVGMarkerElement* marker = toSVGMarkerElement(element());
    ASSERT(marker);

    return marker->viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
}

void LayoutSVGResourceMarker::calcViewport()
{
    if (!selfNeedsLayout())
        return;

    SVGMarkerElement* marker = toSVGMarkerElement(element());
    ASSERT(marker);

    SVGLengthContext lengthContext(marker);
    float w = marker->markerWidth()->currentValue()->value(lengthContext);
    float h = marker->markerHeight()->currentValue()->value(lengthContext);
    m_viewport = FloatRect(0, 0, w, h);
}

SVGTransformChange LayoutSVGResourceMarker::calculateLocalTransform()
{
    // TODO(fs): Temporarily, needing a layout implies that the local transform
    // has changed. This should be updated to be more precise and factor in the
    // actual (relevant) changes to the computed user-space transform.
    return selfNeedsLayout() ? SVGTransformChange::Full : SVGTransformChange::None;
}

} // namespace blink
