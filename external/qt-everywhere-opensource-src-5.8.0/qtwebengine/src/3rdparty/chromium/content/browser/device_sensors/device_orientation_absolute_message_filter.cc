// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_orientation_absolute_message_filter.h"

#include "content/common/device_sensors/device_orientation_messages.h"

namespace content {

DeviceOrientationAbsoluteMessageFilter::DeviceOrientationAbsoluteMessageFilter()
    : DeviceSensorMessageFilter(CONSUMER_TYPE_ORIENTATION_ABSOLUTE,
                                DeviceOrientationMsgStart) {}

DeviceOrientationAbsoluteMessageFilter::
    ~DeviceOrientationAbsoluteMessageFilter() {}

bool DeviceOrientationAbsoluteMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationAbsoluteMessageFilter, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationAbsoluteHostMsg_StartPolling,
                        OnStartPolling)
    IPC_MESSAGE_HANDLER(DeviceOrientationAbsoluteHostMsg_StopPolling,
                        OnStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationAbsoluteMessageFilter::DidStartPolling(
    base::SharedMemoryHandle handle) {
  Send(new DeviceOrientationAbsoluteMsg_DidStartPolling(handle));
}

}  // namespace content
