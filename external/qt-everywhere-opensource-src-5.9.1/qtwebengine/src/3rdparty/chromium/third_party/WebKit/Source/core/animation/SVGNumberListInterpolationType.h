// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGNumberListInterpolationType_h
#define SVGNumberListInterpolationType_h

#include "core/SVGNames.h"
#include "core/animation/SVGInterpolationType.h"

namespace blink {

// TODO(alancutter): The rotate attribute is marked as non-additive in the SVG
// specs:
// http://www.w3.org/TR/SVG/text.html#TSpanElementRotateAttribute
// http://www.w3.org/TR/SVG/text.html#TextElementRotateAttribute

class SVGNumberListInterpolationType : public SVGInterpolationType {
 public:
  SVGNumberListInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute) {}

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
};

}  // namespace blink

#endif  // SVGNumberListInterpolationType_h
