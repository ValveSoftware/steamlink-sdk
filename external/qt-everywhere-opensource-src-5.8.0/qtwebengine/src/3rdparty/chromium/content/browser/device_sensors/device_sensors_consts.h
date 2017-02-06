// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSORS_CONSTS_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSORS_CONSTS_H_

#include "base/time/time.h"

namespace content {

// Constants related to the Device {Motion|Orientation|Light} APIs.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser
enum ConsumerType {
  CONSUMER_TYPE_MOTION = 1 << 0,
  CONSUMER_TYPE_ORIENTATION = 1 << 1,
  CONSUMER_TYPE_ORIENTATION_ABSOLUTE = 1 << 2,
  CONSUMER_TYPE_LIGHT = 1 << 3,
};

// Specifies the sampling rate for sensor data updates.
// Note that when changing this value it is desirable to have an adequate
// matching value |DeviceSensorEventPump::kDefaultPumpFrequencyHz| in
// content/renderer/device_orientation/device_sensor_event_pump.cc.
const int kInertialSensorSamplingRateHz = 60;
const int kInertialSensorIntervalMicroseconds =
    base::Time::kMicrosecondsPerSecond / kInertialSensorSamplingRateHz;

// Corresponding |kDefaultLightPumpFrequencyHz| is in
// content/renderer/device_sensors/device_light_event_pump.cc.
const int kLightSensorSamplingRateHz = 5;
const int kLightSensorIntervalMicroseconds =
    base::Time::kMicrosecondsPerSecond / kLightSensorSamplingRateHz;

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSORS_CONSTS_H_
