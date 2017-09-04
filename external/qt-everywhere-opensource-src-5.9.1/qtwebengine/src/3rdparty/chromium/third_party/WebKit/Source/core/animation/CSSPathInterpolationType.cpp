// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPathInterpolationType.h"

#include "core/animation/PathInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

void CSSPathInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  DCHECK_EQ(cssProperty(), CSSPropertyD);
  std::unique_ptr<SVGPathByteStream> pathByteStream =
      PathInterpolationFunctions::appliedValue(interpolableValue,
                                               nonInterpolableValue);
  if (pathByteStream->isEmpty()) {
    environment.state().style()->setD(nullptr);
    return;
  }
  environment.state().style()->setD(
      StylePath::create(std::move(pathByteStream)));
}

void CSSPathInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  PathInterpolationFunctions::composite(underlyingValueOwner,
                                        underlyingFraction, *this, value);
}

InterpolationValue CSSPathInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  return PathInterpolationFunctions::maybeConvertNeutral(underlying,
                                                         conversionCheckers);
}

InterpolationValue CSSPathInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return PathInterpolationFunctions::convertValue(nullptr);
}

class InheritedPathChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedPathChecker> create(
      PassRefPtr<StylePath> stylePath) {
    return wrapUnique(new InheritedPathChecker(std::move(stylePath)));
  }

 private:
  InheritedPathChecker(PassRefPtr<StylePath> stylePath)
      : m_stylePath(stylePath) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    return environment.state().parentStyle()->svgStyle().d() ==
           m_stylePath.get();
  }

  const RefPtr<StylePath> m_stylePath;
};

InterpolationValue CSSPathInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  DCHECK_EQ(cssProperty(), CSSPropertyD);
  if (!state.parentStyle())
    return nullptr;

  conversionCheckers.append(
      InheritedPathChecker::create(state.parentStyle()->svgStyle().d()));
  return PathInterpolationFunctions::convertValue(
      state.parentStyle()->svgStyle().d());
}

InterpolationValue CSSPathInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!value.isPathValue()) {
    DCHECK_EQ(toCSSIdentifierValue(value).getValueID(), CSSValueNone);
    return nullptr;
  }
  return PathInterpolationFunctions::convertValue(
      toCSSPathValue(value).byteStream());
}

InterpolationValue CSSPathInterpolationType::maybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  DCHECK_EQ(cssProperty(), CSSPropertyD);
  return PathInterpolationFunctions::convertValue(
      environment.state().style()->svgStyle().d());
}

PairwiseInterpolationValue CSSPathInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PathInterpolationFunctions::maybeMergeSingles(std::move(start),
                                                       std::move(end));
}

}  // namespace blink
