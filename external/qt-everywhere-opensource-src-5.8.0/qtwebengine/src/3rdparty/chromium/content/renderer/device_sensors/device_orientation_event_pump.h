// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_

#include <memory>

#include "base/macros.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationData.h"

namespace blink {
class WebDeviceOrientationListener;
}

namespace content {

typedef SharedMemorySeqLockReader<blink::WebDeviceOrientationData>
    DeviceOrientationSharedMemoryReader;

class CONTENT_EXPORT DeviceOrientationEventPump
    : public DeviceSensorEventPump<blink::WebDeviceOrientationListener> {
 public:
  // Angle threshold beyond which two orientation events are considered
  // sufficiently different.
  static const double kOrientationThreshold;

  explicit DeviceOrientationEventPump(RenderThread* thread);
  ~DeviceOrientationEventPump() override;

  // PlatformEventObserver.
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void SendFakeDataForTesting(void* data) override;

 protected:
  void FireEvent() override;
  bool InitializeReader(base::SharedMemoryHandle handle) override;

  // PlatformEventObserver.
  void SendStartMessage() override;
  void SendStopMessage() override;

  bool ShouldFireEvent(const blink::WebDeviceOrientationData& data) const;

  blink::WebDeviceOrientationData data_;
  std::unique_ptr<DeviceOrientationSharedMemoryReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
