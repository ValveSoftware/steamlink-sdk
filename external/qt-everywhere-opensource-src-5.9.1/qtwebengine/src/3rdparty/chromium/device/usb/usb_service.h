// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_SERVICE_H_
#define DEVICE_USB_USB_SERVICE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"

namespace device {

class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public base::NonThreadSafe {
 public:
  using GetDevicesCallback =
      base::Callback<void(const std::vector<scoped_refptr<UsbDevice>>&)>;

  class Observer {
   public:
    virtual ~Observer();

    // These events are delivered from the thread on which the UsbService object
    // was created.
    virtual void OnDeviceAdded(scoped_refptr<UsbDevice> device);
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
    // For observers that need to process device removal after others have run.
    // Should not depend on any other service's knowledge of connected devices.
    virtual void OnDeviceRemovedCleanup(scoped_refptr<UsbDevice> device);

    // Notifies the observer that the UsbService it depends on is shutting down.
    virtual void WillDestroyUsbService();
  };

  // The file task runner reference is used for blocking I/O operations.
  // Returns nullptr when initialization fails.
  static std::unique_ptr<UsbService> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  virtual ~UsbService();

  scoped_refptr<UsbDevice> GetDevice(const std::string& guid);

  // Shuts down the UsbService. Must be called before destroying the UsbService
  // when tasks can still be posted to the |blocking_task_runner| provided to
  // Create().
  virtual void Shutdown();

  // Enumerates available devices.
  virtual void GetDevices(const GetDevicesCallback& callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods to add and remove devices for testing purposes. Only a device added
  // by this method can be removed by RemoveDeviceForTesting().
  void AddDeviceForTesting(scoped_refptr<UsbDevice> device);
  void RemoveDeviceForTesting(const std::string& device_guid);
  void GetTestDevices(std::vector<scoped_refptr<UsbDevice>>* devices);

 protected:
  UsbService(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  void NotifyDeviceAdded(scoped_refptr<UsbDevice> device);
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner() {
    return blocking_task_runner_;
  }

  std::unordered_map<std::string, scoped_refptr<UsbDevice>>& devices() {
    return devices_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unordered_map<std::string, scoped_refptr<UsbDevice>> devices_;
  std::unordered_set<std::string> testing_devices_;
  base::ObserverList<Observer, true> observer_list_;

#if DCHECK_IS_ON()
  bool did_shutdown_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

}  // namespace device

#endif  // DEVICE_USB_USB_SERVICE_H_
