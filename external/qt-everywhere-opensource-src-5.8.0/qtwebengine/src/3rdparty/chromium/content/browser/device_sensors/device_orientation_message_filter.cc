// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_orientation_message_filter.h"

#include "content/common/device_sensors/device_orientation_messages.h"

namespace content {

DeviceOrientationMessageFilter::DeviceOrientationMessageFilter()
    : DeviceSensorMessageFilter(CONSUMER_TYPE_ORIENTATION,
                                DeviceOrientationMsgStart) {}

DeviceOrientationMessageFilter::~DeviceOrientationMessageFilter() {}

bool DeviceOrientationMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationMessageFilter, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StartPolling, OnStartPolling)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StopPolling, OnStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationMessageFilter::DidStartPolling(
    base::SharedMemoryHandle handle) {
  Send(new DeviceOrientationMsg_DidStartPolling(handle));
}

}  // namespace content
