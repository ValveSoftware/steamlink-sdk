// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/GyroscopeReading.h"

#include "modules/sensor/GyroscopeReadingInit.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {
device::SensorReading ToReadingData(const GyroscopeReadingInit& init) {
  device::SensorReading result;
  result.timestamp = WTF::monotonicallyIncreasingTime();
  if (init.hasX())
    result.values[0] = init.x();
  if (init.hasY())
    result.values[1] = init.y();
  if (init.hasZ())
    result.values[2] = init.z();
  return result;
}
}  // namespace

GyroscopeReading::GyroscopeReading(const GyroscopeReadingInit& init)
    : SensorReading(ToReadingData(init)) {}

GyroscopeReading::GyroscopeReading(const device::SensorReading& data)
    : SensorReading(data) {}

GyroscopeReading::~GyroscopeReading() = default;

double GyroscopeReading::x() const {
  return data().values[0];
}

double GyroscopeReading::y() const {
  return data().values[1];
}

double GyroscopeReading::z() const {
  return data().values[2];
}

bool GyroscopeReading::isReadingUpdated(
    const device::SensorReading& previous) const {
  return previous.values[0] != x() || previous.values[1] != y() ||
         previous.values[2] != z();
}

DEFINE_TRACE(GyroscopeReading) {
  SensorReading::trace(visitor);
}

}  // namespace blink
