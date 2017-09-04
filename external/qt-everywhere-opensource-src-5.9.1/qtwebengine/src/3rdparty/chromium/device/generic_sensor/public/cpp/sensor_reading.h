// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_
#define DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_

#include "device/base/synchronization/one_writer_seqlock.h"
#include "device/generic_sensor/generic_sensor_export.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"

namespace device {

// This class is guarantied to have a fixed size of 64 bits on every platform.
// It is introduce to simplify sensors shared buffer memory calculation.
template <typename Data>
class SensorReadingField {
 public:
  static_assert(sizeof(Data) <= sizeof(int64_t),
                "The field size must be <= 64 bits.");
  SensorReadingField() = default;
  SensorReadingField(Data value) { storage_.value = value; }
  SensorReadingField& operator=(Data value) {
    storage_.value = value;
    return *this;
  }
  Data& value() { return storage_.value; }
  const Data& value() const { return storage_.value; }

  operator Data() const { return storage_.value; }

 private:
  union Storage {
    int64_t unused;
    Data value;
    Storage() { new (&value) Data(); }
    ~Storage() { value.~Data(); }
  };
  Storage storage_;
};

// This structure represents sensor reading data: timestamp and 3 values.
struct DEVICE_GENERIC_SENSOR_EXPORT SensorReading {
  SensorReading();
  ~SensorReading();
  SensorReading(const SensorReading& other);
  SensorReadingField<double> timestamp;
  SensorReadingField<double> values[3];
};

// This structure represents sensor reading buffer: sensor reading and seqlock
// for synchronization.
struct DEVICE_GENERIC_SENSOR_EXPORT SensorReadingSharedBuffer {
  SensorReadingSharedBuffer();
  ~SensorReadingSharedBuffer();
  SensorReadingField<OneWriterSeqLock> seqlock;
  SensorReading reading;

  // Gets the shared reading buffer offset for the given sensor type.
  static uint64_t GetOffset(mojom::SensorType type);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_
