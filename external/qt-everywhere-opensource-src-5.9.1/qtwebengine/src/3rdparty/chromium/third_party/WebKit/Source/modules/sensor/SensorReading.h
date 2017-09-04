// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorReading_h
#define SensorReading_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTimeStamp.h"
#include "modules/sensor/SensorProxy.h"

namespace blink {

class ScriptState;

class SensorReading : public GarbageCollectedFinalized<SensorReading>,
                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  DOMHighResTimeStamp timeStamp(ScriptState*) const;

  // Returns 'true' if the current reading value is different than the given
  // previous one; otherwise returns 'false'.
  virtual bool isReadingUpdated(
      const device::SensorReading& previous) const = 0;

  const device::SensorReading& data() const { return m_data; }

  virtual ~SensorReading();

 protected:
  explicit SensorReading(const device::SensorReading&);

 private:
  device::SensorReading m_data;
};

class SensorReadingFactory {
 public:
  virtual SensorReading* createSensorReading(const device::SensorReading&) = 0;

 protected:
  SensorReadingFactory() = default;
};

template <typename SensorReadingType>
class SensorReadingFactoryImpl : public SensorReadingFactory {
 public:
  SensorReading* createSensorReading(
      const device::SensorReading& reading) override {
    return SensorReadingType::create(reading);
  }
};

}  // namepsace blink

#endif  // SensorReading_h
