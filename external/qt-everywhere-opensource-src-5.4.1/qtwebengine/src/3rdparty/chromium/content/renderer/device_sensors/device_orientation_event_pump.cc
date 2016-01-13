// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include <cmath>

#include "content/common/device_sensors/device_orientation_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationListener.h"

namespace content {

const double DeviceOrientationEventPump::kOrientationThreshold = 0.1;

DeviceOrientationEventPump::DeviceOrientationEventPump()
    : DeviceSensorEventPump(), listener_(0) {
}

DeviceOrientationEventPump::DeviceOrientationEventPump(int pump_delay_millis)
    : DeviceSensorEventPump(pump_delay_millis), listener_(0) {
}

DeviceOrientationEventPump::~DeviceOrientationEventPump() {
}

bool DeviceOrientationEventPump::SetListener(
    blink::WebDeviceOrientationListener* listener) {
  listener_ = listener;
  return listener_ ? RequestStart() : Stop();
}

bool DeviceOrientationEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationEventPump, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationMsg_DidStartPolling, OnDidStart)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationEventPump::FireEvent() {
  DCHECK(listener_);
  blink::WebDeviceOrientationData data;
  if (reader_->GetLatestData(&data) && ShouldFireEvent(data)) {
    memcpy(&data_, &data, sizeof(data));
    listener_->didChangeDeviceOrientation(data);
  }
}

static bool IsSignificantlyDifferent(bool hasAngle1, double angle1,
    bool hasAngle2, double angle2) {
  if (hasAngle1 != hasAngle2)
    return true;
  return (hasAngle1 && std::fabs(angle1 - angle2) >=
          DeviceOrientationEventPump::kOrientationThreshold);
}

bool DeviceOrientationEventPump::ShouldFireEvent(
    const blink::WebDeviceOrientationData& data) const {
  if (!data.allAvailableSensorsAreActive)
    return false;

  if (!data.hasAlpha && !data.hasBeta && !data.hasGamma) {
    // no data can be provided, this is an all-null event.
    return true;
  }

  return IsSignificantlyDifferent(
             data_.hasAlpha, data_.alpha, data.hasAlpha, data.alpha) ||
         IsSignificantlyDifferent(
             data_.hasBeta, data_.beta, data.hasBeta, data.beta) ||
         IsSignificantlyDifferent(
             data_.hasGamma, data_.gamma, data.hasGamma, data.gamma);
}

bool DeviceOrientationEventPump::InitializeReader(
    base::SharedMemoryHandle handle) {
  memset(&data_, 0, sizeof(data_));
  if (!reader_)
    reader_.reset(new DeviceOrientationSharedMemoryReader());
  return reader_->Initialize(handle);
}

bool DeviceOrientationEventPump::SendStartMessage() {
  return RenderThread::Get()->Send(new DeviceOrientationHostMsg_StartPolling());
}

bool DeviceOrientationEventPump::SendStopMessage() {
  return RenderThread::Get()->Send(new DeviceOrientationHostMsg_StopPolling());
}

}  // namespace content
