// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGPropertyHelper_h
#define SVGPropertyHelper_h

#include "core/svg/properties/SVGProperty.h"

namespace blink {

template <typename Derived>
class SVGPropertyHelper : public SVGPropertyBase {
 public:
  virtual SVGPropertyBase* cloneForAnimation(const String& value) const {
    Derived* property = Derived::create();
    property->setValueAsString(value);
    return property;
  }

  AnimatedPropertyType type() const override { return Derived::classType(); }
};

}  // namespace blink

#endif  // SVGPropertyHelper_h
