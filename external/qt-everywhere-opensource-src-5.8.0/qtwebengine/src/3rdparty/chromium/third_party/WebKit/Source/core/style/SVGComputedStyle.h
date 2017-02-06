/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Computer, Inc.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

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

#ifndef SVGComputedStyle_h
#define SVGComputedStyle_h

#include "core/CoreExport.h"
#include "core/style/DataRef.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "core/style/StyleDifference.h"
#include "platform/Length.h"
#include "platform/graphics/GraphicsTypes.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace blink {

class CORE_EXPORT SVGComputedStyle : public RefCounted<SVGComputedStyle> {
public:
    static PassRefPtr<SVGComputedStyle> create() { return adoptRef(new SVGComputedStyle); }
    PassRefPtr<SVGComputedStyle> copy() const { return adoptRef(new SVGComputedStyle(*this));}
    ~SVGComputedStyle();

    bool inheritedNotEqual(const SVGComputedStyle*) const;
    void inheritFrom(const SVGComputedStyle*);
    void copyNonInheritedFromCached(const SVGComputedStyle*);

    StyleDifference diff(const SVGComputedStyle*) const;

    bool operator==(const SVGComputedStyle&) const;
    bool operator!=(const SVGComputedStyle& o) const { return !(*this == o); }

    // Initial values for all the properties
    static EAlignmentBaseline initialAlignmentBaseline() { return AB_AUTO; }
    static EDominantBaseline initialDominantBaseline() { return DB_AUTO; }
    static EBaselineShift initialBaselineShift() { return BS_LENGTH; }
    static Length initialBaselineShiftValue() { return Length(Fixed); }
    static EVectorEffect initialVectorEffect() { return VE_NONE; }
    static EBufferedRendering initialBufferedRendering() { return BR_AUTO; }
    static LineCap initialCapStyle() { return ButtCap; }
    static WindRule initialClipRule() { return RULE_NONZERO; }
    static EColorInterpolation initialColorInterpolation() { return CI_SRGB; }
    static EColorInterpolation initialColorInterpolationFilters() { return CI_LINEARRGB; }
    static EColorRendering initialColorRendering() { return CR_AUTO; }
    static WindRule initialFillRule() { return RULE_NONZERO; }
    static LineJoin initialJoinStyle() { return MiterJoin; }
    static EShapeRendering initialShapeRendering() { return SR_AUTO; }
    static ETextAnchor initialTextAnchor() { return TA_START; }
    static float initialFillOpacity() { return 1; }
    static SVGPaintType initialFillPaintType() { return SVG_PAINTTYPE_RGBCOLOR; }
    static Color initialFillPaintColor() { return Color::black; }
    static String initialFillPaintUri() { return String(); }
    static float initialStrokeOpacity() { return 1; }
    static SVGPaintType initialStrokePaintType() { return SVG_PAINTTYPE_NONE; }
    static Color initialStrokePaintColor() { return Color(); }
    static String initialStrokePaintUri() { return String(); }
    static PassRefPtr<SVGDashArray> initialStrokeDashArray();
    static Length initialStrokeDashOffset() { return Length(Fixed); }
    static float initialStrokeMiterLimit() { return 4; }
    static UnzoomedLength initialStrokeWidth() { return UnzoomedLength(Length(1, Fixed)); }
    static float initialStopOpacity() { return 1; }
    static Color initialStopColor() { return Color(0, 0, 0); }
    static float initialFloodOpacity() { return 1; }
    static Color initialFloodColor() { return Color(0, 0, 0); }
    static Color initialLightingColor() { return Color(255, 255, 255); }
    static const AtomicString& initialClipperResource() { return nullAtom; }
    static const AtomicString& initialMaskerResource() { return nullAtom; }
    static const AtomicString& initialMarkerStartResource() { return nullAtom; }
    static const AtomicString& initialMarkerMidResource() { return nullAtom; }
    static const AtomicString& initialMarkerEndResource() { return nullAtom; }
    static EMaskType initialMaskType() { return MT_LUMINANCE; }
    static EPaintOrder initialPaintOrder() { return PaintOrderNormal; }
    static StylePath* initialD() { return nullptr; }
    static Length initialCx() { return Length(Fixed); }
    static Length initialCy() { return Length(Fixed); }
    static Length initialX() { return Length(Fixed); }
    static Length initialY() { return Length(Fixed); }
    static Length initialR() { return Length(Fixed); }
    static Length initialRx() { return Length(Auto); }
    static Length initialRy() { return Length(Auto); }

    // SVG CSS Property setters
    void setAlignmentBaseline(EAlignmentBaseline val) { svg_noninherited_flags.f.alignmentBaseline = val; }
    void setDominantBaseline(EDominantBaseline val) { svg_inherited_flags.dominantBaseline = val; }
    void setBaselineShift(EBaselineShift val) { svg_noninherited_flags.f.baselineShift = val; }
    void setVectorEffect(EVectorEffect val) { svg_noninherited_flags.f.vectorEffect = val; }
    void setBufferedRendering(EBufferedRendering val) { svg_noninherited_flags.f.bufferedRendering = val; }
    void setCapStyle(LineCap val) { svg_inherited_flags.capStyle = val; }
    void setClipRule(WindRule val) { svg_inherited_flags.clipRule = val; }
    void setColorInterpolation(EColorInterpolation val) { svg_inherited_flags.colorInterpolation = val; }
    void setColorInterpolationFilters(EColorInterpolation val) { svg_inherited_flags.colorInterpolationFilters = val; }
    void setColorRendering(EColorRendering val) { svg_inherited_flags.colorRendering = val; }
    void setFillRule(WindRule val) { svg_inherited_flags.fillRule = val; }
    void setJoinStyle(LineJoin val) { svg_inherited_flags.joinStyle = val; }
    void setShapeRendering(EShapeRendering val) { svg_inherited_flags.shapeRendering = val; }
    void setTextAnchor(ETextAnchor val) { svg_inherited_flags.textAnchor = val; }
    void setMaskType(EMaskType val) { svg_noninherited_flags.f.maskType = val; }
    void setPaintOrder(EPaintOrder val) { svg_inherited_flags.paintOrder = (int)val; }
    void setD(PassRefPtr<StylePath> d)
    {
        if (!(geometry->d == d))
            geometry.access()->d = d;
    }
    void setCx(const Length& obj)
    {
        if (!(geometry->cx == obj))
            geometry.access()->cx = obj;
    }
    void setCy(const Length& obj)
    {
        if (!(geometry->cy == obj))
            geometry.access()->cy = obj;
    }
    void setX(const Length& obj)
    {
        if (!(geometry->x == obj))
            geometry.access()->x = obj;
    }
    void setY(const Length& obj)
    {
        if (!(geometry->y == obj))
            geometry.access()->y = obj;
    }
    void setR(const Length& obj)
    {
        if (!(geometry->r == obj))
            geometry.access()->r = obj;
    }
    void setRx(const Length& obj)
    {
        if (!(geometry->rx == obj))
            geometry.access()->rx = obj;
    }
    void setRy(const Length& obj)
    {
        if (!(geometry->ry == obj))
            geometry.access()->ry = obj;
    }
    void setFillOpacity(float obj)
    {
        if (!(fill->opacity == obj))
            fill.access()->opacity = obj;
    }

    void setFillPaint(SVGPaintType type, const Color& color, const String& uri, bool applyToRegularStyle = true, bool applyToVisitedLinkStyle = false)
    {
        if (applyToRegularStyle) {
            if (!(fill->paintType == type))
                fill.access()->paintType = type;
            if (!(fill->paintColor == color))
                fill.access()->paintColor = color;
            if (!(fill->paintUri == uri))
                fill.access()->paintUri = uri;
        }
        if (applyToVisitedLinkStyle) {
            if (!(fill->visitedLinkPaintType == type))
                fill.access()->visitedLinkPaintType = type;
            if (!(fill->visitedLinkPaintColor == color))
                fill.access()->visitedLinkPaintColor = color;
            if (!(fill->visitedLinkPaintUri == uri))
                fill.access()->visitedLinkPaintUri = uri;
        }
    }

    void setStrokeOpacity(float obj)
    {
        if (!(stroke->opacity == obj))
            stroke.access()->opacity = obj;
    }

    void setStrokePaint(SVGPaintType type, const Color& color, const String& uri, bool applyToRegularStyle = true, bool applyToVisitedLinkStyle = false)
    {
        if (applyToRegularStyle) {
            if (!(stroke->paintType == type))
                stroke.access()->paintType = type;
            if (!(stroke->paintColor == color))
                stroke.access()->paintColor = color;
            if (!(stroke->paintUri == uri))
                stroke.access()->paintUri = uri;
        }
        if (applyToVisitedLinkStyle) {
            if (!(stroke->visitedLinkPaintType == type))
                stroke.access()->visitedLinkPaintType = type;
            if (!(stroke->visitedLinkPaintColor == color))
                stroke.access()->visitedLinkPaintColor = color;
            if (!(stroke->visitedLinkPaintUri == uri))
                stroke.access()->visitedLinkPaintUri = uri;
        }
    }

    void setStrokeDashArray(PassRefPtr<SVGDashArray> dashArray)
    {
        if (*stroke->dashArray != *dashArray)
            stroke.access()->dashArray = dashArray;
    }

    void setStrokeMiterLimit(float obj)
    {
        if (!(stroke->miterLimit == obj))
            stroke.access()->miterLimit = obj;
    }

    void setStrokeWidth(const UnzoomedLength& strokeWidth)
    {
        if (!(stroke->width == strokeWidth))
            stroke.access()->width = strokeWidth;
    }

    void setStrokeDashOffset(const Length& dashOffset)
    {
        if (!(stroke->dashOffset == dashOffset))
            stroke.access()->dashOffset = dashOffset;
    }

    void setStopOpacity(float obj)
    {
        if (!(stops->opacity == obj))
            stops.access()->opacity = obj;
    }

    void setStopColor(const Color& obj)
    {
        if (!(stops->color == obj))
            stops.access()->color = obj;
    }

    void setFloodOpacity(float obj)
    {
        if (!(misc->floodOpacity == obj))
            misc.access()->floodOpacity = obj;
    }

    void setFloodColor(const Color& obj)
    {
        if (!(misc->floodColor == obj))
            misc.access()->floodColor = obj;
    }

    void setLightingColor(const Color& obj)
    {
        if (!(misc->lightingColor == obj))
            misc.access()->lightingColor = obj;
    }

    void setBaselineShiftValue(const Length& baselineShiftValue)
    {
        if (!(misc->baselineShiftValue == baselineShiftValue))
            misc.access()->baselineShiftValue = baselineShiftValue;
    }

    // Setters for non-inherited resources
    void setClipperResource(const AtomicString& obj)
    {
        if (!(resources->clipper == obj))
            resources.access()->clipper = obj;
    }

    void setMaskerResource(const AtomicString& obj)
    {
        if (!(resources->masker == obj))
            resources.access()->masker = obj;
    }

    // Setters for inherited resources
    void setMarkerStartResource(const AtomicString& obj)
    {
        if (!(inheritedResources->markerStart == obj))
            inheritedResources.access()->markerStart = obj;
    }

    void setMarkerMidResource(const AtomicString& obj)
    {
        if (!(inheritedResources->markerMid == obj))
            inheritedResources.access()->markerMid = obj;
    }

    void setMarkerEndResource(const AtomicString& obj)
    {
        if (!(inheritedResources->markerEnd == obj))
            inheritedResources.access()->markerEnd = obj;
    }

    // Read accessors for all the properties
    EAlignmentBaseline alignmentBaseline() const { return (EAlignmentBaseline) svg_noninherited_flags.f.alignmentBaseline; }
    EDominantBaseline dominantBaseline() const { return (EDominantBaseline) svg_inherited_flags.dominantBaseline; }
    EBaselineShift baselineShift() const { return (EBaselineShift) svg_noninherited_flags.f.baselineShift; }
    EVectorEffect vectorEffect() const { return (EVectorEffect) svg_noninherited_flags.f.vectorEffect; }
    EBufferedRendering bufferedRendering() const { return (EBufferedRendering) svg_noninherited_flags.f.bufferedRendering; }
    LineCap capStyle() const { return (LineCap) svg_inherited_flags.capStyle; }
    WindRule clipRule() const { return (WindRule) svg_inherited_flags.clipRule; }
    EColorInterpolation colorInterpolation() const { return (EColorInterpolation) svg_inherited_flags.colorInterpolation; }
    EColorInterpolation colorInterpolationFilters() const { return (EColorInterpolation) svg_inherited_flags.colorInterpolationFilters; }
    EColorRendering colorRendering() const { return (EColorRendering) svg_inherited_flags.colorRendering; }
    WindRule fillRule() const { return (WindRule) svg_inherited_flags.fillRule; }
    LineJoin joinStyle() const { return (LineJoin) svg_inherited_flags.joinStyle; }
    EShapeRendering shapeRendering() const { return (EShapeRendering) svg_inherited_flags.shapeRendering; }
    ETextAnchor textAnchor() const { return (ETextAnchor) svg_inherited_flags.textAnchor; }
    float fillOpacity() const { return fill->opacity; }
    const SVGPaintType& fillPaintType() const { return fill->paintType; }
    const Color& fillPaintColor() const { return fill->paintColor; }
    const String& fillPaintUri() const { return fill->paintUri; }
    float strokeOpacity() const { return stroke->opacity; }
    const SVGPaintType& strokePaintType() const { return stroke->paintType; }
    const Color& strokePaintColor() const { return stroke->paintColor; }
    const String& strokePaintUri() const { return stroke->paintUri; }
    SVGDashArray* strokeDashArray() const { return stroke->dashArray.get(); }
    float strokeMiterLimit() const { return stroke->miterLimit; }
    const UnzoomedLength& strokeWidth() const { return stroke->width; }
    const Length& strokeDashOffset() const { return stroke->dashOffset; }
    float stopOpacity() const { return stops->opacity; }
    const Color& stopColor() const { return stops->color; }
    float floodOpacity() const { return misc->floodOpacity; }
    const Color& floodColor() const { return misc->floodColor; }
    const Color& lightingColor() const { return misc->lightingColor; }
    const Length& baselineShiftValue() const { return misc->baselineShiftValue; }
    StylePath* d() const { return geometry->d.get(); }
    const Length& cx() const { return geometry->cx; }
    const Length& cy() const { return geometry->cy; }
    const Length& x() const { return geometry->x; }
    const Length& y() const { return geometry->y; }
    const Length& r() const { return geometry->r; }
    const Length& rx() const { return geometry->rx; }
    const Length& ry() const { return geometry->ry; }
    const AtomicString& clipperResource() const { return resources->clipper; }
    const AtomicString& maskerResource() const { return resources->masker; }
    const AtomicString& markerStartResource() const { return inheritedResources->markerStart; }
    const AtomicString& markerMidResource() const { return inheritedResources->markerMid; }
    const AtomicString& markerEndResource() const { return inheritedResources->markerEnd; }
    EMaskType maskType() const { return (EMaskType) svg_noninherited_flags.f.maskType; }
    EPaintOrder paintOrder() const { return (EPaintOrder) svg_inherited_flags.paintOrder; }
    EPaintOrderType paintOrderType(unsigned index) const;

    const SVGPaintType& visitedLinkFillPaintType() const { return fill->visitedLinkPaintType; }
    const Color& visitedLinkFillPaintColor() const { return fill->visitedLinkPaintColor; }
    const String& visitedLinkFillPaintUri() const { return fill->visitedLinkPaintUri; }
    const SVGPaintType& visitedLinkStrokePaintType() const { return stroke->visitedLinkPaintType; }
    const Color& visitedLinkStrokePaintColor() const { return stroke->visitedLinkPaintColor; }
    const String& visitedLinkStrokePaintUri() const { return stroke->visitedLinkPaintUri; }

    bool isFillColorCurrentColor() const
    {
        return fillPaintType() == SVG_PAINTTYPE_CURRENTCOLOR
            || visitedLinkFillPaintType() == SVG_PAINTTYPE_CURRENTCOLOR
            || fillPaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR
            || visitedLinkFillPaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR;
    }

    bool isStrokeColorCurrentColor() const
    {
        return strokePaintType() == SVG_PAINTTYPE_CURRENTCOLOR
            || visitedLinkStrokePaintType() == SVG_PAINTTYPE_CURRENTCOLOR
            || strokePaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR
            || visitedLinkStrokePaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR;
    }

    // convenience
    bool hasClipper() const { return !clipperResource().isEmpty(); }
    bool hasMasker() const { return !maskerResource().isEmpty(); }
    bool hasMarkers() const { return !markerStartResource().isEmpty() || !markerMidResource().isEmpty() || !markerEndResource().isEmpty(); }
    bool hasStroke() const { return strokePaintType() != SVG_PAINTTYPE_NONE; }
    bool hasVisibleStroke() const { return hasStroke() && !strokeWidth().isZero(); }
    bool hasSquareCapStyle() const { return capStyle() == SquareCap; }
    bool hasMiterJoinStyle() const { return joinStyle() == MiterJoin; }
    bool hasFill() const { return fillPaintType() != SVG_PAINTTYPE_NONE; }

protected:
    // inherit
    struct InheritedFlags {
        bool operator==(const InheritedFlags& other) const
        {
            return (colorRendering == other.colorRendering)
                && (shapeRendering == other.shapeRendering)
                && (clipRule == other.clipRule)
                && (fillRule == other.fillRule)
                && (capStyle == other.capStyle)
                && (joinStyle == other.joinStyle)
                && (textAnchor == other.textAnchor)
                && (colorInterpolation == other.colorInterpolation)
                && (colorInterpolationFilters == other.colorInterpolationFilters)
                && (paintOrder == other.paintOrder)
                && (dominantBaseline == other.dominantBaseline);
        }

        bool operator!=(const InheritedFlags& other) const
        {
            return !(*this == other);
        }

        unsigned colorRendering : 2; // EColorRendering
        unsigned shapeRendering : 2; // EShapeRendering
        unsigned clipRule : 1; // WindRule
        unsigned fillRule : 1; // WindRule
        unsigned capStyle : 2; // LineCap
        unsigned joinStyle : 2; // LineJoin
        unsigned textAnchor : 2; // ETextAnchor
        unsigned colorInterpolation : 2; // EColorInterpolation
        unsigned colorInterpolationFilters : 2; // EColorInterpolation_
        unsigned paintOrder : 3; // EPaintOrder
        unsigned dominantBaseline : 4; // EDominantBaseline
    } svg_inherited_flags;

    // don't inherit
    struct NonInheritedFlags {
        // 32 bit non-inherited, don't add to the struct, or the operator will break.
        bool operator==(const NonInheritedFlags &other) const { return niflags == other.niflags; }
        bool operator!=(const NonInheritedFlags &other) const { return niflags != other.niflags; }

        union {
            struct {
                unsigned alignmentBaseline : 4; // EAlignmentBaseline
                unsigned baselineShift : 2; // EBaselineShift
                unsigned vectorEffect: 1; // EVectorEffect
                unsigned bufferedRendering: 2; // EBufferedRendering
                unsigned maskType: 1; // EMaskType
                // 18 bits unused
            } f;
            uint32_t niflags;
        };
    } svg_noninherited_flags;

    // inherited attributes
    DataRef<StyleFillData> fill;
    DataRef<StyleStrokeData> stroke;
    DataRef<StyleInheritedResourceData> inheritedResources;

    // non-inherited attributes
    DataRef<StyleStopData> stops;
    DataRef<StyleMiscData> misc;
    DataRef<StyleGeometryData> geometry;
    DataRef<StyleResourceData> resources;

private:
    enum CreateInitialType { CreateInitial };

    SVGComputedStyle();
    SVGComputedStyle(const SVGComputedStyle&);
    SVGComputedStyle(CreateInitialType); // Used to create the initial style singleton.

    bool diffNeedsLayoutAndPaintInvalidation(const SVGComputedStyle* other) const;
    bool diffNeedsPaintInvalidation(const SVGComputedStyle* other) const;

    void setBitDefaults()
    {
        svg_inherited_flags.clipRule = initialClipRule();
        svg_inherited_flags.colorRendering = initialColorRendering();
        svg_inherited_flags.fillRule = initialFillRule();
        svg_inherited_flags.shapeRendering = initialShapeRendering();
        svg_inherited_flags.textAnchor = initialTextAnchor();
        svg_inherited_flags.capStyle = initialCapStyle();
        svg_inherited_flags.joinStyle = initialJoinStyle();
        svg_inherited_flags.colorInterpolation = initialColorInterpolation();
        svg_inherited_flags.colorInterpolationFilters = initialColorInterpolationFilters();
        svg_inherited_flags.paintOrder = initialPaintOrder();
        svg_inherited_flags.dominantBaseline = initialDominantBaseline();

        svg_noninherited_flags.niflags = 0;
        svg_noninherited_flags.f.alignmentBaseline = initialAlignmentBaseline();
        svg_noninherited_flags.f.baselineShift = initialBaselineShift();
        svg_noninherited_flags.f.vectorEffect = initialVectorEffect();
        svg_noninherited_flags.f.bufferedRendering = initialBufferedRendering();
        svg_noninherited_flags.f.maskType = initialMaskType();
    }
};

} // namespace blink

#endif // SVGComputedStyle_h
