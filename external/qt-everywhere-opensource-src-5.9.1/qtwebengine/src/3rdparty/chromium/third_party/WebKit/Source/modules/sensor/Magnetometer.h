// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Magnetometer_h
#define Magnetometer_h

#include "modules/sensor/Sensor.h"

namespace blink {

class MagnetometerReading;

class Magnetometer final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Magnetometer* create(ScriptState*,
                              const SensorOptions&,
                              ExceptionState&);
  static Magnetometer* create(ScriptState*, ExceptionState&);

  MagnetometerReading* reading() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Magnetometer(ScriptState*, const SensorOptions&, ExceptionState&);
  // Sensor overrides.
  std::unique_ptr<SensorReadingFactory> createSensorReadingFactory() override;
};

}  // namespace blink

#endif  // Magnetometer_h
