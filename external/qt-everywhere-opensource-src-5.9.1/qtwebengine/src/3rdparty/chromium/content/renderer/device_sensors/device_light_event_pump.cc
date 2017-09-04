// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_light_event_pump.h"

#include "base/time/time.h"
#include "content/public/renderer/render_thread.h"
#include "device/sensors/public/cpp/device_sensors_consts.h"
#include "third_party/WebKit/public/platform/WebDeviceLightListener.h"

namespace {
// Default rate for firing of DeviceLight events.
const int kDefaultLightPumpDelayMicroseconds =
    base::Time::kMicrosecondsPerSecond /
    device::kDefaultAmbientLightFrequencyHz;
}  // namespace

namespace content {

DeviceLightEventPump::DeviceLightEventPump(RenderThread* thread)
    : DeviceSensorMojoClientMixin<
          DeviceSensorEventPump<blink::WebDeviceLightListener>,
          device::mojom::LightSensor>(thread),
      last_seen_data_(-1) {
  pump_delay_microseconds_ = kDefaultLightPumpDelayMicroseconds;
}

DeviceLightEventPump::~DeviceLightEventPump() {
}

void DeviceLightEventPump::FireEvent() {
  DCHECK(listener());
  DeviceLightData data;
  if (reader_->GetLatestData(&data) && ShouldFireEvent(data.value)) {
    last_seen_data_ = data.value;
    listener()->didChangeDeviceLight(data.value);
  }
}

bool DeviceLightEventPump::ShouldFireEvent(double lux) const {
  if (lux < 0)
    return false;

  if (lux == std::numeric_limits<double>::infinity()) {
    // no sensor data can be provided, fire an Infinity event to Blink.
    return true;
  }

  return lux != last_seen_data_;
}

bool DeviceLightEventPump::InitializeReader(base::SharedMemoryHandle handle) {
  if (!reader_)
    reader_.reset(new DeviceLightSharedMemoryReader());
  return reader_->Initialize(handle);
}

void DeviceLightEventPump::SendFakeDataForTesting(void* fake_data) {
  double data = *static_cast<double*>(fake_data);

  listener()->didChangeDeviceLight(data);
}

}  // namespace content
