// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_AMBIENT_LIGHT_MAC_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_AMBIENT_LIGHT_MAC_H_

#include <IOKit/IOKitLib.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"

namespace content {

// Provides an interface to retrieve ambient light data from MacBooks.
class AmbientLightSensor {
 public:
  // Create AmbientLightSensor object, return NULL if no valid is sensor found.
  static std::unique_ptr<AmbientLightSensor> Create();

  ~AmbientLightSensor();

  // Retrieve lux values from Ambient Light Sensor.
  bool ReadSensorValue(uint64_t lux_value[2]);

 private:
  AmbientLightSensor();

  // Probe the local hardware looking for Ambient Light sensor and
  // initialize an I/O connection to it.
  bool Init();

  // IOKit connection to the local sensor.
  io_connect_t io_connection_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_AMBIENT_LIGHT_MAC_H_
