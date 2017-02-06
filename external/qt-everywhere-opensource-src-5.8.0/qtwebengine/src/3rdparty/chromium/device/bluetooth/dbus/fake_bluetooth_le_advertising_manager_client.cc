// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char FakeBluetoothLEAdvertisingManagerClient::kAdvertisingManagerPath[] =
    "/fake/hci0";

FakeBluetoothLEAdvertisingManagerClient::
    FakeBluetoothLEAdvertisingManagerClient() {}

FakeBluetoothLEAdvertisingManagerClient::
    ~FakeBluetoothLEAdvertisingManagerClient() {}

void FakeBluetoothLEAdvertisingManagerClient::Init(dbus::Bus* bus) {}

void FakeBluetoothLEAdvertisingManagerClient::AddObserver(Observer* observer) {}

void FakeBluetoothLEAdvertisingManagerClient::RemoveObserver(
    Observer* observer) {}

void FakeBluetoothLEAdvertisingManagerClient::RegisterAdvertisement(
    const dbus::ObjectPath& manager_object_path,
    const dbus::ObjectPath& advertisement_object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "RegisterAdvertisment: " << advertisement_object_path.value();

  if (manager_object_path != dbus::ObjectPath(kAdvertisingManagerPath)) {
    error_callback.Run(kNoResponseError, "Invalid Advertising Manager path.");
    return;
  }

  ServiceProviderMap::iterator iter =
      service_provider_map_.find(advertisement_object_path);
  if (iter == service_provider_map_.end()) {
    error_callback.Run(bluetooth_advertising_manager::kErrorInvalidArguments,
                       "Advertisement object not registered");
  } else if (!currently_registered_.value().empty()) {
    error_callback.Run(bluetooth_advertising_manager::kErrorFailed,
                       "Maximum advertisements reached");
  } else {
    currently_registered_ = advertisement_object_path;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  }
}

void FakeBluetoothLEAdvertisingManagerClient::UnregisterAdvertisement(
    const dbus::ObjectPath& manager_object_path,
    const dbus::ObjectPath& advertisement_object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "UnregisterAdvertisment: " << advertisement_object_path.value();

  if (manager_object_path != dbus::ObjectPath(kAdvertisingManagerPath)) {
    error_callback.Run(kNoResponseError, "Invalid Advertising Manager path.");
    return;
  }

  ServiceProviderMap::iterator iter =
      service_provider_map_.find(advertisement_object_path);
  if (iter == service_provider_map_.end()) {
    error_callback.Run(bluetooth_advertising_manager::kErrorDoesNotExist,
                       "Advertisement not registered");
  } else if (advertisement_object_path != currently_registered_) {
    error_callback.Run(bluetooth_advertising_manager::kErrorDoesNotExist,
                       "Does not exist");
  } else {
    currently_registered_ = dbus::ObjectPath("");
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  }
}

void FakeBluetoothLEAdvertisingManagerClient::
    RegisterAdvertisementServiceProvider(
        FakeBluetoothLEAdvertisementServiceProvider* service_provider) {
  service_provider_map_[service_provider->object_path_] = service_provider;
}

void FakeBluetoothLEAdvertisingManagerClient::
    UnregisterAdvertisementServiceProvider(
        FakeBluetoothLEAdvertisementServiceProvider* service_provider) {
  ServiceProviderMap::iterator iter =
      service_provider_map_.find(service_provider->object_path_);
  if (iter != service_provider_map_.end() && iter->second == service_provider)
    service_provider_map_.erase(iter);
}

}  // namespace bluez
