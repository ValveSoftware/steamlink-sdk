// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "content/common/device_sensors/device_motion_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"

namespace content {

DeviceMotionEventPump::DeviceMotionEventPump(RenderThread* thread)
    : DeviceSensorEventPump<blink::WebDeviceMotionListener>(thread) {
}

DeviceMotionEventPump::~DeviceMotionEventPump() {
}

bool DeviceMotionEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceMotionEventPump, message)
    IPC_MESSAGE_HANDLER(DeviceMotionMsg_DidStartPolling, OnDidStart)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceMotionEventPump::FireEvent() {
  DCHECK(listener());
  blink::WebDeviceMotionData data;
  if (reader_->GetLatestData(&data) && data.allAvailableSensorsAreActive)
    listener()->didChangeDeviceMotion(data);
}

bool DeviceMotionEventPump::InitializeReader(base::SharedMemoryHandle handle) {
  if (!reader_)
    reader_.reset(new DeviceMotionSharedMemoryReader());
  return reader_->Initialize(handle);
}

void DeviceMotionEventPump::SendStartMessage() {
  RenderThread::Get()->Send(new DeviceMotionHostMsg_StartPolling());
}

void DeviceMotionEventPump::SendStopMessage() {
  RenderThread::Get()->Send(new DeviceMotionHostMsg_StopPolling());
}

void DeviceMotionEventPump::SendFakeDataForTesting(void* fake_data) {
  blink::WebDeviceMotionData data =
      *static_cast<blink::WebDeviceMotionData*>(fake_data);

  listener()->didChangeDeviceMotion(data);
}

}  // namespace content
