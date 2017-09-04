// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGRectInterpolationType_h
#define SVGRectInterpolationType_h

#include "core/animation/SVGInterpolationType.h"

namespace blink {

class SVGRectInterpolationType : public SVGInterpolationType {
 public:
  SVGRectInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute) {}

 private:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertSVGValue(
      const SVGPropertyBase& svgValue) const final;
  SVGPropertyBase* appliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
};

}  // namespace blink

#endif  // SVGRectInterpolationType_h
