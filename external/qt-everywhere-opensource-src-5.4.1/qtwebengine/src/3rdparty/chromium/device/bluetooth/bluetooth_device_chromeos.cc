// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_chromeos.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_gatt_connection_chromeos.h"
#include "device/bluetooth/bluetooth_pairing_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothDevice;
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
                   uint16* vendor_id,
                   uint16* product_id,
                   uint16* device_id) {
  chromeos::BluetoothDeviceClient::Properties* properties =
      chromeos::DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path);
  DCHECK(properties);

  std::string modalias = properties->modalias.value();
  BluetoothDevice::VendorIDSource source_value;
  int vendor_value, product_value, device_value;

  if (sscanf(modalias.c_str(), "bluetooth:v%04xp%04xd%04x",
             &vendor_value, &product_value, &device_value) == 3) {
    source_value = BluetoothDevice::VENDOR_ID_BLUETOOTH;
  } else if (sscanf(modalias.c_str(), "usb:v%04xp%04xd%04x",
                    &vendor_value, &product_value, &device_value) == 3) {
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

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingResult",
                            pairing_result,
                            UMA_PAIRING_RESULT_COUNT);
}

}  // namespace

namespace chromeos {

BluetoothDeviceChromeOS::BluetoothDeviceChromeOS(
    BluetoothAdapterChromeOS* adapter,
    const dbus::ObjectPath& object_path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread)
    : adapter_(adapter),
      object_path_(object_path),
      num_connecting_calls_(0),
      connection_monitor_started_(false),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      weak_ptr_factory_(this) {
  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->AddObserver(this);

  // Add all known GATT services.
  const std::vector<dbus::ObjectPath> gatt_services =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->GetServices();
  for (std::vector<dbus::ObjectPath>::const_iterator it = gatt_services.begin();
       it != gatt_services.end(); ++it) {
    GattServiceAdded(*it);
  }
}

BluetoothDeviceChromeOS::~BluetoothDeviceChromeOS() {
  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
      RemoveObserver(this);

  // Copy the GATT services list here and clear the original so that when we
  // send GattServiceRemoved(), GetGattServices() returns no services.
  GattServiceMap gatt_services = gatt_services_;
  gatt_services_.clear();
  for (GattServiceMap::iterator iter = gatt_services.begin();
       iter != gatt_services.end(); ++iter) {
    FOR_EACH_OBSERVER(BluetoothDevice::Observer, observers_,
                      GattServiceRemoved(this, iter->second));
    delete iter->second;
  }
}

void BluetoothDeviceChromeOS::AddObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothDeviceChromeOS::RemoveObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

uint32 BluetoothDeviceChromeOS::GetBluetoothClass() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->bluetooth_class.value();
}

std::string BluetoothDeviceChromeOS::GetDeviceName() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

std::string BluetoothDeviceChromeOS::GetAddress() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return CanonicalizeAddress(properties->address.value());
}

BluetoothDevice::VendorIDSource
BluetoothDeviceChromeOS::GetVendorIDSource() const {
  VendorIDSource vendor_id_source = VENDOR_ID_UNKNOWN;
  ParseModalias(object_path_, &vendor_id_source, NULL, NULL, NULL);
  return vendor_id_source;
}

uint16 BluetoothDeviceChromeOS::GetVendorID() const {
  uint16 vendor_id  = 0;
  ParseModalias(object_path_, NULL, &vendor_id, NULL, NULL);
  return vendor_id;
}

uint16 BluetoothDeviceChromeOS::GetProductID() const {
  uint16 product_id  = 0;
  ParseModalias(object_path_, NULL, NULL, &product_id, NULL);
  return product_id;
}

uint16 BluetoothDeviceChromeOS::GetDeviceID() const {
  uint16 device_id  = 0;
  ParseModalias(object_path_, NULL, NULL, NULL, &device_id);
  return device_id;
}

int BluetoothDeviceChromeOS::GetRSSI() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!IsConnected()) {
    NOTIMPLEMENTED();
    return kUnknownPower;
  }

  return connection_monitor_started_ ? properties->connection_rssi.value()
                                     : kUnknownPower;
}

int BluetoothDeviceChromeOS::GetCurrentHostTransmitPower() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return IsConnected() && connection_monitor_started_
             ? properties->connection_tx_power.value()
             : kUnknownPower;
}

int BluetoothDeviceChromeOS::GetMaximumHostTransmitPower() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return IsConnected() ? properties->connection_tx_power_max.value()
                       : kUnknownPower;
}

bool BluetoothDeviceChromeOS::IsPaired() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  // Trusted devices are devices that don't support pairing but that the
  // user has explicitly connected; it makes no sense for UI purposes to
  // treat them differently from each other.
  return properties->paired.value() || properties->trusted.value();
}

bool BluetoothDeviceChromeOS::IsConnected() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->connected.value();
}

bool BluetoothDeviceChromeOS::IsConnectable() const {
  BluetoothInputClient::Properties* input_properties =
      DBusThreadManager::Get()->GetBluetoothInputClient()->
          GetProperties(object_path_);
  // GetProperties returns NULL when the device does not implement the given
  // interface. Non HID devices are normally connectable.
  if (!input_properties)
    return true;

  return input_properties->reconnect_mode.value() != "device";
}

bool BluetoothDeviceChromeOS::IsConnecting() const {
  return num_connecting_calls_ > 0;
}

BluetoothDeviceChromeOS::UUIDList BluetoothDeviceChromeOS::GetUUIDs() const {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  std::vector<device::BluetoothUUID> uuids;
  const std::vector<std::string> &dbus_uuids = properties->uuids.value();
  for (std::vector<std::string>::const_iterator iter = dbus_uuids.begin();
       iter != dbus_uuids.end(); ++iter) {
    device::BluetoothUUID uuid(*iter);
    DCHECK(uuid.IsValid());
    uuids.push_back(uuid);
  }
  return uuids;
}

bool BluetoothDeviceChromeOS::ExpectingPinCode() const {
  return pairing_.get() && pairing_->ExpectingPinCode();
}

bool BluetoothDeviceChromeOS::ExpectingPasskey() const {
  return pairing_.get() && pairing_->ExpectingPasskey();
}

bool BluetoothDeviceChromeOS::ExpectingConfirmation() const {
  return pairing_.get() && pairing_->ExpectingConfirmation();
}

void BluetoothDeviceChromeOS::Connect(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  if (num_connecting_calls_++ == 0)
    adapter_->NotifyDeviceChanged(this);

  VLOG(1) << object_path_.value() << ": Connecting, " << num_connecting_calls_
          << " in progress";

  if (IsPaired() || !pairing_delegate || !IsPairable()) {
    // No need to pair, or unable to, skip straight to connection.
    ConnectInternal(false, callback, error_callback);
  } else {
    // Initiate high-security connection with pairing.
    BeginPairing(pairing_delegate);

    DBusThreadManager::Get()->GetBluetoothDeviceClient()->
        Pair(object_path_,
             base::Bind(&BluetoothDeviceChromeOS::OnPair,
                        weak_ptr_factory_.GetWeakPtr(),
                        callback, error_callback),
             base::Bind(&BluetoothDeviceChromeOS::OnPairError,
                        weak_ptr_factory_.GetWeakPtr(),
                        error_callback));
  }
}

void BluetoothDeviceChromeOS::SetPinCode(const std::string& pincode) {
  if (!pairing_.get())
    return;

  pairing_->SetPinCode(pincode);
}

void BluetoothDeviceChromeOS::SetPasskey(uint32 passkey) {
  if (!pairing_.get())
    return;

  pairing_->SetPasskey(passkey);
}

void BluetoothDeviceChromeOS::ConfirmPairing() {
  if (!pairing_.get())
    return;

  pairing_->ConfirmPairing();
}

void BluetoothDeviceChromeOS::RejectPairing() {
  if (!pairing_.get())
    return;

  pairing_->RejectPairing();
}

void BluetoothDeviceChromeOS::CancelPairing() {
  bool canceled = false;

  // If there is a callback in progress that we can reply to then use that
  // to cancel the current pairing request.
  if (pairing_.get() && pairing_->CancelPairing())
    canceled = true;

  // If not we have to send an explicit CancelPairing() to the device instead.
  if (!canceled) {
    VLOG(1) << object_path_.value() << ": No pairing context or callback. "
            << "Sending explicit cancel";
    DBusThreadManager::Get()->GetBluetoothDeviceClient()->
        CancelPairing(
            object_path_,
            base::Bind(&base::DoNothing),
            base::Bind(&BluetoothDeviceChromeOS::OnCancelPairingError,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Since there is no callback to this method it's possible that the pairing
  // delegate is going to be freed before things complete (indeed it's
  // documented that this is the method you should call while freeing the
  // pairing delegate), so clear our the context holding on to it.
  EndPairing();
}

void BluetoothDeviceChromeOS::Disconnect(const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Disconnecting";
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      Disconnect(
          object_path_,
          base::Bind(&BluetoothDeviceChromeOS::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          base::Bind(&BluetoothDeviceChromeOS::OnDisconnectError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
}

void BluetoothDeviceChromeOS::Forget(const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Removing device";
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      RemoveDevice(
          adapter_->object_path(),
          object_path_,
          base::Bind(&base::DoNothing),
          base::Bind(&BluetoothDeviceChromeOS::OnForgetError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
}

void BluetoothDeviceChromeOS::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Connecting to service: "
          << uuid.canonical_value();
  scoped_refptr<BluetoothSocketChromeOS> socket =
      BluetoothSocketChromeOS::CreateBluetoothSocket(
          ui_task_runner_,
          socket_thread_,
          NULL,
          net::NetLog::Source());
  socket->Connect(this, uuid,
                  base::Bind(callback, socket),
                  error_callback);
}

void BluetoothDeviceChromeOS::CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) {
  // TODO(armansito): Until there is a way to create a reference counted GATT
  // connection in bluetoothd, simply do a regular connect.
  Connect(NULL,
          base::Bind(&BluetoothDeviceChromeOS::OnCreateGattConnection,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          error_callback);
}

void BluetoothDeviceChromeOS::StartConnectionMonitor(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->StartConnectionMonitor(
      object_path_,
      base::Bind(&BluetoothDeviceChromeOS::OnStartConnectionMonitor,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothDeviceChromeOS::OnStartConnectionMonitorError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

BluetoothPairingChromeOS* BluetoothDeviceChromeOS::BeginPairing(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  pairing_.reset(new BluetoothPairingChromeOS(this, pairing_delegate));
  return pairing_.get();
}

void BluetoothDeviceChromeOS::EndPairing() {
  pairing_.reset();
}

BluetoothPairingChromeOS* BluetoothDeviceChromeOS::GetPairing() const {
  return pairing_.get();
}

void BluetoothDeviceChromeOS::GattServiceAdded(
    const dbus::ObjectPath& object_path) {
  if (GetGattService(object_path.value())) {
    VLOG(1) << "Remote GATT service already exists: " << object_path.value();
    return;
  }

  BluetoothGattServiceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
          GetProperties(object_path);
  DCHECK(properties);
  if (properties->device.value() != object_path_) {
    VLOG(2) << "Remote GATT service does not belong to this device.";
    return;
  }

  VLOG(1) << "Adding new remote GATT service for device: " << GetAddress();

  BluetoothRemoteGattServiceChromeOS* service =
      new BluetoothRemoteGattServiceChromeOS(adapter_, this, object_path);

  gatt_services_[service->GetIdentifier()] = service;
  DCHECK(service->object_path() == object_path);
  DCHECK(service->GetUUID().IsValid());

  FOR_EACH_OBSERVER(device::BluetoothDevice::Observer, observers_,
                    GattServiceAdded(this, service));
}

void BluetoothDeviceChromeOS::GattServiceRemoved(
    const dbus::ObjectPath& object_path) {
  GattServiceMap::iterator iter = gatt_services_.find(object_path.value());
  if (iter == gatt_services_.end()) {
    VLOG(2) << "Unknown GATT service removed: " << object_path.value();
    return;
  }

  VLOG(1) << "Removing remote GATT service from device: " << GetAddress();

  BluetoothRemoteGattServiceChromeOS* service =
      static_cast<BluetoothRemoteGattServiceChromeOS*>(iter->second);
  DCHECK(service->object_path() == object_path);
  gatt_services_.erase(iter);
  FOR_EACH_OBSERVER(device::BluetoothDevice::Observer, observers_,
                    GattServiceRemoved(this, service));
  delete service;
}

void BluetoothDeviceChromeOS::ConnectInternal(
    bool after_pairing,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Connecting";
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      Connect(
          object_path_,
          base::Bind(&BluetoothDeviceChromeOS::OnConnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     after_pairing,
                     callback),
          base::Bind(&BluetoothDeviceChromeOS::OnConnectError,
                     weak_ptr_factory_.GetWeakPtr(),
                     after_pairing,
                     error_callback));
}

void BluetoothDeviceChromeOS::OnConnect(bool after_pairing,
                                        const base::Closure& callback) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

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

void BluetoothDeviceChromeOS::OnCreateGattConnection(
    const GattConnectionCallback& callback) {
  scoped_ptr<device::BluetoothGattConnection> conn(
      new BluetoothGattConnectionChromeOS(
          adapter_, GetAddress(), object_path_));
  callback.Run(conn.Pass());
}

void BluetoothDeviceChromeOS::OnConnectError(
    bool after_pairing,
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value() << ": Failed to connect device: "
               << error_name << ": " << error_message;
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

void BluetoothDeviceChromeOS::OnPair(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Paired";

  EndPairing();

  ConnectInternal(true, callback, error_callback);
}

void BluetoothDeviceChromeOS::OnPairError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value() << ": Failed to pair device: "
               << error_name << ": " << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  EndPairing();

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_device::kErrorConnectionAttemptFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationFailed) {
    error_code = ERROR_AUTH_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationCanceled) {
    error_code = ERROR_AUTH_CANCELED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationRejected) {
    error_code = ERROR_AUTH_REJECTED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationTimeout) {
    error_code = ERROR_AUTH_TIMEOUT;
  }

  RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceChromeOS::OnCancelPairingError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to cancel pairing: "
               << error_name << ": " << error_message;
}

void BluetoothDeviceChromeOS::SetTrusted() {
  // Unconditionally send the property change, rather than checking the value
  // first; there's no harm in doing this and it solves any race conditions
  // with the property becoming true or false and this call happening before
  // we get the D-Bus signal about the earlier change.
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      GetProperties(object_path_)->trusted.Set(
          true,
          base::Bind(&BluetoothDeviceChromeOS::OnSetTrusted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceChromeOS::OnSetTrusted(bool success) {
  LOG_IF(WARNING, !success) << object_path_.value()
                            << ": Failed to set device as trusted";
}

void BluetoothDeviceChromeOS::OnStartConnectionMonitor(
    const base::Closure& callback) {
  connection_monitor_started_ = true;
  callback.Run();
}

void BluetoothDeviceChromeOS::OnStartConnectionMonitorError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to start connection monitor: " << error_name << ": "
               << error_message;
  error_callback.Run();
}

void BluetoothDeviceChromeOS::OnDisconnect(const base::Closure& callback) {
  VLOG(1) << object_path_.value() << ": Disconnected";
  callback.Run();
}

void BluetoothDeviceChromeOS::OnDisconnectError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to disconnect device: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothDeviceChromeOS::OnForgetError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to remove device: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

}  // namespace chromeos
