// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Gyroscope_h
#define Gyroscope_h

#include "modules/sensor/Sensor.h"

namespace blink {

class GyroscopeReading;

class Gyroscope final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Gyroscope* create(ScriptState*, const SensorOptions&, ExceptionState&);
  static Gyroscope* create(ScriptState*, ExceptionState&);

  GyroscopeReading* reading() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Gyroscope(ScriptState*, const SensorOptions&, ExceptionState&);
  // Sensor overrides.
  std::unique_ptr<SensorReadingFactory> createSensorReadingFactory() override;
};

}  // namespace blink

#endif  // Gyroscope_h
