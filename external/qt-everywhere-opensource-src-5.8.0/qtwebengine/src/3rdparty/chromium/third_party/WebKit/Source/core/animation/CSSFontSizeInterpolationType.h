// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontSizeInterpolationType_h
#define CSSFontSizeInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSFontSizeInterpolationType : public CSSInterpolationType {
public:
    CSSFontSizeInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    {
        ASSERT(property == CSSPropertyFontSize);
    }

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
};

} // namespace blink

#endif // CSSFontSizeInterpolationType_h
