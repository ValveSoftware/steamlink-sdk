// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is based on http://osxbook.com/book/bonus/chapter10/light/

#include "content/browser/device_sensors/ambient_light_mac.h"

#include <utility>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace content {

namespace {
enum LmuFunctionIndex {
  kGetSensorReadingID = 0,  // getSensorReading(int *, int *)
  kGetLEDBrightnessID = 1,  // getLEDBrightness(int, int *)
  kSetLEDBrightnessID = 2,  // setLEDBrightness(int, int, int *)
  kSetLEDFadeID = 3,        // setLEDFade(int, int, int, int *)
};
}  // namespace

// static
std::unique_ptr<AmbientLightSensor> AmbientLightSensor::Create() {
  std::unique_ptr<AmbientLightSensor> light_sensor(new AmbientLightSensor);
  return light_sensor->Init() ? std::move(light_sensor) : nullptr;
}

AmbientLightSensor::~AmbientLightSensor() {
  if (!io_connection_)
    IOServiceClose(io_connection_);
}

AmbientLightSensor::AmbientLightSensor() : io_connection_(IO_OBJECT_NULL) {
}

bool AmbientLightSensor::Init() {
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
  base::mac::ScopedIOObject<io_service_t> service_object(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("AppleLMUController")));

  // Return early if the ambient light sensor is not present.
  if (!service_object)
    return false;

  // Create a connection to the IOService object.
  kern_return_t kr =
      IOServiceOpen(service_object, mach_task_self(), 0, &io_connection_);

  // IOServiceOpen error.
  if (kr != KERN_SUCCESS || io_connection_ == IO_OBJECT_NULL)
    return false;

  uint64_t lux_values[2];
  return ReadSensorValue(lux_values);
}

bool AmbientLightSensor::ReadSensorValue(uint64_t lux_values[2]) {
  uint32_t scalar_output_count = 2;
  kern_return_t kr = IOConnectCallMethod(
      io_connection_, LmuFunctionIndex::kGetSensorReadingID, nullptr, 0,
      nullptr, 0, lux_values, &scalar_output_count, nullptr, 0);

  return kr == KERN_SUCCESS;
}

}  // namespace content
