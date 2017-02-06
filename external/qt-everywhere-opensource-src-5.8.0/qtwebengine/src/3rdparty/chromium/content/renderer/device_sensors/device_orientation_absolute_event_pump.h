// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_EVENT_PUMP_H_

#include "base/macros.h"
#include "content/renderer/device_sensors/device_orientation_event_pump.h"

namespace content {

class CONTENT_EXPORT DeviceOrientationAbsoluteEventPump
    : public DeviceOrientationEventPump {
 public:
  explicit DeviceOrientationAbsoluteEventPump(RenderThread* thread);
  ~DeviceOrientationAbsoluteEventPump() override;

  // DeviceOrientationEventPump
  bool OnControlMessageReceived(const IPC::Message& message) override;

 protected:
  // DeviceOrientationEventPump
  void SendStartMessage() override;
  void SendStopMessage() override;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationAbsoluteEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_EVENT_PUMP_H_
