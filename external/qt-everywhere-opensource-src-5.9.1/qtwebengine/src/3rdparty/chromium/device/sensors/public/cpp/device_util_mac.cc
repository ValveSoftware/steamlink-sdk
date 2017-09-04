// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/public/cpp/device_util_mac.h"

#include <math.h>

namespace device {

double LMUvalueToLux(uint64_t raw_value) {
  // Conversion formula from regression.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=793728
  // Let x = raw_value, then
  // lux = -2.978303814*(10^-27)*x^4 + 2.635687683*(10^-19)*x^3 -
  //       3.459747434*(10^-12)*x^2 + 3.905829689*(10^-5)*x - 0.1932594532

  static const long double k4 = pow(10.L, -7);
  static const long double k3 = pow(10.L, -4);
  static const long double k2 = pow(10.L, -2);
  static const long double k1 = pow(10.L, 5);
  long double scaled_value = raw_value / k1;

  long double lux_value =
      (-3 * k4 * pow(scaled_value, 4)) + (2.6 * k3 * pow(scaled_value, 3)) +
      (-3.4 * k2 * pow(scaled_value, 2)) + (3.9 * scaled_value) - 0.19;

  double lux = ceil(static_cast<double>(lux_value));
  return lux > 0 ? lux : 0;
}

}  // namespace device
