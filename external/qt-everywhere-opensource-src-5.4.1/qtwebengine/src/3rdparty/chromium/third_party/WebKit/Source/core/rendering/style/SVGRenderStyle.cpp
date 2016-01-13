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

#include "config.h"

#include "core/rendering/style/SVGRenderStyle.h"


using namespace std;

namespace WebCore {

SVGRenderStyle::SVGRenderStyle()
{
    static SVGRenderStyle* defaultStyle = new SVGRenderStyle(CreateDefault);

    fill = defaultStyle->fill;
    stroke = defaultStyle->stroke;
    stops = defaultStyle->stops;
    misc = defaultStyle->misc;
    inheritedResources = defaultStyle->inheritedResources;
    resources = defaultStyle->resources;

    setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(CreateDefaultType)
{
    setBitDefaults();

    fill.init();
    stroke.init();
    stops.init();
    misc.init();
    inheritedResources.init();
    resources.init();
}

SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle& other)
    : RefCounted<SVGRenderStyle>()
{
    fill = other.fill;
    stroke = other.stroke;
    stops = other.stops;
    misc = other.misc;
    inheritedResources = other.inheritedResources;
    resources = other.resources;

    svg_inherited_flags = other.svg_inherited_flags;
    svg_noninherited_flags = other.svg_noninherited_flags;
}

SVGRenderStyle::~SVGRenderStyle()
{
}

bool SVGRenderStyle::operator==(const SVGRenderStyle& other) const
{
    return fill == other.fill
        && stroke == other.stroke
        && stops == other.stops
        && misc == other.misc
        && inheritedResources == other.inheritedResources
        && resources == other.resources
        && svg_inherited_flags == other.svg_inherited_flags
        && svg_noninherited_flags == other.svg_noninherited_flags;
}

bool SVGRenderStyle::inheritedNotEqual(const SVGRenderStyle* other) const
{
    return fill != other->fill
        || stroke != other->stroke
        || inheritedResources != other->inheritedResources
        || svg_inherited_flags != other->svg_inherited_flags;
}

void SVGRenderStyle::inheritFrom(const SVGRenderStyle* svgInheritParent)
{
    if (!svgInheritParent)
        return;

    fill = svgInheritParent->fill;
    stroke = svgInheritParent->stroke;
    inheritedResources = svgInheritParent->inheritedResources;

    svg_inherited_flags = svgInheritParent->svg_inherited_flags;
}

void SVGRenderStyle::copyNonInheritedFrom(const SVGRenderStyle* other)
{
    svg_noninherited_flags = other->svg_noninherited_flags;
    stops = other->stops;
    misc = other->misc;
    resources = other->resources;
}

StyleDifference SVGRenderStyle::diff(const SVGRenderStyle* other) const
{
    StyleDifference styleDifference;

    if (diffNeedsLayoutAndRepaint(other)) {
        styleDifference.setNeedsFullLayout();
        styleDifference.setNeedsRepaintObject();
    } else if (diffNeedsRepaint(other)) {
        styleDifference.setNeedsRepaintObject();
    }

    return styleDifference;
}

bool SVGRenderStyle::diffNeedsLayoutAndRepaint(const SVGRenderStyle* other) const
{
    // If resources change, we need a relayout, as the presence of resources influences the repaint rect.
    if (resources != other->resources)
        return true;

    // If markers change, we need a relayout, as marker boundaries are cached in RenderSVGPath.
    if (inheritedResources != other->inheritedResources)
        return true;

    // All text related properties influence layout.
    if (svg_inherited_flags._textAnchor != other->svg_inherited_flags._textAnchor
        || svg_inherited_flags._writingMode != other->svg_inherited_flags._writingMode
        || svg_inherited_flags._glyphOrientationHorizontal != other->svg_inherited_flags._glyphOrientationHorizontal
        || svg_inherited_flags._glyphOrientationVertical != other->svg_inherited_flags._glyphOrientationVertical
        || svg_noninherited_flags.f._alignmentBaseline != other->svg_noninherited_flags.f._alignmentBaseline
        || svg_noninherited_flags.f._dominantBaseline != other->svg_noninherited_flags.f._dominantBaseline
        || svg_noninherited_flags.f._baselineShift != other->svg_noninherited_flags.f._baselineShift)
        return true;

    // Text related properties influence layout.
    if (misc->baselineShiftValue != other->misc->baselineShiftValue)
        return true;

    // These properties affect the cached stroke bounding box rects.
    if (svg_inherited_flags._capStyle != other->svg_inherited_flags._capStyle
        || svg_inherited_flags._joinStyle != other->svg_inherited_flags._joinStyle)
        return true;

    // vector-effect changes require a re-layout.
    if (svg_noninherited_flags.f._vectorEffect != other->svg_noninherited_flags.f._vectorEffect)
        return true;

    // Some stroke properties, requires relayouts, as the cached stroke boundaries need to be recalculated.
    if (stroke.get() != other->stroke.get()) {
        if (stroke->width != other->stroke->width
            || stroke->paintType != other->stroke->paintType
            || stroke->paintColor != other->stroke->paintColor
            || stroke->paintUri != other->stroke->paintUri
            || stroke->miterLimit != other->stroke->miterLimit
            || stroke->dashArray != other->stroke->dashArray
            || stroke->dashOffset != other->stroke->dashOffset
            || stroke->visitedLinkPaintColor != other->stroke->visitedLinkPaintColor
            || stroke->visitedLinkPaintUri != other->stroke->visitedLinkPaintUri
            || stroke->visitedLinkPaintType != other->stroke->visitedLinkPaintType)
            return true;
    }

    return false;
}

bool SVGRenderStyle::diffNeedsRepaint(const SVGRenderStyle* other) const
{
    if (stroke->opacity != other->stroke->opacity)
        return true;

    // Painting related properties only need repaints.
    if (misc.get() != other->misc.get()) {
        if (misc->floodColor != other->misc->floodColor
            || misc->floodOpacity != other->misc->floodOpacity
            || misc->lightingColor != other->misc->lightingColor)
            return true;
    }

    // If fill changes, we just need to repaint. Fill boundaries are not influenced by this, only by the Path, that RenderSVGPath contains.
    if (fill.get() != other->fill.get()) {
        if (fill->paintType != other->fill->paintType
            || fill->paintColor != other->fill->paintColor
            || fill->paintUri != other->fill->paintUri
            || fill->opacity != other->fill->opacity)
            return true;
    }

    // If gradient stops change, we just need to repaint. Style updates are already handled through RenderSVGGradientSTop.
    if (stops != other->stops)
        return true;

    // Changes of these flags only cause repaints.
    if (svg_inherited_flags._colorRendering != other->svg_inherited_flags._colorRendering
        || svg_inherited_flags._shapeRendering != other->svg_inherited_flags._shapeRendering
        || svg_inherited_flags._clipRule != other->svg_inherited_flags._clipRule
        || svg_inherited_flags._fillRule != other->svg_inherited_flags._fillRule
        || svg_inherited_flags._colorInterpolation != other->svg_inherited_flags._colorInterpolation
        || svg_inherited_flags._colorInterpolationFilters != other->svg_inherited_flags._colorInterpolationFilters
        || svg_inherited_flags._paintOrder != other->svg_inherited_flags._paintOrder)
        return true;

    if (svg_noninherited_flags.f.bufferedRendering != other->svg_noninherited_flags.f.bufferedRendering)
        return true;

    if (svg_noninherited_flags.f.maskType != other->svg_noninherited_flags.f.maskType)
        return true;

    return false;
}

EPaintOrderType SVGRenderStyle::paintOrderType(unsigned index) const
{
    ASSERT(index < ((1 << kPaintOrderBitwidth)-1));
    unsigned pt = (paintOrder() >> (kPaintOrderBitwidth*index)) & ((1u << kPaintOrderBitwidth) - 1);
    return (EPaintOrderType)pt;
}

}
