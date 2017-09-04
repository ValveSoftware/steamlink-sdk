// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AccelerometerReading_h
#define AccelerometerReading_h

#include "modules/sensor/AccelerometerReadingInit.h"
#include "modules/sensor/SensorReading.h"

namespace blink {

class AccelerometerReading final : public SensorReading {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AccelerometerReading* create(const AccelerometerReadingInit& init) {
    return new AccelerometerReading(init);
  }

  static AccelerometerReading* create(const device::SensorReading& data) {
    return new AccelerometerReading(data);
  }

  ~AccelerometerReading() override;

  double x() const;
  double y() const;
  double z() const;

  bool isReadingUpdated(const device::SensorReading&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AccelerometerReading(const AccelerometerReadingInit&);
  explicit AccelerometerReading(const device::SensorReading&);
};

}  // namepsace blink

#endif  // AccelerometerReading_h
