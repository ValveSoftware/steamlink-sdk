// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PUBLIC_PLATFORM_SENSOR_PROVIDER_LINUX_H_
#define DEVICE_GENERIC_SENSOR_PUBLIC_PLATFORM_SENSOR_PROVIDER_LINUX_H_

#include "device/generic_sensor/platform_sensor_provider.h"

namespace base {
class Thread;
}

namespace device {

struct SensorDataLinux;
class SensorReader;

class PlatformSensorProviderLinux : public PlatformSensorProvider {
 public:
  PlatformSensorProviderLinux();
  ~PlatformSensorProviderLinux() override;

  static PlatformSensorProviderLinux* GetInstance();

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

  void AllSensorsRemoved() override;

  void SetFileTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) override;

 private:
  void SensorReaderFound(
      mojom::SensorType type,
      mojo::ScopedSharedBufferMapping mapping,
      const PlatformSensorProviderBase::CreateSensorCallback& callback,
      const SensorDataLinux& data,
      std::unique_ptr<SensorReader> sensor_reader);

  // Stops a polling thread if there are no sensors left. Must be called on
  // a different that polling thread that allows io.
  void StopPollingThread();

  // TODO(maksims): make a separate class Manager that will
  // create provide sensors with a polling task runner, check sensors existence
  // and notify provider if a new sensor has appeared and it can be created if a
  // request comes again for the same sensor.
  // A use case example: a request for a sensor X comes, manager checks if the
  // sensor exists on a platform and notifies a provider it is not found.
  // The provider stores this information into its cache and doesn't try to
  // create this specific sensor if a request comes. But when, for example,
  // the sensor X is plugged into a usb port, the manager notices that and
  // notifies the provider, which updates its cache and starts handling requests
  // for the sensor X.
  //
  // Right now, this thread is used to find sensors files and poll data.
  std::unique_ptr<base::Thread> polling_thread_;

  // A task runner that is passed to polling sensors to poll data.
  scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner_;

  // Browser's file thread task runner passed from renderer. Used to
  // stop a polling thread.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderLinux);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PUBLIC_PLATFORM_SENSOR_PROVIDER_LINUX_H_
