// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SAFE_INTEGER_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_SAFE_INTEGER_CONVERSIONS_H_

#include <cmath>
#include <limits>

#include "base/numerics/safe_conversions.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

inline int ToFlooredInt(float value) {
  return base::saturated_cast<int>(std::floor(value));
}

inline int ToCeiledInt(float value) {
  return base::saturated_cast<int>(std::ceil(value));
}

inline int ToFlooredInt(double value) {
  return base::saturated_cast<int>(std::floor(value));
}

inline int ToCeiledInt(double value) {
  return base::saturated_cast<int>(std::ceil(value));
}

inline int ToRoundedInt(float value) {
  float rounded;
  if (value >= 0.0f)
    rounded = std::floor(value + 0.5f);
  else
    rounded = std::ceil(value - 0.5f);
  return base::saturated_cast<int>(rounded);
}

inline int ToRoundedInt(double value) {
  double rounded;
  if (value >= 0.0)
    rounded = std::floor(value + 0.5);
  else
    rounded = std::ceil(value - 0.5);
  return base::saturated_cast<int>(rounded);
}

inline bool IsExpressibleAsInt(float value) {
  if (value != value)
    return false; // no int NaN.
  if (value > std::numeric_limits<int>::max())
    return false;
  if (value < std::numeric_limits<int>::min())
    return false;
  return true;
}

// Returns true iff a+b would overflow max int.
constexpr bool AddWouldOverflow(int a, int b) {
  // In this function, GCC tries to make optimizations that would only
  // work if max - a wouldn't overflow but it isn't smart enough to notice that
  // a > 0.  So cast everything to unsigned to avoid this.  As it is guaranteed
  // that max - a and b are both already positive, the cast is a noop.
  //
  // This is intended to be: a > 0 && max - a < b
  return a > 0 && b > 0 &&
         static_cast<unsigned>(std::numeric_limits<int>::max() - a) <
             static_cast<unsigned>(b);
}

// Returns true iff a+b would underflow min int.
constexpr bool AddWouldUnderflow(int a, int b) {
  return a < 0 && b < std::numeric_limits<int>::min() - a;
}

// Returns true iff a-b would overflow max int.
constexpr bool SubtractWouldOverflow(int a, int b) {
  return b < 0 && a > std::numeric_limits<int>::max() + b;
}

// Returns true iff a-b would underflow min int.
constexpr bool SubtractWouldUnderflow(int a, int b) {
  return b > 0 && a < std::numeric_limits<int>::min() + b;
}

// Safely adds a+b without integer overflow/underflow.  Exceeding these
// bounds will clamp to the max or min int limit.
constexpr int SafeAdd(int a, int b) {
  return AddWouldOverflow(a, b)
             ? std::numeric_limits<int>::max()
             : AddWouldUnderflow(a, b) ? std::numeric_limits<int>::min()
                                       : a + b;
}

// Safely subtracts a-b without integer overflow/underflow.  Exceeding these
// bounds will clamp to the max or min int limit.
constexpr int SafeSubtract(int a, int b) {
  return SubtractWouldOverflow(a, b)
             ? std::numeric_limits<int>::max()
             : SubtractWouldUnderflow(a, b) ? std::numeric_limits<int>::min()
                                            : a - b;
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SAFE_INTEGER_CONVERSIONS_H_
