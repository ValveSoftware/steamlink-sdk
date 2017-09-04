// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_INPUT_DEVICES_INPUT_DEVICE_SERVER_H_
#define SERVICES_UI_INPUT_DEVICES_INPUT_DEVICE_SERVER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/ui/public/interfaces/input_devices/input_device_server.mojom.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace service_manager {
class InterfaceRegistry;
}

namespace ui {

// Listens to DeviceDataManager for updates on input-devices and forwards those
// updates to any registered InputDeviceObserverMojo in other processes via
// Mojo IPC. This runs in the mus-ws process.
class InputDeviceServer
    : public service_manager::InterfaceFactory<mojom::InputDeviceServer>,
      public mojom::InputDeviceServer,
      public ui::InputDeviceEventObserver {
 public:
  InputDeviceServer();
  ~InputDeviceServer() override;

  // Registers this instance as a local observer with DeviceDataManager.
  void RegisterAsObserver();
  bool IsRegisteredAsObserver() const;

  // Adds interface with the connection registry so remote observers can
  // connect. You should have already called RegisterAsObserver() to get local
  // input-device event updates and checked it was successful by calling
  // IsRegisteredAsObserver().
  void AddInterface(service_manager::InterfaceRegistry* registry);

  // mojom::InputDeviceServer:
  void AddObserver(mojom::InputDeviceObserverMojoPtr observer) override;

  // ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;
  void OnTouchscreenDeviceConfigurationChanged() override;
  void OnMouseDeviceConfigurationChanged() override;
  void OnTouchpadDeviceConfigurationChanged() override;
  void OnDeviceListsComplete() override;
  void OnStylusStateChanged(StylusState state) override;

 private:
  // Sends the current state of all input-devices to an observer.
  void SendDeviceListsComplete(mojom::InputDeviceObserverMojo* observer);

  // mojo::InterfaceFactory<mojom::InputDeviceServer>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::InputDeviceServerRequest request) override;

  mojo::BindingSet<mojom::InputDeviceServer> bindings_;
  mojo::InterfacePtrSet<mojom::InputDeviceObserverMojo> observers_;

  // DeviceDataManager instance we are registered as an observer with.
  ui::DeviceDataManager* manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceServer);
};

}  // namespace ui

#endif  // SERVICES_UI_INPUT_DEVICES_INPUT_DEVICE_SERVER_H_
