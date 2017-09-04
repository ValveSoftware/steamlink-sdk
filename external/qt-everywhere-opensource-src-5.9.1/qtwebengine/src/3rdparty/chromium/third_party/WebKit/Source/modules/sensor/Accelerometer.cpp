// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Accelerometer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/sensor/AccelerometerReading.h"

using device::mojom::blink::SensorType;

namespace blink {

Accelerometer* Accelerometer::create(ScriptState* scriptState,
                                     const AccelerometerOptions& options,
                                     ExceptionState& exceptionState) {
  return new Accelerometer(scriptState, options, exceptionState);
}

// static
Accelerometer* Accelerometer::create(ScriptState* scriptState,
                                     ExceptionState& exceptionState) {
  return create(scriptState, AccelerometerOptions(), exceptionState);
}

Accelerometer::Accelerometer(ScriptState* scriptState,
                             const AccelerometerOptions& options,
                             ExceptionState& exceptionState)
    : Sensor(scriptState,
             options,
             exceptionState,
             options.includeGravity() ? SensorType::ACCELEROMETER
                                      : SensorType::LINEAR_ACCELERATION),
      m_accelerometerOptions(options) {}

AccelerometerReading* Accelerometer::reading() const {
  return static_cast<AccelerometerReading*>(Sensor::reading());
}

bool Accelerometer::includesGravity() const {
  return m_accelerometerOptions.includeGravity();
}

std::unique_ptr<SensorReadingFactory>
Accelerometer::createSensorReadingFactory() {
  return std::unique_ptr<SensorReadingFactory>(
      new SensorReadingFactoryImpl<AccelerometerReading>());
}

DEFINE_TRACE(Accelerometer) {
  Sensor::trace(visitor);
}

}  // namespace blink
