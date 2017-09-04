// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MagnetometerReading_h
#define MagnetometerReading_h

#include "modules/sensor/SensorReading.h"

namespace blink {

class MagnetometerReadingInit;

class MagnetometerReading final : public SensorReading {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MagnetometerReading* create(const MagnetometerReadingInit& init) {
    return new MagnetometerReading(init);
  }

  static MagnetometerReading* create(const device::SensorReading& data) {
    return new MagnetometerReading(data);
  }

  ~MagnetometerReading() override;

  double x() const;
  double y() const;
  double z() const;

  bool isReadingUpdated(const device::SensorReading&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MagnetometerReading(const MagnetometerReadingInit&);
  explicit MagnetometerReading(const device::SensorReading&);
};

}  // namepsace blink

#endif  // MagnetometerReading_h
