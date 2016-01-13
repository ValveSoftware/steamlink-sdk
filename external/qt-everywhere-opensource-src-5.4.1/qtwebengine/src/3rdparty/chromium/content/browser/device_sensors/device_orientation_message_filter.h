// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

class DeviceOrientationMessageFilter : public BrowserMessageFilter {
 public:
  DeviceOrientationMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~DeviceOrientationMessageFilter();

  void OnDeviceOrientationStartPolling();
  void OnDeviceOrientationStopPolling();
  void DidStartDeviceOrientationPolling();

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
