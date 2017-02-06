// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/BorderEdge.h"

namespace blink {

BorderEdge::BorderEdge(int edgeWidth, const Color& edgeColor, EBorderStyle edgeStyle, bool edgeIsPresent)
    : width(edgeWidth)
    , color(edgeColor)
    , isPresent(edgeIsPresent)
    , style(edgeStyle)
{
    if (style == BorderStyleDouble && edgeWidth < 3)
        style = BorderStyleSolid;
}

BorderEdge::BorderEdge()
    : width(0)
    , isPresent(false)
    , style(BorderStyleHidden)
{
}

bool BorderEdge::hasVisibleColorAndStyle() const
{
    return style > BorderStyleHidden && color.alpha() > 0;
}

bool BorderEdge::shouldRender() const
{
    return isPresent && width && hasVisibleColorAndStyle();
}

bool BorderEdge::presentButInvisible() const
{
    return usedWidth() && !hasVisibleColorAndStyle();
}

bool BorderEdge::obscuresBackgroundEdge() const
{
    if (!isPresent || color.hasAlpha() || style == BorderStyleHidden)
        return false;

    if (style == BorderStyleDotted || style == BorderStyleDashed)
        return false;

    return true;
}

bool BorderEdge::obscuresBackground() const
{
    if (!isPresent || color.hasAlpha() || style == BorderStyleHidden)
        return false;

    if (style == BorderStyleDotted || style == BorderStyleDashed || style == BorderStyleDouble)
        return false;

    return true;
}

int BorderEdge::usedWidth() const
{
    return isPresent ? width : 0;
}

int BorderEdge::getDoubleBorderStripeWidth(DoubleBorderStripe stripe) const
{
    ASSERT(stripe == DoubleBorderStripeOuter || stripe == DoubleBorderStripeInner);

    // We need certain integer rounding results.
    return stripe == DoubleBorderStripeOuter
        ? (usedWidth() + 1) / 3
        : (usedWidth() * 2 + 1) / 3;
}

bool BorderEdge::sharesColorWith(const BorderEdge& other) const
{
    return color == other.color;
}

} // namespace blink
