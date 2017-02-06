// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_peripheral_delegate.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"

using device::BluetoothDevice;
using device::BluetoothLowEnergyDeviceMac;

BluetoothLowEnergyDeviceMac::BluetoothLowEnergyDeviceMac(
    BluetoothAdapterMac* adapter,
    CBPeripheral* peripheral,
    NSDictionary* advertisement_data,
    int rssi)
    : BluetoothDeviceMac(adapter),
      peripheral_(peripheral, base::scoped_policy::RETAIN) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  DCHECK(peripheral_.get());
  peripheral_delegate_.reset([[BluetoothLowEnergyPeripheralDelegate alloc]
      initWithBluetoothLowEnergyDeviceMac:this]);
  [peripheral_ setDelegate:peripheral_delegate_];
  identifier_ = GetPeripheralIdentifier(peripheral);
  hash_address_ = GetPeripheralHashAddress(peripheral);
  Update(advertisement_data, rssi);
}

BluetoothLowEnergyDeviceMac::~BluetoothLowEnergyDeviceMac() {
  if (IsGattConnected()) {
    GetMacAdapter()->DisconnectGatt(this);
  }
}

void BluetoothLowEnergyDeviceMac::Update(NSDictionary* advertisement_data,
                                         int rssi) {
  UpdateTimestamp();
  rssi_ = rssi;
  NSNumber* connectable =
      [advertisement_data objectForKey:CBAdvertisementDataIsConnectable];
  connectable_ = [connectable boolValue];
  ClearServiceData();
  NSDictionary* service_data =
      [advertisement_data objectForKey:CBAdvertisementDataServiceDataKey];
  for (CBUUID* uuid in service_data) {
    NSData* data = [service_data objectForKey:uuid];
    BluetoothUUID service_uuid =
        BluetoothAdapterMac::BluetoothUUIDWithCBUUID(uuid);
    SetServiceData(service_uuid, static_cast<const char*>([data bytes]),
                   [data length]);
  }
  NSArray* service_uuids =
      [advertisement_data objectForKey:CBAdvertisementDataServiceUUIDsKey];
  for (CBUUID* uuid in service_uuids) {
    advertised_uuids_.insert(
        BluetoothUUID(std::string([[uuid UUIDString] UTF8String])));
  }
  NSArray* overflow_service_uuids = [advertisement_data
      objectForKey:CBAdvertisementDataOverflowServiceUUIDsKey];
  for (CBUUID* uuid in overflow_service_uuids) {
    advertised_uuids_.insert(
        BluetoothUUID(std::string([[uuid UUIDString] UTF8String])));
  }
}

std::string BluetoothLowEnergyDeviceMac::GetIdentifier() const {
  return identifier_;
}

uint32_t BluetoothLowEnergyDeviceMac::GetBluetoothClass() const {
  return 0x1F00;  // Unspecified Device Class
}

std::string BluetoothLowEnergyDeviceMac::GetAddress() const {
  return hash_address_;
}

BluetoothDevice::VendorIDSource BluetoothLowEnergyDeviceMac::GetVendorIDSource()
    const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothLowEnergyDeviceMac::GetVendorID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetProductID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetAppearance() const {
  // TODO(crbug.com/588083): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

int BluetoothLowEnergyDeviceMac::GetRSSI() const {
  return rssi_;
}

bool BluetoothLowEnergyDeviceMac::IsPaired() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothLowEnergyDeviceMac::IsGattConnected() const {
  return ([peripheral_ state] == CBPeripheralStateConnected);
}

bool BluetoothLowEnergyDeviceMac::IsConnectable() const {
  return connectable_;
}

bool BluetoothLowEnergyDeviceMac::IsConnecting() const {
  return ([peripheral_ state] == CBPeripheralStateConnecting);
}

BluetoothDevice::UUIDList BluetoothLowEnergyDeviceMac::GetUUIDs() const {
  return BluetoothDevice::UUIDList(advertised_uuids_.begin(),
                                   advertised_uuids_.end());
}

int16_t BluetoothLowEnergyDeviceMac::GetInquiryRSSI() const {
  return kUnknownPower;
}

int16_t BluetoothLowEnergyDeviceMac::GetInquiryTxPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothLowEnergyDeviceMac::ExpectingPinCode() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingPasskey() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingConfirmation() const {
  return false;
}

void BluetoothLowEnergyDeviceMac::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Forget(const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

std::string BluetoothLowEnergyDeviceMac::GetDeviceName() const {
  return base::SysNSStringToUTF8([peripheral_ name]);
}

void BluetoothLowEnergyDeviceMac::CreateGattConnectionImpl() {
  if (!IsGattConnected()) {
    GetMacAdapter()->CreateGattConnection(this);
  }
}

void BluetoothLowEnergyDeviceMac::DisconnectGatt() {
  GetMacAdapter()->DisconnectGatt(this);
}

void BluetoothLowEnergyDeviceMac::DidDiscoverPrimaryServices(NSError* error) {
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    // a device services.
    VLOG(1) << "Can't discover primary services: "
            << error.localizedDescription.UTF8String << " (" << error.domain
            << ": " << error.code << ")";
    return;
  }
  for (CBService* cb_service in GetPeripheral().services) {
    BluetoothRemoteGattServiceMac* gatt_service =
        GetBluetoothRemoteGattService(cb_service);
    if (!gatt_service) {
      gatt_service = new BluetoothRemoteGattServiceMac(this, cb_service,
                                                       true /* is_primary */);
      auto result_iter = gatt_services_.add(gatt_service->GetIdentifier(),
                                            base::WrapUnique(gatt_service));
      DCHECK(result_iter.second);
      adapter_->NotifyGattServiceAdded(gatt_service);
    }
  }
  for (GattServiceMap::const_iterator it = gatt_services_.begin();
       it != gatt_services_.end(); ++it) {
    device::BluetoothRemoteGattService* gatt_service = it->second;
    device::BluetoothRemoteGattServiceMac* gatt_service_mac =
        static_cast<BluetoothRemoteGattServiceMac*>(gatt_service);
    gatt_service_mac->DiscoverCharacteristics();
  }
}

void BluetoothLowEnergyDeviceMac::DidDiscoverCharacteristics(
    CBService* cb_service,
    NSError* error) {
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    VLOG(1) << "Can't discover characteristics: "
            << error.localizedDescription.UTF8String << " (" << error.domain
            << ": " << error.code << ")";
    return;
  }
  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattService(cb_service);
  DCHECK(gatt_service);
  gatt_service->DidDiscoverCharacteristics();

  // Notify when all services have been discovered.
  bool discovery_complete =
      std::find_if_not(
          gatt_services_.begin(), gatt_services_.end(),
          [](std::pair<std::string, BluetoothRemoteGattService*> pair) {
            BluetoothRemoteGattService* gatt_service = pair.second;
            return static_cast<BluetoothRemoteGattServiceMac*>(gatt_service)
                ->IsDiscoveryComplete();
          }) == gatt_services_.end();
  if (discovery_complete) {
    SetGattServicesDiscoveryComplete(true);
    adapter_->NotifyGattServicesDiscovered(this);
  }
}

void BluetoothLowEnergyDeviceMac::DidModifyServices(
    NSArray* invalidatedServices) {
  for (CBService* cb_service in invalidatedServices) {
    BluetoothRemoteGattServiceMac* gatt_service =
        GetBluetoothRemoteGattService(cb_service);
    DCHECK(gatt_service);
    std::unique_ptr<BluetoothRemoteGattService> scoped_service =
        gatt_services_.take_and_erase(gatt_service->GetIdentifier());
    adapter_->NotifyGattServiceRemoved(scoped_service.get());
  }
  SetGattServicesDiscoveryComplete(false);
  [GetPeripheral() discoverServices:nil];
}

void BluetoothLowEnergyDeviceMac::DidUpdateValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattService(characteristic.service);
  DCHECK(gatt_service);
  gatt_service->DidUpdateValue(characteristic, error);
}

void BluetoothLowEnergyDeviceMac::DidWriteValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattService(characteristic.service);
  DCHECK(gatt_service);
  gatt_service->DidWriteValue(characteristic, error);
}

void BluetoothLowEnergyDeviceMac::DidUpdateNotificationState(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattService(characteristic.service);
  DCHECK(gatt_service);
  gatt_service->DidUpdateNotificationState(characteristic, error);
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(
    CBPeripheral* peripheral) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  NSUUID* uuid = [peripheral identifier];
  NSString* uuidString = [uuid UUIDString];
  return base::SysNSStringToUTF8(uuidString);
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(
    CBPeripheral* peripheral) {
  const size_t kCanonicalAddressNumberOfBytes = 6;
  char raw[kCanonicalAddressNumberOfBytes];
  crypto::SHA256HashString(GetPeripheralIdentifier(peripheral), raw,
                           sizeof(raw));
  std::string hash = base::HexEncode(raw, sizeof(raw));
  return BluetoothDevice::CanonicalizeAddress(hash);
}

device::BluetoothAdapterMac* BluetoothLowEnergyDeviceMac::GetMacAdapter() {
  return static_cast<BluetoothAdapterMac*>(this->adapter_);
}

CBPeripheral* BluetoothLowEnergyDeviceMac::GetPeripheral() {
  return peripheral_;
}

device::BluetoothRemoteGattServiceMac*
BluetoothLowEnergyDeviceMac::GetBluetoothRemoteGattService(
    CBService* cb_service) const {
  for (GattServiceMap::const_iterator it = gatt_services_.begin();
       it != gatt_services_.end(); ++it) {
    device::BluetoothRemoteGattService* gatt_service = it->second;
    device::BluetoothRemoteGattServiceMac* gatt_service_mac =
        static_cast<BluetoothRemoteGattServiceMac*>(gatt_service);
    if (gatt_service_mac->GetService() == cb_service)
      return gatt_service_mac;
  }
  return nullptr;
}

void BluetoothLowEnergyDeviceMac::DidDisconnectPeripheral(
    BluetoothDevice::ConnectErrorCode error_code) {
  SetGattServicesDiscoveryComplete(false);
  // Removing all services at once to ensure that calling GetGattService on
  // removed service in GattServiceRemoved returns null.
  GattServiceMap gatt_services_swapped;
  gatt_services_swapped.swap(gatt_services_);
  gatt_services_swapped.clear();
  if (create_gatt_connection_error_callbacks_.empty()) {
    // TODO(http://crbug.com/585897): Need to pass the error.
    DidDisconnectGatt();
  } else {
    DidFailToConnectGatt(error_code);
  }
}
