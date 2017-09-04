// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Gyroscope.h"

#include "modules/sensor/GyroscopeReading.h"

using device::mojom::blink::SensorType;

namespace blink {

Gyroscope* Gyroscope::create(ScriptState* scriptState,
                             const SensorOptions& options,
                             ExceptionState& exceptionState) {
  return new Gyroscope(scriptState, options, exceptionState);
}

// static
Gyroscope* Gyroscope::create(ScriptState* scriptState,
                             ExceptionState& exceptionState) {
  return create(scriptState, SensorOptions(), exceptionState);
}

Gyroscope::Gyroscope(ScriptState* scriptState,
                     const SensorOptions& options,
                     ExceptionState& exceptionState)
    : Sensor(scriptState, options, exceptionState, SensorType::GYROSCOPE) {}

GyroscopeReading* Gyroscope::reading() const {
  return static_cast<GyroscopeReading*>(Sensor::reading());
}

std::unique_ptr<SensorReadingFactory> Gyroscope::createSensorReadingFactory() {
  return makeUnique<SensorReadingFactoryImpl<GyroscopeReading>>();
}

DEFINE_TRACE(Gyroscope) {
  Sensor::trace(visitor);
}

}  // namespace blink
