// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BorderImageLengthBoxPropertyFunctions_h
#define BorderImageLengthBoxPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class BorderImageLengthBoxPropertyFunctions {
public:
    static const BorderImageLengthBox& getInitialBorderImageLengthBox(CSSPropertyID property) { return getBorderImageLengthBox(property, ComputedStyle::initialStyle()); }

    static const BorderImageLengthBox& getBorderImageLengthBox(CSSPropertyID property, const ComputedStyle& style)
    {
        switch (property) {
        case CSSPropertyBorderImageOutset:
            return style.borderImageOutset();
        case CSSPropertyBorderImageWidth:
            return style.borderImageWidth();
        case CSSPropertyWebkitMaskBoxImageOutset:
            return style.maskBoxImageOutset();
        case CSSPropertyWebkitMaskBoxImageWidth:
            return style.maskBoxImageWidth();
        default:
            NOTREACHED();
            return getInitialBorderImageLengthBox(CSSPropertyBorderImageOutset);
        }
    }

    static void setBorderImageLengthBox(CSSPropertyID property, ComputedStyle& style, const BorderImageLengthBox& box)
    {
        switch (property) {
        case CSSPropertyBorderImageOutset:
            style.setBorderImageOutset(box);
            break;
        case CSSPropertyWebkitMaskBoxImageOutset:
            style.setMaskBoxImageOutset(box);
            break;
        case CSSPropertyBorderImageWidth:
            style.setBorderImageWidth(box);
            break;
        case CSSPropertyWebkitMaskBoxImageWidth:
            style.setMaskBoxImageWidth(box);
            break;
        default:
            NOTREACHED();
            break;
        }
    }
};

} // namespace blink

#endif // BorderImageLengthBoxPropertyFunctions_h
