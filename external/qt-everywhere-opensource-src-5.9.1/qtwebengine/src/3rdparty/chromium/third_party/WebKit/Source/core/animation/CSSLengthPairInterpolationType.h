// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthPairInterpolationType_h
#define CSSLengthPairInterpolationType_h

#include "core/animation/CSSLengthListInterpolationType.h"
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSLengthPairInterpolationType : public CSSLengthListInterpolationType {
 public:
  CSSLengthPairInterpolationType(PropertyHandle property)
      : CSSLengthListInterpolationType(property) {}

 private:
  InterpolationValue maybeConvertValue(const CSSValue& value,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const final {
    const CSSValuePair& pair = toCSSValuePair(value);
    return ListInterpolationFunctions::createList(2, [&pair](size_t index) {
      const CSSValue& item = index == 0 ? pair.first() : pair.second();
      return LengthInterpolationFunctions::maybeConvertCSSValue(item);
    });
  }
};

}  // namespace blink

#endif  // CSSLengthPairInterpolationType_h
