// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Accelerometer_h
#define Accelerometer_h

#include "modules/sensor/AccelerometerOptions.h"
#include "modules/sensor/Sensor.h"

namespace blink {

class AccelerometerReading;

class Accelerometer final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Accelerometer* create(ScriptState*,
                               const AccelerometerOptions&,
                               ExceptionState&);
  static Accelerometer* create(ScriptState*, ExceptionState&);

  AccelerometerReading* reading() const;
  bool includesGravity() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Accelerometer(ScriptState*, const AccelerometerOptions&, ExceptionState&);
  // Sensor overrides.
  std::unique_ptr<SensorReadingFactory> createSensorReadingFactory() override;
  AccelerometerOptions m_accelerometerOptions;
};

}  // namespace blink

#endif  // Accelerometer_h
