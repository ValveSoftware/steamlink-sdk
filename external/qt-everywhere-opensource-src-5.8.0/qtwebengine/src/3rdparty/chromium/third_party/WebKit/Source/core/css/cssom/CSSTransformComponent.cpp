// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTransformComponent.h"

#include "core/css/cssom/CSSMatrixTransformComponent.h"
#include "core/css/cssom/CSSPerspective.h"
#include "core/css/cssom/CSSRotation.h"
#include "core/css/cssom/CSSScale.h"
#include "core/css/cssom/CSSSkew.h"
#include "core/css/cssom/CSSTranslation.h"

namespace blink {

CSSTransformComponent* CSSTransformComponent::fromCSSValue(const CSSValue& value)
{
    const CSSFunctionValue& functionValue = toCSSFunctionValue(value);
    switch (functionValue.functionType()) {
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        return CSSMatrixTransformComponent::fromCSSValue(functionValue);
    case CSSValuePerspective:
        return CSSPerspective::fromCSSValue(functionValue);
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueRotate3d:
        return CSSRotation::fromCSSValue(functionValue);
    case CSSValueScale:
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale3d:
        return CSSScale::fromCSSValue(functionValue);
    case CSSValueSkew:
    case CSSValueSkewX:
    case CSSValueSkewY:
        return CSSSkew::fromCSSValue(functionValue);
    case CSSValueTranslate:
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslateZ:
    case CSSValueTranslate3d:
        return CSSTranslation::fromCSSValue(functionValue);
    default:
        return nullptr;
    }
}

} // namespace blink
