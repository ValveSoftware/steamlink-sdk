// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

// TODO(alancutter): Generate these functions.

ValueRange LengthPropertyFunctions::getValueRange(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyFlexBasis:
    case CSSPropertyHeight:
    case CSSPropertyLineHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPerspective:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyShapeMargin:
    case CSSPropertyStrokeWidth:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyColumnGap:
    case CSSPropertyColumnWidth:
    case CSSPropertyWidth:
        return ValueRangeNonNegative;
    default:
        return ValueRangeAll;
    }
}

bool LengthPropertyFunctions::isZoomedLength(CSSPropertyID property)
{
    return property != CSSPropertyStrokeWidth;
}

bool LengthPropertyFunctions::getPixelsForKeyword(CSSPropertyID property, CSSValueID valueID, double& result)
{
    switch (property) {
    case CSSPropertyBaselineShift:
        if (valueID == CSSValueBaseline) {
            result = 0;
            return true;
        }
        return false;
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyOutlineWidth:
        if (valueID == CSSValueThin) {
            result = 1;
            return true;
        }
        if (valueID == CSSValueMedium) {
            result = 3;
            return true;
        }
        if (valueID == CSSValueThick) {
            result = 5;
            return true;
        }
        return false;
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
        if (valueID == CSSValueNormal) {
            result = 0;
            return true;
        }
        return false;
    default:
        return false;
    }
}

static Length lengthFromUnsigned(unsigned short value)
{
    return Length(static_cast<float>(value), Fixed);
}

bool LengthPropertyFunctions::getInitialLength(CSSPropertyID property, Length& result)
{
    switch (property) {
    // The computed value of "initial" for the following properties is 0px if the associated *-style property resolves to "none" or "hidden".
    // border-width: https://drafts.csswg.org/css-backgrounds-3/#the-border-width
    // outline-width: https://drafts.csswg.org/css-ui-3/#outline-width
    // column-rule-width: https://drafts.csswg.org/css-multicol-1/#crw
    // We ignore this value adjustment for animations and use the wrong value for hidden widths to avoid
    // having to restart our animations based on the computed *-style values.
    // This is acceptable since animations running on hidden widths are unobservable to the user, even via getComputedStyle().
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
        result = lengthFromUnsigned(ComputedStyle::initialBorderWidth());
        return true;
    case CSSPropertyOutlineWidth:
        result = lengthFromUnsigned(ComputedStyle::initialOutlineWidth());
        return true;
    case CSSPropertyColumnRuleWidth:
        result = lengthFromUnsigned(ComputedStyle::initialColumnRuleWidth());
        return true;

    default:
        return getLength(property, ComputedStyle::initialStyle(), result);
    }
}

bool LengthPropertyFunctions::getLength(CSSPropertyID property, const ComputedStyle& style, Length& result)
{
    switch (property) {
    case CSSPropertyBottom:
        result = style.bottom();
        return true;
    case CSSPropertyCx:
        result = style.svgStyle().cx();
        return true;
    case CSSPropertyCy:
        result = style.svgStyle().cy();
        return true;
    case CSSPropertyFlexBasis:
        result = style.flexBasis();
        return true;
    case CSSPropertyHeight:
        result = style.height();
        return true;
    case CSSPropertyLeft:
        result = style.left();
        return true;
    case CSSPropertyMarginBottom:
        result = style.marginBottom();
        return true;
    case CSSPropertyMarginLeft:
        result = style.marginLeft();
        return true;
    case CSSPropertyMarginRight:
        result = style.marginRight();
        return true;
    case CSSPropertyMarginTop:
        result = style.marginTop();
        return true;
    case CSSPropertyMaxHeight:
        result = style.maxHeight();
        return true;
    case CSSPropertyMaxWidth:
        result = style.maxWidth();
        return true;
    case CSSPropertyMinHeight:
        result = style.minHeight();
        return true;
    case CSSPropertyMinWidth:
        result = style.minWidth();
        return true;
    case CSSPropertyMotionOffset:
        result = style.motionOffset();
        return true;
    case CSSPropertyPaddingBottom:
        result = style.paddingBottom();
        return true;
    case CSSPropertyPaddingLeft:
        result = style.paddingLeft();
        return true;
    case CSSPropertyPaddingRight:
        result = style.paddingRight();
        return true;
    case CSSPropertyPaddingTop:
        result = style.paddingTop();
        return true;
    case CSSPropertyR:
        result = style.svgStyle().r();
        return true;
    case CSSPropertyRight:
        result = style.right();
        return true;
    case CSSPropertyRx:
        result = style.svgStyle().rx();
        return true;
    case CSSPropertyRy:
        result = style.svgStyle().ry();
        return true;
    case CSSPropertyShapeMargin:
        result = style.shapeMargin();
        return true;
    case CSSPropertyStrokeDashoffset:
        result = style.strokeDashOffset();
        return true;
    case CSSPropertyTextIndent:
        result = style.textIndent();
        return true;
    case CSSPropertyTop:
        result = style.top();
        return true;
    case CSSPropertyWebkitPerspectiveOriginX:
        result = style.perspectiveOriginX();
        return true;
    case CSSPropertyWebkitPerspectiveOriginY:
        result = style.perspectiveOriginY();
        return true;
    case CSSPropertyWebkitTransformOriginX:
        result = style.transformOriginX();
        return true;
    case CSSPropertyWebkitTransformOriginY:
        result = style.transformOriginY();
        return true;
    case CSSPropertyWidth:
        result = style.width();
        return true;
    case CSSPropertyX:
        result = style.svgStyle().x();
        return true;
    case CSSPropertyY:
        result = style.svgStyle().y();
        return true;

    case CSSPropertyBorderBottomWidth:
        result = Length(style.borderBottomWidth(), Fixed);
        return true;
    case CSSPropertyBorderLeftWidth:
        result = Length(style.borderLeftWidth(), Fixed);
        return true;
    case CSSPropertyBorderRightWidth:
        result = Length(style.borderRightWidth(), Fixed);
        return true;
    case CSSPropertyBorderTopWidth:
        result = Length(style.borderTopWidth(), Fixed);
        return true;
    case CSSPropertyLetterSpacing:
        result = Length(style.letterSpacing(), Fixed);
        return true;
    case CSSPropertyOutlineOffset:
        result = Length(style.outlineOffset(), Fixed);
        return true;
    case CSSPropertyOutlineWidth:
        result = Length(style.outlineWidth(), Fixed);
        return true;
    case CSSPropertyWebkitBorderHorizontalSpacing:
        result = Length(style.horizontalBorderSpacing(), Fixed);
        return true;
    case CSSPropertyWebkitBorderVerticalSpacing:
        result = Length(style.verticalBorderSpacing(), Fixed);
        return true;
    case CSSPropertyColumnGap:
        result = Length(style.columnGap(), Fixed);
        return true;
    case CSSPropertyColumnRuleWidth:
        result = Length(style.columnRuleWidth(), Fixed);
        return true;
    case CSSPropertyWebkitTransformOriginZ:
        result = Length(style.transformOriginZ(), Fixed);
        return true;
    case CSSPropertyWordSpacing:
        result = Length(style.wordSpacing(), Fixed);
        return true;

    case CSSPropertyBaselineShift:
        if (style.baselineShift() != BS_LENGTH)
            return false;
        result = style.baselineShiftValue();
        return true;
    case CSSPropertyLineHeight:
        // Percent Lengths are used to represent numbers on line-height.
        if (style.specifiedLineHeight().hasPercent())
            return false;
        result = style.specifiedLineHeight();
        return true;
    case CSSPropertyPerspective:
        if (!style.hasPerspective())
            return false;
        result = Length(style.perspective(), Fixed);
        return true;
    case CSSPropertyStrokeWidth:
        ASSERT(!isZoomedLength(CSSPropertyStrokeWidth));
        result = style.strokeWidth().length();
        return true;
    case CSSPropertyVerticalAlign:
        if (style.verticalAlign() != VerticalAlignLength)
            return false;
        result = style.getVerticalAlignLength();
        return true;
    case CSSPropertyColumnWidth:
        if (style.hasAutoColumnWidth())
            return false;
        result = Length(style.columnWidth(), Fixed);
        return true;
    default:
        return false;
    }
}

bool LengthPropertyFunctions::setLength(CSSPropertyID property, ComputedStyle& style, const Length& value)
{
    switch (property) {
    // Setters that take a Length value.
    case CSSPropertyBaselineShift:
        style.setBaselineShiftValue(value);
        return true;
    case CSSPropertyBottom:
        style.setBottom(value);
        return true;
    case CSSPropertyCx:
        style.setCx(value);
        return true;
    case CSSPropertyCy:
        style.setCy(value);
        return true;
    case CSSPropertyFlexBasis:
        style.setFlexBasis(value);
        return true;
    case CSSPropertyHeight:
        style.setHeight(value);
        return true;
    case CSSPropertyLeft:
        style.setLeft(value);
        return true;
    case CSSPropertyMarginBottom:
        style.setMarginBottom(value);
        return true;
    case CSSPropertyMarginLeft:
        style.setMarginLeft(value);
        return true;
    case CSSPropertyMarginRight:
        style.setMarginRight(value);
        return true;
    case CSSPropertyMarginTop:
        style.setMarginTop(value);
        return true;
    case CSSPropertyMaxHeight:
        style.setMaxHeight(value);
        return true;
    case CSSPropertyMaxWidth:
        style.setMaxWidth(value);
        return true;
    case CSSPropertyMinHeight:
        style.setMinHeight(value);
        return true;
    case CSSPropertyMinWidth:
        style.setMinWidth(value);
        return true;
    case CSSPropertyMotionOffset:
        style.setMotionOffset(value);
        return true;
    case CSSPropertyPaddingBottom:
        style.setPaddingBottom(value);
        return true;
    case CSSPropertyPaddingLeft:
        style.setPaddingLeft(value);
        return true;
    case CSSPropertyPaddingRight:
        style.setPaddingRight(value);
        return true;
    case CSSPropertyPaddingTop:
        style.setPaddingTop(value);
        return true;
    case CSSPropertyR:
        style.setR(value);
        return true;
    case CSSPropertyRx:
        style.setRx(value);
        return true;
    case CSSPropertyRy:
        style.setRy(value);
        return true;
    case CSSPropertyRight:
        style.setRight(value);
        return true;
    case CSSPropertyShapeMargin:
        style.setShapeMargin(value);
        return true;
    case CSSPropertyStrokeDashoffset:
        style.setStrokeDashOffset(value);
        return true;
    case CSSPropertyTop:
        style.setTop(value);
        return true;
    case CSSPropertyWidth:
        style.setWidth(value);
        return true;
    case CSSPropertyWebkitPerspectiveOriginX:
        style.setPerspectiveOriginX(value);
        return true;
    case CSSPropertyWebkitPerspectiveOriginY:
        style.setPerspectiveOriginY(value);
        return true;
    case CSSPropertyWebkitTransformOriginX:
        style.setTransformOriginX(value);
        return true;
    case CSSPropertyWebkitTransformOriginY:
        style.setTransformOriginY(value);
        return true;
    case CSSPropertyX:
        style.setX(value);
        return true;
    case CSSPropertyY:
        style.setY(value);
        return true;

    case CSSPropertyLineHeight:
        // Percent Lengths are used to represent numbers on line-height.
        if (value.hasPercent())
            return false;
        style.setLineHeight(value);
        return true;

    // TODO(alancutter): Support setters that take a numeric value (need to resolve percentages).
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyLetterSpacing:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPerspective:
    case CSSPropertyStrokeWidth:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyColumnGap:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyColumnWidth:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWordSpacing:
        return false;

    default:
        return false;
    }
}

} // namespace blink
