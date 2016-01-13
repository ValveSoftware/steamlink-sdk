/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

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

#include "core/css/CSSComputedStyleDeclaration.h"

#include "core/CSSPropertyNames.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/dom/Document.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/svg/SVGPaint.h"

namespace WebCore {

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> glyphOrientationToCSSPrimitiveValue(EGlyphOrientation orientation)
{
    switch (orientation) {
        case GO_0DEG:
            return CSSPrimitiveValue::create(0.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_90DEG:
            return CSSPrimitiveValue::create(90.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_180DEG:
            return CSSPrimitiveValue::create(180.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_270DEG:
            return CSSPrimitiveValue::create(270.0f, CSSPrimitiveValue::CSS_DEG);
        default:
            return nullptr;
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> strokeDashArrayToCSSValueList(PassRefPtr<SVGLengthList> passDashes)
{
    RefPtr<SVGLengthList> dashes = passDashes;

    if (dashes->isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    SVGLengthList::ConstIterator it = dashes->begin();
    SVGLengthList::ConstIterator itEnd = dashes->end();
    for (; it != itEnd; ++it)
        list->append(SVGLength::toCSSPrimitiveValue(*it));

    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> paintOrderToCSSValueList(EPaintOrder paintorder)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        EPaintOrderType paintOrderType = (EPaintOrderType)(paintorder & ((1 << kPaintOrderBitwidth) - 1));
        switch (paintOrderType) {
        case PT_FILL:
        case PT_STROKE:
        case PT_MARKERS:
            list->append(CSSPrimitiveValue::create(paintOrderType));
            break;
        case PT_NONE:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } while (paintorder >>= kPaintOrderBitwidth);

    return list.release();
}

PassRefPtrWillBeRawPtr<SVGPaint> CSSComputedStyleDeclaration::adjustSVGPaintForCurrentColor(PassRefPtrWillBeRawPtr<SVGPaint> newPaint, RenderStyle& style) const
{
    RefPtrWillBeRawPtr<SVGPaint> paint = newPaint;
    if (paint->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR || paint->paintType() == SVGPaint::SVG_PAINTTYPE_URI_CURRENTCOLOR)
        paint->setColor(style.color());
    return paint.release();
}

static inline String serializeAsFragmentIdentifier(const AtomicString& resource)
{
    return "#" + resource;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getSVGPropertyCSSValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return nullptr;

    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document().updateLayout();

    RenderStyle* style = node->computedStyle();
    if (!style)
        return nullptr;

    const SVGRenderStyle* svgStyle = style->svgStyle();
    if (!svgStyle)
        return nullptr;

    switch (propertyID) {
        case CSSPropertyClipRule:
            return CSSPrimitiveValue::create(svgStyle->clipRule());
        case CSSPropertyFloodOpacity:
            return CSSPrimitiveValue::create(svgStyle->floodOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyStopOpacity:
            return CSSPrimitiveValue::create(svgStyle->stopOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyColorInterpolation:
            return CSSPrimitiveValue::create(svgStyle->colorInterpolation());
        case CSSPropertyColorInterpolationFilters:
            return CSSPrimitiveValue::create(svgStyle->colorInterpolationFilters());
        case CSSPropertyFillOpacity:
            return CSSPrimitiveValue::create(svgStyle->fillOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyFillRule:
            return CSSPrimitiveValue::create(svgStyle->fillRule());
        case CSSPropertyColorRendering:
            return CSSPrimitiveValue::create(svgStyle->colorRendering());
        case CSSPropertyShapeRendering:
            return CSSPrimitiveValue::create(svgStyle->shapeRendering());
        case CSSPropertyStrokeLinecap:
            return CSSPrimitiveValue::create(svgStyle->capStyle());
        case CSSPropertyStrokeLinejoin:
            return CSSPrimitiveValue::create(svgStyle->joinStyle());
        case CSSPropertyStrokeMiterlimit:
            return CSSPrimitiveValue::create(svgStyle->strokeMiterLimit(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyStrokeOpacity:
            return CSSPrimitiveValue::create(svgStyle->strokeOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyAlignmentBaseline:
            return CSSPrimitiveValue::create(svgStyle->alignmentBaseline());
        case CSSPropertyDominantBaseline:
            return CSSPrimitiveValue::create(svgStyle->dominantBaseline());
        case CSSPropertyTextAnchor:
            return CSSPrimitiveValue::create(svgStyle->textAnchor());
        case CSSPropertyWritingMode:
            return CSSPrimitiveValue::create(svgStyle->writingMode());
        case CSSPropertyClipPath:
            if (!svgStyle->clipperResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->clipperResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMask:
            if (!svgStyle->maskerResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->maskerResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyFilter:
            if (!svgStyle->filterResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->filterResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyFloodColor:
            return currentColorOrValidColor(*style, svgStyle->floodColor());
        case CSSPropertyLightingColor:
            return currentColorOrValidColor(*style, svgStyle->lightingColor());
        case CSSPropertyStopColor:
            return currentColorOrValidColor(*style, svgStyle->stopColor());
        case CSSPropertyFill:
            return adjustSVGPaintForCurrentColor(SVGPaint::create(svgStyle->fillPaintType(), svgStyle->fillPaintUri(), svgStyle->fillPaintColor()), *style);
        case CSSPropertyMarkerEnd:
            if (!svgStyle->markerEndResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->markerEndResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMarkerMid:
            if (!svgStyle->markerMidResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->markerMidResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMarkerStart:
            if (!svgStyle->markerStartResource().isEmpty())
                return CSSPrimitiveValue::create(serializeAsFragmentIdentifier(svgStyle->markerStartResource()), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyStroke:
            return adjustSVGPaintForCurrentColor(SVGPaint::create(svgStyle->strokePaintType(), svgStyle->strokePaintUri(), svgStyle->strokePaintColor()), *style);
        case CSSPropertyStrokeDasharray:
            return strokeDashArrayToCSSValueList(svgStyle->strokeDashArray());
        case CSSPropertyStrokeDashoffset:
            return SVGLength::toCSSPrimitiveValue(svgStyle->strokeDashOffset());
        case CSSPropertyStrokeWidth:
            return SVGLength::toCSSPrimitiveValue(svgStyle->strokeWidth());
        case CSSPropertyBaselineShift: {
            switch (svgStyle->baselineShift()) {
                case BS_BASELINE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBaseline);
                case BS_SUPER:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
                case BS_SUB:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSub);
                case BS_LENGTH:
                    return SVGLength::toCSSPrimitiveValue(svgStyle->baselineShiftValue());
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        case CSSPropertyBufferedRendering:
            return CSSPrimitiveValue::create(svgStyle->bufferedRendering());
        case CSSPropertyGlyphOrientationHorizontal:
            return glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationHorizontal());
        case CSSPropertyGlyphOrientationVertical: {
            if (RefPtrWillBeRawPtr<CSSPrimitiveValue> value = glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationVertical()))
                return value.release();

            if (svgStyle->glyphOrientationVertical() == GO_AUTO)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);

            return nullptr;
        }
        case CSSPropertyPaintOrder:
            return paintOrderToCSSValueList(svgStyle->paintOrder());
        case CSSPropertyVectorEffect:
            return CSSPrimitiveValue::create(svgStyle->vectorEffect());
        case CSSPropertyMaskType:
            return CSSPrimitiveValue::create(svgStyle->maskType());
        case CSSPropertyMarker:
        case CSSPropertyEnableBackground:
            // the above properties are not yet implemented in the engine
            break;
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSComputedStyleDelcaration::getPropertyCSSValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propertyID);
    }
    WTF_LOG_ERROR("unimplemented propertyID: %d", propertyID);
    return nullptr;
}

}
