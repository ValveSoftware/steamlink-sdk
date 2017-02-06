// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePropertyFunctions_h
#define ImagePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class ImagePropertyFunctions {
public:
    static const StyleImage* getInitialStyleImage(CSSPropertyID) { return nullptr; }

    static const StyleImage* getStyleImage(CSSPropertyID property, const ComputedStyle& style)
    {
        switch (property) {
        case CSSPropertyBorderImageSource:
            return style.borderImageSource();
        case CSSPropertyListStyleImage:
            return style.listStyleImage();
        case CSSPropertyWebkitMaskBoxImageSource:
            return style.maskBoxImageSource();
        default:
            NOTREACHED();
            return nullptr;
        }
    }

    static void setStyleImage(CSSPropertyID property, ComputedStyle& style, StyleImage* image)
    {
        switch (property) {
        case CSSPropertyBorderImageSource:
            style.setBorderImageSource(image);
            break;
        case CSSPropertyListStyleImage:
            style.setListStyleImage(image);
            break;
        case CSSPropertyWebkitMaskBoxImageSource:
            style.setMaskBoxImageSource(image);
            break;
        default:
            NOTREACHED();
        }
    }
};

} // namespace blink

#endif // ImagePropertyFunctions_h
