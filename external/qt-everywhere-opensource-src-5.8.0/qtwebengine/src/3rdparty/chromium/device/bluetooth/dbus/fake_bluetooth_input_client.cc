// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"

#include <map>

#include "base/logging.h"
#include "base/stl_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothInputClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothInputClient::Properties(
          NULL,
          bluetooth_input::kBluetoothInputInterface,
          callback) {}

FakeBluetoothInputClient::Properties::~Properties() {}

void FakeBluetoothInputClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothInputClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothInputClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeBluetoothInputClient::FakeBluetoothInputClient() {}

FakeBluetoothInputClient::~FakeBluetoothInputClient() {
  // Clean up Properties structures
  STLDeleteValues(&properties_map_);
}

void FakeBluetoothInputClient::Init(dbus::Bus* bus) {}

void FakeBluetoothInputClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothInputClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeBluetoothInputClient::Properties* FakeBluetoothInputClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second;
  return NULL;
}

void FakeBluetoothInputClient::AddInputDevice(
    const dbus::ObjectPath& object_path) {
  if (properties_map_.find(object_path) != properties_map_.end())
    return;

  Properties* properties =
      new Properties(base::Bind(&FakeBluetoothInputClient::OnPropertyChanged,
                                base::Unretained(this), object_path));

  // The LegacyAutopair and DisplayPinCode devices represent a typical mouse
  // and keyboard respectively, so mark them as ReconnectMode "any". The
  // DisplayPasskey device represents a Bluetooth 2.1+ keyboard and the
  // ConnectUnpairable device represents a pre-standardization mouse, so mark
  // them as ReconnectMode "device".
  if (object_path.value() == FakeBluetoothDeviceClient::kDisplayPasskeyPath ||
      object_path.value() ==
          FakeBluetoothDeviceClient::kConnectUnpairablePath) {
    properties->reconnect_mode.ReplaceValue(
        bluetooth_input::kDeviceReconnectModeProperty);
  } else {
    properties->reconnect_mode.ReplaceValue(
        bluetooth_input::kAnyReconnectModeProperty);
  }

  properties_map_[object_path] = properties;

  FOR_EACH_OBSERVER(BluetoothInputClient::Observer, observers_,
                    InputAdded(object_path));
}

void FakeBluetoothInputClient::RemoveInputDevice(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::iterator it = properties_map_.find(object_path);

  if (it == properties_map_.end())
    return;

  FOR_EACH_OBSERVER(BluetoothInputClient::Observer, observers_,
                    InputRemoved(object_path));

  delete it->second;
  properties_map_.erase(it);
}

void FakeBluetoothInputClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(BluetoothInputClient::Observer, observers_,
                    InputPropertyChanged(object_path, property_name));
}

}  // namespace bluez
