// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGNumberInterpolationType_h
#define SVGNumberInterpolationType_h

#include "core/animation/NumberAttributeFunctions.h"
#include "core/animation/SVGInterpolationType.h"

namespace blink {

class SVGNumberInterpolationType : public SVGInterpolationType {
 public:
  SVGNumberInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        m_isNonNegative(NumberAttributeFunctions::isNonNegative(attribute)) {}

 private:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertSVGValue(
      const SVGPropertyBase& svgValue) const final;
  SVGPropertyBase* appliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;

  bool m_isNonNegative;
};

}  // namespace blink

#endif  // SVGNumberInterpolationType_h
