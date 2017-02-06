// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInterpolationType_h
#define SVGInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

class SVGPropertyBase;

class SVGInterpolationType : public InterpolationType {
protected:
    SVGInterpolationType(const QualifiedName& attribute)
        : InterpolationType(PropertyHandle(attribute))
    { }

    const QualifiedName& attribute() const { return getProperty().svgAttribute(); }

    virtual InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const = 0;
    virtual InterpolationValue maybeConvertSVGValue(const SVGPropertyBase&) const = 0;
    virtual SVGPropertyBase* appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const = 0;

    InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const override;
    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const override;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const override;
};

} // namespace blink

#endif // SVGInterpolationType_h
