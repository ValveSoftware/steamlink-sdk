// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_sensor_message_filter.h"

#include "content/browser/device_sensors/device_inertial_sensor_service.h"

namespace content {

DeviceSensorMessageFilter::DeviceSensorMessageFilter(
    ConsumerType consumer_type, uint32_t message_class_to_filter)
    : BrowserMessageFilter(message_class_to_filter),
      consumer_type_(consumer_type),
      is_started_(false) {
}

DeviceSensorMessageFilter::~DeviceSensorMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_started_)
    DeviceInertialSensorService::GetInstance()->RemoveConsumer(consumer_type_);
}

void DeviceSensorMessageFilter::OnStartPolling() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceInertialSensorService::GetInstance()->AddConsumer(consumer_type_);
  DidStartPolling(DeviceInertialSensorService::GetInstance()->
      GetSharedMemoryHandleForProcess(consumer_type_, PeerHandle()));
}

void DeviceSensorMessageFilter::OnStopPolling() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceInertialSensorService::GetInstance()->RemoveConsumer(consumer_type_);
}

}  // namespace content
