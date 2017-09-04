// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/generic_sensor/generic_sensor_export.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"
#include "mojo/public/cpp/system/buffer.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensorConfiguration;

// Base class for the sensors provided by the platform. Concrete instances of
// this class are created by platform specific PlatformSensorProvider.
class DEVICE_GENERIC_SENSOR_EXPORT PlatformSensor
    : public base::RefCountedThreadSafe<PlatformSensor> {
 public:
  // The interface that must be implemented by PlatformSensor clients.
  class Client {
   public:
    virtual void OnSensorReadingChanged() = 0;
    virtual void OnSensorError() = 0;
    virtual bool IsNotificationSuspended() = 0;

   protected:
    virtual ~Client() {}
  };

  virtual mojom::ReportingMode GetReportingMode() = 0;
  virtual PlatformSensorConfiguration GetDefaultConfiguration() = 0;

  // Can be overriden to return the sensor maximum sampling frequency
  // value obtained from the platform if it is available. If platfrom
  // does not provide maximum sampling frequency this method must
  // return default frequency.
  // The default implementation returns default frequency.
  virtual double GetMaximumSupportedFrequency();

  mojom::SensorType GetType() const;

  bool StartListening(Client* client,
                      const PlatformSensorConfiguration& config);
  bool StopListening(Client* client, const PlatformSensorConfiguration& config);

  void UpdateSensor();

  void AddClient(Client*);
  void RemoveClient(Client*);

 protected:
  virtual ~PlatformSensor();
  PlatformSensor(mojom::SensorType type,
                 mojo::ScopedSharedBufferMapping mapping,
                 PlatformSensorProvider* provider);

  using ConfigMap = std::map<Client*, std::list<PlatformSensorConfiguration>>;
  using ReadingBuffer = SensorReadingSharedBuffer;

  virtual bool UpdateSensorInternal(const ConfigMap& configurations);
  virtual bool StartSensor(
      const PlatformSensorConfiguration& configuration) = 0;
  virtual void StopSensor() = 0;
  virtual bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) = 0;

  // Updates shared buffer with new sensor reading data.
  // Note: this method is thread-safe.
  void UpdateSensorReading(const SensorReading& reading, bool notify_clients);

  void NotifySensorReadingChanged();
  void NotifySensorError();

  // For testing purposes.
  const ConfigMap& config_map() const { return config_map_; }

  // Task runner that is used by mojo objects for the IPC.
  // If platfrom sensor events are processed on a different
  // thread, notifications are forwarded to |task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

 private:
  friend class base::RefCountedThreadSafe<PlatformSensor>;
  const mojo::ScopedSharedBufferMapping shared_buffer_mapping_;
  mojom::SensorType type_;
  base::ObserverList<Client, true> clients_;
  ConfigMap config_map_;
  PlatformSensorProvider* provider_;
  base::WeakPtrFactory<PlatformSensor> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PlatformSensor);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
