// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_inertial_sensor_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/device_sensors/data_fetcher_shared_memory.h"

namespace content {

DeviceInertialSensorService::DeviceInertialSensorService()
    : num_light_readers_(0),
      num_motion_readers_(0),
      num_orientation_readers_(0),
      num_orientation_absolute_readers_(0),
      is_shutdown_(false) {
}

DeviceInertialSensorService::~DeviceInertialSensorService() {
}

DeviceInertialSensorService* DeviceInertialSensorService::GetInstance() {
  return base::Singleton<
      DeviceInertialSensorService,
      base::LeakySingletonTraits<DeviceInertialSensorService>>::get();
}

void DeviceInertialSensorService::AddConsumer(ConsumerType consumer_type) {
  if (!ChangeNumberConsumers(consumer_type, 1))
    return;

  DCHECK(GetNumberConsumers(consumer_type));

  if (!data_fetcher_)
    data_fetcher_.reset(new DataFetcherSharedMemory);
  data_fetcher_->StartFetchingDeviceData(consumer_type);
}

void DeviceInertialSensorService::RemoveConsumer(ConsumerType consumer_type) {
  if (!ChangeNumberConsumers(consumer_type, -1))
    return;

  if (GetNumberConsumers(consumer_type) == 0)
    data_fetcher_->StopFetchingDeviceData(consumer_type);
}

bool DeviceInertialSensorService::ChangeNumberConsumers(
    ConsumerType consumer_type, int delta) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return false;

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      num_motion_readers_ += delta;
      DCHECK_GE(num_motion_readers_, 0);
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      num_orientation_readers_ += delta;
      DCHECK_GE(num_orientation_readers_ , 0);
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      num_orientation_absolute_readers_ += delta;
      DCHECK_GE(num_orientation_absolute_readers_ , 0);
      return true;
    case CONSUMER_TYPE_LIGHT:
      num_light_readers_ += delta;
      DCHECK_GE(num_light_readers_, 0);
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

int DeviceInertialSensorService::GetNumberConsumers(
    ConsumerType consumer_type) const {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return num_motion_readers_;
    case CONSUMER_TYPE_ORIENTATION:
      return num_orientation_readers_;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      return num_orientation_absolute_readers_;
    case CONSUMER_TYPE_LIGHT:
      return num_light_readers_;
    default:
      NOTREACHED();
  }
  return 0;
}

base::SharedMemoryHandle
DeviceInertialSensorService::GetSharedMemoryHandleForProcess(
    ConsumerType consumer_type, base::ProcessHandle handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_fetcher_->GetSharedMemoryHandleForProcess(consumer_type, handle);
}

void DeviceInertialSensorService::Shutdown() {
  if (data_fetcher_) {
    data_fetcher_->Shutdown();
    data_fetcher_.reset();
  }
  is_shutdown_ = true;
}

void DeviceInertialSensorService::SetDataFetcherForTesting(
    DataFetcherSharedMemory* test_data_fetcher) {
  if (data_fetcher_)
    data_fetcher_->Shutdown();
  data_fetcher_.reset(test_data_fetcher);
}

}  // namespace content
