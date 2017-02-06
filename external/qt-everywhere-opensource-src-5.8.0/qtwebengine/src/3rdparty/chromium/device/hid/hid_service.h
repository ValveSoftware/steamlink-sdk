// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"

namespace device {

class HidConnection;

// The HidService keeps track of human interface devices connected to the
// system. Call HidService::GetInstance to get the singleton instance.
class HidService {
 public:
  class Observer {
   public:
    virtual void OnDeviceAdded(scoped_refptr<HidDeviceInfo> info);
    // Notifies all observers that a device is being removed, called before
    // removing the device from HidService. Observers should not depend on the
    // order in which they are notified of the OnDeviceRemove event.
    virtual void OnDeviceRemoved(scoped_refptr<HidDeviceInfo> info);
    // Notifies all observers again, after having first notified all observers
    // with OnDeviceRemoved and removed the device from internal structures.
    // Each observer must not depend on any other observers' awareness of the
    // device as they could be cleaned up in any order.
    virtual void OnDeviceRemovedCleanup(scoped_refptr<HidDeviceInfo> info);
  };

  typedef base::Callback<void(const std::vector<scoped_refptr<HidDeviceInfo>>&)>
      GetDevicesCallback;
  typedef base::Callback<void(scoped_refptr<HidConnection> connection)>
      ConnectCallback;

  // This function should be called on a thread with a MessageLoopForUI and be
  // passed the task runner for a thread with a MessageLoopForIO.
  static std::unique_ptr<HidService> Create(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

  virtual ~HidService();

  // Enumerates available devices. The provided callback will always be posted
  // to the calling thread's task runner.
  virtual void GetDevices(const GetDevicesCallback& callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Fills in a DeviceInfo struct with info for the given device_id.
  // Returns |nullptr| if |device_id| is invalid.
  scoped_refptr<HidDeviceInfo> GetDeviceInfo(
      const HidDeviceId& device_id) const;

  // Opens a connection to a device. The callback will be run with null on
  // failure.
  virtual void Connect(const HidDeviceId& device_id,
                       const ConnectCallback& callback) = 0;

 protected:
  friend class HidConnectionTest;

  typedef std::map<HidDeviceId, scoped_refptr<HidDeviceInfo>> DeviceMap;

  HidService();

  void AddDevice(scoped_refptr<HidDeviceInfo> info);
  void RemoveDevice(const HidDeviceId& device_id);
  void FirstEnumerationComplete();

  const DeviceMap& devices() const { return devices_; }

  base::ThreadChecker thread_checker_;

 private:
  DeviceMap devices_;
  bool enumeration_ready_;
  std::vector<GetDevicesCallback> pending_enumerations_;
  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
