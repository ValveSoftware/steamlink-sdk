// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_LIGHT_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_LIGHT_EVENT_PUMP_H_

#include <memory>

#include "base/macros.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "device/sensors/public/cpp/device_light_data.h"
#include "device/sensors/public/interfaces/light.mojom.h"

namespace blink {
class WebDeviceLightListener;
}

namespace content {

typedef SharedMemorySeqLockReader<DeviceLightData>
    DeviceLightSharedMemoryReader;

class CONTENT_EXPORT DeviceLightEventPump
    : public DeviceSensorMojoClientMixin<
          DeviceSensorEventPump<blink::WebDeviceLightListener>,
          device::mojom::LightSensor> {
 public:
  explicit DeviceLightEventPump(RenderThread* thread);
  ~DeviceLightEventPump() override;

  // Sets the listener to receive updates for device light data at
  // regular intervals. Returns true if the registration was successful.
  bool SetListener(blink::WebDeviceLightListener* listener);

  // PlatformEventObserver implementation.
  void SendFakeDataForTesting(void* data) override;

 protected:
  // Methods overriden from base class DeviceSensorEventPump
  void FireEvent() override;
  bool InitializeReader(base::SharedMemoryHandle handle) override;

 private:
  bool ShouldFireEvent(double data) const;

  std::unique_ptr<DeviceLightSharedMemoryReader> reader_;
  double last_seen_data_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLightEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_LIGHT_EVENT_PUMP_H_
