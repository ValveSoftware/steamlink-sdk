// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGLengthInterpolationType_h
#define SVGLengthInterpolationType_h

#include "core/animation/SVGInterpolationType.h"
#include "core/svg/SVGLength.h"
#include <memory>

namespace blink {

class SVGLengthContext;
enum class SVGLengthMode;

class SVGLengthInterpolationType : public SVGInterpolationType {
public:
    SVGLengthInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
        , m_unitMode(SVGLength::lengthModeForAnimatedLengthAttribute(attribute))
        , m_negativeValuesForbidden(SVGLength::negativeValuesForbiddenForAnimatedLengthAttribute(attribute))
    { }

    static std::unique_ptr<InterpolableValue> neutralInterpolableValue();
    static InterpolationValue convertSVGLength(const SVGLength&);
    static SVGLength* resolveInterpolableSVGLength(const InterpolableValue&, const SVGLengthContext&, SVGLengthMode, bool negativeValuesForbidden);

private:
    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertSVGValue(const SVGPropertyBase& svgValue) const final;
    SVGPropertyBase* appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    const SVGLengthMode m_unitMode;
    const bool m_negativeValuesForbidden;
};

} // namespace blink

#endif // SVGLengthInterpolationType_h
