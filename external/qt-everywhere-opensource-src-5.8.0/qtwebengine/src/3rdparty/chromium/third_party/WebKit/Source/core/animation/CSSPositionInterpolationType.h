// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionInterpolationType_h
#define CSSPositionInterpolationType_h

#include "core/animation/CSSLengthListInterpolationType.h"

#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSPositionInterpolationType : public CSSLengthListInterpolationType {
public:
    CSSPositionInterpolationType(CSSPropertyID property)
        : CSSLengthListInterpolationType(property)
    { }

private:
    InterpolationValue maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const final
    {
        const CSSValuePair& pair = toCSSValuePair(value);
        return ListInterpolationFunctions::createList(2, [&pair](size_t index) {
            return CSSPositionAxisListInterpolationType::convertPositionAxisCSSValue(index == 0 ? pair.first() : pair.second());
        });
    }
};

} // namespace blink

#endif // CSSPositionInterpolationType_h
