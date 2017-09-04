// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include <string.h>

#include <cmath>

#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace content {

const double DeviceOrientationEventPumpBase::kOrientationThreshold = 0.1;

DeviceOrientationEventPumpBase::DeviceOrientationEventPumpBase(
    RenderThread* thread)
    : DeviceSensorEventPump<blink::WebDeviceOrientationListener>(thread) {}

DeviceOrientationEventPumpBase::~DeviceOrientationEventPumpBase() {}

void DeviceOrientationEventPumpBase::FireEvent() {
  DCHECK(listener());
  blink::WebDeviceOrientationData data;
  if (reader_->GetLatestData(&data) && ShouldFireEvent(data)) {
    memcpy(&data_, &data, sizeof(data));
    listener()->didChangeDeviceOrientation(data);
  }
}

static bool IsSignificantlyDifferent(bool hasAngle1, double angle1,
    bool hasAngle2, double angle2) {
  if (hasAngle1 != hasAngle2)
    return true;
  return (hasAngle1 &&
          std::fabs(angle1 - angle2) >=
              DeviceOrientationEventPumpBase::kOrientationThreshold);
}

bool DeviceOrientationEventPumpBase::ShouldFireEvent(
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

bool DeviceOrientationEventPumpBase::InitializeReader(
    base::SharedMemoryHandle handle) {
  memset(&data_, 0, sizeof(data_));
  if (!reader_)
    reader_.reset(new DeviceOrientationSharedMemoryReader());
  return reader_->Initialize(handle);
}

void DeviceOrientationEventPumpBase::SendFakeDataForTesting(void* fake_data) {
  blink::WebDeviceOrientationData data =
      *static_cast<blink::WebDeviceOrientationData*>(fake_data);

  listener()->didChangeDeviceOrientation(data);
}

}  // namespace content
