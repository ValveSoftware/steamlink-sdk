// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_H_
#define COMPONENTS_MUS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/public/interfaces/input_devices/input_device_server.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"

namespace mus {

// Allows in-process client code to register as a InputDeviceEventObserver and
// get information about input-devices. InputDeviceClient itself acts as an
// InputDeviceObserverMojo and registers to get updates from InputDeviceServer.
// Essentially, InputDeviceClient forwards input-device events and caches
// input-device state.
class InputDeviceClient : public mojom::InputDeviceObserverMojo,
                          public ui::InputDeviceManager {
 public:
  InputDeviceClient();
  ~InputDeviceClient() override;

  // Connects to mojo:mus as an observer on InputDeviceServer to receive input
  // device updates.
  void Connect(mojom::InputDeviceServerPtr server);

  // ui::InputDeviceManager:
  const std::vector<ui::InputDevice>& GetKeyboardDevices() const override;
  const std::vector<ui::TouchscreenDevice>& GetTouchscreenDevices()
      const override;
  const std::vector<ui::InputDevice>& GetMouseDevices() const override;
  const std::vector<ui::InputDevice>& GetTouchpadDevices() const override;

  bool AreDeviceListsComplete() const override;
  bool AreTouchscreensEnabled() const override;

  void AddObserver(ui::InputDeviceEventObserver* observer) override;
  void RemoveObserver(ui::InputDeviceEventObserver* observer) override;

 private:
  // mojom::InputDeviceObserverMojo:
  void OnKeyboardDeviceConfigurationChanged(
      mojo::Array<ui::InputDevice> devices) override;
  void OnTouchscreenDeviceConfigurationChanged(
      mojo::Array<ui::TouchscreenDevice> devices) override;
  void OnMouseDeviceConfigurationChanged(
      mojo::Array<ui::InputDevice> devices) override;
  void OnTouchpadDeviceConfigurationChanged(
      mojo::Array<ui::InputDevice> devices) override;
  void OnDeviceListsComplete(
      mojo::Array<ui::InputDevice> keyboard_devices,
      mojo::Array<ui::TouchscreenDevice> touchscreen_devices,
      mojo::Array<ui::InputDevice> mouse_devices,
      mojo::Array<ui::InputDevice> touchpad_devices) override;

  mojo::Binding<mojom::InputDeviceObserverMojo> binding_;

  // Holds the list of input devices and signal that we have received the lists
  // after initialization.
  std::vector<ui::InputDevice> keyboard_devices_;
  std::vector<ui::TouchscreenDevice> touchscreen_devices_;
  std::vector<ui::InputDevice> mouse_devices_;
  std::vector<ui::InputDevice> touchpad_devices_;
  bool device_lists_complete_ = false;

  // List of in-process observers.
  base::ObserverList<ui::InputDeviceEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceClient);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_H_
