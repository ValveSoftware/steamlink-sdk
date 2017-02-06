// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"

#import <CoreBluetooth/CoreBluetooth.h>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

BluetoothRemoteGattServiceMac::BluetoothRemoteGattServiceMac(
    BluetoothLowEnergyDeviceMac* bluetooth_device_mac,
    CBService* service,
    bool is_primary)
    : bluetooth_device_mac_(bluetooth_device_mac),
      service_(service, base::scoped_policy::RETAIN),
      is_primary_(is_primary),
      is_discovery_complete_(false) {
  uuid_ = BluetoothAdapterMac::BluetoothUUIDWithCBUUID([service_.get() UUID]);
  identifier_ =
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 (void*)service_]
          .UTF8String;
}

BluetoothRemoteGattServiceMac::~BluetoothRemoteGattServiceMac() {}

std::string BluetoothRemoteGattServiceMac::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattServiceMac::GetUUID() const {
  return uuid_;
}

bool BluetoothRemoteGattServiceMac::IsPrimary() const {
  return is_primary_;
}

BluetoothDevice* BluetoothRemoteGattServiceMac::GetDevice() const {
  return bluetooth_device_mac_;
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceMac::GetCharacteristics() const {
  std::vector<BluetoothRemoteGattCharacteristic*> gatt_characteristics;
  for (const auto& iter : gatt_characteristic_macs_) {
    BluetoothRemoteGattCharacteristic* gatt_characteristic =
        static_cast<BluetoothRemoteGattCharacteristic*>(iter.second.get());
    gatt_characteristics.push_back(gatt_characteristic);
  }
  return gatt_characteristics;
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceMac::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothRemoteGattService*>();
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattServiceMac::GetCharacteristic(
    const std::string& identifier) const {
  auto searched_pair = gatt_characteristic_macs_.find(identifier);
  if (searched_pair == gatt_characteristic_macs_.end()) {
    return nullptr;
  }
  return static_cast<BluetoothRemoteGattCharacteristic*>(
      searched_pair->second.get());
}

void BluetoothRemoteGattServiceMac::DiscoverCharacteristics() {
  is_discovery_complete_ = false;
  [GetCBPeripheral() discoverCharacteristics:nil forService:GetService()];
}

void BluetoothRemoteGattServiceMac::DidDiscoverCharacteristics() {
  DCHECK(!is_discovery_complete_);
  std::unordered_set<std::string> characteristic_identifier_to_remove;
  for (const auto& iter : gatt_characteristic_macs_) {
    characteristic_identifier_to_remove.insert(iter.first);
  }

  for (CBCharacteristic* cb_characteristic in GetService().characteristics) {
    BluetoothRemoteGattCharacteristicMac* gatt_characteristic_mac =
        GetBluetoothRemoteGattCharacteristicMac(cb_characteristic);
    if (gatt_characteristic_mac) {
      const std::string& identifier = gatt_characteristic_mac->GetIdentifier();
      characteristic_identifier_to_remove.erase(identifier);
      continue;
    }
    gatt_characteristic_mac =
        new BluetoothRemoteGattCharacteristicMac(this, cb_characteristic);
    const std::string identifier = gatt_characteristic_mac->GetIdentifier();
    std::unordered_map<std::string,
                       std::unique_ptr<BluetoothRemoteGattCharacteristicMac>>::value_type value =
{identifier, base::WrapUnique(gatt_characteristic_mac)};
    auto result_iter = gatt_characteristic_macs_.insert(std::move(value));
    DCHECK(result_iter.second);
    GetMacAdapter()->NotifyGattCharacteristicAdded(gatt_characteristic_mac);
  }

  for (const std::string& identifier : characteristic_identifier_to_remove) {
    auto pair_to_remove = gatt_characteristic_macs_.find(identifier);
    std::unique_ptr<BluetoothRemoteGattCharacteristicMac>
        characteristic_to_remove;
    pair_to_remove->second.swap(characteristic_to_remove);
    gatt_characteristic_macs_.erase(pair_to_remove);
    GetMacAdapter()->NotifyGattCharacteristicRemoved(
        characteristic_to_remove.get());
  }
  is_discovery_complete_ = true;
  GetMacAdapter()->NotifyGattServiceChanged(this);
}

void BluetoothRemoteGattServiceMac::DidUpdateValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic);
  gatt_characteristic->DidUpdateValue(error);
}

void BluetoothRemoteGattServiceMac::DidWriteValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic);
  gatt_characteristic->DidWriteValue(error);
}

void BluetoothRemoteGattServiceMac::DidUpdateNotificationState(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic);
  gatt_characteristic->DidUpdateNotificationState(error);
}

bool BluetoothRemoteGattServiceMac::IsDiscoveryComplete() {
  return is_discovery_complete_;
}

BluetoothAdapterMac* BluetoothRemoteGattServiceMac::GetMacAdapter() const {
  return bluetooth_device_mac_->GetMacAdapter();
}

CBPeripheral* BluetoothRemoteGattServiceMac::GetCBPeripheral() const {
  return bluetooth_device_mac_->GetPeripheral();
}

CBService* BluetoothRemoteGattServiceMac::GetService() const {
  return service_.get();
}

BluetoothRemoteGattCharacteristicMac*
BluetoothRemoteGattServiceMac::GetBluetoothRemoteGattCharacteristicMac(
    CBCharacteristic* characteristic) const {
  auto found = std::find_if(
      gatt_characteristic_macs_.begin(), gatt_characteristic_macs_.end(),
      [characteristic](
          const std::pair<
              const std::string,
              std::unique_ptr<BluetoothRemoteGattCharacteristicMac>>& pair) {
        return pair.second->GetCBCharacteristic() == characteristic;
      });
  if (found == gatt_characteristic_macs_.end()) {
    return nullptr;
  } else {
    return found->second.get();
  }
}

}  // namespace device
