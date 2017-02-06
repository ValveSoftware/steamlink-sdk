// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_light_event_pump.h"

#include "base/time/time.h"
#include "content/common/device_sensors/device_light_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebDeviceLightListener.h"

namespace {
// Default rate for firing of DeviceLight events.
const int kDefaultLightPumpFrequencyHz = 5;
const int kDefaultLightPumpDelayMicroseconds =
    base::Time::kMicrosecondsPerSecond / kDefaultLightPumpFrequencyHz;
}  // namespace

namespace content {

DeviceLightEventPump::DeviceLightEventPump(RenderThread* thread)
    : DeviceSensorEventPump<blink::WebDeviceLightListener>(thread),
      last_seen_data_(-1) {
  pump_delay_microseconds_ = kDefaultLightPumpDelayMicroseconds;
}

DeviceLightEventPump::~DeviceLightEventPump() {
}

bool DeviceLightEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceLightEventPump, message)
    IPC_MESSAGE_HANDLER(DeviceLightMsg_DidStartPolling, OnDidStart)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

void DeviceLightEventPump::SendStartMessage() {
  RenderThread::Get()->Send(new DeviceLightHostMsg_StartPolling());
}

void DeviceLightEventPump::SendStopMessage() {
  last_seen_data_ = -1;
  RenderThread::Get()->Send(new DeviceLightHostMsg_StopPolling());
}

void DeviceLightEventPump::SendFakeDataForTesting(void* fake_data) {
  double data = *static_cast<double*>(fake_data);

  listener()->didChangeDeviceLight(data);
}

}  // namespace content
