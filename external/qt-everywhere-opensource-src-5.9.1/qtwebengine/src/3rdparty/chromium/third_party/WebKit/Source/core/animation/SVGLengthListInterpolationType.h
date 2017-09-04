// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGLengthListInterpolationType_h
#define SVGLengthListInterpolationType_h

#include "core/SVGNames.h"
#include "core/animation/SVGInterpolationType.h"
#include "core/svg/SVGLength.h"

namespace blink {

enum class SVGLengthMode;

class SVGLengthListInterpolationType : public SVGInterpolationType {
 public:
  SVGLengthListInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        m_unitMode(SVGLength::lengthModeForAnimatedLengthAttribute(attribute)),
        m_negativeValuesForbidden(
            SVGLength::negativeValuesForbiddenForAnimatedLengthAttribute(
                attribute)) {}

 private:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertSVGValue(
      const SVGPropertyBase& svgValue) const final;
  PairwiseInterpolationValue maybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void composite(UnderlyingValueOwner&,
                 double underlyingFraction,
                 const InterpolationValue&,
                 double interpolationFraction) const final;
  SVGPropertyBase* appliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
  void apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const final;

  const SVGLengthMode m_unitMode;
  const bool m_negativeValuesForbidden;
};

}  // namespace blink

#endif  // SVGLengthListInterpolationType_h
