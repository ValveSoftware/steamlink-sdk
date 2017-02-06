// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char BluetoothLEAdvertisingManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

// The BluetoothAdvertisementManagerClient implementation used in production.
class BluetoothAdvertisementManagerClientImpl
    : public BluetoothLEAdvertisingManagerClient,
      public dbus::ObjectManager::Interface {
 public:
  BluetoothAdvertisementManagerClientImpl()
      : object_manager_(NULL), weak_ptr_factory_(this) {}

  ~BluetoothAdvertisementManagerClientImpl() override {
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface);
    }
  }

  // BluetoothAdapterClient override.
  void AddObserver(
      BluetoothLEAdvertisingManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAdapterClient override.
  void RemoveObserver(
      BluetoothLEAdvertisingManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new dbus::PropertySet(object_proxy, interface_name,
                                 dbus::PropertySet::PropertyChangedCallback());
  }

  // BluetoothAdvertisementManagerClient override.
  void RegisterAdvertisement(const dbus::ObjectPath& manager_object_path,
                             const dbus::ObjectPath& advertisement_object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface,
        bluetooth_advertising_manager::kRegisterAdvertisement);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(advertisement_object_path);

    // Empty dictionary for options.
    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);
    writer.CloseContainer(&array_writer);

    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(manager_object_path);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdvertisementManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothAdvertisementManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothAdvertisementManagerClient override.
  void UnregisterAdvertisement(
      const dbus::ObjectPath& manager_object_path,
      const dbus::ObjectPath& advertisement_object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface,
        bluetooth_advertising_manager::kUnregisterAdvertisement);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(advertisement_object_path);

    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(manager_object_path);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdvertisementManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothAdvertisementManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    DCHECK(bus);
    object_manager_ = bus->GetObjectManager(
        bluetooth_object_manager::kBluetoothObjectManagerServiceName,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface,
        this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the advertising manager
  // interface is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    FOR_EACH_OBSERVER(BluetoothLEAdvertisingManagerClient::Observer, observers_,
                      AdvertisingManagerAdded(object_path));
  }

  // Called by dbus::ObjectManager when an object with the advertising manager
  // interface is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    FOR_EACH_OBSERVER(BluetoothLEAdvertisingManagerClient::Observer, observers_,
                      AdvertisingManagerRemoved(object_path));
  }

  // Called when a response for successful method call is received.
  void OnSuccess(const base::Closure& callback, dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(error_name, error_message);
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothLEAdvertisingManagerClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdvertisementManagerClientImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisementManagerClientImpl);
};

BluetoothLEAdvertisingManagerClient::BluetoothLEAdvertisingManagerClient() {}

BluetoothLEAdvertisingManagerClient::~BluetoothLEAdvertisingManagerClient() {}

BluetoothLEAdvertisingManagerClient*
BluetoothLEAdvertisingManagerClient::Create() {
  return new BluetoothAdvertisementManagerClientImpl();
}

}  // namespace bluez
