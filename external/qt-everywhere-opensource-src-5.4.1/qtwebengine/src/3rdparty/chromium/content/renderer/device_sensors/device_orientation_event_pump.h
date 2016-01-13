// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationData.h"

namespace blink {
class WebDeviceOrientationListener;
}

namespace content {

typedef SharedMemorySeqLockReader<blink::WebDeviceOrientationData>
    DeviceOrientationSharedMemoryReader;

class CONTENT_EXPORT DeviceOrientationEventPump : public DeviceSensorEventPump {
 public:
  // Angle threshold beyond which two orientation events are considered
  // sufficiently different.
  static const double kOrientationThreshold;

  DeviceOrientationEventPump();
  explicit DeviceOrientationEventPump(int pump_delay_millis);
  virtual ~DeviceOrientationEventPump();

  // Sets the listener to receive updates for device orientation data at
  // regular intervals. Returns true if the registration was successful.
  bool SetListener(blink::WebDeviceOrientationListener* listener);

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual void FireEvent() OVERRIDE;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) OVERRIDE;
  virtual bool SendStartMessage() OVERRIDE;
  virtual bool SendStopMessage() OVERRIDE;

  bool ShouldFireEvent(const blink::WebDeviceOrientationData& data) const;

  blink::WebDeviceOrientationListener* listener_;
  blink::WebDeviceOrientationData data_;
  scoped_ptr<DeviceOrientationSharedMemoryReader> reader_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
