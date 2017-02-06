/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "core/style/SVGComputedStyle.h"

namespace blink {

static const int kPaintOrderBitwidth = 2;

SVGComputedStyle::SVGComputedStyle()
{
    static SVGComputedStyle* initialStyle = new SVGComputedStyle(CreateInitial);

    fill = initialStyle->fill;
    stroke = initialStyle->stroke;
    stops = initialStyle->stops;
    misc = initialStyle->misc;
    inheritedResources = initialStyle->inheritedResources;
    geometry = initialStyle->geometry;
    resources = initialStyle->resources;

    setBitDefaults();
}

SVGComputedStyle::SVGComputedStyle(CreateInitialType)
{
    setBitDefaults();

    fill.init();
    stroke.init();
    stops.init();
    misc.init();
    inheritedResources.init();
    geometry.init();
    resources.init();
}

SVGComputedStyle::SVGComputedStyle(const SVGComputedStyle& other)
    : RefCounted<SVGComputedStyle>()
{
    fill = other.fill;
    stroke = other.stroke;
    stops = other.stops;
    misc = other.misc;
    inheritedResources = other.inheritedResources;
    geometry = other.geometry;
    resources = other.resources;

    svg_inherited_flags = other.svg_inherited_flags;
    svg_noninherited_flags = other.svg_noninherited_flags;
}

SVGComputedStyle::~SVGComputedStyle()
{
}

bool SVGComputedStyle::operator==(const SVGComputedStyle& other) const
{
    return fill == other.fill
        && stroke == other.stroke
        && stops == other.stops
        && misc == other.misc
        && inheritedResources == other.inheritedResources
        && geometry == other.geometry
        && resources == other.resources
        && svg_inherited_flags == other.svg_inherited_flags
        && svg_noninherited_flags == other.svg_noninherited_flags;
}

bool SVGComputedStyle::inheritedNotEqual(const SVGComputedStyle* other) const
{
    return fill != other->fill
        || stroke != other->stroke
        || inheritedResources != other->inheritedResources
        || svg_inherited_flags != other->svg_inherited_flags;
}

void SVGComputedStyle::inheritFrom(const SVGComputedStyle* svgInheritParent)
{
    if (!svgInheritParent)
        return;

    fill = svgInheritParent->fill;
    stroke = svgInheritParent->stroke;
    inheritedResources = svgInheritParent->inheritedResources;

    svg_inherited_flags = svgInheritParent->svg_inherited_flags;
}

void SVGComputedStyle::copyNonInheritedFromCached(const SVGComputedStyle* other)
{
    svg_noninherited_flags = other->svg_noninherited_flags;
    stops = other->stops;
    misc = other->misc;
    geometry = other->geometry;
    resources = other->resources;
}

PassRefPtr<SVGDashArray> SVGComputedStyle::initialStrokeDashArray()
{
    DEFINE_STATIC_REF(SVGDashArray, initialDashArray, SVGDashArray::create());
    return initialDashArray;
}

StyleDifference SVGComputedStyle::diff(const SVGComputedStyle* other) const
{
    StyleDifference styleDifference;

    if (diffNeedsLayoutAndPaintInvalidation(other)) {
        styleDifference.setNeedsFullLayout();
        styleDifference.setNeedsPaintInvalidationObject();
    } else if (diffNeedsPaintInvalidation(other)) {
        styleDifference.setNeedsPaintInvalidationObject();
    }

    return styleDifference;
}

bool SVGComputedStyle::diffNeedsLayoutAndPaintInvalidation(const SVGComputedStyle* other) const
{
    // If resources change, we need a relayout, as the presence of resources influences the paint invalidation rect.
    if (resources != other->resources)
        return true;

    // If markers change, we need a relayout, as marker boundaries are cached in LayoutSVGPath.
    if (inheritedResources != other->inheritedResources)
        return true;

    // All text related properties influence layout.
    if (svg_inherited_flags.textAnchor != other->svg_inherited_flags.textAnchor
        || svg_inherited_flags.dominantBaseline != other->svg_inherited_flags.dominantBaseline
        || svg_noninherited_flags.f.alignmentBaseline != other->svg_noninherited_flags.f.alignmentBaseline
        || svg_noninherited_flags.f.baselineShift != other->svg_noninherited_flags.f.baselineShift)
        return true;

    // Text related properties influence layout.
    if (misc->baselineShiftValue != other->misc->baselineShiftValue)
        return true;

    // These properties affect the cached stroke bounding box rects.
    if (svg_inherited_flags.capStyle != other->svg_inherited_flags.capStyle
        || svg_inherited_flags.joinStyle != other->svg_inherited_flags.joinStyle)
        return true;

    // vector-effect changes require a re-layout.
    if (svg_noninherited_flags.f.vectorEffect != other->svg_noninherited_flags.f.vectorEffect)
        return true;

    // Some stroke properties, requires relayouts, as the cached stroke boundaries need to be recalculated.
    if (stroke.get() != other->stroke.get()) {
        if (stroke->width != other->stroke->width
            || stroke->paintType != other->stroke->paintType
            || stroke->paintColor != other->stroke->paintColor
            || stroke->paintUri != other->stroke->paintUri
            || stroke->miterLimit != other->stroke->miterLimit
            || *stroke->dashArray != *other->stroke->dashArray
            || stroke->dashOffset != other->stroke->dashOffset
            || stroke->visitedLinkPaintColor != other->stroke->visitedLinkPaintColor
            || stroke->visitedLinkPaintUri != other->stroke->visitedLinkPaintUri
            || stroke->visitedLinkPaintType != other->stroke->visitedLinkPaintType)
            return true;
    }

    // The geometry properties require a re-layout.
    if (geometry.get() != other->geometry.get() && *geometry != *other->geometry)
        return true;

    return false;
}

bool SVGComputedStyle::diffNeedsPaintInvalidation(const SVGComputedStyle* other) const
{
    if (stroke->opacity != other->stroke->opacity)
        return true;

    // Painting related properties only need paint invalidation.
    if (misc.get() != other->misc.get()) {
        if (misc->floodColor != other->misc->floodColor
            || misc->floodOpacity != other->misc->floodOpacity
            || misc->lightingColor != other->misc->lightingColor)
            return true;
    }

    // If fill changes, we just need to issue paint invalidations. Fill boundaries are not influenced by this, only by the Path, that LayoutSVGPath contains.
    if (fill.get() != other->fill.get()) {
        if (fill->paintType != other->fill->paintType
            || fill->paintColor != other->fill->paintColor
            || fill->paintUri != other->fill->paintUri
            || fill->opacity != other->fill->opacity)
            return true;
    }

    // If gradient stops change, we just need to issue paint invalidations. Style updates are already handled through LayoutSVGGradientSTop.
    if (stops != other->stops)
        return true;

    // Changes of these flags only cause paint invalidations.
    if (svg_inherited_flags.colorRendering != other->svg_inherited_flags.colorRendering
        || svg_inherited_flags.shapeRendering != other->svg_inherited_flags.shapeRendering
        || svg_inherited_flags.clipRule != other->svg_inherited_flags.clipRule
        || svg_inherited_flags.fillRule != other->svg_inherited_flags.fillRule
        || svg_inherited_flags.colorInterpolation != other->svg_inherited_flags.colorInterpolation
        || svg_inherited_flags.colorInterpolationFilters != other->svg_inherited_flags.colorInterpolationFilters
        || svg_inherited_flags.paintOrder != other->svg_inherited_flags.paintOrder)
        return true;

    if (svg_noninherited_flags.f.bufferedRendering != other->svg_noninherited_flags.f.bufferedRendering)
        return true;

    if (svg_noninherited_flags.f.maskType != other->svg_noninherited_flags.f.maskType)
        return true;

    return false;
}

unsigned paintOrderSequence(EPaintOrderType first, EPaintOrderType second, EPaintOrderType third)
{
    return (((third << kPaintOrderBitwidth) | second) << kPaintOrderBitwidth) | first;
}

EPaintOrderType SVGComputedStyle::paintOrderType(unsigned index) const
{
    unsigned pt = 0;
    ASSERT(index < ((1 << kPaintOrderBitwidth)-1));
    switch (this->paintOrder()) {
    case PaintOrderNormal:
    case PaintOrderFillStrokeMarkers:
        pt = paintOrderSequence(PT_FILL, PT_STROKE, PT_MARKERS);
        break;
    case PaintOrderFillMarkersStroke:
        pt = paintOrderSequence(PT_FILL, PT_MARKERS, PT_STROKE);
        break;
    case PaintOrderStrokeFillMarkers:
        pt = paintOrderSequence(PT_STROKE, PT_FILL, PT_MARKERS);
        break;
    case PaintOrderStrokeMarkersFill:
        pt = paintOrderSequence(PT_STROKE, PT_MARKERS, PT_FILL);
        break;
    case PaintOrderMarkersFillStroke:
        pt = paintOrderSequence(PT_MARKERS, PT_FILL, PT_STROKE);
        break;
    case PaintOrderMarkersStrokeFill:
        pt = paintOrderSequence(PT_MARKERS, PT_STROKE, PT_FILL);
        break;
    }

    pt = (pt >> (kPaintOrderBitwidth*index)) & ((1u << kPaintOrderBitwidth) - 1);
    return (EPaintOrderType)pt;
}

} // namespace blink
