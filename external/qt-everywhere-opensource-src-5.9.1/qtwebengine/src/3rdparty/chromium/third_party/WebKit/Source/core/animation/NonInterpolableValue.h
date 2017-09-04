// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NonInterpolableValue_h
#define NonInterpolableValue_h

#include "wtf/RefCounted.h"

namespace blink {

// Represents components of a PropertySpecificKeyframe's value that either do
// not change or 50% flip when interpolating with an adjacent value.
class NonInterpolableValue : public RefCounted<NonInterpolableValue> {
 public:
  virtual ~NonInterpolableValue() {}

  typedef const void* Type;
  virtual Type getType() const = 0;
};

// These macros provide safe downcasts of NonInterpolableValue subclasses with
// debug assertions.
// See CSSValueInterpolationType.cpp for example usage.
#define DECLARE_NON_INTERPOLABLE_VALUE_TYPE() \
  static Type staticType;                     \
  Type getType() const final { return staticType; }

#define DEFINE_NON_INTERPOLABLE_VALUE_TYPE(T) \
  NonInterpolableValue::Type T::staticType = &T::staticType;

#define DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(T)               \
  inline bool is##T(const NonInterpolableValue* value) {          \
    return !value || value->getType() == T::staticType;           \
  }                                                               \
  DEFINE_TYPE_CASTS(T, NonInterpolableValue, value, is##T(value), \
                    is##T(&value));

}  // namespace blink

#endif  // NonInterpolableValue_h
