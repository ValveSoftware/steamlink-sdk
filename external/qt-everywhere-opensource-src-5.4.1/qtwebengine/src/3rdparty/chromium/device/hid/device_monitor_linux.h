// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_DEVICE_MONITOR_LINUX_H_
#define DEVICE_HID_DEVICE_MONITOR_LINUX_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "device/udev_linux/udev.h"

struct udev_device;

namespace device {

// This class listends for notifications from libudev about
// connected/disconnected devices. This class is *NOT* thread-safe and
// all methods must be accessed from the FILE thread.
class DeviceMonitorLinux : public base::MessageLoop::DestructionObserver,
                           public base::MessagePumpLibevent::Watcher {
 public:
  typedef base::Callback<void(udev_device* device)> EnumerateCallback;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnDeviceAdded(udev_device* device) = 0;
    virtual void OnDeviceRemoved(udev_device* device) = 0;
  };

  DeviceMonitorLinux();

  static DeviceMonitorLinux* GetInstance();
  static bool HasInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  ScopedUdevDevicePtr GetDeviceFromPath(const std::string& path);
  void Enumerate(const EnumerateCallback& callback);

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Implements base::MessagePumpLibevent::Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  friend struct base::DefaultDeleter<DeviceMonitorLinux>;

  virtual ~DeviceMonitorLinux();

  ScopedUdevPtr udev_;
  ScopedUdevMonitorPtr monitor_;
  int monitor_fd_;
  base::MessagePumpLibevent::FileDescriptorWatcher monitor_watcher_;

  ObserverList<Observer> observers_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorLinux);
};

}  // namespace device

#endif  // DEVICE_HID_DEVICE_MONITOR_LINUX_H_
