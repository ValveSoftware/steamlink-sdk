// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

#include <stdio.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_connection_bluez.h"
#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothDevice;
using device::BluetoothRemoteGattService;
using device::BluetoothSocket;
using device::BluetoothUUID;

namespace {

// Histogram enumerations for pairing results.
enum UMAPairingResult {
  UMA_PAIRING_RESULT_SUCCESS,
  UMA_PAIRING_RESULT_INPROGRESS,
  UMA_PAIRING_RESULT_FAILED,
  UMA_PAIRING_RESULT_AUTH_FAILED,
  UMA_PAIRING_RESULT_AUTH_CANCELED,
  UMA_PAIRING_RESULT_AUTH_REJECTED,
  UMA_PAIRING_RESULT_AUTH_TIMEOUT,
  UMA_PAIRING_RESULT_UNSUPPORTED_DEVICE,
  UMA_PAIRING_RESULT_UNKNOWN_ERROR,
  // NOTE: Add new pairing results immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordinly.
  UMA_PAIRING_RESULT_COUNT
};

void ParseModalias(const dbus::ObjectPath& object_path,
                   BluetoothDevice::VendorIDSource* vendor_id_source,
                   uint16_t* vendor_id,
                   uint16_t* product_id,
                   uint16_t* device_id) {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path);
  DCHECK(properties);

  std::string modalias = properties->modalias.value();
  BluetoothDevice::VendorIDSource source_value;
  int vendor_value, product_value, device_value;

  if (sscanf(modalias.c_str(), "bluetooth:v%04xp%04xd%04x", &vendor_value,
             &product_value, &device_value) == 3) {
    source_value = BluetoothDevice::VENDOR_ID_BLUETOOTH;
  } else if (sscanf(modalias.c_str(), "usb:v%04xp%04xd%04x", &vendor_value,
                    &product_value, &device_value) == 3) {
    source_value = BluetoothDevice::VENDOR_ID_USB;
  } else {
    return;
  }

  if (vendor_id_source != NULL)
    *vendor_id_source = source_value;
  if (vendor_id != NULL)
    *vendor_id = vendor_value;
  if (product_id != NULL)
    *product_id = product_value;
  if (device_id != NULL)
    *device_id = device_value;
}

void RecordPairingResult(BluetoothDevice::ConnectErrorCode error_code) {
  UMAPairingResult pairing_result;
  switch (error_code) {
    case BluetoothDevice::ERROR_INPROGRESS:
      pairing_result = UMA_PAIRING_RESULT_INPROGRESS;
      break;
    case BluetoothDevice::ERROR_FAILED:
      pairing_result = UMA_PAIRING_RESULT_FAILED;
      break;
    case BluetoothDevice::ERROR_AUTH_FAILED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_FAILED;
      break;
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_CANCELED;
      break;
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_REJECTED;
      break;
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      pairing_result = UMA_PAIRING_RESULT_AUTH_TIMEOUT;
      break;
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      pairing_result = UMA_PAIRING_RESULT_UNSUPPORTED_DEVICE;
      break;
    default:
      pairing_result = UMA_PAIRING_RESULT_UNKNOWN_ERROR;
  }

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingResult", pairing_result,
                            UMA_PAIRING_RESULT_COUNT);
}

BluetoothDevice::ConnectErrorCode DBusErrorToConnectError(
    const std::string& error_name) {
  BluetoothDevice::ConnectErrorCode error_code = BluetoothDevice::ERROR_UNKNOWN;
  if (error_name == bluetooth_device::kErrorConnectionAttemptFailed) {
    error_code = BluetoothDevice::ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorFailed) {
    error_code = BluetoothDevice::ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationFailed) {
    error_code = BluetoothDevice::ERROR_AUTH_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationCanceled) {
    error_code = BluetoothDevice::ERROR_AUTH_CANCELED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationRejected) {
    error_code = BluetoothDevice::ERROR_AUTH_REJECTED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationTimeout) {
    error_code = BluetoothDevice::ERROR_AUTH_TIMEOUT;
  }
  return error_code;
}

}  // namespace

namespace bluez {

BluetoothDeviceBlueZ::BluetoothDeviceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const dbus::ObjectPath& object_path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread)
    : BluetoothDevice(adapter),
      object_path_(object_path),
      num_connecting_calls_(0),
      connection_monitor_started_(false),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      weak_ptr_factory_(this) {
  bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient()->AddObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);

  InitializeGattServiceMap();
}

BluetoothDeviceBlueZ::~BluetoothDeviceBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattServiceClient()
      ->RemoveObserver(this);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(
      this);
  // Copy the GATT services list here and clear the original so that when we
  // send GattServiceRemoved(), GetGattServices() returns no services.
  GattServiceMap gatt_services_swapped;
  gatt_services_swapped.swap(gatt_services_);
  for (const auto& iter : gatt_services_swapped) {
    DCHECK(adapter());
    adapter()->NotifyGattServiceRemoved(
        static_cast<BluetoothRemoteGattServiceBlueZ*>(iter.second));
  }
}

uint32_t BluetoothDeviceBlueZ::GetBluetoothClass() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->bluetooth_class.value();
}

device::BluetoothTransport BluetoothDeviceBlueZ::GetType() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->type.is_valid())
    return device::BLUETOOTH_TRANSPORT_INVALID;

  std::string type = properties->type.value();
  if (type == bluez::BluetoothDeviceClient::kTypeBredr) {
    return device::BLUETOOTH_TRANSPORT_CLASSIC;
  } else if (type == bluez::BluetoothDeviceClient::kTypeLe) {
    return device::BLUETOOTH_TRANSPORT_LE;
  } else if (type == bluez::BluetoothDeviceClient::kTypeDual) {
    return device::BLUETOOTH_TRANSPORT_DUAL;
  }

  NOTREACHED();
  return device::BLUETOOTH_TRANSPORT_INVALID;
}

std::string BluetoothDeviceBlueZ::GetDeviceName() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

void BluetoothDeviceBlueZ::CreateGattConnectionImpl() {
  // BlueZ implementation does not use the default CreateGattConnection
  // implementation.
  NOTIMPLEMENTED();
}

void BluetoothDeviceBlueZ::SetGattServicesDiscoveryComplete(bool complete) {
  // BlueZ implementation already tracks service discovery state.
  NOTIMPLEMENTED();
}

bool BluetoothDeviceBlueZ::IsGattServicesDiscoveryComplete() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->services_resolved.value();
}

void BluetoothDeviceBlueZ::DisconnectGatt() {
  Disconnect(base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
}

std::string BluetoothDeviceBlueZ::GetAddress() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return CanonicalizeAddress(properties->address.value());
}

BluetoothDevice::VendorIDSource BluetoothDeviceBlueZ::GetVendorIDSource()
    const {
  VendorIDSource vendor_id_source = VENDOR_ID_UNKNOWN;
  ParseModalias(object_path_, &vendor_id_source, NULL, NULL, NULL);
  return vendor_id_source;
}

uint16_t BluetoothDeviceBlueZ::GetVendorID() const {
  uint16_t vendor_id = 0;
  ParseModalias(object_path_, NULL, &vendor_id, NULL, NULL);
  return vendor_id;
}

uint16_t BluetoothDeviceBlueZ::GetProductID() const {
  uint16_t product_id = 0;
  ParseModalias(object_path_, NULL, NULL, &product_id, NULL);
  return product_id;
}

uint16_t BluetoothDeviceBlueZ::GetDeviceID() const {
  uint16_t device_id = 0;
  ParseModalias(object_path_, NULL, NULL, NULL, &device_id);
  return device_id;
}

uint16_t BluetoothDeviceBlueZ::GetAppearance() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->appearance.is_valid())
    return kAppearanceNotPresent;

  return properties->appearance.value();
}

bool BluetoothDeviceBlueZ::IsPaired() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  // Trusted devices are devices that don't support pairing but that the
  // user has explicitly connected; it makes no sense for UI purposes to
  // treat them differently from each other.
  return properties->paired.value() || properties->trusted.value();
}

bool BluetoothDeviceBlueZ::IsConnected() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->connected.value();
}

bool BluetoothDeviceBlueZ::IsGattConnected() const {
  // Bluez uses the same attribute for GATT Connections and Classic BT
  // Connections.
  return IsConnected();
}

bool BluetoothDeviceBlueZ::IsConnectable() const {
  bluez::BluetoothInputClient::Properties* input_properties =
      bluez::BluezDBusManager::Get()->GetBluetoothInputClient()->GetProperties(
          object_path_);
  // GetProperties returns NULL when the device does not implement the given
  // interface. Non HID devices are normally connectable.
  if (!input_properties)
    return true;

  return input_properties->reconnect_mode.value() != "device";
}

bool BluetoothDeviceBlueZ::IsConnecting() const {
  return num_connecting_calls_ > 0;
}

BluetoothDeviceBlueZ::UUIDList BluetoothDeviceBlueZ::GetUUIDs() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  std::vector<device::BluetoothUUID> uuids;
  const std::vector<std::string>& dbus_uuids = properties->uuids.value();
  for (std::vector<std::string>::const_iterator iter = dbus_uuids.begin();
       iter != dbus_uuids.end(); ++iter) {
    device::BluetoothUUID uuid(*iter);
    DCHECK(uuid.IsValid());
    uuids.push_back(uuid);
  }
  return uuids;
}

int16_t BluetoothDeviceBlueZ::GetInquiryRSSI() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->rssi.is_valid())
    return kUnknownPower;

  return properties->rssi.value();
}

int16_t BluetoothDeviceBlueZ::GetInquiryTxPower() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->tx_power.is_valid())
    return kUnknownPower;

  return properties->tx_power.value();
}

bool BluetoothDeviceBlueZ::ExpectingPinCode() const {
  return pairing_.get() && pairing_->ExpectingPinCode();
}

bool BluetoothDeviceBlueZ::ExpectingPasskey() const {
  return pairing_.get() && pairing_->ExpectingPasskey();
}

bool BluetoothDeviceBlueZ::ExpectingConfirmation() const {
  return pairing_.get() && pairing_->ExpectingConfirmation();
}

void BluetoothDeviceBlueZ::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  // DBus method call should gracefully return an error if the device is not
  // currently connected.
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetConnInfo(
      object_path_, base::Bind(&BluetoothDeviceBlueZ::OnGetConnInfo,
                               weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnGetConnInfoError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BluetoothDeviceBlueZ::Connect(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  if (num_connecting_calls_++ == 0)
    adapter()->NotifyDeviceChanged(this);

  VLOG(1) << object_path_.value() << ": Connecting, " << num_connecting_calls_
          << " in progress";

  if (IsPaired() || !pairing_delegate || !IsPairable()) {
    // No need to pair, or unable to, skip straight to connection.
    ConnectInternal(false, callback, error_callback);
  } else {
    // Initiate high-security connection with pairing.
    BeginPairing(pairing_delegate);

    bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Pair(
        object_path_,
        base::Bind(&BluetoothDeviceBlueZ::OnPairDuringConnect,
                   weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
        base::Bind(&BluetoothDeviceBlueZ::OnPairDuringConnectError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }
}

void BluetoothDeviceBlueZ::Pair(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  DCHECK(pairing_delegate);
  BeginPairing(pairing_delegate);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Pair(
      object_path_, base::Bind(&BluetoothDeviceBlueZ::OnPair,
                               weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnPairError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::SetPinCode(const std::string& pincode) {
  if (!pairing_.get())
    return;

  pairing_->SetPinCode(pincode);
}

void BluetoothDeviceBlueZ::SetPasskey(uint32_t passkey) {
  if (!pairing_.get())
    return;

  pairing_->SetPasskey(passkey);
}

void BluetoothDeviceBlueZ::ConfirmPairing() {
  if (!pairing_.get())
    return;

  pairing_->ConfirmPairing();
}

void BluetoothDeviceBlueZ::RejectPairing() {
  if (!pairing_.get())
    return;

  pairing_->RejectPairing();
}

void BluetoothDeviceBlueZ::CancelPairing() {
  bool canceled = false;

  // If there is a callback in progress that we can reply to then use that
  // to cancel the current pairing request.
  if (pairing_.get() && pairing_->CancelPairing())
    canceled = true;

  // If not we have to send an explicit CancelPairing() to the device instead.
  if (!canceled) {
    VLOG(1) << object_path_.value() << ": No pairing context or callback. "
            << "Sending explicit cancel";
    bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->CancelPairing(
        object_path_, base::Bind(&base::DoNothing),
        base::Bind(&BluetoothDeviceBlueZ::OnCancelPairingError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Since there is no callback to this method it's possible that the pairing
  // delegate is going to be freed before things complete (indeed it's
  // documented that this is the method you should call while freeing the
  // pairing delegate), so clear our the context holding on to it.
  EndPairing();
}

void BluetoothDeviceBlueZ::Disconnect(const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Disconnecting";
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Disconnect(
      object_path_, base::Bind(&BluetoothDeviceBlueZ::OnDisconnect,
                               weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnDisconnectError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::Forget(const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Removing device";
  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->RemoveDevice(
      adapter()->object_path(), object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnForgetError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  VLOG(1) << object_path_.value()
          << ": Connecting to service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Connect(this, uuid, BluetoothSocketBlueZ::SECURITY_LEVEL_MEDIUM,
                  base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceBlueZ::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  VLOG(1) << object_path_.value()
          << ": Connecting insecurely to service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Connect(this, uuid, BluetoothSocketBlueZ::SECURITY_LEVEL_LOW,
                  base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceBlueZ::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  // TODO(sacomoto): Workaround to retrieve the connection for already connected
  // devices. Currently, BluetoothGattConnection::Disconnect doesn't do
  // anything, the unique underlying physical GATT connection is kept. This
  // should be removed once the correct behavour is implemented and the GATT
  // connections are reference counted (see todo below).
  if (IsConnected()) {
    OnCreateGattConnection(callback);
    return;
  }

  // TODO(armansito): Until there is a way to create a reference counted GATT
  // connection in bluetoothd, simply do a regular connect.
  Connect(NULL, base::Bind(&BluetoothDeviceBlueZ::OnCreateGattConnection,
                           weak_ptr_factory_.GetWeakPtr(), callback),
          error_callback);
}

void BluetoothDeviceBlueZ::GetServiceRecords(
    const GetServiceRecordsCallback& callback,
    const GetServiceRecordsErrorCallback& error_callback) {
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetServiceRecords(
      object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnGetServiceRecordsError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

BluetoothPairingBlueZ* BluetoothDeviceBlueZ::BeginPairing(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  pairing_.reset(new BluetoothPairingBlueZ(this, pairing_delegate));
  return pairing_.get();
}

void BluetoothDeviceBlueZ::EndPairing() {
  pairing_.reset();
}

BluetoothPairingBlueZ* BluetoothDeviceBlueZ::GetPairing() const {
  return pairing_.get();
}

BluetoothAdapterBlueZ* BluetoothDeviceBlueZ::adapter() const {
  return static_cast<BluetoothAdapterBlueZ*>(adapter_);
}

void BluetoothDeviceBlueZ::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path);
  DCHECK(properties);

  if (property_name == properties->services_resolved.name() &&
      properties->services_resolved.value()) {
    VLOG(3) << "All services were discovered for device: "
            << object_path.value();

    for (const auto iter : newly_discovered_gatt_services_) {
      adapter()->NotifyGattDiscoveryComplete(
          static_cast<BluetoothRemoteGattService*>(iter));
    }
    newly_discovered_gatt_services_.clear();
  }
}

void BluetoothDeviceBlueZ::GattServiceAdded(
    const dbus::ObjectPath& object_path) {
  if (GetGattService(object_path.value())) {
    VLOG(1) << "Remote GATT service already exists: " << object_path.value();
    return;
  }

  bluez::BluetoothGattServiceClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetProperties(object_path);
  DCHECK(properties);
  if (properties->device.value() != object_path_) {
    VLOG(2) << "Remote GATT service does not belong to this device.";
    return;
  }

  VLOG(1) << "Adding new remote GATT service for device: " << GetAddress();

  BluetoothRemoteGattServiceBlueZ* service =
      new BluetoothRemoteGattServiceBlueZ(adapter(), this, object_path);

  newly_discovered_gatt_services_.push_back(
      static_cast<BluetoothRemoteGattServiceBlueZ*>(service));

  gatt_services_.set(service->GetIdentifier(),
                     std::unique_ptr<BluetoothRemoteGattService>(service));
  DCHECK(service->object_path() == object_path);
  DCHECK(service->GetUUID().IsValid());

  DCHECK(adapter());
  adapter()->NotifyGattServiceAdded(service);
}

void BluetoothDeviceBlueZ::GattServiceRemoved(
    const dbus::ObjectPath& object_path) {
  GattServiceMap::const_iterator iter =
      gatt_services_.find(object_path.value());
  if (iter == gatt_services_.end()) {
    VLOG(3) << "Unknown GATT service removed: " << object_path.value();
    return;
  }

  BluetoothRemoteGattServiceBlueZ* service =
      static_cast<BluetoothRemoteGattServiceBlueZ*>(iter->second);

  VLOG(1) << "Removing remote GATT service with UUID: '"
          << service->GetUUID().canonical_value()
          << "' from device: " << GetAddress();

  DCHECK(service->object_path() == object_path);
  std::unique_ptr<BluetoothRemoteGattService> scoped_service =
      gatt_services_.take_and_erase(iter->first);

  DCHECK(adapter());
  adapter()->NotifyGattServiceRemoved(service);
}

void BluetoothDeviceBlueZ::InitializeGattServiceMap() {
  DCHECK(gatt_services_.empty());

  if (!IsGattServicesDiscoveryComplete()) {
    VLOG(2) << "Gatt services have not been fully resolved for device "
            << object_path_.value();
    return;
  }

  VLOG(3) << "Initializing the list of GATT services associated with device "
          << object_path_.value();

  // Add all known GATT services associated with the device.
  const std::vector<dbus::ObjectPath> gatt_services =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetServices();
  for (const auto& it : gatt_services)
    GattServiceAdded(it);

  // Notify on the discovery complete for each service which is found in the
  // first discovery.
  DCHECK(adapter());
  for (const auto& iter : gatt_services_)
    adapter()->NotifyGattDiscoveryComplete(iter.second);
}

void BluetoothDeviceBlueZ::OnGetConnInfo(const ConnectionInfoCallback& callback,
                                         int16_t rssi,
                                         int16_t transmit_power,
                                         int16_t max_transmit_power) {
  callback.Run(ConnectionInfo(rssi, transmit_power, max_transmit_power));
}

void BluetoothDeviceBlueZ::OnGetConnInfoError(
    const ConnectionInfoCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to get connection info: " << error_name << ": "
               << error_message;
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceBlueZ::OnGetServiceRecordsError(
    const GetServiceRecordsErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << object_path_.value()
          << ": Failed to get service records: " << error_name << ": "
          << error_message;
  BluetoothServiceRecordBlueZ::ErrorCode code =
      BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN;
  if (error_name == bluetooth_device::kErrorNotConnected) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_DEVICE_DISCONNECTED;
  }
  error_callback.Run(code);
}

void BluetoothDeviceBlueZ::ConnectInternal(
    bool after_pairing,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Connecting";
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Connect(
      object_path_,
      base::Bind(&BluetoothDeviceBlueZ::OnConnect,
                 weak_ptr_factory_.GetWeakPtr(), after_pairing, callback),
      base::Bind(&BluetoothDeviceBlueZ::OnConnectError,
                 weak_ptr_factory_.GetWeakPtr(), after_pairing,
                 error_callback));
}

void BluetoothDeviceBlueZ::OnConnect(bool after_pairing,
                                     const base::Closure& callback) {
  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  VLOG(1) << object_path_.value() << ": Connected, " << num_connecting_calls_
          << " still in progress";

  SetTrusted();

  if (after_pairing)
    UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingResult",
                              UMA_PAIRING_RESULT_SUCCESS,
                              UMA_PAIRING_RESULT_COUNT);

  callback.Run();
}

void BluetoothDeviceBlueZ::OnCreateGattConnection(
    const GattConnectionCallback& callback) {
  std::unique_ptr<device::BluetoothGattConnection> conn(
      new BluetoothGattConnectionBlueZ(adapter_, GetAddress(), object_path_));
  callback.Run(std::move(conn));
}

void BluetoothDeviceBlueZ::OnConnectError(
    bool after_pairing,
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value()
               << ": Failed to connect device: " << error_name << ": "
               << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_device::kErrorFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorInProgress) {
    error_code = ERROR_INPROGRESS;
  } else if (error_name == bluetooth_device::kErrorNotSupported) {
    error_code = ERROR_UNSUPPORTED_DEVICE;
  }

  if (after_pairing)
    RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnPairDuringConnect(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Paired";

  EndPairing();

  ConnectInternal(true, callback, error_callback);
}

void BluetoothDeviceBlueZ::OnPairDuringConnectError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value()
               << ": Failed to pair device: " << error_name << ": "
               << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  EndPairing();

  // Determine the error code from error_name.
  ConnectErrorCode error_code = DBusErrorToConnectError(error_name);

  RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnPair(const base::Closure& callback) {
  VLOG(1) << object_path_.value() << ": Paired";
  EndPairing();
  callback.Run();
}

void BluetoothDeviceBlueZ::OnPairError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to pair device: " << error_name << ": "
               << error_message;
  EndPairing();
  ConnectErrorCode error_code = DBusErrorToConnectError(error_name);
  RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnCancelPairingError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to cancel pairing: " << error_name << ": "
               << error_message;
}

void BluetoothDeviceBlueZ::SetTrusted() {
  // Unconditionally send the property change, rather than checking the value
  // first; there's no harm in doing this and it solves any race conditions
  // with the property becoming true or false and this call happening before
  // we get the D-Bus signal about the earlier change.
  bluez::BluezDBusManager::Get()
      ->GetBluetoothDeviceClient()
      ->GetProperties(object_path_)
      ->trusted.Set(true, base::Bind(&BluetoothDeviceBlueZ::OnSetTrusted,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceBlueZ::OnSetTrusted(bool success) {
  LOG_IF(WARNING, !success) << object_path_.value()
                            << ": Failed to set device as trusted";
}

void BluetoothDeviceBlueZ::OnDisconnect(const base::Closure& callback) {
  VLOG(1) << object_path_.value() << ": Disconnected";
  callback.Run();
}

void BluetoothDeviceBlueZ::OnDisconnectError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to disconnect device: " << error_name << ": "
               << error_message;
  error_callback.Run();
}

void BluetoothDeviceBlueZ::OnForgetError(const ErrorCallback& error_callback,
                                         const std::string& error_name,
                                         const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to remove device: " << error_name << ": "
               << error_message;
  error_callback.Run();
}

}  // namespace bluez
