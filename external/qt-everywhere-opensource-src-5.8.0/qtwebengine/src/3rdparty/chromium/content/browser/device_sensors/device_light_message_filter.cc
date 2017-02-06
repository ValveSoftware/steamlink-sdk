// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_light_message_filter.h"

#include "content/common/device_sensors/device_light_messages.h"

namespace content {

DeviceLightMessageFilter::DeviceLightMessageFilter()
    : DeviceSensorMessageFilter(CONSUMER_TYPE_LIGHT, DeviceLightMsgStart) {}

DeviceLightMessageFilter::~DeviceLightMessageFilter() {}

bool DeviceLightMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceLightMessageFilter, message)
  IPC_MESSAGE_HANDLER(DeviceLightHostMsg_StartPolling, OnStartPolling)
  IPC_MESSAGE_HANDLER(DeviceLightHostMsg_StopPolling, OnStopPolling)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceLightMessageFilter::DidStartPolling(
    base::SharedMemoryHandle handle) {
  Send(new DeviceLightMsg_DidStartPolling(handle));
}

}  // namespace content
