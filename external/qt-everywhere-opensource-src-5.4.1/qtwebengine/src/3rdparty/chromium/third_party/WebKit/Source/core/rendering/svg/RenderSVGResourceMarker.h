/*
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

#ifndef RenderSVGResourceMarker_h
#define RenderSVGResourceMarker_h

#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGMarkerElement.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/HashSet.h"

namespace WebCore {

class RenderObject;

class RenderSVGResourceMarker FINAL : public RenderSVGResourceContainer {
public:
    explicit RenderSVGResourceMarker(SVGMarkerElement*);
    virtual ~RenderSVGResourceMarker();

    virtual const char* renderName() const OVERRIDE { return "RenderSVGResourceMarker"; }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) OVERRIDE;
    virtual void removeClientFromCache(RenderObject*, bool markForInvalidation = true) OVERRIDE;

    void draw(PaintInfo&, const AffineTransform&);

    // Calculates marker boundaries, mapped to the target element's coordinate space
    FloatRect markerBoundaries(const AffineTransform& markerTransformation) const;

    virtual void applyViewportClip(PaintInfo&) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void calcViewport() OVERRIDE;

    virtual const AffineTransform& localToParentTransform() const OVERRIDE;
    AffineTransform markerTransformation(const FloatPoint& origin, float angle, float strokeWidth) const;

    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*&, unsigned short) OVERRIDE { return false; }

    FloatPoint referencePoint() const;
    float angle() const;
    SVGMarkerUnitsType markerUnits() const { return toSVGMarkerElement(element())->markerUnits()->currentValue()->enumValue(); }

    virtual RenderSVGResourceType resourceType() const OVERRIDE { return s_resourceType; }
    static const RenderSVGResourceType s_resourceType;

private:
    // Generates a transformation matrix usable to render marker content. Handles scaling the marker content
    // acording to SVGs markerUnits="strokeWidth" concept, when a strokeWidth value != -1 is passed in.
    AffineTransform markerContentTransformation(const AffineTransform& contentTransformation, const FloatPoint& origin, float strokeWidth = -1) const;

    AffineTransform viewportTransform() const;

    mutable AffineTransform m_localToParentTransform;
    FloatRect m_viewport;
};

DEFINE_RENDER_SVG_RESOURCE_TYPE_CASTS(RenderSVGResourceMarker, MarkerResourceType);

}

#endif
