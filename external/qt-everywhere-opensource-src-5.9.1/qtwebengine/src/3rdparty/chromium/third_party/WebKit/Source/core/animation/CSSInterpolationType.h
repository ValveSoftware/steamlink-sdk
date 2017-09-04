// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationType_h
#define CSSInterpolationType_h

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"

namespace blink {

class CSSInterpolationType : public InterpolationType {
 protected:
  CSSInterpolationType(PropertyHandle);

  CSSPropertyID cssProperty() const { return getProperty().cssProperty(); }

  InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&,
                                        const InterpolationEnvironment&,
                                        const InterpolationValue& underlying,
                                        ConversionCheckers&) const override;
  virtual InterpolationValue maybeConvertNeutral(
      const InterpolationValue& underlying,
      ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertValue(const CSSValue&,
                                               const StyleResolverState&,
                                               ConversionCheckers&) const = 0;
};

}  // namespace blink

#endif  // CSSInterpolationType_h
