// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/MagnetometerReading.h"

#include "modules/sensor/MagnetometerReadingInit.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

device::SensorReading ToReadingData(const MagnetometerReadingInit& init) {
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

MagnetometerReading::MagnetometerReading(const MagnetometerReadingInit& init)
    : SensorReading(ToReadingData(init)) {}

MagnetometerReading::MagnetometerReading(const device::SensorReading& data)
    : SensorReading(data) {}

MagnetometerReading::~MagnetometerReading() = default;

double MagnetometerReading::x() const {
  return data().values[0];
}

double MagnetometerReading::y() const {
  return data().values[1];
}

double MagnetometerReading::z() const {
  return data().values[2];
}

bool MagnetometerReading::isReadingUpdated(
    const device::SensorReading& previous) const {
  return previous.values[0] != x() && previous.values[1] != y() &&
         previous.values[2] != z();
}

DEFINE_TRACE(MagnetometerReading) {
  SensorReading::trace(visitor);
}

}  // namespace blink
