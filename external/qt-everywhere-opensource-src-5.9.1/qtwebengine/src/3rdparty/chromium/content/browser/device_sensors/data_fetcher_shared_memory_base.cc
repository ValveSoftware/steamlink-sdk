// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory_base.h"

#include <stddef.h>
#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "device/sensors/public/cpp/device_light_hardware_buffer.h"
#include "device/sensors/public/cpp/device_motion_hardware_buffer.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"

namespace content {

namespace {

size_t GetConsumerSharedMemoryBufferSize(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return sizeof(DeviceMotionHardwareBuffer);
    case CONSUMER_TYPE_ORIENTATION:
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      return sizeof(DeviceOrientationHardwareBuffer);
    case CONSUMER_TYPE_LIGHT:
      return sizeof(DeviceLightHardwareBuffer);
    default:
      NOTREACHED();
  }
  return 0;
}

}  // namespace

class DataFetcherSharedMemoryBase::PollingThread : public base::Thread {
 public:
  PollingThread(const char* name, DataFetcherSharedMemoryBase* fetcher);
  ~PollingThread() override;

  void AddConsumer(ConsumerType consumer_type, void* buffer);
  void RemoveConsumer(ConsumerType consumer_type);

  unsigned GetConsumersBitmask() const { return consumers_bitmask_; }
  bool IsTimerRunning() const { return timer_ ? timer_->IsRunning() : false; }

 private:
  void DoPoll();

  unsigned consumers_bitmask_;
  DataFetcherSharedMemoryBase* fetcher_;
  std::unique_ptr<base::RepeatingTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(PollingThread);
};

// --- PollingThread methods

DataFetcherSharedMemoryBase::PollingThread::PollingThread(
    const char* name, DataFetcherSharedMemoryBase* fetcher)
    : base::Thread(name),
      consumers_bitmask_(0),
      fetcher_(fetcher) {
}

DataFetcherSharedMemoryBase::PollingThread::~PollingThread() {
}

void DataFetcherSharedMemoryBase::PollingThread::AddConsumer(
    ConsumerType consumer_type, void* buffer) {
  DCHECK(fetcher_);
  if (!fetcher_->Start(consumer_type, buffer))
    return;

  consumers_bitmask_ |= consumer_type;

  if (!timer_ && fetcher_->GetType() == FETCHER_TYPE_POLLING_CALLBACK) {
    timer_.reset(new base::RepeatingTimer());
    timer_->Start(FROM_HERE,
                  fetcher_->GetInterval(),
                  this, &PollingThread::DoPoll);
  }
}

void DataFetcherSharedMemoryBase::PollingThread::RemoveConsumer(
    ConsumerType consumer_type) {
  DCHECK(fetcher_);
  if (!fetcher_->Stop(consumer_type))
    return;

  consumers_bitmask_ &= ~consumer_type;

  if (!consumers_bitmask_)
    timer_.reset();  // will also stop the timer.
}

void DataFetcherSharedMemoryBase::PollingThread::DoPoll() {
  DCHECK(fetcher_);
  DCHECK(consumers_bitmask_);
  fetcher_->Fetch(consumers_bitmask_);
}

// --- end of PollingThread methods

DataFetcherSharedMemoryBase::DataFetcherSharedMemoryBase()
    : started_consumers_(0) {
}

DataFetcherSharedMemoryBase::~DataFetcherSharedMemoryBase() {
  DCHECK_EQ(0u, started_consumers_);

  // make sure polling thread stops asap.
  if (polling_thread_)
    polling_thread_->Stop();
}

bool DataFetcherSharedMemoryBase::StartFetchingDeviceData(
    ConsumerType consumer_type) {
  if (started_consumers_ & consumer_type)
    return true;

  void* buffer = GetSharedMemoryBuffer(consumer_type);
  if (!buffer)
    return false;

  size_t buffer_size = GetConsumerSharedMemoryBufferSize(consumer_type);
  // buffer size should be strictly positive because buffer is non-zero.
  DCHECK(buffer_size > 0);
  // make sure to clear any potentially stale values in the memory buffer.
  memset(buffer, 0, buffer_size);

  if (GetType() != FETCHER_TYPE_DEFAULT) {
    if (!InitAndStartPollingThreadIfNecessary())
      return false;
    polling_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PollingThread::AddConsumer,
                              base::Unretained(polling_thread_.get()),
                              consumer_type, buffer));
  } else {
    if (!Start(consumer_type, buffer))
      return false;
  }

  started_consumers_ |= consumer_type;

  return true;
}

bool DataFetcherSharedMemoryBase::StopFetchingDeviceData(
    ConsumerType consumer_type) {
  if (!(started_consumers_ & consumer_type))
    return true;

  if (GetType() != FETCHER_TYPE_DEFAULT) {
    polling_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&PollingThread::RemoveConsumer,
                   base::Unretained(polling_thread_.get()), consumer_type));
  } else {
    if (!Stop(consumer_type))
      return false;
  }

  started_consumers_ ^= consumer_type;

  return true;
}

void DataFetcherSharedMemoryBase::Shutdown() {
  StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  StopFetchingDeviceData(CONSUMER_TYPE_LIGHT);
}

mojo::ScopedSharedBufferHandle
DataFetcherSharedMemoryBase::GetSharedMemoryHandle(ConsumerType consumer_type) {
  auto it = shared_memory_map_.find(consumer_type);
  DCHECK(it != shared_memory_map_.end());
  return it->second.first->Clone();
}

bool DataFetcherSharedMemoryBase::InitAndStartPollingThreadIfNecessary() {
  if (polling_thread_)
    return true;

  polling_thread_.reset(new PollingThread("Device Sensor poller", this));

  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start sensor data polling thread";
    return false;
  }
  return true;
}

void DataFetcherSharedMemoryBase::Fetch(unsigned consumer_bitmask) {
  NOTIMPLEMENTED();
}

DataFetcherSharedMemoryBase::FetcherType
DataFetcherSharedMemoryBase::GetType() const {
  return FETCHER_TYPE_DEFAULT;
}

base::TimeDelta DataFetcherSharedMemoryBase::GetInterval() const {
  return base::TimeDelta::FromMicroseconds(kDeviceSensorIntervalMicroseconds);
}

void* DataFetcherSharedMemoryBase::GetSharedMemoryBuffer(
    ConsumerType consumer_type) {
  auto it = shared_memory_map_.find(consumer_type);
  if (it != shared_memory_map_.end())
    return it->second.second.get();

  size_t buffer_size = GetConsumerSharedMemoryBufferSize(consumer_type);
  if (buffer_size == 0)
    return nullptr;

  mojo::ScopedSharedBufferHandle buffer =
      mojo::SharedBufferHandle::Create(buffer_size);
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(buffer_size);
  if (!mapping)
    return nullptr;
  void* mem = mapping.get();
  memset(mem, 0, buffer_size);
  shared_memory_map_[consumer_type] =
      std::make_pair(std::move(buffer), std::move(mapping));
  return mem;
}

base::MessageLoop* DataFetcherSharedMemoryBase::GetPollingMessageLoop() const {
  return polling_thread_ ? polling_thread_->message_loop() : nullptr;
}

bool DataFetcherSharedMemoryBase::IsPollingTimerRunningForTesting() const {
  return polling_thread_ ? polling_thread_->IsTimerRunning() : false;
}

}  // namespace content
