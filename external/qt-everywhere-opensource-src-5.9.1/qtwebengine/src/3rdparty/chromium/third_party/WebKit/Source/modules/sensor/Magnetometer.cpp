// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Magnetometer.h"

#include "modules/sensor/MagnetometerReading.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
Magnetometer* Magnetometer::create(ScriptState* scriptState,
                                   const SensorOptions& options,
                                   ExceptionState& exceptionState) {
  return new Magnetometer(scriptState, options, exceptionState);
}

// static
Magnetometer* Magnetometer::create(ScriptState* scriptState,
                                   ExceptionState& exceptionState) {
  return create(scriptState, SensorOptions(), exceptionState);
}

Magnetometer::Magnetometer(ScriptState* scriptState,
                           const SensorOptions& options,
                           ExceptionState& exceptionState)
    : Sensor(scriptState, options, exceptionState, SensorType::MAGNETOMETER) {}

MagnetometerReading* Magnetometer::reading() const {
  return static_cast<MagnetometerReading*>(Sensor::reading());
}

std::unique_ptr<SensorReadingFactory>
Magnetometer::createSensorReadingFactory() {
  return makeUnique<SensorReadingFactoryImpl<MagnetometerReading>>();
}

DEFINE_TRACE(Magnetometer) {
  Sensor::trace(visitor);
}

}  // namespace blink
