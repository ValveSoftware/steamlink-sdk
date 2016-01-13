// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"

namespace content {

namespace {

static size_t GetConsumerSharedMemoryBufferSize(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return sizeof(DeviceMotionHardwareBuffer);
    case CONSUMER_TYPE_ORIENTATION:
      return sizeof(DeviceOrientationHardwareBuffer);
    default:
      NOTREACHED();
  }
  return 0;
}

}  // namespace

class DataFetcherSharedMemoryBase::PollingThread : public base::Thread {
 public:
  PollingThread(const char* name, DataFetcherSharedMemoryBase* fetcher);
  virtual ~PollingThread();

  void AddConsumer(ConsumerType consumer_type, void* buffer);
  void RemoveConsumer(ConsumerType consumer_type);

  unsigned GetConsumersBitmask() const { return consumers_bitmask_; }
  bool IsTimerRunning() const { return timer_ ? timer_->IsRunning() : false; }

 private:
  void DoPoll();

  unsigned consumers_bitmask_;
  DataFetcherSharedMemoryBase* fetcher_;
  scoped_ptr<base::RepeatingTimer<PollingThread> > timer_;

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
    timer_.reset(new base::RepeatingTimer<PollingThread>());
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

  consumers_bitmask_ ^= consumer_type;

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
  StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);

  // make sure polling thread stops asap.
  if (polling_thread_)
    polling_thread_->Stop();

  STLDeleteContainerPairSecondPointers(shared_memory_map_.begin(),
      shared_memory_map_.end());
}

bool DataFetcherSharedMemoryBase::StartFetchingDeviceData(
    ConsumerType consumer_type) {
  if (started_consumers_ & consumer_type)
    return true;

  void* buffer = GetSharedMemoryBuffer(consumer_type);
  if (!buffer)
    return false;

  if (GetType() != FETCHER_TYPE_DEFAULT) {
    if (!InitAndStartPollingThreadIfNecessary())
      return false;
    polling_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PollingThread::AddConsumer,
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
    polling_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PollingThread::RemoveConsumer,
                   base::Unretained(polling_thread_.get()),
                   consumer_type));
  } else {
    if (!Stop(consumer_type))
      return false;
  }

  started_consumers_ ^= consumer_type;

  return true;
}

base::SharedMemoryHandle
DataFetcherSharedMemoryBase::GetSharedMemoryHandleForProcess(
    ConsumerType consumer_type, base::ProcessHandle process) {
  SharedMemoryMap::const_iterator it = shared_memory_map_.find(consumer_type);
  if (it == shared_memory_map_.end())
    return base::SharedMemory::NULLHandle();

  base::SharedMemoryHandle renderer_handle;
  it->second->ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

bool DataFetcherSharedMemoryBase::InitAndStartPollingThreadIfNecessary() {
  if (polling_thread_)
    return true;

  polling_thread_.reset(
      new PollingThread("Inertial Device Sensor poller", this));

  if (!polling_thread_->Start()) {
      LOG(ERROR) << "Failed to start inertial sensor data polling thread";
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
  return base::TimeDelta::FromMilliseconds(kInertialSensorIntervalMillis);
}

base::SharedMemory* DataFetcherSharedMemoryBase::GetSharedMemory(
    ConsumerType consumer_type) {
  SharedMemoryMap::const_iterator it = shared_memory_map_.find(consumer_type);
  if (it != shared_memory_map_.end())
    return it->second;

  size_t buffer_size = GetConsumerSharedMemoryBufferSize(consumer_type);
  if (buffer_size == 0)
    return NULL;

  scoped_ptr<base::SharedMemory> new_shared_mem(new base::SharedMemory);
  if (new_shared_mem->CreateAndMapAnonymous(buffer_size)) {
    if (void* mem = new_shared_mem->memory()) {
      memset(mem, 0, buffer_size);
      base::SharedMemory* shared_mem = new_shared_mem.release();
      shared_memory_map_[consumer_type] = shared_mem;
      return shared_mem;
    }
  }
  LOG(ERROR) << "Failed to initialize shared memory";
  return NULL;
}

void* DataFetcherSharedMemoryBase::GetSharedMemoryBuffer(
    ConsumerType consumer_type) {
  if (base::SharedMemory* shared_memory = GetSharedMemory(consumer_type))
    return shared_memory->memory();
  return NULL;
}

base::MessageLoop* DataFetcherSharedMemoryBase::GetPollingMessageLoop() const {
  return polling_thread_ ? polling_thread_->message_loop() : NULL;
}

bool DataFetcherSharedMemoryBase::IsPollingTimerRunningForTesting() const {
  return polling_thread_ ? polling_thread_->IsTimerRunning() : false;
}


}  // namespace content
