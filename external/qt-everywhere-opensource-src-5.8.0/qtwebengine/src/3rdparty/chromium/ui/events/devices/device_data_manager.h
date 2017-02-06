// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace test {
class DeviceDataManagerTestAPI;
}  // namespace test

class InputDeviceEventObserver;

// Keeps track of device mappings and event transformations.
class EVENTS_DEVICES_EXPORT DeviceDataManager
    : public InputDeviceManager,
      public DeviceHotplugEventObserver {
 public:
  static const int kMaxDeviceNum = 128;
  ~DeviceDataManager() override;

  static void CreateInstance();
  static void DeleteInstance();
  static DeviceDataManager* GetInstance();
  static bool HasInstance();

  void ClearTouchDeviceAssociations();
  void UpdateTouchInfoForDisplay(int64_t target_display_id,
                                 int touch_device_id,
                                 const gfx::Transform& touch_transformer);
  void ApplyTouchTransformer(int touch_device_id, float* x, float* y);

  // Gets the display that touches from |touch_device_id| should be sent to.
  int64_t GetTargetDisplayForTouchDevice(int touch_device_id) const;

  void UpdateTouchRadiusScale(int touch_device_id, double scale);
  void ApplyTouchRadiusScale(int touch_device_id, double* radius);

  void SetTouchscreensEnabled(bool enabled);

  // InputDeviceManager:
  const std::vector<TouchscreenDevice>& GetTouchscreenDevices() const override;
  const std::vector<InputDevice>& GetKeyboardDevices() const override;
  const std::vector<InputDevice>& GetMouseDevices() const override;
  const std::vector<InputDevice>& GetTouchpadDevices() const override;
  bool AreDeviceListsComplete() const override;
  bool AreTouchscreensEnabled() const override;

  void AddObserver(InputDeviceEventObserver* observer) override;
  void RemoveObserver(InputDeviceEventObserver* observer) override;

 protected:
  DeviceDataManager();

  static DeviceDataManager* instance();

  static void set_instance(DeviceDataManager* instance);

  // DeviceHotplugEventObserver:
  void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override;
  void OnKeyboardDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnTouchpadDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnDeviceListsComplete() override;

 private:
  // Information related to a touchscreen device.
  struct TouchscreenInfo {
    TouchscreenInfo();
    void Reset();

    double radius_scale;
    int64_t target_display;
    gfx::Transform device_transform;
  };

  friend class test::DeviceDataManagerTestAPI;

  static DeviceDataManager* instance_;

  bool IsTouchDeviceIdValid(int touch_device_id) const;

  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversMouseDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void NotifyObserversDeviceListsComplete();

  std::vector<TouchscreenDevice> touchscreen_devices_;
  std::vector<InputDevice> keyboard_devices_;
  std::vector<InputDevice> mouse_devices_;
  std::vector<InputDevice> touchpad_devices_;
  bool device_lists_complete_ = false;

  base::ObserverList<InputDeviceEventObserver> observers_;

  bool touch_screens_enabled_ = true;

  // Contains touchscreen device info for each device mapped by device ID. Will
  // have default values if the device with corresponding ID isn't a touchscreen
  // or doesn't exist.
  std::array<TouchscreenInfo, kMaxDeviceNum> touch_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDataManager);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
