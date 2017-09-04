// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_PUBLIC_CPP_DEVICE_UTIL_MAC_H_
#define DEVICE_SENSORS_PUBLIC_CPP_DEVICE_UTIL_MAC_H_

#include <stdint.h>

namespace device {

// Convert the value returned by the ambient light LMU sensor on Mac
// hardware to a lux value.
double LMUvalueToLux(uint64_t raw_value);

}  // namespace device

#endif  // DEVICE_SENSORS_PUBLIC_CPP_DEVICE_UTIL_MAC_H_
