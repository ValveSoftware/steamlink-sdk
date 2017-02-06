/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/ContentData.h"
#include "core/style/CounterContent.h"
#include "core/style/QuotesData.h"
#include "core/style/SVGComputedStyle.h"
#include "core/style/StyleGeneratedImage.h"
#include "core/style/StyleVariableData.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

namespace {

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        return false;
    }
}

} // namespace

void StyleBuilder::applyProperty(CSSPropertyID id, StyleResolverState& state, const CSSValue& value)
{
    if (RuntimeEnabledFeatures::cssVariablesEnabled() && id != CSSPropertyVariable
        && (value.isVariableReferenceValue() || value.isPendingSubstitutionValue())) {

        const CSSValue* resolvedValue = value.isVariableReferenceValue() ?
            CSSVariableResolver::resolveVariableReferences(state, id, toCSSVariableReferenceValue(value)) :
            CSSVariableResolver::resolvePendingSubstitutions(state, id, toCSSPendingSubstitutionValue(value));
        applyProperty(id, state, *resolvedValue);

        if (!state.style()->hasVariableReferenceFromNonInheritedProperty() && !CSSPropertyMetadata::isInheritedProperty(id))
            state.style()->setHasVariableReferenceFromNonInheritedProperty();
        return;
    }

    DCHECK(!isShorthandProperty(id)) << "Shorthand property id = " << id << " wasn't expanded at parsing time";

    bool isInherit = state.parentNode() && value.isInheritedValue();
    bool isInitial = value.isInitialValue() || (!state.parentNode() && value.isInheritedValue());

    DCHECK(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    DCHECK(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSPropertyMetadata::isInheritedProperty(id)) {
        state.parentStyle()->setHasExplicitlyInheritedProperties();
    } else if (value.isUnsetValue()) {
        DCHECK(!isInherit && !isInitial);
        if (CSSPropertyMetadata::isInheritedProperty(id))
            isInherit = true;
        else
            isInitial = true;
    }

    StyleBuilder::applyProperty(id, state, value, isInitial, isInherit);
}

void StyleBuilderFunctions::applyInitialCSSPropertyColor(StyleResolverState& state)
{
    Color color = ComputedStyle::initialColor();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyInheritCSSPropertyColor(StyleResolverState& state)
{
    Color color = state.parentStyle()->color();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyValueCSSPropertyColor(StyleResolverState& state, const CSSValue& value)
{
    // As per the spec, 'color: currentColor' is treated as 'color: inherit'
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueCurrentcolor) {
        applyInheritCSSPropertyColor(state);
        return;
    }

    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(StyleBuilderConverter::convertColor(state, value));
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(StyleBuilderConverter::convertColor(state, value, true));
}

void StyleBuilderFunctions::applyInitialCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->clearCursorList();
    state.style()->setCursor(ComputedStyle::initialCursor());
}

void StyleBuilderFunctions::applyInheritCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->setCursor(state.parentStyle()->cursor());
    state.style()->setCursorList(state.parentStyle()->cursors());
}

void StyleBuilderFunctions::applyValueCSSPropertyCursor(StyleResolverState& state, const CSSValue& value)
{
    state.style()->clearCursorList();
    if (value.isValueList()) {
        const CSSValueList& list = toCSSValueList(value);
        int len = list.length();
        state.style()->setCursor(CURSOR_AUTO);
        for (int i = 0; i < len; i++) {
            const CSSValue& item = list.item(i);
            if (item.isCursorImageValue()) {
                const CSSCursorImageValue& image = toCSSCursorImageValue(item);
                IntPoint hotSpot = image.hotSpot();
                bool hotSpotSpecified = image.hotSpotSpecified();

                Element* element = state.element();
                if (SVGCursorElement* cursorElement = image.getSVGCursorElement(element)) {
                    if (image.cachedImageURL() != element->document().completeURL(cursorElement->href()->currentValue()->value()))
                        image.clearImageResource();

                    // Set the hot spot if it wasn't specified in the CSS but is specified in the SVG.
                    if (!hotSpotSpecified) {
                        hotSpotSpecified = true;
                        SVGLengthContext lengthContext(0);
                        // x() and y() return 0 if they're not specified in cursorElement.
                        float svgX = roundf(cursorElement->x()->currentValue()->value(lengthContext));
                        hotSpot.setX(static_cast<int>(svgX));
                        float svgY = roundf(cursorElement->y()->currentValue()->value(lengthContext));
                        hotSpot.setY(static_cast<int>(svgY));
                    }

                    SVGElement* svgElement = toSVGElement(element);
                    svgElement->setCursorImageValue(&image);
                    cursorElement->addClient(svgElement);

                    // Elements with SVG cursors are not allowed to share style.
                    state.style()->setUnique();
                }

                state.style()->addCursor(state.styleImage(CSSPropertyCursor, image), hotSpotSpecified, hotSpot);
            } else {
                state.style()->setCursor(toCSSPrimitiveValue(item).convertTo<ECursor>());
            }
        }
    } else {
        state.style()->setCursor(toCSSPrimitiveValue(value).convertTo<ECursor>());
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyDirection(StyleResolverState& state, const CSSValue& value)
{
    state.style()->setDirection(toCSSPrimitiveValue(value).convertTo<TextDirection>());
}

void StyleBuilderFunctions::applyInitialCSSPropertyGridTemplateAreas(StyleResolverState& state)
{
    state.style()->setNamedGridArea(ComputedStyle::initialNamedGridArea());
    state.style()->setNamedGridAreaRowCount(ComputedStyle::initialNamedGridAreaCount());
    state.style()->setNamedGridAreaColumnCount(ComputedStyle::initialNamedGridAreaCount());
}

void StyleBuilderFunctions::applyInheritCSSPropertyGridTemplateAreas(StyleResolverState& state)
{
    state.style()->setNamedGridArea(state.parentStyle()->namedGridArea());
    state.style()->setNamedGridAreaRowCount(state.parentStyle()->namedGridAreaRowCount());
    state.style()->setNamedGridAreaColumnCount(state.parentStyle()->namedGridAreaColumnCount());
}

void StyleBuilderFunctions::applyValueCSSPropertyGridTemplateAreas(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        // FIXME: Shouldn't we clear the grid-area values
        DCHECK_EQ(toCSSPrimitiveValue(value).getValueID(), CSSValueNone);
        return;
    }

    const CSSGridTemplateAreasValue& gridTemplateAreasValue = toCSSGridTemplateAreasValue(value);
    const NamedGridAreaMap& newNamedGridAreas = gridTemplateAreasValue.gridAreaMap();

    NamedGridLinesMap namedGridColumnLines;
    NamedGridLinesMap namedGridRowLines;
    StyleBuilderConverter::convertOrderedNamedGridLinesMapToNamedGridLinesMap(state.style()->orderedNamedGridColumnLines(), namedGridColumnLines);
    StyleBuilderConverter::convertOrderedNamedGridLinesMapToNamedGridLinesMap(state.style()->orderedNamedGridRowLines(), namedGridRowLines);
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridColumnLines, ForColumns);
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridRowLines, ForRows);
    state.style()->setNamedGridColumnLines(namedGridColumnLines);
    state.style()->setNamedGridRowLines(namedGridRowLines);

    state.style()->setNamedGridArea(newNamedGridAreas);
    state.style()->setNamedGridAreaRowCount(gridTemplateAreasValue.rowCount());
    state.style()->setNamedGridAreaColumnCount(gridTemplateAreasValue.columnCount());
}

void StyleBuilderFunctions::applyValueCSSPropertyListStyleImage(StyleResolverState& state, const CSSValue& value)
{
    state.style()->setListStyleImage(state.styleImage(CSSPropertyListStyleImage, value));
}

void StyleBuilderFunctions::applyInitialCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(ComputedStyle::initialOutlineStyleIsAuto());
    state.style()->setOutlineStyle(ComputedStyle::initialBorderStyle());
}

void StyleBuilderFunctions::applyInheritCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(state.parentStyle()->outlineStyleIsAuto());
    state.style()->setOutlineStyle(state.parentStyle()->outlineStyle());
}

void StyleBuilderFunctions::applyValueCSSPropertyOutlineStyle(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    state.style()->setOutlineStyleIsAuto(primitiveValue.convertTo<OutlineIsAuto>());
    state.style()->setOutlineStyle(primitiveValue.convertTo<EBorderStyle>());
}

void StyleBuilderFunctions::applyValueCSSPropertyResize(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    EResize r = RESIZE_NONE;
    if (primitiveValue.getValueID() == CSSValueAuto) {
        if (Settings* settings = state.document().settings())
            r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
    } else {
        r = primitiveValue.convertTo<EResize>();
    }
    state.style()->setResize(r);
}

static float mmToPx(float mm) { return mm * cssPixelsPerMillimeter; }
static float inchToPx(float inch) { return inch * cssPixelsPerInch; }
static FloatSize getPageSizeFromName(const CSSPrimitiveValue& pageSizeName)
{
    switch (pageSizeName.getValueID()) {
    case CSSValueA5:
        return FloatSize(mmToPx(148), mmToPx(210));
    case CSSValueA4:
        return FloatSize(mmToPx(210), mmToPx(297));
    case CSSValueA3:
        return FloatSize(mmToPx(297), mmToPx(420));
    case CSSValueB5:
        return FloatSize(mmToPx(176), mmToPx(250));
    case CSSValueB4:
        return FloatSize(mmToPx(250), mmToPx(353));
    case CSSValueLetter:
        return FloatSize(inchToPx(8.5), inchToPx(11));
    case CSSValueLegal:
        return FloatSize(inchToPx(8.5), inchToPx(14));
    case CSSValueLedger:
        return FloatSize(inchToPx(11), inchToPx(17));
    default:
        NOTREACHED();
        return FloatSize(0, 0);
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyInheritCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyValueCSSPropertySize(StyleResolverState& state, const CSSValue& value)
{
    state.style()->resetPageSizeType();
    FloatSize size;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;
    const CSSValueList& list = toCSSValueList(value);
    if (list.length() == 2) {
        // <length>{2} | <page-size> <orientation>
        const CSSPrimitiveValue& first = toCSSPrimitiveValue(list.item(0));
        const CSSPrimitiveValue& second = toCSSPrimitiveValue(list.item(1));
        if (first.isLength()) {
            // <length>{2}
            size = FloatSize(first.computeLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0)),
                second.computeLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0)));
        } else {
            // <page-size> <orientation>
            size = getPageSizeFromName(first);

            DCHECK(second.getValueID() == CSSValueLandscape || second.getValueID() == CSSValuePortrait);
            if (second.getValueID() == CSSValueLandscape)
                size = size.transposedSize();
        }
        pageSizeType = PAGE_SIZE_RESOLVED;
    } else {
        DCHECK_EQ(list.length(), 1U);
        // <length> | auto | <page-size> | [ portrait | landscape]
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(list.item(0));
        if (primitiveValue.isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            float width = primitiveValue.computeLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            size = FloatSize(width, width);
        } else {
            switch (primitiveValue.getValueID()) {
            case CSSValueAuto:
                pageSizeType = PAGE_SIZE_AUTO;
                break;
            case CSSValuePortrait:
                pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                break;
            case CSSValueLandscape:
                pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                break;
            default:
                // <page-size>
                pageSizeType = PAGE_SIZE_RESOLVED;
                size = getPageSizeFromName(primitiveValue);
            }
        }
    }
    state.style()->setPageSizeType(pageSizeType);
    state.style()->setPageSize(size);
}

void StyleBuilderFunctions::applyInitialCSSPropertySnapHeight(StyleResolverState& state)
{
    state.style()->setSnapHeightUnit(0);
    state.style()->setSnapHeightPosition(0);
}

void StyleBuilderFunctions::applyInheritCSSPropertySnapHeight(StyleResolverState& state)
{
    state.style()->setSnapHeightUnit(state.parentStyle()->snapHeightUnit());
    state.style()->setSnapHeightPosition(state.parentStyle()->snapHeightPosition());
}

void StyleBuilderFunctions::applyValueCSSPropertySnapHeight(StyleResolverState& state, const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);
    const CSSPrimitiveValue& first = toCSSPrimitiveValue(list.item(0));
    DCHECK(first.isLength());
    int unit = first.computeLength<int>(state.cssToLengthConversionData());
    DCHECK_GE(unit, 0);
    state.style()->setSnapHeightUnit(clampTo<uint8_t>(unit));

    if (list.length() == 1) {
        state.style()->setSnapHeightPosition(0);
        return;
    }

    DCHECK_EQ(list.length(), 2U);
    const CSSPrimitiveValue& second = toCSSPrimitiveValue(list.item(1));
    DCHECK(second.isNumber());
    int position = second.getIntValue();
    DCHECK(position > 0 && position <= 100);
    state.style()->setSnapHeightPosition(position);
}

void StyleBuilderFunctions::applyValueCSSPropertyTextAlign(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.isValueID() && primitiveValue.getValueID() != CSSValueWebkitMatchParent) {
        // Special case for th elements - UA stylesheet text-align does not apply if parent's computed value for text-align is not its initial value
        // https://html.spec.whatwg.org/multipage/rendering.html#tables-2
        if (primitiveValue.getValueID() == CSSValueInternalCenter && state.parentStyle()->textAlign() != ComputedStyle::initialTextAlign())
            state.style()->setTextAlign(state.parentStyle()->textAlign());
        else
            state.style()->setTextAlign(primitiveValue.convertTo<ETextAlign>());
    }
    else if (state.parentStyle()->textAlign() == TASTART)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
    else if (state.parentStyle()->textAlign() == TAEND)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
    else
        state.style()->setTextAlign(state.parentStyle()->textAlign());
}

void StyleBuilderFunctions::applyInheritCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(state.parentStyle()->textIndent());
    state.style()->setTextIndentLine(state.parentStyle()->getTextIndentLine());
    state.style()->setTextIndentType(state.parentStyle()->getTextIndentType());
}

void StyleBuilderFunctions::applyInitialCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(ComputedStyle::initialTextIndent());
    state.style()->setTextIndentLine(ComputedStyle::initialTextIndentLine());
    state.style()->setTextIndentType(ComputedStyle::initialTextIndentType());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextIndent(StyleResolverState& state, const CSSValue& value)
{
    Length lengthOrPercentageValue;
    TextIndentLine textIndentLineValue = ComputedStyle::initialTextIndentLine();
    TextIndentType textIndentTypeValue = ComputedStyle::initialTextIndentType();

    for (auto& listValue : toCSSValueList(value)) {
        const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(listValue.get());
        if (!primitiveValue->getValueID())
            lengthOrPercentageValue = primitiveValue->convertToLength(state.cssToLengthConversionData());
        else if (primitiveValue->getValueID() == CSSValueEachLine)
            textIndentLineValue = TextIndentEachLine;
        else if (primitiveValue->getValueID() == CSSValueHanging)
            textIndentTypeValue = TextIndentHanging;
        else
            NOTREACHED();
    }

    state.style()->setTextIndent(lengthOrPercentageValue);
    state.style()->setTextIndentLine(textIndentLineValue);
    state.style()->setTextIndentType(textIndentTypeValue);
}

void StyleBuilderFunctions::applyValueCSSPropertyTransform(StyleResolverState& state, const CSSValue& value)
{
    // FIXME: We should just make this a converter
    TransformOperations operations;
    TransformBuilder::createTransformOperations(value, state.cssToLengthConversionData(), operations);
    state.style()->setTransform(operations);
}

void StyleBuilderFunctions::applyInheritCSSPropertyVerticalAlign(StyleResolverState& state)
{
    EVerticalAlign verticalAlign = state.parentStyle()->verticalAlign();
    state.style()->setVerticalAlign(verticalAlign);
    if (verticalAlign == VerticalAlignLength)
        state.style()->setVerticalAlignLength(state.parentStyle()->getVerticalAlignLength());
}

void StyleBuilderFunctions::applyValueCSSPropertyVerticalAlign(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue.getValueID())
        state.style()->setVerticalAlign(primitiveValue.convertTo<EVerticalAlign>());
    else
        state.style()->setVerticalAlignLength(primitiveValue.convertToLength(state.cssToLengthConversionData()));
}

static void resetEffectiveZoom(StyleResolverState& state)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    state.setEffectiveZoom(state.parentStyle() ? state.parentStyle()->effectiveZoom() : ComputedStyle::initialZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(ComputedStyle::initialZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(state.parentStyle()->zoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyZoom(StyleResolverState& state, const CSSValue& value)
{
    SECURITY_DCHECK(value.isPrimitiveValue());
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue.getValueID() == CSSValueNormal) {
        resetEffectiveZoom(state);
        state.setZoom(ComputedStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueReset) {
        state.setEffectiveZoom(ComputedStyle::initialZoom());
        state.setZoom(ComputedStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueDocument) {
        float docZoom = state.rootElementStyle() ? state.rootElementStyle()->zoom() : ComputedStyle::initialZoom();
        state.setEffectiveZoom(docZoom);
        state.setZoom(docZoom);
    } else if (primitiveValue.isPercentage()) {
        resetEffectiveZoom(state);
        if (float percent = primitiveValue.getFloatValue())
            state.setZoom(percent / 100.0f);
    } else if (primitiveValue.isNumber()) {
        resetEffectiveZoom(state);
        if (float number = primitiveValue.getFloatValue())
            state.setZoom(number);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitBorderImage(StyleResolverState& state, const CSSValue& value)
{
    NinePieceImage image;
    CSSToStyleMap::mapNinePieceImage(state, CSSPropertyWebkitBorderImage, value, image);
    state.style()->setBorderImage(image);
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(ComputedStyle::initialTextEmphasisFill());
    state.style()->setTextEmphasisMark(ComputedStyle::initialTextEmphasisMark());
    state.style()->setTextEmphasisCustomMark(ComputedStyle::initialTextEmphasisCustomMark());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(state.parentStyle()->getTextEmphasisFill());
    state.style()->setTextEmphasisMark(state.parentStyle()->getTextEmphasisMark());
    state.style()->setTextEmphasisCustomMark(state.parentStyle()->textEmphasisCustomMark());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state, const CSSValue& value)
{
    if (value.isValueList()) {
        const CSSValueList& list = toCSSValueList(value);
        DCHECK_EQ(list.length(), 2U);
        for (unsigned i = 0; i < 2; ++i) {
            const CSSPrimitiveValue& value = toCSSPrimitiveValue(list.item(i));
            if (value.getValueID() == CSSValueFilled || value.getValueID() == CSSValueOpen)
                state.style()->setTextEmphasisFill(value.convertTo<TextEmphasisFill>());
            else
                state.style()->setTextEmphasisMark(value.convertTo<TextEmphasisMark>());
        }
        state.style()->setTextEmphasisCustomMark(nullAtom);
        return;
    }

    if (value.isStringValue()) {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(TextEmphasisMarkCustom);
        state.style()->setTextEmphasisCustomMark(AtomicString(toCSSStringValue(value).value()));
        return;
    }

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    state.style()->setTextEmphasisCustomMark(nullAtom);

    if (primitiveValue.getValueID() == CSSValueFilled || primitiveValue.getValueID() == CSSValueOpen) {
        state.style()->setTextEmphasisFill(primitiveValue.convertTo<TextEmphasisFill>());
        state.style()->setTextEmphasisMark(TextEmphasisMarkAuto);
    } else {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(primitiveValue.convertTo<TextEmphasisMark>());
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(false);
    state.style()->setWillChangeScrollPosition(false);
    state.style()->setWillChangeProperties(Vector<CSSPropertyID>());
    state.style()->setSubtreeWillChangeContents(state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(state.parentStyle()->willChangeContents());
    state.style()->setWillChangeScrollPosition(state.parentStyle()->willChangeScrollPosition());
    state.style()->setWillChangeProperties(state.parentStyle()->willChangeProperties());
    state.style()->setSubtreeWillChangeContents(state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyValueCSSPropertyWillChange(StyleResolverState& state, const CSSValue& value)
{
    bool willChangeContents = false;
    bool willChangeScrollPosition = false;
    Vector<CSSPropertyID> willChangeProperties;

    if (value.isPrimitiveValue()) {
        DCHECK_EQ(toCSSPrimitiveValue(value).getValueID(), CSSValueAuto);
    } else {
        DCHECK(value.isValueList());
        for (auto& willChangeValue : toCSSValueList(value)) {
            if (willChangeValue->isCustomIdentValue())
                willChangeProperties.append(toCSSCustomIdentValue(*willChangeValue).valueAsPropertyID());
            else if (toCSSPrimitiveValue(*willChangeValue).getValueID() == CSSValueContents)
                willChangeContents = true;
            else if (toCSSPrimitiveValue(*willChangeValue).getValueID() == CSSValueScrollPosition)
                willChangeScrollPosition = true;
            else
                NOTREACHED();
        }
    }
    state.style()->setWillChangeContents(willChangeContents);
    state.style()->setWillChangeScrollPosition(willChangeScrollPosition);
    state.style()->setWillChangeProperties(willChangeProperties);
    state.style()->setSubtreeWillChangeContents(willChangeContents || state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyInitialCSSPropertyContent(StyleResolverState& state)
{
    state.style()->setContent(nullptr);
}

void StyleBuilderFunctions::applyInheritCSSPropertyContent(StyleResolverState&)
{
    // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
    // note is a reminder that eventually "inherit" needs to be supported.
}

void StyleBuilderFunctions::applyValueCSSPropertyContent(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        DCHECK(toCSSPrimitiveValue(value).getValueID() == CSSValueNormal || toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
        state.style()->setContent(nullptr);
        return;
    }

    ContentData* firstContent = nullptr;
    ContentData* prevContent = nullptr;
    for (auto& item : toCSSValueList(value)) {
        ContentData* nextContent = nullptr;
        if (item->isImageGeneratorValue() || item->isImageSetValue() || item->isImageValue()) {
            nextContent = ContentData::create(state.styleImage(CSSPropertyContent, *item));
        } else if (item->isCounterValue()) {
            const CSSCounterValue* counterValue = toCSSCounterValue(item.get());
            EListStyleType listStyleType = NoneListStyle;
            CSSValueID listStyleIdent = counterValue->listStyle();
            if (listStyleIdent != CSSValueNone)
                listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
            std::unique_ptr<CounterContent> counter = wrapUnique(new CounterContent(AtomicString(counterValue->identifier()), listStyleType, AtomicString(counterValue->separator())));
            nextContent = ContentData::create(std::move(counter));
        } else if (item->isPrimitiveValue()) {
            QuoteType quoteType;
            switch (toCSSPrimitiveValue(*item).getValueID()) {
            default:
                NOTREACHED();
            case CSSValueOpenQuote:
                quoteType = OPEN_QUOTE;
                break;
            case CSSValueCloseQuote:
                quoteType = CLOSE_QUOTE;
                break;
            case CSSValueNoOpenQuote:
                quoteType = NO_OPEN_QUOTE;
                break;
            case CSSValueNoCloseQuote:
                quoteType = NO_CLOSE_QUOTE;
                break;
            }
            nextContent = ContentData::create(quoteType);
        } else {
            String string;
            if (item->isFunctionValue()) {
                const CSSFunctionValue* functionValue = toCSSFunctionValue(item.get());
                DCHECK_EQ(functionValue->functionType(), CSSValueAttr);
                // FIXME: Can a namespace be specified for an attr(foo)?
                if (state.style()->styleType() == PseudoIdNone)
                    state.style()->setUnique();
                else
                    state.parentStyle()->setUnique();
                QualifiedName attr(nullAtom, AtomicString(toCSSCustomIdentValue(functionValue->item(0)).value()), nullAtom);
                const AtomicString& value = state.element()->getAttribute(attr);
                string = value.isNull() ? emptyString() : value.getString();
            } else {
                string = toCSSStringValue(*item).value();
            }
            if (prevContent && prevContent->isText()) {
                TextContentData* textContent = toTextContentData(prevContent);
                textContent->setText(textContent->text() + string);
                continue;
            }
            nextContent = ContentData::create(string);
        }

        if (!firstContent)
            firstContent = nextContent;
        else
            prevContent->setNext(nextContent);

        prevContent = nextContent;
    }
    DCHECK(firstContent);
    state.style()->setContent(firstContent);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitLocale(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        DCHECK_EQ(toCSSPrimitiveValue(value).getValueID(), CSSValueAuto);
        state.fontBuilder().setLocale(nullAtom);
    } else {
        state.fontBuilder().setLocale(AtomicString(toCSSStringValue(value).value()));
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitAppRegion(StyleResolverState&)
{
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitAppRegion(StyleResolverState&)
{
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitAppRegion(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    state.style()->setDraggableRegionMode(primitiveValue.getValueID() == CSSValueDrag ? DraggableRegionDrag : DraggableRegionNoDrag);
    state.document().setHasAnnotatedRegions(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyWritingMode(StyleResolverState& state, const CSSValue& value)
{
    state.setWritingMode(toCSSPrimitiveValue(value).convertTo<WritingMode>());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitWritingMode(StyleResolverState& state, const CSSValue& value)
{
    state.setWritingMode(toCSSPrimitiveValue(value).convertTo<WritingMode>());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextOrientation(StyleResolverState& state, const CSSValue& value)
{
    state.setTextOrientation(toCSSPrimitiveValue(value).convertTo<TextOrientation>());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextOrientation(StyleResolverState& state, const CSSValue& value)
{
    state.setTextOrientation(toCSSPrimitiveValue(value).convertTo<TextOrientation>());
}

void StyleBuilderFunctions::applyValueCSSPropertyVariable(StyleResolverState& state, const CSSValue& value)
{
    const CSSCustomPropertyDeclaration& declaration = toCSSCustomPropertyDeclaration(value);
    switch (declaration.id()) {
    case CSSValueInitial:
        state.style()->removeVariable(declaration.name());
        break;

    case CSSValueUnset:
    case CSSValueInherit: {
        state.style()->removeVariable(declaration.name());
        StyleVariableData* parentVariables = state.parentStyle()->variables();
        if (!parentVariables)
            return;
        CSSVariableData* value = parentVariables->getVariable(declaration.name());
        if (!value)
            return;
        state.style()->setVariable(declaration.name(), value);
        break;
    }
    case CSSValueInternalVariableValue:
        state.style()->setVariable(declaration.name(), declaration.value());
        break;
    default:
        NOTREACHED();
    }
}

void StyleBuilderFunctions::applyInheritCSSPropertyBaselineShift(StyleResolverState& state)
{
    const SVGComputedStyle& parentSvgStyle = state.parentStyle()->svgStyle();
    EBaselineShift baselineShift = parentSvgStyle.baselineShift();
    SVGComputedStyle& svgStyle = state.style()->accessSVGStyle();
    svgStyle.setBaselineShift(baselineShift);
    if (baselineShift == BS_LENGTH)
        svgStyle.setBaselineShiftValue(parentSvgStyle.baselineShiftValue());
}

void StyleBuilderFunctions::applyValueCSSPropertyBaselineShift(StyleResolverState& state, const CSSValue& value)
{
    SVGComputedStyle& svgStyle = state.style()->accessSVGStyle();
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue.isValueID()) {
        svgStyle.setBaselineShift(BS_LENGTH);
        svgStyle.setBaselineShiftValue(StyleBuilderConverter::convertLength(state, primitiveValue));
        return;
    }
    switch (primitiveValue.getValueID()) {
    case CSSValueBaseline:
        svgStyle.setBaselineShift(BS_LENGTH);
        svgStyle.setBaselineShiftValue(Length(Fixed));
        return;
    case CSSValueSub:
        svgStyle.setBaselineShift(BS_SUB);
        return;
    case CSSValueSuper:
        svgStyle.setBaselineShift(BS_SUPER);
        return;
    default:
        NOTREACHED();
    }
}

void StyleBuilderFunctions::applyInheritCSSPropertyPosition(StyleResolverState& state)
{
    if (!state.parentNode()->isDocumentNode())
        state.style()->setPosition(state.parentStyle()->position());
}

} // namespace blink
