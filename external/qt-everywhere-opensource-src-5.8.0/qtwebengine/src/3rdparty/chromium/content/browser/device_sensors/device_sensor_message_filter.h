// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSOR_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSOR_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "content/browser/device_sensors/device_sensors_consts.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// A base class for device sensor related message filters.
class DeviceSensorMessageFilter : public BrowserMessageFilter {
 public:
  DeviceSensorMessageFilter(ConsumerType consumer_type,
                            uint32_t message_class_to_filter);

 protected:
  // All methods below to be called on the IO thread.
  ~DeviceSensorMessageFilter() override;

  virtual void OnStartPolling();
  virtual void OnStopPolling();
  // To be overriden by the subclass in order to send the appropriate message
  // to the renderer with a handle to shared memory.
  virtual void DidStartPolling(base::SharedMemoryHandle handle) = 0;

 private:
  ConsumerType consumer_type_;
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_SENSOR_MESSAGE_FILTER_H_
