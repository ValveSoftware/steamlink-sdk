// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformOriginInterpolationType_h
#define CSSTransformOriginInterpolationType_h

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/CSSLengthListInterpolationType.h"
#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValueList.h"

namespace blink {

class CSSTransformOriginInterpolationType : public CSSLengthListInterpolationType {
public:
    CSSTransformOriginInterpolationType(CSSPropertyID property)
        : CSSLengthListInterpolationType(property)
    { }

private:
    InterpolationValue maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const final
    {
        const CSSValueList& list = toCSSValueList(value);
        ASSERT(list.length() == 3);
        return ListInterpolationFunctions::createList(list.length(), [&list](size_t index) {
            const CSSValue& item = list.item(index);
            if (index < 2)
                return CSSPositionAxisListInterpolationType::convertPositionAxisCSSValue(item);
            return CSSLengthInterpolationType::maybeConvertCSSValue(item);
        });
    }
};

} // namespace blink

#endif // CSSTransformOriginInterpolationType_h
