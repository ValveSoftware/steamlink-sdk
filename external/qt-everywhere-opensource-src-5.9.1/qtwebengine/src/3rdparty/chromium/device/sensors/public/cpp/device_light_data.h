// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_PUBLIC_CPP_DEVICE_LIGHT_DATA_H_
#define DEVICE_SENSORS_PUBLIC_CPP_DEVICE_LIGHT_DATA_H_

namespace content {

// This struct is intentionally POD and fixed size so that it can be stored
// in shared memory between the sensor interface impl and its clients.
// POD class should be a struct, should have an inline cstor that uses
// initializer lists and no dstor.
struct DeviceLightData {
  DeviceLightData() : value(-1) {}
  double value;
};

}  // namespace content

#endif  // DEVICE_SENSORS_PUBLIC_CPP_DEVICE_LIGHT_DATA_H_
