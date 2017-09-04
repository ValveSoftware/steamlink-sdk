// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/public/cpp/sensor_reading.h"

namespace device {

SensorReading::SensorReading() = default;
SensorReading::SensorReading(const SensorReading& other) = default;
SensorReading::~SensorReading() = default;

SensorReadingSharedBuffer::SensorReadingSharedBuffer() = default;
SensorReadingSharedBuffer::~SensorReadingSharedBuffer() = default;

// static
uint64_t SensorReadingSharedBuffer::GetOffset(mojom::SensorType type) {
  return (static_cast<uint64_t>(mojom::SensorType::LAST) -
          static_cast<uint64_t>(type)) *
         sizeof(SensorReadingSharedBuffer);
}

}  // namespace device
