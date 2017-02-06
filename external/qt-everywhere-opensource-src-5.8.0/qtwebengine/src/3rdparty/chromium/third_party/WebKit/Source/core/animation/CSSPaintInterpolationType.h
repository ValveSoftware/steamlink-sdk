// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintInterpolationType_h
#define CSSPaintInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSPaintInterpolationType : public CSSInterpolationType {
public:
    CSSPaintInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
};

} // namespace blink

#endif // CSSPaintInterpolationType_h
