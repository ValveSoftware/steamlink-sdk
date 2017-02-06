// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageSlicePropertyFunctions_h
#define ImageSlicePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

// This struct doesn't retain ownership of the slices, treat it like a reference.
struct ImageSlice {
    ImageSlice(const LengthBox& slices, bool fill)
        : slices(slices)
        , fill(fill)
    { }

    const LengthBox& slices;
    bool fill;
};

class ImageSlicePropertyFunctions {
public:
    static ImageSlice getInitialImageSlice(CSSPropertyID property) { return getImageSlice(property, ComputedStyle::initialStyle()); }

    static ImageSlice getImageSlice(CSSPropertyID property, const ComputedStyle& style)
    {
        switch (property) {
        default:
            NOTREACHED();
            // Fall through.
        case CSSPropertyBorderImageSlice:
            return ImageSlice(style.borderImageSlices(), style.borderImageSlicesFill());
        case CSSPropertyWebkitMaskBoxImageSlice:
            return ImageSlice(style.maskBoxImageSlices(), style.maskBoxImageSlicesFill());
        }
    }

    static void setImageSlice(CSSPropertyID property, ComputedStyle& style, const ImageSlice& slice)
    {
        switch (property) {
        case CSSPropertyBorderImageSlice:
            style.setBorderImageSlices(slice.slices);
            style.setBorderImageSlicesFill(slice.fill);
            break;
        case CSSPropertyWebkitMaskBoxImageSlice:
            style.setMaskBoxImageSlices(slice.slices);
            style.setMaskBoxImageSlicesFill(slice.fill);
            break;
        default:
            NOTREACHED();
        }
    }
};

} // namespace blink

#endif // ImageSlicePropertyFunctions_h
