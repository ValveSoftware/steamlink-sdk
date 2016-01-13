/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"

#include "core/animation/AnimatableClipPathOperation.h"
#include "core/animation/AnimatableColor.h"
#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimatableFilterOperations.h"
#include "core/animation/AnimatableImage.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimatableLengthBox.h"
#include "core/animation/AnimatableLengthBoxAndBool.h"
#include "core/animation/AnimatableLengthPoint.h"
#include "core/animation/AnimatableLengthPoint3D.h"
#include "core/animation/AnimatableLengthSize.h"
#include "core/animation/AnimatableRepeatable.h"
#include "core/animation/AnimatableSVGLength.h"
#include "core/animation/AnimatableSVGPaint.h"
#include "core/animation/AnimatableShadow.h"
#include "core/animation/AnimatableShapeValue.h"
#include "core/animation/AnimatableStrokeDasharrayList.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/AnimatableVisibility.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/RenderStyle.h"
#include "wtf/MathExtras.h"
#include "wtf/TypeTraits.h"

namespace WebCore {

namespace {

Length animatableValueToLength(const AnimatableValue* value, const StyleResolverState& state, ValueRange range = ValueRangeAll)
{
    if (value->isLength())
        return toAnimatableLength(value)->length(state.style()->effectiveZoom(), range);
    RefPtrWillBeRawPtr<CSSValue> cssValue = toAnimatableUnknown(value)->toCSSValue();
    CSSPrimitiveValue* cssPrimitiveValue = toCSSPrimitiveValue(cssValue.get());
    return cssPrimitiveValue->convertToLength<AnyConversion>(state.cssToLengthConversionData());
}

BorderImageLength animatableValueToBorderImageLength(const AnimatableValue* value, const StyleResolverState& state)
{
    if (value->isLength())
        return BorderImageLength(toAnimatableLength(value)->length(state.style()->effectiveZoom(), ValueRangeNonNegative));
    if (value->isDouble())
        return BorderImageLength(clampTo<double>(toAnimatableDouble(value)->toDouble(), 0));
    RefPtrWillBeRawPtr<CSSValue> cssValue = toAnimatableUnknown(value)->toCSSValue();
    CSSPrimitiveValue* cssPrimitiveValue = toCSSPrimitiveValue(cssValue.get());
    return BorderImageLength(cssPrimitiveValue->convertToLength<AnyConversion>(state.cssToLengthConversionData()));
}

template<typename T> T animatableValueRoundClampTo(const AnimatableValue* value, T min = defaultMinimumForClamp<T>(), T max = defaultMaximumForClamp<T>())
{
    COMPILE_ASSERT(WTF::IsInteger<T>::value, ShouldUseIntegralTypeTWhenRoundingValues);
    return clampTo<T>(round(toAnimatableDouble(value)->toDouble()), min, max);
}

LengthBox animatableValueToLengthBox(const AnimatableValue* value, const StyleResolverState& state, ValueRange range = ValueRangeAll)
{
    const AnimatableLengthBox* animatableLengthBox = toAnimatableLengthBox(value);
    return LengthBox(
        animatableValueToLength(animatableLengthBox->top(), state, range),
        animatableValueToLength(animatableLengthBox->right(), state, range),
        animatableValueToLength(animatableLengthBox->bottom(), state, range),
        animatableValueToLength(animatableLengthBox->left(), state, range));
}

BorderImageLengthBox animatableValueToBorderImageLengthBox(const AnimatableValue* value, const StyleResolverState& state)
{
    const AnimatableLengthBox* animatableLengthBox = toAnimatableLengthBox(value);
    return BorderImageLengthBox(
        animatableValueToBorderImageLength(animatableLengthBox->top(), state),
        animatableValueToBorderImageLength(animatableLengthBox->right(), state),
        animatableValueToBorderImageLength(animatableLengthBox->bottom(), state),
        animatableValueToBorderImageLength(animatableLengthBox->left(), state));
}

LengthPoint animatableValueToLengthPoint(const AnimatableValue* value, const StyleResolverState& state, ValueRange range = ValueRangeAll)
{
    const AnimatableLengthPoint* animatableLengthPoint = toAnimatableLengthPoint(value);
    return LengthPoint(
        animatableValueToLength(animatableLengthPoint->x(), state, range),
        animatableValueToLength(animatableLengthPoint->y(), state, range));
}

LengthSize animatableValueToLengthSize(const AnimatableValue* value, const StyleResolverState& state, ValueRange range)
{
    const AnimatableLengthSize* animatableLengthSize = toAnimatableLengthSize(value);
    return LengthSize(
        animatableValueToLength(animatableLengthSize->width(), state, range),
        animatableValueToLength(animatableLengthSize->height(), state, range));
}

template <CSSPropertyID property>
void setFillSize(FillLayer* fillLayer, const AnimatableValue* value, const StyleResolverState& state)
{
    if (value->isLengthSize())
        fillLayer->setSize(FillSize(SizeLength, animatableValueToLengthSize(value, state, ValueRangeNonNegative)));
    else
        state.styleMap().mapFillSize(property, fillLayer, toAnimatableUnknown(value)->toCSSValue().get());
}

PassRefPtr<SVGLength> animatableValueToNonNegativeSVGLength(const AnimatableValue* value)
{
    RefPtr<SVGLength> length = toAnimatableSVGLength(value)->toSVGLength();
    if (length->valueInSpecifiedUnits() < 0)
        length->setValueInSpecifiedUnits(0);
    return length.release();
}

template <CSSPropertyID property>
void setOnFillLayers(FillLayer* fillLayer, const AnimatableValue* value, StyleResolverState& state)
{
    const WillBeHeapVector<RefPtrWillBeMember<AnimatableValue> >& values = toAnimatableRepeatable(value)->values();
    ASSERT(!values.isEmpty());
    FillLayer* prev = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!fillLayer) {
            switch (property) {
            case CSSPropertyBackgroundImage:
            case CSSPropertyBackgroundPositionX:
            case CSSPropertyBackgroundPositionY:
            case CSSPropertyBackgroundSize:
            case CSSPropertyWebkitBackgroundSize:
                fillLayer = new FillLayer(BackgroundFillLayer);
                break;
            case CSSPropertyWebkitMaskImage:
            case CSSPropertyWebkitMaskPositionX:
            case CSSPropertyWebkitMaskPositionY:
            case CSSPropertyWebkitMaskSize:
                fillLayer = new FillLayer(MaskFillLayer);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            prev->setNext(fillLayer);
        }
        const AnimatableValue* layerValue = values[i].get();
        switch (property) {
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage:
            if (layerValue->isImage()) {
                fillLayer->setImage(state.styleImage(property, toAnimatableImage(layerValue)->toCSSValue()));
            } else {
                ASSERT(toAnimatableUnknown(layerValue)->toCSSValueID() == CSSValueNone);
                fillLayer->setImage(nullptr);
            }
            break;
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            fillLayer->setXPosition(animatableValueToLength(layerValue, state));
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            fillLayer->setYPosition(animatableValueToLength(layerValue, state));
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            setFillSize<property>(fillLayer, layerValue, state);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        prev = fillLayer;
        fillLayer = fillLayer->next();
    }
    while (fillLayer) {
        switch (property) {
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage:
            fillLayer->clearImage();
            break;
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            fillLayer->clearXPosition();
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            fillLayer->clearYPosition();
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            fillLayer->clearSize();
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        fillLayer = fillLayer->next();
    }
}

FontWeight animatableValueToFontWeight(const AnimatableValue* value)
{
    int index = round(toAnimatableDouble(value)->toDouble() / 100) - 1;

    static const FontWeight weights[] = {
        FontWeight100,
        FontWeight200,
        FontWeight300,
        FontWeight400,
        FontWeight500,
        FontWeight600,
        FontWeight700,
        FontWeight800,
        FontWeight900
    };

    index = clampTo<int>(index, 0, WTF_ARRAY_LENGTH(weights) - 1);

    return weights[index];
}

} // namespace

// FIXME: Generate this function.
void AnimatedStyleBuilder::applyProperty(CSSPropertyID property, StyleResolverState& state, const AnimatableValue* value)
{
    ASSERT(CSSAnimations::isAnimatableProperty(property));
    if (value->isUnknown()) {
        StyleBuilder::applyProperty(property, state, toAnimatableUnknown(value)->toCSSValue().get());
        return;
    }
    RenderStyle* style = state.style();
    switch (property) {
    case CSSPropertyBackgroundColor:
        style->setBackgroundColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBackgroundColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBackgroundImage:
        setOnFillLayers<CSSPropertyBackgroundImage>(style->accessBackgroundLayers(), value, state);
        return;
    case CSSPropertyBackgroundPositionX:
        setOnFillLayers<CSSPropertyBackgroundPositionX>(style->accessBackgroundLayers(), value, state);
        return;
    case CSSPropertyBackgroundPositionY:
        setOnFillLayers<CSSPropertyBackgroundPositionY>(style->accessBackgroundLayers(), value, state);
        return;
    case CSSPropertyBackgroundSize:
        setOnFillLayers<CSSPropertyBackgroundSize>(style->accessBackgroundLayers(), value, state);
        return;
    case CSSPropertyBaselineShift:
        style->setBaselineShiftValue(toAnimatableSVGLength(value)->toSVGLength());
        return;
    case CSSPropertyBorderBottomColor:
        style->setBorderBottomColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderBottomColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderBottomLeftRadius:
        style->setBorderBottomLeftRadius(animatableValueToLengthSize(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyBorderBottomRightRadius:
        style->setBorderBottomRightRadius(animatableValueToLengthSize(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyBorderBottomWidth:
        style->setBorderBottomWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderImageOutset:
        style->setBorderImageOutset(animatableValueToBorderImageLengthBox(value, state));
        return;
    case CSSPropertyBorderImageSlice:
        style->setBorderImageSlices(animatableValueToLengthBox(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyBorderImageSource:
        style->setBorderImageSource(state.styleImage(property, toAnimatableImage(value)->toCSSValue()));
        return;
    case CSSPropertyBorderImageWidth:
        style->setBorderImageWidth(animatableValueToBorderImageLengthBox(value, state));
        return;
    case CSSPropertyBorderLeftColor:
        style->setBorderLeftColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderLeftColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderLeftWidth:
        style->setBorderLeftWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderRightColor:
        style->setBorderRightColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderRightColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderRightWidth:
        style->setBorderRightWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderTopColor:
        style->setBorderTopColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderTopColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderTopLeftRadius:
        style->setBorderTopLeftRadius(animatableValueToLengthSize(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyBorderTopRightRadius:
        style->setBorderTopRightRadius(animatableValueToLengthSize(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyBorderTopWidth:
        style->setBorderTopWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBottom:
        style->setBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        style->setBoxShadow(toAnimatableShadow(value)->shadowList());
        return;
    case CSSPropertyClip:
        style->setClip(animatableValueToLengthBox(value, state));
        style->setHasClip(true);
        return;
    case CSSPropertyColor:
        style->setColor(toAnimatableColor(value)->color());
        style->setVisitedLinkColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyFillOpacity:
        style->setFillOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, 1));
        return;
    case CSSPropertyFill:
        {
            const AnimatableSVGPaint* svgPaint = toAnimatableSVGPaint(value);
            style->accessSVGStyle()->setFillPaint(svgPaint->paintType(), svgPaint->color(), svgPaint->uri(), true, false);
            style->accessSVGStyle()->setFillPaint(svgPaint->visitedLinkPaintType(), svgPaint->visitedLinkColor(), svgPaint->visitedLinkURI(), false, true);
        }
        return;
    case CSSPropertyFlexGrow:
        style->setFlexGrow(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0));
        return;
    case CSSPropertyFlexShrink:
        style->setFlexShrink(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0));
        return;
    case CSSPropertyFlexBasis:
        style->setFlexBasis(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyFloodColor:
        style->setFloodColor(toAnimatableColor(value)->color());
        return;
    case CSSPropertyFloodOpacity:
        style->setFloodOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, 1));
        return;
    case CSSPropertyFontSize:
        style->setFontSize(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0));
        return;
    case CSSPropertyFontWeight:
        style->setFontWeight(animatableValueToFontWeight(value));
        return;
    case CSSPropertyHeight:
        style->setHeight(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyLeft:
        style->setLeft(animatableValueToLength(value, state));
        return;
    case CSSPropertyLightingColor:
        style->setLightingColor(toAnimatableColor(value)->color());
        return;
    case CSSPropertyLineHeight:
        if (value->isLength())
            style->setLineHeight(animatableValueToLength(value, state, ValueRangeNonNegative));
        else
            style->setLineHeight(Length(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0), Percent));
        return;
    case CSSPropertyListStyleImage:
        style->setListStyleImage(state.styleImage(property, toAnimatableImage(value)->toCSSValue()));
        return;
    case CSSPropertyLetterSpacing:
        style->setLetterSpacing(clampTo<float>(toAnimatableDouble(value)->toDouble()));
        return;
    case CSSPropertyMarginBottom:
        style->setMarginBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginLeft:
        style->setMarginLeft(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginRight:
        style->setMarginRight(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginTop:
        style->setMarginTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyMaxHeight:
        style->setMaxHeight(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyMaxWidth:
        style->setMaxWidth(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyMinHeight:
        style->setMinHeight(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyMinWidth:
        style->setMinWidth(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyObjectPosition:
        style->setObjectPosition(animatableValueToLengthPoint(value, state));
        return;
    case CSSPropertyOpacity:
        // Avoiding a value of 1 forces a layer to be created.
        style->setOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, nextafterf(1, 0)));
        return;
    case CSSPropertyOrphans:
        style->setOrphans(animatableValueRoundClampTo<unsigned short>(value, 1));
        return;
    case CSSPropertyOutlineColor:
        style->setOutlineColor(toAnimatableColor(value)->color());
        style->setVisitedLinkOutlineColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyOutlineOffset:
        style->setOutlineOffset(animatableValueRoundClampTo<int>(value));
        return;
    case CSSPropertyOutlineWidth:
        style->setOutlineWidth(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyPaddingBottom:
        style->setPaddingBottom(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyPaddingLeft:
        style->setPaddingLeft(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyPaddingRight:
        style->setPaddingRight(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyPaddingTop:
        style->setPaddingTop(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyRight:
        style->setRight(animatableValueToLength(value, state));
        return;
    case CSSPropertyStrokeWidth:
        style->setStrokeWidth(animatableValueToNonNegativeSVGLength(value));
        return;
    case CSSPropertyStopColor:
        style->setStopColor(toAnimatableColor(value)->color());
        return;
    case CSSPropertyStopOpacity:
        style->setStopOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, 1));
        return;
    case CSSPropertyStrokeDasharray:
        style->setStrokeDashArray(toAnimatableStrokeDasharrayList(value)->toSVGLengthList());
        return;
    case CSSPropertyStrokeDashoffset:
        style->setStrokeDashOffset(toAnimatableSVGLength(value)->toSVGLength());
        return;
    case CSSPropertyStrokeMiterlimit:
        style->setStrokeMiterLimit(clampTo<float>(toAnimatableDouble(value)->toDouble(), 1));
        return;
    case CSSPropertyStrokeOpacity:
        style->setStrokeOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, 1));
        return;
    case CSSPropertyStroke:
        {
            const AnimatableSVGPaint* svgPaint = toAnimatableSVGPaint(value);
            style->accessSVGStyle()->setStrokePaint(svgPaint->paintType(), svgPaint->color(), svgPaint->uri(), true, false);
            style->accessSVGStyle()->setStrokePaint(svgPaint->visitedLinkPaintType(), svgPaint->visitedLinkColor(), svgPaint->visitedLinkURI(), false, true);
        }
        return;
    case CSSPropertyTextDecorationColor:
        style->setTextDecorationColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextDecorationColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyTextIndent:
        style->setTextIndent(animatableValueToLength(value, state));
        return;
    case CSSPropertyTextShadow:
        style->setTextShadow(toAnimatableShadow(value)->shadowList());
        return;
    case CSSPropertyTop:
        style->setTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitBackgroundSize:
        setOnFillLayers<CSSPropertyWebkitBackgroundSize>(style->accessBackgroundLayers(), value, state);
        return;
    case CSSPropertyWebkitBorderHorizontalSpacing:
        style->setHorizontalBorderSpacing(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitBorderVerticalSpacing:
        style->setVerticalBorderSpacing(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitClipPath:
        style->setClipPath(toAnimatableClipPathOperation(value)->clipPathOperation());
        return;
    case CSSPropertyWebkitColumnCount:
        style->setColumnCount(animatableValueRoundClampTo<unsigned short>(value, 1));
        return;
    case CSSPropertyWebkitColumnGap:
        style->setColumnGap(clampTo(toAnimatableDouble(value)->toDouble(), 0));
        return;
    case CSSPropertyWebkitColumnRuleColor:
        style->setColumnRuleColor(toAnimatableColor(value)->color());
        style->setVisitedLinkColumnRuleColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyWebkitColumnWidth:
        style->setColumnWidth(clampTo(toAnimatableDouble(value)->toDouble(), std::numeric_limits<float>::epsilon()));
        return;
    case CSSPropertyWebkitColumnRuleWidth:
        style->setColumnRuleWidth(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitFilter:
        style->setFilter(toAnimatableFilterOperations(value)->operations());
        return;
    case CSSPropertyWebkitMaskBoxImageOutset:
        style->setMaskBoxImageOutset(animatableValueToBorderImageLengthBox(value, state));
        return;
    case CSSPropertyWebkitMaskBoxImageSlice:
        style->setMaskBoxImageSlices(animatableValueToLengthBox(toAnimatableLengthBoxAndBool(value)->box(), state, ValueRangeNonNegative));
        style->setMaskBoxImageSlicesFill(toAnimatableLengthBoxAndBool(value)->flag());
        return;
    case CSSPropertyWebkitMaskBoxImageSource:
        style->setMaskBoxImageSource(state.styleImage(property, toAnimatableImage(value)->toCSSValue()));
        return;
    case CSSPropertyWebkitMaskBoxImageWidth:
        style->setMaskBoxImageWidth(animatableValueToBorderImageLengthBox(value, state));
        return;
    case CSSPropertyWebkitMaskImage:
        setOnFillLayers<CSSPropertyWebkitMaskImage>(style->accessMaskLayers(), value, state);
        return;
    case CSSPropertyWebkitMaskPositionX:
        setOnFillLayers<CSSPropertyWebkitMaskPositionX>(style->accessMaskLayers(), value, state);
        return;
    case CSSPropertyWebkitMaskPositionY:
        setOnFillLayers<CSSPropertyWebkitMaskPositionY>(style->accessMaskLayers(), value, state);
        return;
    case CSSPropertyWebkitMaskSize:
        setOnFillLayers<CSSPropertyWebkitMaskSize>(style->accessMaskLayers(), value, state);
        return;
    case CSSPropertyPerspective:
        style->setPerspective(clampTo<float>(toAnimatableDouble(value)->toDouble()));
        return;
    case CSSPropertyPerspectiveOrigin: {
        ASSERT(RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        const AnimatableLengthPoint* animatableLengthPoint = toAnimatableLengthPoint(value);
        style->setPerspectiveOriginX(animatableValueToLength(animatableLengthPoint->x(), state));
        style->setPerspectiveOriginY(animatableValueToLength(animatableLengthPoint->y(), state));
        return;
    }
    case CSSPropertyWebkitPerspectiveOriginX:
        ASSERT(!RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        style->setPerspectiveOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitPerspectiveOriginY:
        ASSERT(!RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        style->setPerspectiveOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyShapeOutside:
        style->setShapeOutside(toAnimatableShapeValue(value)->shapeValue());
        return;
    case CSSPropertyShapeMargin:
        style->setShapeMargin(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyShapeImageThreshold:
        style->setShapeImageThreshold(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0, 1));
        return;
    case CSSPropertyWebkitTextStrokeColor:
        style->setTextStrokeColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextStrokeColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyTransform: {
        const TransformOperations& operations = toAnimatableTransform(value)->transformOperations();
        // FIXME: This normalization (handling of 'none') should be performed at input in AnimatableValueFactory.
        style->setTransform(operations.size() ? operations : TransformOperations(true));
        return;
    }
    case CSSPropertyTransformOrigin: {
        ASSERT(RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        const AnimatableLengthPoint3D* animatableLengthPoint3D = toAnimatableLengthPoint3D(value);
        style->setTransformOriginX(animatableValueToLength(animatableLengthPoint3D->x(), state));
        style->setTransformOriginY(animatableValueToLength(animatableLengthPoint3D->y(), state));
        style->setTransformOriginZ(clampTo<float>(toAnimatableDouble(animatableLengthPoint3D->z())->toDouble()));
        return;
    }
    case CSSPropertyWebkitTransformOriginX:
        ASSERT(!RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        style->setTransformOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginY:
        ASSERT(!RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        style->setTransformOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginZ:
        ASSERT(!RuntimeEnabledFeatures::cssTransformsUnprefixedEnabled());
        style->setTransformOriginZ(toAnimatableDouble(value)->toDouble());
        return;
    case CSSPropertyWidows:
        style->setWidows(animatableValueRoundClampTo<unsigned short>(value, 1));
        return;
    case CSSPropertyWidth:
        style->setWidth(animatableValueToLength(value, state, ValueRangeNonNegative));
        return;
    case CSSPropertyWordSpacing:
        style->setWordSpacing(clampTo<float>(toAnimatableDouble(value)->toDouble()));
        return;
    case CSSPropertyVerticalAlign:
        style->setVerticalAlignLength(animatableValueToLength(value, state));
        return;
    case CSSPropertyVisibility:
        style->setVisibility(toAnimatableVisibility(value)->visibility());
        return;
    case CSSPropertyZIndex:
        style->setZIndex(animatableValueRoundClampTo<int>(value));
        return;
    case CSSPropertyZoom:
        style->setZoom(clampTo<float>(toAnimatableDouble(value)->toDouble(), std::numeric_limits<float>::denorm_min()));
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore
