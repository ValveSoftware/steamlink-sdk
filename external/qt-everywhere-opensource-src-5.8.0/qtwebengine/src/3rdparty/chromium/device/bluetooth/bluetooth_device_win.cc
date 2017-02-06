// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <memory>
#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

const char kApiUnavailable[] = "This API is not implemented on this platform.";

}  // namespace

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin(
    BluetoothAdapterWin* adapter,
    const BluetoothTaskManagerWin::DeviceState& device_state,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<BluetoothSocketThread>& socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& net_log_source)
    : BluetoothDevice(adapter),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      net_log_(net_log),
      net_log_source_(net_log_source) {
  Update(device_state);
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
  // Explicitly take and erase GATT services one by one to ensure that calling
  // GetGattService on removed service in GattServiceRemoved returns null.
  std::vector<std::string> service_keys;
  for (const auto& gatt_service : gatt_services_) {
    service_keys.push_back(gatt_service.first);
  }
  for (const auto& key : service_keys) {
    gatt_services_.take_and_erase(key);
  }
}

uint32_t BluetoothDeviceWin::GetBluetoothClass() const {
  return bluetooth_class_;
}

std::string BluetoothDeviceWin::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource
BluetoothDeviceWin::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceWin::GetVendorID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetProductID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetAppearance() const {
  // TODO(crbug.com/588083): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

bool BluetoothDeviceWin::IsPaired() const {
  return paired_;
}

bool BluetoothDeviceWin::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceWin::IsGattConnected() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::IsConnectable() const {
  return false;
}

bool BluetoothDeviceWin::IsConnecting() const {
  return false;
}

BluetoothDevice::UUIDList BluetoothDeviceWin::GetUUIDs() const {
  return uuids_;
}

int16_t BluetoothDeviceWin::GetInquiryRSSI() const {
  return kUnknownPower;
}

int16_t BluetoothDeviceWin::GetInquiryTxPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothDeviceWin::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceWin::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(const base::Closure& callback,
                                const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_));
  socket->Connect(this, uuid, base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceWin::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  error_callback.Run(kApiUnavailable);
}

void BluetoothDeviceWin::CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) {
  // TODO(armansito): Implement.
  error_callback.Run(ERROR_UNSUPPORTED_DEVICE);
}

const BluetoothServiceRecordWin* BluetoothDeviceWin::GetServiceRecord(
    const device::BluetoothUUID& uuid) const {
  for (ServiceRecordList::const_iterator iter = service_record_list_.begin();
       iter != service_record_list_.end();
       ++iter) {
    if ((*iter)->uuid() == uuid)
      return *iter;
  }
  return NULL;
}

bool BluetoothDeviceWin::IsEqual(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  if (address_ != device_state.address || name_ != device_state.name ||
      bluetooth_class_ != device_state.bluetooth_class ||
      visible_ != device_state.visible ||
      connected_ != device_state.connected ||
      paired_ != device_state.authenticated) {
    return false;
  }

  // Checks service collection
  typedef std::set<BluetoothUUID> UUIDSet;
  typedef base::ScopedPtrHashMap<std::string,
                                 std::unique_ptr<BluetoothServiceRecordWin>>
      ServiceRecordMap;

  UUIDSet known_services;
  for (UUIDList::const_iterator iter = uuids_.begin(); iter != uuids_.end();
       ++iter) {
    known_services.insert((*iter));
  }

  UUIDSet new_services;
  ServiceRecordMap new_service_records;
  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
           iter = device_state.service_record_states.begin();
       iter != device_state.service_record_states.end(); ++iter) {
    BluetoothServiceRecordWin* service_record = new BluetoothServiceRecordWin(
        address_, (*iter)->name, (*iter)->sdp_bytes, (*iter)->gatt_uuid);
    new_services.insert(service_record->uuid());
    new_service_records.set(
        service_record->uuid().canonical_value(),
        std::unique_ptr<BluetoothServiceRecordWin>(service_record));
  }

  UUIDSet removed_services =
      base::STLSetDifference<UUIDSet>(known_services, new_services);
  if (!removed_services.empty()) {
    return false;
  }
  UUIDSet added_devices =
      base::STLSetDifference<UUIDSet>(new_services, known_services);
  if (!added_devices.empty()) {
    return false;
  }

  for (ServiceRecordList::const_iterator iter = service_record_list_.begin();
       iter != service_record_list_.end(); ++iter) {
    BluetoothServiceRecordWin* service_record = (*iter);
    BluetoothServiceRecordWin* new_service_record =
        new_service_records.get((*iter)->uuid().canonical_value());
    if (!service_record->IsEqual(*new_service_record))
      return false;
  }
  return true;
}

void BluetoothDeviceWin::Update(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  address_ = device_state.address;
  // Note: Callers are responsible for providing a canonicalized address.
  DCHECK_EQ(address_, BluetoothDevice::CanonicalizeAddress(address_));
  name_ = device_state.name;
  bluetooth_class_ = device_state.bluetooth_class;
  visible_ = device_state.visible;
  connected_ = device_state.connected;
  paired_ = device_state.authenticated;
  UpdateServices(device_state);
}

std::string BluetoothDeviceWin::GetDeviceName() const {
  return name_;
}

void BluetoothDeviceWin::CreateGattConnectionImpl() {
  // Windows implementation does not use the default CreateGattConnection
  // implementation.
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::DisconnectGatt() {
  // Windows implementation does not use the default CreateGattConnection
  // implementation.
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetVisible(bool visible) {
  visible_ = visible;
}

void BluetoothDeviceWin::UpdateServices(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  uuids_.clear();
  service_record_list_.clear();

  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
           iter = device_state.service_record_states.begin();
       iter != device_state.service_record_states.end(); ++iter) {
    BluetoothServiceRecordWin* service_record =
        new BluetoothServiceRecordWin(device_state.address, (*iter)->name,
                                      (*iter)->sdp_bytes, (*iter)->gatt_uuid);
    service_record_list_.push_back(service_record);
    uuids_.push_back(service_record->uuid());
  }

  if (!device_state.is_bluetooth_classic())
    UpdateGattServices(device_state.service_record_states);
}

bool BluetoothDeviceWin::IsGattServiceDiscovered(BluetoothUUID& uuid,
                                                 uint16_t attribute_handle) {
  GattServiceMap::iterator it = gatt_services_.begin();
  for (; it != gatt_services_.end(); it++) {
    uint16_t it_att_handle =
        static_cast<BluetoothRemoteGattServiceWin*>(it->second)
            ->GetAttributeHandle();
    BluetoothUUID it_uuid = it->second->GetUUID();
    if (attribute_handle == it_att_handle && uuid == it_uuid) {
      return true;
    }
  }
  return false;
}

bool BluetoothDeviceWin::DoesGattServiceExist(
    const ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>&
        service_state,
    BluetoothRemoteGattService* service) {
  uint16_t attribute_handle =
      static_cast<BluetoothRemoteGattServiceWin*>(service)
          ->GetAttributeHandle();
  BluetoothUUID uuid = service->GetUUID();
  ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator it =
      service_state.begin();
  for (; it != service_state.end(); ++it) {
    if (attribute_handle == (*it)->attribute_handle && uuid == (*it)->gatt_uuid)
      return true;
  }
  return false;
}

void BluetoothDeviceWin::UpdateGattServices(
    const ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>&
        service_state) {
  // First, remove no longer exist GATT service.
  {
    std::vector<std::string> to_be_removed_services;
    for (const auto& gatt_service : gatt_services_) {
      if (!DoesGattServiceExist(service_state, gatt_service.second)) {
        to_be_removed_services.push_back(gatt_service.first);
      }
    }
    for (const auto& service : to_be_removed_services) {
      gatt_services_.take_and_erase(service);
    }
    // Update previously discovered services.
    for (auto gatt_service : gatt_services_) {
      static_cast<BluetoothRemoteGattServiceWin*>(gatt_service.second)
          ->Update();
    }
  }

  // Return if no new services have been added.
  if (gatt_services_.size() == service_state.size())
    return;

  // Add new services.
  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
           it = service_state.begin();
       it != service_state.end(); ++it) {
    if (!IsGattServiceDiscovered((*it)->gatt_uuid, (*it)->attribute_handle)) {
      BluetoothRemoteGattServiceWin* primary_service =
          new BluetoothRemoteGattServiceWin(this, (*it)->path, (*it)->gatt_uuid,
                                            (*it)->attribute_handle, true,
                                            nullptr, ui_task_runner_);
      gatt_services_.add(
          primary_service->GetIdentifier(),
          std::unique_ptr<BluetoothRemoteGattService>(primary_service));
      adapter_->NotifyGattServiceAdded(primary_service);
    }
  }

  adapter_->NotifyGattServicesDiscovered(this);
}

}  // namespace device
