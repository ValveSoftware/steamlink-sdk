// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_orientation_message_filter.h"

#include "content/browser/device_sensors/device_inertial_sensor_service.h"
#include "content/common/device_sensors/device_orientation_messages.h"

namespace content {

DeviceOrientationMessageFilter::DeviceOrientationMessageFilter()
    : BrowserMessageFilter(DeviceOrientationMsgStart),
      is_started_(false) {
}

DeviceOrientationMessageFilter::~DeviceOrientationMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (is_started_)
    DeviceInertialSensorService::GetInstance()->RemoveConsumer(
        CONSUMER_TYPE_ORIENTATION);
}

bool DeviceOrientationMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationMessageFilter, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StartPolling,
                        OnDeviceOrientationStartPolling)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StopPolling,
                        OnDeviceOrientationStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationMessageFilter::OnDeviceOrientationStartPolling() {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceInertialSensorService::GetInstance()->AddConsumer(
      CONSUMER_TYPE_ORIENTATION);
  DidStartDeviceOrientationPolling();
}

void DeviceOrientationMessageFilter::OnDeviceOrientationStopPolling() {
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceInertialSensorService::GetInstance()->RemoveConsumer(
      CONSUMER_TYPE_ORIENTATION);
}

void DeviceOrientationMessageFilter::DidStartDeviceOrientationPolling() {
  Send(new DeviceOrientationMsg_DidStartPolling(
      DeviceInertialSensorService::GetInstance()->
          GetSharedMemoryHandleForProcess(
              CONSUMER_TYPE_ORIENTATION,
              PeerHandle())));
}

}  // namespace content
