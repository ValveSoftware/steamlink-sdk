// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_ambient_light_mac.h"

#include <IOKit/IOMessage.h>

#include "base/bind.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "device/generic_sensor/platform_sensor_provider_mac.h"
#include "device/sensors/public/cpp/device_sensors_consts.h"
#include "device/sensors/public/cpp/device_util_mac.h"

namespace device {

enum LmuFunctionIndex {
  kGetSensorReadingID = 0,  // getSensorReading(int *, int *)
};

PlatformSensorAmbientLightMac::PlatformSensorAmbientLightMac(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider)
    : PlatformSensor(type, std::move(mapping), provider),
      light_sensor_port_(nullptr),
      current_lux_(0.0) {
  DCHECK_EQ(type, mojom::SensorType::AMBIENT_LIGHT);
}

PlatformSensorAmbientLightMac::~PlatformSensorAmbientLightMac() = default;

mojom::ReportingMode PlatformSensorAmbientLightMac::GetReportingMode() {
  return mojom::ReportingMode::ON_CHANGE;
}

bool PlatformSensorAmbientLightMac::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() > 0 &&
         configuration.frequency() <=
             mojom::SensorConfiguration::kMaxAllowedFrequency;
}

PlatformSensorConfiguration
PlatformSensorAmbientLightMac::GetDefaultConfiguration() {
  PlatformSensorConfiguration default_configuration;
  default_configuration.set_frequency(kDefaultAmbientLightFrequencyHz);
  return default_configuration;
}

void PlatformSensorAmbientLightMac::IOServiceCallback(void* context,
                                                      io_service_t service,
                                                      natural_t message_type,
                                                      void* message_argument) {
  PlatformSensorAmbientLightMac* sensor =
      static_cast<PlatformSensorAmbientLightMac*>(context);
  if (!sensor->ReadAndUpdate()) {
    sensor->NotifySensorError();
    sensor->StopSensor();
  }
}

bool PlatformSensorAmbientLightMac::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  // Tested and verified by riju that the following call works on
  // MacBookPro9,1 : Macbook Pro 15" (Mid 2012 model)
  // MacBookPro10,1 : Macbook Pro 15" (Retina Display, Early 2013 model).
  // MacBookPro10,2 : Macbook Pro 13" (Retina Display, Early 2013 model).
  // MacBookAir5,2 : Macbook Air 13" (Mid 2012 model) (by François Beaufort).
  // MacBookAir6,2 : Macbook Air 13" (Mid 2013 model).
  // Testing plans : please download the code and follow the comments :-
  // https://gist.github.com/riju/74af8c81a665e412d122/
  // and add an entry here about the model and the status returned by the code.

  // Look up a registered IOService object whose class is AppleLMUController.
  light_sensor_service_.reset(IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("AppleLMUController")));

  // Return early if the ambient light sensor is not present.
  if (!light_sensor_service_)
    return false;

  light_sensor_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  if (!light_sensor_port_.is_valid())
    return false;

  IONotificationPortSetDispatchQueue(
      light_sensor_port_.get(),
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));

  kern_return_t kr = IOServiceAddInterestNotification(
      light_sensor_port_.get(), light_sensor_service_, kIOGeneralInterest,
      IOServiceCallback, this, light_sensor_notification_.InitializeInto());
  if (kr != KERN_SUCCESS)
    return false;

  kr = IOServiceAddInterestNotification(
      light_sensor_port_.get(), light_sensor_service_, kIOBusyInterest,
      IOServiceCallback, this,
      light_sensor_busy_notification_.InitializeInto());
  if (kr != KERN_SUCCESS)
    return false;

  kr = IOServiceOpen(light_sensor_service_, mach_task_self(), 0,
                     light_sensor_object_.InitializeInto());
  if (kr != KERN_SUCCESS)
    return false;

  bool success = ReadAndUpdate();
  if (!success)
    StopSensor();

  return success;
}

void PlatformSensorAmbientLightMac::StopSensor() {
  light_sensor_port_.reset();
  light_sensor_notification_.reset();
  light_sensor_busy_notification_.reset();
  light_sensor_object_.reset();
  light_sensor_service_.reset();
  current_lux_ = 0.0;
}

bool PlatformSensorAmbientLightMac::ReadAndUpdate() {
  uint32_t scalar_output_count = 2;
  uint64_t lux_values[2];
  kern_return_t kr = IOConnectCallMethod(
      light_sensor_object_, LmuFunctionIndex::kGetSensorReadingID, nullptr, 0,
      nullptr, 0, lux_values, &scalar_output_count, nullptr, 0);

  if (kr != KERN_SUCCESS)
    return false;

  uint64_t mean = (lux_values[0] + lux_values[1]) / 2;
  double lux = LMUvalueToLux(mean);
  if (lux == current_lux_)
    return true;
  current_lux_ = lux;

  SensorReading reading;
  reading.timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.values[0] = current_lux_;
  UpdateSensorReading(reading, true);
  return true;
}

}  // namespace device
