// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/input_devices/input_device_client.h"

#include "base/logging.h"

namespace mus {

InputDeviceClient::InputDeviceClient() : binding_(this) {
  InputDeviceManager::SetInstance(this);
}

InputDeviceClient::~InputDeviceClient() {
  InputDeviceManager::ClearInstance();
}

void InputDeviceClient::Connect(mojom::InputDeviceServerPtr server) {
  DCHECK(server.is_bound());

  server->AddObserver(binding_.CreateInterfacePtrAndBind());
}

void InputDeviceClient::OnKeyboardDeviceConfigurationChanged(
    mojo::Array<ui::InputDevice> devices) {
  keyboard_devices_ = devices.To<std::vector<ui::InputDevice>>();
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnKeyboardDeviceConfigurationChanged());
}

void InputDeviceClient::OnTouchscreenDeviceConfigurationChanged(
    mojo::Array<ui::TouchscreenDevice> devices) {
  touchscreen_devices_ = devices.To<std::vector<ui::TouchscreenDevice>>();
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnTouchscreenDeviceConfigurationChanged());
}

void InputDeviceClient::OnMouseDeviceConfigurationChanged(
    mojo::Array<ui::InputDevice> devices) {
  mouse_devices_ = devices.To<std::vector<ui::InputDevice>>();
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnMouseDeviceConfigurationChanged());
}

void InputDeviceClient::OnTouchpadDeviceConfigurationChanged(
    mojo::Array<ui::InputDevice> devices) {
  touchpad_devices_ = devices.To<std::vector<ui::InputDevice>>();
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnTouchpadDeviceConfigurationChanged());
}

void InputDeviceClient::OnDeviceListsComplete(
    mojo::Array<ui::InputDevice> keyboard_devices,
    mojo::Array<ui::TouchscreenDevice> touchscreen_devices,
    mojo::Array<ui::InputDevice> mouse_devices,
    mojo::Array<ui::InputDevice> touchpad_devices) {
  // Update the cached device lists if the received list isn't empty.
  if (!keyboard_devices.empty())
    OnKeyboardDeviceConfigurationChanged(std::move(keyboard_devices));
  if (!touchscreen_devices.empty())
    OnTouchscreenDeviceConfigurationChanged(std::move(touchscreen_devices));
  if (!mouse_devices.empty())
    OnMouseDeviceConfigurationChanged(std::move(mouse_devices));
  if (!touchpad_devices.empty())
    OnTouchpadDeviceConfigurationChanged(std::move(touchpad_devices));

  if (!device_lists_complete_) {
    device_lists_complete_ = true;
    FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                      OnDeviceListsComplete());
  }
}

void InputDeviceClient::AddObserver(ui::InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceClient::RemoveObserver(ui::InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetKeyboardDevices()
    const {
  return keyboard_devices_;
}

const std::vector<ui::TouchscreenDevice>&
InputDeviceClient::GetTouchscreenDevices() const {
  return touchscreen_devices_;
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetMouseDevices() const {
  return mouse_devices_;
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetTouchpadDevices()
    const {
  return touchpad_devices_;
}

bool InputDeviceClient::AreDeviceListsComplete() const {
  return device_lists_complete_;
}

bool InputDeviceClient::AreTouchscreensEnabled() const {
  // TODO(kylechar): This obviously isn't right. We either need to pass this
  // state around or modify the interface.
  return true;
}

}  // namespace mus
