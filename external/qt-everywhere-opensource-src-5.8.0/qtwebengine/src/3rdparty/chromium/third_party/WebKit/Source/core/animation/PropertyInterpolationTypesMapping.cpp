// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PropertyInterpolationTypesMapping.h"

#include "core/HTMLNames.h"
#include "core/animation/CSSBasicShapeInterpolationType.h"
#include "core/animation/CSSBorderImageLengthBoxInterpolationType.h"
#include "core/animation/CSSClipInterpolationType.h"
#include "core/animation/CSSColorInterpolationType.h"
#include "core/animation/CSSFilterListInterpolationType.h"
#include "core/animation/CSSFontSizeInterpolationType.h"
#include "core/animation/CSSFontWeightInterpolationType.h"
#include "core/animation/CSSImageInterpolationType.h"
#include "core/animation/CSSImageListInterpolationType.h"
#include "core/animation/CSSImageSliceInterpolationType.h"
#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/CSSLengthListInterpolationType.h"
#include "core/animation/CSSLengthPairInterpolationType.h"
#include "core/animation/CSSMotionRotationInterpolationType.h"
#include "core/animation/CSSNumberInterpolationType.h"
#include "core/animation/CSSPaintInterpolationType.h"
#include "core/animation/CSSPathInterpolationType.h"
#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/animation/CSSPositionInterpolationType.h"
#include "core/animation/CSSRotateInterpolationType.h"
#include "core/animation/CSSScaleInterpolationType.h"
#include "core/animation/CSSShadowListInterpolationType.h"
#include "core/animation/CSSSizeListInterpolationType.h"
#include "core/animation/CSSTextIndentInterpolationType.h"
#include "core/animation/CSSTransformInterpolationType.h"
#include "core/animation/CSSTransformOriginInterpolationType.h"
#include "core/animation/CSSTranslateInterpolationType.h"
#include "core/animation/CSSValueInterpolationType.h"
#include "core/animation/CSSVisibilityInterpolationType.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/SVGAngleInterpolationType.h"
#include "core/animation/SVGIntegerInterpolationType.h"
#include "core/animation/SVGIntegerOptionalIntegerInterpolationType.h"
#include "core/animation/SVGLengthInterpolationType.h"
#include "core/animation/SVGLengthListInterpolationType.h"
#include "core/animation/SVGNumberInterpolationType.h"
#include "core/animation/SVGNumberListInterpolationType.h"
#include "core/animation/SVGNumberOptionalNumberInterpolationType.h"
#include "core/animation/SVGPathInterpolationType.h"
#include "core/animation/SVGPointListInterpolationType.h"
#include "core/animation/SVGRectInterpolationType.h"
#include "core/animation/SVGTransformListInterpolationType.h"
#include "core/animation/SVGValueInterpolationType.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

const InterpolationTypes& PropertyInterpolationTypesMapping::get(const PropertyHandle& property)
{
    using ApplicableTypesMap = HashMap<PropertyHandle, std::unique_ptr<const InterpolationTypes>>;
    DEFINE_STATIC_LOCAL(ApplicableTypesMap, applicableTypesMap, ());
    auto entry = applicableTypesMap.find(property);
    if (entry != applicableTypesMap.end())
        return *entry->value.get();

    std::unique_ptr<InterpolationTypes> applicableTypes = wrapUnique(new InterpolationTypes());

    if (property.isCSSProperty() || property.isPresentationAttribute()) {
        CSSPropertyID cssProperty = property.isCSSProperty() ? property.cssProperty() : property.presentationAttribute();
        switch (cssProperty) {
        case CSSPropertyBaselineShift:
        case CSSPropertyBorderBottomWidth:
        case CSSPropertyBorderLeftWidth:
        case CSSPropertyBorderRightWidth:
        case CSSPropertyBorderTopWidth:
        case CSSPropertyBottom:
        case CSSPropertyCx:
        case CSSPropertyCy:
        case CSSPropertyFlexBasis:
        case CSSPropertyHeight:
        case CSSPropertyLeft:
        case CSSPropertyLetterSpacing:
        case CSSPropertyMarginBottom:
        case CSSPropertyMarginLeft:
        case CSSPropertyMarginRight:
        case CSSPropertyMarginTop:
        case CSSPropertyMaxHeight:
        case CSSPropertyMaxWidth:
        case CSSPropertyMinHeight:
        case CSSPropertyMinWidth:
        case CSSPropertyMotionOffset:
        case CSSPropertyOutlineOffset:
        case CSSPropertyOutlineWidth:
        case CSSPropertyPaddingBottom:
        case CSSPropertyPaddingLeft:
        case CSSPropertyPaddingRight:
        case CSSPropertyPaddingTop:
        case CSSPropertyPerspective:
        case CSSPropertyR:
        case CSSPropertyRight:
        case CSSPropertyRx:
        case CSSPropertyRy:
        case CSSPropertyShapeMargin:
        case CSSPropertyStrokeDashoffset:
        case CSSPropertyStrokeWidth:
        case CSSPropertyTop:
        case CSSPropertyVerticalAlign:
        case CSSPropertyWebkitBorderHorizontalSpacing:
        case CSSPropertyWebkitBorderVerticalSpacing:
        case CSSPropertyColumnGap:
        case CSSPropertyColumnRuleWidth:
        case CSSPropertyColumnWidth:
        case CSSPropertyWebkitPerspectiveOriginX:
        case CSSPropertyWebkitPerspectiveOriginY:
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
        case CSSPropertyWidth:
        case CSSPropertyWordSpacing:
        case CSSPropertyX:
        case CSSPropertyY:
            applicableTypes->append(wrapUnique(new CSSLengthInterpolationType(cssProperty)));
            break;
        case CSSPropertyFlexGrow:
        case CSSPropertyFlexShrink:
        case CSSPropertyFillOpacity:
        case CSSPropertyFloodOpacity:
        case CSSPropertyFontSizeAdjust:
        case CSSPropertyOpacity:
        case CSSPropertyOrphans:
        case CSSPropertyShapeImageThreshold:
        case CSSPropertyStopOpacity:
        case CSSPropertyStrokeMiterlimit:
        case CSSPropertyStrokeOpacity:
        case CSSPropertyColumnCount:
        case CSSPropertyWidows:
        case CSSPropertyZIndex:
            applicableTypes->append(wrapUnique(new CSSNumberInterpolationType(cssProperty)));
            break;
        case CSSPropertyLineHeight:
            applicableTypes->append(wrapUnique(new CSSLengthInterpolationType(cssProperty)));
            applicableTypes->append(wrapUnique(new CSSNumberInterpolationType(cssProperty)));
            break;
        case CSSPropertyBackgroundColor:
        case CSSPropertyBorderBottomColor:
        case CSSPropertyBorderLeftColor:
        case CSSPropertyBorderRightColor:
        case CSSPropertyBorderTopColor:
        case CSSPropertyColor:
        case CSSPropertyFloodColor:
        case CSSPropertyLightingColor:
        case CSSPropertyOutlineColor:
        case CSSPropertyStopColor:
        case CSSPropertyTextDecorationColor:
        case CSSPropertyColumnRuleColor:
        case CSSPropertyWebkitTextStrokeColor:
            applicableTypes->append(wrapUnique(new CSSColorInterpolationType(cssProperty)));
            break;
        case CSSPropertyFill:
        case CSSPropertyStroke:
            applicableTypes->append(wrapUnique(new CSSPaintInterpolationType(cssProperty)));
            break;
        case CSSPropertyD:
            applicableTypes->append(wrapUnique(new CSSPathInterpolationType(cssProperty)));
            break;
        case CSSPropertyBoxShadow:
        case CSSPropertyTextShadow:
            applicableTypes->append(wrapUnique(new CSSShadowListInterpolationType(cssProperty)));
            break;
        case CSSPropertyBorderImageSource:
        case CSSPropertyListStyleImage:
        case CSSPropertyWebkitMaskBoxImageSource:
            applicableTypes->append(wrapUnique(new CSSImageInterpolationType(cssProperty)));
            break;
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage:
            applicableTypes->append(wrapUnique(new CSSImageListInterpolationType(cssProperty)));
            break;
        case CSSPropertyStrokeDasharray:
            applicableTypes->append(wrapUnique(new CSSLengthListInterpolationType(cssProperty)));
            break;
        case CSSPropertyFontWeight:
            applicableTypes->append(wrapUnique(new CSSFontWeightInterpolationType(cssProperty)));
            break;
        case CSSPropertyVisibility:
            applicableTypes->append(wrapUnique(new CSSVisibilityInterpolationType(cssProperty)));
            break;
        case CSSPropertyClip:
            applicableTypes->append(wrapUnique(new CSSClipInterpolationType(cssProperty)));
            break;
        case CSSPropertyMotionRotation:
            applicableTypes->append(wrapUnique(new CSSMotionRotationInterpolationType(cssProperty)));
            break;
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionX:
        case CSSPropertyWebkitMaskPositionY:
            applicableTypes->append(wrapUnique(new CSSPositionAxisListInterpolationType(cssProperty)));
            break;
        case CSSPropertyPerspectiveOrigin:
        case CSSPropertyObjectPosition:
            applicableTypes->append(wrapUnique(new CSSPositionInterpolationType(cssProperty)));
            break;
        case CSSPropertyBorderBottomLeftRadius:
        case CSSPropertyBorderBottomRightRadius:
        case CSSPropertyBorderTopLeftRadius:
        case CSSPropertyBorderTopRightRadius:
            applicableTypes->append(wrapUnique(new CSSLengthPairInterpolationType(cssProperty)));
            break;
        case CSSPropertyTranslate:
            applicableTypes->append(wrapUnique(new CSSTranslateInterpolationType(cssProperty)));
            break;
        case CSSPropertyTransformOrigin:
            applicableTypes->append(wrapUnique(new CSSTransformOriginInterpolationType(cssProperty)));
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            applicableTypes->append(wrapUnique(new CSSSizeListInterpolationType(cssProperty)));
            break;
        case CSSPropertyBorderImageOutset:
        case CSSPropertyBorderImageWidth:
        case CSSPropertyWebkitMaskBoxImageOutset:
        case CSSPropertyWebkitMaskBoxImageWidth:
            applicableTypes->append(wrapUnique(new CSSBorderImageLengthBoxInterpolationType(cssProperty)));
            break;
        case CSSPropertyScale:
            applicableTypes->append(wrapUnique(new CSSScaleInterpolationType(cssProperty)));
            break;
        case CSSPropertyFontSize:
            applicableTypes->append(wrapUnique(new CSSFontSizeInterpolationType(cssProperty)));
            break;
        case CSSPropertyTextIndent:
            applicableTypes->append(wrapUnique(new CSSTextIndentInterpolationType(cssProperty)));
            break;
        case CSSPropertyBorderImageSlice:
        case CSSPropertyWebkitMaskBoxImageSlice:
            applicableTypes->append(wrapUnique(new CSSImageSliceInterpolationType(cssProperty)));
            break;
        case CSSPropertyWebkitClipPath:
        case CSSPropertyShapeOutside:
            applicableTypes->append(wrapUnique(new CSSBasicShapeInterpolationType(cssProperty)));
            break;
        case CSSPropertyRotate:
            applicableTypes->append(wrapUnique(new CSSRotateInterpolationType(cssProperty)));
            break;
        case CSSPropertyBackdropFilter:
        case CSSPropertyFilter:
            applicableTypes->append(wrapUnique(new CSSFilterListInterpolationType(cssProperty)));
            break;
        case CSSPropertyTransform:
            applicableTypes->append(wrapUnique(new CSSTransformInterpolationType(cssProperty)));
            break;
        default:
            ASSERT(!CSSPropertyMetadata::isInterpolableProperty(cssProperty));
        }

        applicableTypes->append(wrapUnique(new CSSValueInterpolationType(cssProperty)));

    } else {
        const QualifiedName& attribute = property.svgAttribute();
        if (attribute == SVGNames::orientAttr) {
            applicableTypes->append(wrapUnique(new SVGAngleInterpolationType(attribute)));
        } else if (attribute == SVGNames::numOctavesAttr
            || attribute == SVGNames::targetXAttr
            || attribute == SVGNames::targetYAttr) {
            applicableTypes->append(wrapUnique(new SVGIntegerInterpolationType(attribute)));
        } else if (attribute == SVGNames::orderAttr) {
            applicableTypes->append(wrapUnique(new SVGIntegerOptionalIntegerInterpolationType(attribute)));
        } else if (attribute == SVGNames::cxAttr
            || attribute == SVGNames::cyAttr
            || attribute == SVGNames::fxAttr
            || attribute == SVGNames::fyAttr
            || attribute == SVGNames::heightAttr
            || attribute == SVGNames::markerHeightAttr
            || attribute == SVGNames::markerWidthAttr
            || attribute == SVGNames::rAttr
            || attribute == SVGNames::refXAttr
            || attribute == SVGNames::refYAttr
            || attribute == SVGNames::rxAttr
            || attribute == SVGNames::ryAttr
            || attribute == SVGNames::startOffsetAttr
            || attribute == SVGNames::textLengthAttr
            || attribute == SVGNames::widthAttr
            || attribute == SVGNames::x1Attr
            || attribute == SVGNames::x2Attr
            || attribute == SVGNames::y1Attr
            || attribute == SVGNames::y2Attr) {
            applicableTypes->append(wrapUnique(new SVGLengthInterpolationType(attribute)));
        } else if (attribute == SVGNames::dxAttr
            || attribute == SVGNames::dyAttr) {
            applicableTypes->append(wrapUnique(new SVGNumberInterpolationType(attribute)));
            applicableTypes->append(wrapUnique(new SVGLengthListInterpolationType(attribute)));
        } else if (attribute == SVGNames::xAttr
            || attribute == SVGNames::yAttr) {
            applicableTypes->append(wrapUnique(new SVGLengthInterpolationType(attribute)));
            applicableTypes->append(wrapUnique(new SVGLengthListInterpolationType(attribute)));
        } else if (attribute == SVGNames::amplitudeAttr
            || attribute == SVGNames::azimuthAttr
            || attribute == SVGNames::biasAttr
            || attribute == SVGNames::diffuseConstantAttr
            || attribute == SVGNames::divisorAttr
            || attribute == SVGNames::elevationAttr
            || attribute == SVGNames::exponentAttr
            || attribute == SVGNames::interceptAttr
            || attribute == SVGNames::k1Attr
            || attribute == SVGNames::k2Attr
            || attribute == SVGNames::k3Attr
            || attribute == SVGNames::k4Attr
            || attribute == SVGNames::limitingConeAngleAttr
            || attribute == SVGNames::offsetAttr
            || attribute == SVGNames::pathLengthAttr
            || attribute == SVGNames::pointsAtXAttr
            || attribute == SVGNames::pointsAtYAttr
            || attribute == SVGNames::pointsAtZAttr
            || attribute == SVGNames::scaleAttr
            || attribute == SVGNames::seedAttr
            || attribute == SVGNames::slopeAttr
            || attribute == SVGNames::specularConstantAttr
            || attribute == SVGNames::specularExponentAttr
            || attribute == SVGNames::surfaceScaleAttr
            || attribute == SVGNames::zAttr) {
            applicableTypes->append(wrapUnique(new SVGNumberInterpolationType(attribute)));
        } else if (attribute == SVGNames::kernelMatrixAttr
            || attribute == SVGNames::rotateAttr
            || attribute == SVGNames::tableValuesAttr
            || attribute == SVGNames::valuesAttr) {
            applicableTypes->append(wrapUnique(new SVGNumberListInterpolationType(attribute)));
        } else if (attribute == SVGNames::baseFrequencyAttr
            || attribute == SVGNames::kernelUnitLengthAttr
            || attribute == SVGNames::radiusAttr
            || attribute == SVGNames::stdDeviationAttr) {
            applicableTypes->append(wrapUnique(new SVGNumberOptionalNumberInterpolationType(attribute)));
        } else if (attribute == SVGNames::dAttr) {
            applicableTypes->append(wrapUnique(new SVGPathInterpolationType(attribute)));
        } else if (attribute == SVGNames::pointsAttr) {
            applicableTypes->append(wrapUnique(new SVGPointListInterpolationType(attribute)));
        } else if (attribute == SVGNames::viewBoxAttr) {
            applicableTypes->append(wrapUnique(new SVGRectInterpolationType(attribute)));
        } else if (attribute == SVGNames::gradientTransformAttr
            || attribute == SVGNames::patternTransformAttr
            || attribute == SVGNames::transformAttr) {
            applicableTypes->append(wrapUnique(new SVGTransformListInterpolationType(attribute)));
        } else if (attribute == HTMLNames::classAttr
            || attribute == SVGNames::clipPathUnitsAttr
            || attribute == SVGNames::edgeModeAttr
            || attribute == SVGNames::filterUnitsAttr
            || attribute == SVGNames::gradientUnitsAttr
            || attribute == SVGNames::hrefAttr
            || attribute == SVGNames::inAttr
            || attribute == SVGNames::in2Attr
            || attribute == SVGNames::lengthAdjustAttr
            || attribute == SVGNames::markerUnitsAttr
            || attribute == SVGNames::maskContentUnitsAttr
            || attribute == SVGNames::maskUnitsAttr
            || attribute == SVGNames::methodAttr
            || attribute == SVGNames::modeAttr
            || attribute == SVGNames::operatorAttr
            || attribute == SVGNames::patternContentUnitsAttr
            || attribute == SVGNames::patternUnitsAttr
            || attribute == SVGNames::preserveAlphaAttr
            || attribute == SVGNames::preserveAspectRatioAttr
            || attribute == SVGNames::primitiveUnitsAttr
            || attribute == SVGNames::resultAttr
            || attribute == SVGNames::spacingAttr
            || attribute == SVGNames::spreadMethodAttr
            || attribute == SVGNames::stitchTilesAttr
            || attribute == SVGNames::targetAttr
            || attribute == SVGNames::typeAttr
            || attribute == SVGNames::xChannelSelectorAttr
            || attribute == SVGNames::yChannelSelectorAttr) {
            // Use default SVGValueInterpolationType.
        } else {
            NOTREACHED();
        }

        applicableTypes->append(wrapUnique(new SVGValueInterpolationType(attribute)));
    }

    auto addResult = applicableTypesMap.add(property, std::move(applicableTypes));
    return *addResult.storedValue->value.get();
}

} // namespace blink
