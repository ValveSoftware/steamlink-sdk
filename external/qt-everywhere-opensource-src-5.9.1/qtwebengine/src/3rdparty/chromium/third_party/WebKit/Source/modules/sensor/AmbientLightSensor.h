// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AmbientLightSensor_h
#define AmbientLightSensor_h

#include "modules/sensor/Sensor.h"

namespace blink {

class AmbientLightSensorReading;

class AmbientLightSensor final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AmbientLightSensor* create(ScriptState*,
                                    const SensorOptions&,
                                    ExceptionState&);
  static AmbientLightSensor* create(ScriptState*, ExceptionState&);

  AmbientLightSensorReading* reading() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  AmbientLightSensor(ScriptState*, const SensorOptions&, ExceptionState&);
  // Sensor overrides.
  std::unique_ptr<SensorReadingFactory> createSensorReadingFactory() override;
};

}  // namespace blink

#endif  // AmbientLightSensor_h
