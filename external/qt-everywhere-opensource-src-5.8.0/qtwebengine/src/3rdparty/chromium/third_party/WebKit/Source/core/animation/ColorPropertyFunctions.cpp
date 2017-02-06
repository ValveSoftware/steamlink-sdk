// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ColorPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

StyleColor ColorPropertyFunctions::getInitialColor(CSSPropertyID property)
{
    return getUnvisitedColor(property, ComputedStyle::initialStyle());
}

StyleColor ColorPropertyFunctions::getUnvisitedColor(CSSPropertyID property, const ComputedStyle& style)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
        return style.backgroundColor();
    case CSSPropertyBorderLeftColor:
        return style.borderLeftColor();
    case CSSPropertyBorderRightColor:
        return style.borderRightColor();
    case CSSPropertyBorderTopColor:
        return style.borderTopColor();
    case CSSPropertyBorderBottomColor:
        return style.borderBottomColor();
    case CSSPropertyColor:
        return style.color();
    case CSSPropertyOutlineColor:
        return style.outlineColor();
    case CSSPropertyColumnRuleColor:
        return style.columnRuleColor();
    case CSSPropertyWebkitTextEmphasisColor:
        return style.textEmphasisColor();
    case CSSPropertyWebkitTextFillColor:
        return style.textFillColor();
    case CSSPropertyWebkitTextStrokeColor:
        return style.textStrokeColor();
    case CSSPropertyFloodColor:
        return style.floodColor();
    case CSSPropertyLightingColor:
        return style.lightingColor();
    case CSSPropertyStopColor:
        return style.stopColor();
    case CSSPropertyWebkitTapHighlightColor:
        return style.tapHighlightColor();
    case CSSPropertyTextDecorationColor:
        return style.textDecorationColor();
    default:
        NOTREACHED();
        return StyleColor::currentColor();
    }
}

StyleColor ColorPropertyFunctions::getVisitedColor(CSSPropertyID property, const ComputedStyle& style)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
        return style.visitedLinkBackgroundColor();
    case CSSPropertyBorderLeftColor:
        return style.visitedLinkBorderLeftColor();
    case CSSPropertyBorderRightColor:
        return style.visitedLinkBorderRightColor();
    case CSSPropertyBorderTopColor:
        return style.visitedLinkBorderTopColor();
    case CSSPropertyBorderBottomColor:
        return style.visitedLinkBorderBottomColor();
    case CSSPropertyColor:
        return style.visitedLinkColor();
    case CSSPropertyOutlineColor:
        return style.visitedLinkOutlineColor();
    case CSSPropertyColumnRuleColor:
        return style.visitedLinkColumnRuleColor();
    case CSSPropertyWebkitTextEmphasisColor:
        return style.visitedLinkTextEmphasisColor();
    case CSSPropertyWebkitTextFillColor:
        return style.visitedLinkTextFillColor();
    case CSSPropertyWebkitTextStrokeColor:
        return style.visitedLinkTextStrokeColor();
    case CSSPropertyFloodColor:
        return style.floodColor();
    case CSSPropertyLightingColor:
        return style.lightingColor();
    case CSSPropertyStopColor:
        return style.stopColor();
    case CSSPropertyWebkitTapHighlightColor:
        return style.tapHighlightColor();
    case CSSPropertyTextDecorationColor:
        return style.visitedLinkTextDecorationColor();
    default:
        NOTREACHED();
        return StyleColor::currentColor();
    }
}

void ColorPropertyFunctions::setUnvisitedColor(CSSPropertyID property, ComputedStyle& style, const Color& color)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
        style.setBackgroundColor(color);
        return;
    case CSSPropertyBorderBottomColor:
        style.setBorderBottomColor(color);
        return;
    case CSSPropertyBorderLeftColor:
        style.setBorderLeftColor(color);
        return;
    case CSSPropertyBorderRightColor:
        style.setBorderRightColor(color);
        return;
    case CSSPropertyBorderTopColor:
        style.setBorderTopColor(color);
        return;
    case CSSPropertyColor:
        style.setColor(color);
        return;
    case CSSPropertyFloodColor:
        style.setFloodColor(color);
        return;
    case CSSPropertyLightingColor:
        style.setLightingColor(color);
        return;
    case CSSPropertyOutlineColor:
        style.setOutlineColor(color);
        return;
    case CSSPropertyStopColor:
        style.setStopColor(color);
        return;
    case CSSPropertyTextDecorationColor:
        style.setTextDecorationColor(color);
        return;
    case CSSPropertyColumnRuleColor:
        style.setColumnRuleColor(color);
        return;
    case CSSPropertyWebkitTextStrokeColor:
        style.setTextStrokeColor(color);
        return;
    default:
        NOTREACHED();
        return;
    }
}

void ColorPropertyFunctions::setVisitedColor(CSSPropertyID property, ComputedStyle& style, const Color& color)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
        style.setVisitedLinkBackgroundColor(color);
        return;
    case CSSPropertyBorderBottomColor:
        style.setVisitedLinkBorderBottomColor(color);
        return;
    case CSSPropertyBorderLeftColor:
        style.setVisitedLinkBorderLeftColor(color);
        return;
    case CSSPropertyBorderRightColor:
        style.setVisitedLinkBorderRightColor(color);
        return;
    case CSSPropertyBorderTopColor:
        style.setVisitedLinkBorderTopColor(color);
        return;
    case CSSPropertyColor:
        style.setVisitedLinkColor(color);
        return;
    case CSSPropertyFloodColor:
        style.setFloodColor(color);
        return;
    case CSSPropertyLightingColor:
        style.setLightingColor(color);
        return;
    case CSSPropertyOutlineColor:
        style.setVisitedLinkOutlineColor(color);
        return;
    case CSSPropertyStopColor:
        style.setStopColor(color);
        return;
    case CSSPropertyTextDecorationColor:
        style.setVisitedLinkTextDecorationColor(color);
        return;
    case CSSPropertyColumnRuleColor:
        style.setVisitedLinkColumnRuleColor(color);
        return;
    case CSSPropertyWebkitTextStrokeColor:
        style.setVisitedLinkTextStrokeColor(color);
        return;
    default:
        NOTREACHED();
        return;
    }
}

} // namespace blink
