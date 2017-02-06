// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/NumberPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

bool NumberPropertyFunctions::getInitialNumber(CSSPropertyID property, double& result)
{
    return getNumber(property, ComputedStyle::initialStyle(), result);
}

bool NumberPropertyFunctions::getNumber(CSSPropertyID property, const ComputedStyle& style, double& result)
{
    switch (property) {
    case CSSPropertyFillOpacity:
        result = style.fillOpacity();
        return true;
    case CSSPropertyFlexGrow:
        result = style.flexGrow();
        return true;
    case CSSPropertyFlexShrink:
        result = style.flexShrink();
        return true;
    case CSSPropertyFloodOpacity:
        result = style.floodOpacity();
        return true;
    case CSSPropertyOpacity:
        result = style.opacity();
        return true;
    case CSSPropertyOrphans:
        result = style.orphans();
        return true;
    case CSSPropertyShapeImageThreshold:
        result = style.shapeImageThreshold();
        return true;
    case CSSPropertyStopOpacity:
        result = style.stopOpacity();
        return true;
    case CSSPropertyStrokeMiterlimit:
        result = style.strokeMiterLimit();
        return true;
    case CSSPropertyStrokeOpacity:
        result = style.strokeOpacity();
        return true;
    case CSSPropertyWidows:
        result = style.widows();
        return true;

    case CSSPropertyFontSizeAdjust:
        if (!style.hasFontSizeAdjust())
            return false;
        result = style.fontSizeAdjust();
        return true;
    case CSSPropertyColumnCount:
        if (style.hasAutoColumnCount())
            return false;
        result = style.columnCount();
        return true;
    case CSSPropertyZIndex:
        if (style.hasAutoZIndex())
            return false;
        result = style.zIndex();
        return true;

    case CSSPropertyLineHeight: {
        const Length& length = style.specifiedLineHeight();
        // Numbers are represented by percentages.
        if (length.type() != Percent)
            return false;
        double value = length.value();
        // -100% represents the keyword "normal".
        if (value == -100)
            return false;
        result = value / 100;
        return true;
    }

    default:
        return false;
    }
}

double NumberPropertyFunctions::clampNumber(CSSPropertyID property, double value)
{
    switch (property) {
    case CSSPropertyStrokeMiterlimit:
        return clampTo<float>(value, 1);

    case CSSPropertyFloodOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyShapeImageThreshold:
        return clampTo<float>(value, 0, 1);

    case CSSPropertyFillOpacity:
    case CSSPropertyOpacity:
        return clampTo<float>(value, 0, nextafterf(1, 0));

    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyLineHeight:
        return clampTo<float>(value, 0);

    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return clampTo<short>(round(value), 1);

    case CSSPropertyColumnCount:
        return clampTo<unsigned short>(round(value), 1);

    case CSSPropertyColumnRuleWidth:
        return clampTo<unsigned short>(round(value));

    case CSSPropertyZIndex:
        return clampTo<int>(round(value));

    default:
        NOTREACHED();
        return value;
    }
}

bool NumberPropertyFunctions::setNumber(CSSPropertyID property, ComputedStyle& style, double value)
{
    ASSERT(value == clampNumber(property, value));
    switch (property) {
    case CSSPropertyFillOpacity:
        style.setFillOpacity(value);
        return true;
    case CSSPropertyFlexGrow:
        style.setFlexGrow(value);
        return true;
    case CSSPropertyFlexShrink:
        style.setFlexShrink(value);
        return true;
    case CSSPropertyFloodOpacity:
        style.setFloodOpacity(value);
        return true;
    case CSSPropertyLineHeight:
        style.setLineHeight(Length(value * 100, Percent));
        return true;
    case CSSPropertyOpacity:
        style.setOpacity(value);
        return true;
    case CSSPropertyOrphans:
        style.setOrphans(value);
        return true;
    case CSSPropertyShapeImageThreshold:
        style.setShapeImageThreshold(value);
        return true;
    case CSSPropertyStopOpacity:
        style.setStopOpacity(value);
        return true;
    case CSSPropertyStrokeMiterlimit:
        style.setStrokeMiterLimit(value);
        return true;
    case CSSPropertyStrokeOpacity:
        style.setStrokeOpacity(value);
        return true;
    case CSSPropertyColumnCount:
        style.setColumnCount(value);
        return true;
    case CSSPropertyWidows:
        style.setWidows(value);
        return true;
    case CSSPropertyZIndex:
        style.setZIndex(value);
        return true;
    default:
        return false;
    }
}

} // namespace blink
