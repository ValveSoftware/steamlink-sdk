// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_orientation_absolute_event_pump.h"

#include "content/common/device_sensors/device_orientation_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace content {

DeviceOrientationAbsoluteEventPump::DeviceOrientationAbsoluteEventPump(
    RenderThread* thread) : DeviceOrientationEventPump(thread) {
}

DeviceOrientationAbsoluteEventPump::~DeviceOrientationAbsoluteEventPump() {
}

bool DeviceOrientationAbsoluteEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationAbsoluteEventPump, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationAbsoluteMsg_DidStartPolling,
        OnDidStart)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationAbsoluteEventPump::SendStartMessage() {
  RenderThread::Get()->Send(
      new DeviceOrientationAbsoluteHostMsg_StartPolling());
}

void DeviceOrientationAbsoluteEventPump::SendStopMessage() {
  RenderThread::Get()->Send(new DeviceOrientationAbsoluteHostMsg_StopPolling());
}

}  // namespace content
