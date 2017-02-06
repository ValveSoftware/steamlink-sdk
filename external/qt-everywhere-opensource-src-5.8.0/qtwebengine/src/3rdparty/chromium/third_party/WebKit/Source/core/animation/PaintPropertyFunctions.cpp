// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PaintPropertyFunctions.h"

#include "core/css/StyleColor.h"
#include "core/style/ComputedStyle.h"

namespace blink {

bool PaintPropertyFunctions::getInitialColor(CSSPropertyID property, StyleColor& result)
{
    return getColor(property, ComputedStyle::initialStyle(), result);
}

static bool getColorFromPaint(const SVGPaintType type, const Color color, StyleColor& result)
{
    switch (type) {
    case SVG_PAINTTYPE_RGBCOLOR:
        result = color;
        return true;
    case SVG_PAINTTYPE_CURRENTCOLOR:
        result = StyleColor::currentColor();
        return true;
    default:
        return false;
    }
}

bool PaintPropertyFunctions::getColor(CSSPropertyID property, const ComputedStyle& style, StyleColor& result)
{
    switch (property) {
    case CSSPropertyFill:
        return getColorFromPaint(style.svgStyle().fillPaintType(), style.svgStyle().fillPaintColor(), result);
    case CSSPropertyStroke:
        return getColorFromPaint(style.svgStyle().strokePaintType(), style.svgStyle().strokePaintColor(), result);
    default:
        NOTREACHED();
        return false;
    }
}

void PaintPropertyFunctions::setColor(CSSPropertyID property, ComputedStyle& style, const Color& color)
{
    switch (property) {
    case CSSPropertyFill:
        style.accessSVGStyle().setFillPaint(SVG_PAINTTYPE_RGBCOLOR, color, String(), true, true);
        break;
    case CSSPropertyStroke:
        style.accessSVGStyle().setStrokePaint(SVG_PAINTTYPE_RGBCOLOR, color, String(), true, true);
        break;
    default:
        NOTREACHED();
    }
}


} // namespace blink
