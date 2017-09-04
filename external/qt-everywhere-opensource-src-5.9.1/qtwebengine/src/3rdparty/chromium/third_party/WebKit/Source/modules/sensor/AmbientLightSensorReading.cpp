// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AmbientLightSensorReading.h"

#include "modules/sensor/SensorProxy.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

device::SensorReading ToReadingData(const AmbientLightSensorReadingInit& init) {
  device::SensorReading result;
  result.timestamp = WTF::monotonicallyIncreasingTime();
  if (init.hasIlluminance())
    result.values[0] = init.illuminance();

  return result;
}

}  // namespace

AmbientLightSensorReading::AmbientLightSensorReading(
    const AmbientLightSensorReadingInit& init)
    : SensorReading(ToReadingData(init)) {}

AmbientLightSensorReading::AmbientLightSensorReading(
    const device::SensorReading& data)
    : SensorReading(data) {}

AmbientLightSensorReading::~AmbientLightSensorReading() = default;

double AmbientLightSensorReading::illuminance() const {
  return data().values[0];
}

bool AmbientLightSensorReading::isReadingUpdated(
    const device::SensorReading& previous) const {
  return previous.values[0] != illuminance();
}

DEFINE_TRACE(AmbientLightSensorReading) {
  SensorReading::trace(visitor);
}

}  // namespace blink
