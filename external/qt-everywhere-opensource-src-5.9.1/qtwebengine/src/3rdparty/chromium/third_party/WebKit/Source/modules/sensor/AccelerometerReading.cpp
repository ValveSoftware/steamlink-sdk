// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AccelerometerReading.h"

#include "modules/sensor/SensorProxy.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {
device::SensorReading ToReadingData(const AccelerometerReadingInit& init) {
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

AccelerometerReading::AccelerometerReading(const AccelerometerReadingInit& init)
    : SensorReading(ToReadingData(init)) {}

AccelerometerReading::AccelerometerReading(const device::SensorReading& data)
    : SensorReading(data) {}

AccelerometerReading::~AccelerometerReading() = default;

double AccelerometerReading::x() const {
  return data().values[0];
}

double AccelerometerReading::y() const {
  return data().values[1];
}

double AccelerometerReading::z() const {
  return data().values[2];
}

bool AccelerometerReading::isReadingUpdated(
    const device::SensorReading& previous) const {
  return (previous.values[0] != x() || previous.values[1] != y() ||
          previous.values[2] != z());
}

DEFINE_TRACE(AccelerometerReading) {
  SensorReading::trace(visitor);
}

}  // namespace blink
