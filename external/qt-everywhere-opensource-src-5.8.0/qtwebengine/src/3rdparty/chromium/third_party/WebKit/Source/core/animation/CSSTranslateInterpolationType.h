// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslateInterpolationType_h
#define CSSTranslateInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSTranslateInterpolationType : public CSSInterpolationType {
public:
    CSSTranslateInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    {
        ASSERT(property == CSSPropertyTranslate);
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

#endif // CSSTranslateInterpolationType_h
