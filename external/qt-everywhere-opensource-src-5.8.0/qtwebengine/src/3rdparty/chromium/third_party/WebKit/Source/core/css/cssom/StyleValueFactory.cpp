// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSValue.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformValue.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"

namespace blink {

CSSStyleValueVector StyleValueFactory::cssValueToStyleValueVector(CSSPropertyID propertyID, const CSSValue& value)
{
    CSSStyleValueVector styleValueVector;

    if (value.isPrimitiveValue()) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue.isLength() && !primitiveValue.isCalculated()) {
            styleValueVector.append(CSSSimpleLength::create(primitiveValue.getDoubleValue(), primitiveValue.typeWithCalcResolved()));
            return styleValueVector;
        }
    }

    CSSStyleValue* styleValue = nullptr;
    switch (propertyID) {
    case CSSPropertyTransform:
        styleValue  = CSSTransformValue::fromCSSValue(value);
        if (styleValue)
            styleValueVector.append(styleValue);
        return styleValueVector;
    default:
        // TODO(meade): Implement the rest.
        break;
    }

    styleValueVector.append(CSSUnsupportedStyleValue::create(value.cssText()));
    return styleValueVector;
}

} // namespace blink
