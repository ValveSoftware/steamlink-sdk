// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_BASE_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_BASE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/device_sensors/device_sensors_consts.h"
#include "content/common/content_export.h"

namespace content {

// Sensor data fetchers should derive from this base class and implement
// the abstract Start() and Stop() methods.
// If the fetcher requires polling it should also implement IsPolling()
// to return true and the Fetch() method which will be called from the
// polling thread to fetch data at regular intervals.
class CONTENT_EXPORT DataFetcherSharedMemoryBase {
 public:
  // Starts updating the shared memory buffer with sensor data at
  // regular intervals. Returns true if the relevant sensors could
  // be successfully activated.
  bool StartFetchingDeviceData(ConsumerType consumer_type);

  // Stops updating the shared memory buffer. Returns true if the
  // relevant sensors could be successfully deactivated.
  bool StopFetchingDeviceData(ConsumerType consumer_type);

  // Should be called before destruction to make sure all active
  // sensors are unregistered.
  virtual void Shutdown();

  // Returns the shared memory handle of the device sensor data
  // duplicated into the given process. This method should only be
  // called after a call to StartFetchingDeviceData method with
  // corresponding |consumer_type| parameter.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      ConsumerType consumer_type, base::ProcessHandle process);

  enum FetcherType {
    // Fetcher runs on the same thread as its creator.
    FETCHER_TYPE_DEFAULT,
    // Fetcher runs on a separate thread calling |Fetch()| at regular intervals.
    FETCHER_TYPE_POLLING_CALLBACK,
    // Fetcher runs on a separate thread, but no callbacks are executed.
    FETCHER_TYPE_SEPARATE_THREAD
  };

 protected:
  class PollingThread;

  DataFetcherSharedMemoryBase();
  virtual ~DataFetcherSharedMemoryBase();

  // Returns the message loop of the polling thread.
  // Returns NULL if there is no polling thread.
  base::MessageLoop* GetPollingMessageLoop() const;

  // If IsPolling() is true this method is called from the |polling_thread_|
  // at regular intervals.
  virtual void Fetch(unsigned consumer_bitmask);

  // Returns the type of thread this fetcher runs on.
  virtual FetcherType GetType() const;

  // Returns the sensor sampling interval. In particular if this fetcher
  // GetType() == FETCHER_TYPE_POLLING_CALLBACK the interval between
  // successive calls to Fetch().
  virtual base::TimeDelta GetInterval() const;

  // Start() method should call InitSharedMemoryBuffer() to get the shared
  // memory pointer. If IsPolling() is true both Start() and Stop() methods
  // are called from the |polling_thread_|.
  virtual bool Start(ConsumerType consumer_type, void* buffer) = 0;
  virtual bool Stop(ConsumerType consumer_type) = 0;

  bool IsPollingTimerRunningForTesting() const;

 private:
  bool InitAndStartPollingThreadIfNecessary();
  base::SharedMemory* GetSharedMemory(ConsumerType consumer_type);
  void* GetSharedMemoryBuffer(ConsumerType consumer_type);

  unsigned started_consumers_;

  std::unique_ptr<PollingThread> polling_thread_;

  // Owning pointers. Objects in the map are deleted in dtor.
  typedef std::map<ConsumerType, base::SharedMemory*> SharedMemoryMap;
  SharedMemoryMap shared_memory_map_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemoryBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_BASE_H_
