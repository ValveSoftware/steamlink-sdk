// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_motion_message_filter.h"

#include "content/common/device_sensors/device_motion_messages.h"

namespace content {

DeviceMotionMessageFilter::DeviceMotionMessageFilter()
    : DeviceSensorMessageFilter(CONSUMER_TYPE_MOTION, DeviceMotionMsgStart) {}

DeviceMotionMessageFilter::~DeviceMotionMessageFilter() {}

bool DeviceMotionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceMotionMessageFilter, message)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StartPolling, OnStartPolling)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StopPolling, OnStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceMotionMessageFilter::DidStartPolling(
    base::SharedMemoryHandle handle) {
  Send(new DeviceMotionMsg_DidStartPolling(handle));
}

}  // namespace content
