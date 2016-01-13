// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_sensor_event_pump.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"

namespace content {

// Default interval between successive polls, should take into account the
// value of |kInertialSensorIntervalMillis| in
// content/browser/device_sensors/inertial_sensor_consts.h.
const int DeviceSensorEventPump::kDefaultPumpDelayMillis = 50;

int DeviceSensorEventPump::GetDelayMillis() const {
  return pump_delay_millis_;
}

DeviceSensorEventPump::DeviceSensorEventPump()
    : pump_delay_millis_(kDefaultPumpDelayMillis),
      state_(STOPPED) {
}

DeviceSensorEventPump::DeviceSensorEventPump(int pump_delay_millis)
    : pump_delay_millis_(pump_delay_millis),
      state_(STOPPED) {
  DCHECK_GE(pump_delay_millis_, 0);
}

DeviceSensorEventPump::~DeviceSensorEventPump() {
}

bool DeviceSensorEventPump::RequestStart() {
  DVLOG(2) << "requested start";

  if (state_ != STOPPED)
    return false;

  DCHECK(!timer_.IsRunning());

  if (SendStartMessage()) {
    state_ = PENDING_START;
    return true;
  }
  return false;
}

bool DeviceSensorEventPump::Stop() {
  DVLOG(2) << "stop";

  if (state_ == STOPPED)
    return true;

  DCHECK((state_ == PENDING_START && !timer_.IsRunning()) ||
      (state_ == RUNNING && timer_.IsRunning()));

  if (timer_.IsRunning())
    timer_.Stop();
  SendStopMessage();
  state_ = STOPPED;
  return true;
}

void DeviceSensorEventPump::Attach(RenderThread* thread) {
  if (!thread)
    return;
  thread->AddObserver(this);
}

void DeviceSensorEventPump::OnDidStart(base::SharedMemoryHandle handle) {
  DVLOG(2) << "did start sensor event pump";

  if (state_ != PENDING_START)
    return;

  DCHECK(!timer_.IsRunning());

  if (InitializeReader(handle)) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(GetDelayMillis()),
                 this, &DeviceSensorEventPump::FireEvent);
    state_ = RUNNING;
  }
}

}  // namespace content
