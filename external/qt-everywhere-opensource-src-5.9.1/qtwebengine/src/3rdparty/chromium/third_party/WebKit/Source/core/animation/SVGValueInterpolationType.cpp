// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGValueInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

class SVGValueNonInterpolableValue : public NonInterpolableValue {
 public:
  virtual ~SVGValueNonInterpolableValue() {}

  static PassRefPtr<SVGValueNonInterpolableValue> create(
      SVGPropertyBase* svgValue) {
    return adoptRef(new SVGValueNonInterpolableValue(svgValue));
  }

  SVGPropertyBase* svgValue() const { return m_svgValue; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGValueNonInterpolableValue(SVGPropertyBase* svgValue)
      : m_svgValue(svgValue) {}

  Persistent<SVGPropertyBase> m_svgValue;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGValueNonInterpolableValue);

InterpolationValue SVGValueInterpolationType::maybeConvertSVGValue(
    const SVGPropertyBase& value) const {
  SVGPropertyBase* referencedValue =
      const_cast<SVGPropertyBase*>(&value);  // Take ref.
  return InterpolationValue(
      InterpolableList::create(0),
      SVGValueNonInterpolableValue::create(referencedValue));
}

SVGPropertyBase* SVGValueInterpolationType::appliedSVGValue(
    const InterpolableValue&,
    const NonInterpolableValue* nonInterpolableValue) const {
  return toSVGValueNonInterpolableValue(*nonInterpolableValue).svgValue();
}

}  // namespace blink
