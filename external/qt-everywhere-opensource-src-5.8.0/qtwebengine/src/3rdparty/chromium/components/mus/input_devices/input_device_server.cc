// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/input_devices/input_device_server.h"

#include <utility>
#include <vector>

#include "mojo/public/cpp/bindings/array.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace mus {

InputDeviceServer::InputDeviceServer() {}

InputDeviceServer::~InputDeviceServer() {
  if (manager_ && ui::DeviceDataManager::HasInstance()) {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }
}

void InputDeviceServer::RegisterAsObserver() {
  if (!manager_ && ui::DeviceDataManager::HasInstance()) {
    manager_ = ui::DeviceDataManager::GetInstance();
    manager_->AddObserver(this);
  }
}

bool InputDeviceServer::IsRegisteredAsObserver() const {
  return manager_ != nullptr;
}

void InputDeviceServer::AddInterface(shell::Connection* connection) {
  DCHECK(manager_);
  connection->AddInterface<mojom::InputDeviceServer>(this);
}

void InputDeviceServer::AddObserver(
    mojom::InputDeviceObserverMojoPtr observer) {
  // We only want to send this message once, so we need to check to make sure
  // device lists are actually complete before sending it to a new observer.
  if (manager_->AreDeviceListsComplete())
    SendDeviceListsComplete(observer.get());
  observers_.AddPtr(std::move(observer));
}

void InputDeviceServer::OnKeyboardDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetKeyboardDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnKeyboardDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnTouchscreenDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetTouchscreenDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnTouchscreenDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnMouseDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetMouseDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnMouseDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnTouchpadDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetTouchpadDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnTouchpadDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnDeviceListsComplete() {
  observers_.ForAllPtrs([this](mojom::InputDeviceObserverMojo* observer) {
    SendDeviceListsComplete(observer);
  });
}

void InputDeviceServer::SendDeviceListsComplete(
    mojom::InputDeviceObserverMojo* observer) {
  DCHECK(manager_->AreDeviceListsComplete());

  observer->OnDeviceListsComplete(
      manager_->GetKeyboardDevices(), manager_->GetTouchscreenDevices(),
      manager_->GetMouseDevices(), manager_->GetTouchpadDevices());
}

void InputDeviceServer::Create(shell::Connection* connection,
                               mojom::InputDeviceServerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace mus
