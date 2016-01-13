// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_INPUT_SERVICE_LINUX_H_
#define DEVICE_HID_INPUT_SERVICE_LINUX_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "device/hid/device_monitor_linux.h"

namespace device {

// This class provides information and notifications about
// connected/disconnected input/HID devices. This class is *NOT*
// thread-safe and all methods must be called from the FILE thread.
class InputServiceLinux : public base::MessageLoop::DestructionObserver {
 public:
  struct InputDeviceInfo {
    enum Subsystem { SUBSYSTEM_HID, SUBSYSTEM_INPUT, SUBSYSTEM_UNKNOWN };
    enum Type { TYPE_BLUETOOTH, TYPE_USB, TYPE_SERIO, TYPE_UNKNOWN };

    InputDeviceInfo();

    std::string id;
    std::string name;
    Subsystem subsystem;
    Type type;

    bool is_accelerometer : 1;
    bool is_joystick : 1;
    bool is_key : 1;
    bool is_keyboard : 1;
    bool is_mouse : 1;
    bool is_tablet : 1;
    bool is_touchpad : 1;
    bool is_touchscreen : 1;
  };

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnInputDeviceAdded(const InputDeviceInfo& info) = 0;
    virtual void OnInputDeviceRemoved(const std::string& id) = 0;
  };

  InputServiceLinux();

  static InputServiceLinux* GetInstance();
  static bool HasInstance();
  static void SetForTesting(InputServiceLinux* service);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns list of all currently connected input/hid devices.
  void GetDevices(std::vector<InputDeviceInfo>* devices);

  // Returns an info about input device identified by |id|. When there're
  // no input or hid device with such id, returns false and doesn't
  // modify |info|.
  bool GetDeviceInfo(const std::string& id, InputDeviceInfo* info) const;

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 protected:
  virtual ~InputServiceLinux();

  void AddDevice(const InputDeviceInfo& info);
  void RemoveDevice(const std::string& id);

  bool CalledOnValidThread() const;

 private:
  friend struct base::DefaultDeleter<InputServiceLinux>;

  typedef base::hash_map<std::string, InputDeviceInfo> DeviceMap;

  DeviceMap devices_;
  ObserverList<Observer> observers_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_INPUT_SERVICE_LINUX_H_
