// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GyroscopeReading_h
#define GyroscopeReading_h

#include "modules/sensor/SensorReading.h"

namespace blink {

class GyroscopeReadingInit;

class GyroscopeReading final : public SensorReading {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GyroscopeReading* create(const GyroscopeReadingInit& init) {
    return new GyroscopeReading(init);
  }

  static GyroscopeReading* create(const device::SensorReading& data) {
    return new GyroscopeReading(data);
  }

  ~GyroscopeReading() override;

  double x() const;
  double y() const;
  double z() const;

  bool isReadingUpdated(const device::SensorReading&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit GyroscopeReading(const GyroscopeReadingInit&);
  explicit GyroscopeReading(const device::SensorReading&);
};

}  // namepsace blink

#endif  // GyroscopeReading_h
