// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "content/common/device_sensors/device_motion_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionListener.h"

namespace content {

DeviceMotionEventPump::DeviceMotionEventPump()
    : DeviceSensorEventPump(), listener_(0) {
}

DeviceMotionEventPump::DeviceMotionEventPump(int pump_delay_millis)
    : DeviceSensorEventPump(pump_delay_millis), listener_(0) {
}

DeviceMotionEventPump::~DeviceMotionEventPump() {
}

bool DeviceMotionEventPump::SetListener(
    blink::WebDeviceMotionListener* listener) {
  listener_ = listener;
  return listener_ ? RequestStart() : Stop();
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
  DCHECK(listener_);
  blink::WebDeviceMotionData data;
  if (reader_->GetLatestData(&data) && data.allAvailableSensorsAreActive)
    listener_->didChangeDeviceMotion(data);
}

bool DeviceMotionEventPump::InitializeReader(base::SharedMemoryHandle handle) {
  if (!reader_)
    reader_.reset(new DeviceMotionSharedMemoryReader());
  return reader_->Initialize(handle);
}

bool DeviceMotionEventPump::SendStartMessage() {
  return RenderThread::Get()->Send(new DeviceMotionHostMsg_StartPolling());
}


bool DeviceMotionEventPump::SendStopMessage() {
  return RenderThread::Get()->Send(new DeviceMotionHostMsg_StopPolling());
}

}  // namespace content
