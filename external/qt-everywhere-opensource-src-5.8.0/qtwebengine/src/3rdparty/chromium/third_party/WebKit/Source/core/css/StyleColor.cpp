// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleColor.h"

#include "core/layout/LayoutTheme.h"

namespace blink {

Color StyleColor::colorFromKeyword(CSSValueID keyword)
{
    if (const char* valueName = getValueName(keyword)) {
        if (const NamedColor* namedColor = findColor(valueName, strlen(valueName)))
            return Color(namedColor->ARGBValue);
    }
    return LayoutTheme::theme().systemColor(keyword);
}

bool StyleColor::isColorKeyword(CSSValueID id)
{
    // Named colors and color keywords:
    //
    // <named-color>
    //   'aqua', 'black', 'blue', ..., 'yellow' (CSS3: "basic color keywords")
    //   'aliceblue', ..., 'yellowgreen'        (CSS3: "extended color keywords")
    //   'transparent'
    //
    // 'currentcolor'
    //
    // <deprecated-system-color>
    //   'ActiveBorder', ..., 'WindowText'
    //
    // WebKit proprietary/internal:
    //   '-webkit-link'
    //   '-webkit-activelink'
    //   '-internal-active-list-box-selection'
    //   '-internal-active-list-box-selection-text'
    //   '-internal-inactive-list-box-selection'
    //   '-internal-inactive-list-box-selection-text'
    //   '-webkit-focus-ring-color'
    //   '-internal-quirk-inherit'
    //
    return (id >= CSSValueAqua && id <= CSSValueInternalQuirkInherit)
        || (id >= CSSValueAliceblue && id <= CSSValueYellowgreen)
        || id == CSSValueMenu;
}

bool StyleColor::isSystemColor(CSSValueID id)
{
    return (id >= CSSValueActiveborder && id <= CSSValueWindowtext) || id == CSSValueMenu;
}

} // namespace blink
