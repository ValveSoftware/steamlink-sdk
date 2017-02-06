// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_

#include <stdint.h>

#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

@class CBCharacteristic;
@class CBPeripheral;
@class CBService;

namespace device {

class BluetoothAdapterMac;
class BluetoothDevice;
class BluetoothRemoteGattCharacteristicMac;
class BluetoothLowEnergyDeviceMac;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceMac
    : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceMac(
      BluetoothLowEnergyDeviceMac* bluetooth_device_mac,
      CBService* service,
      bool is_primary);
  ~BluetoothRemoteGattServiceMac() override;

  // BluetoothRemoteGattService override.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;

 private:
  friend class BluetoothLowEnergyDeviceMac;
  friend class BluetoothRemoteGattCharacteristicMac;
  friend class BluetoothTestMac;

  // Starts discovering characteristics by calling CoreBluetooth.
  void DiscoverCharacteristics();
  // Called by the BluetoothLowEnergyDeviceMac instance when the characteristics
  // has been discovered.
  void DidDiscoverCharacteristics();
  // Called by the BluetoothLowEnergyDeviceMac instance when the
  // characteristics value has been read.
  void DidUpdateValue(CBCharacteristic* characteristic, NSError* error);
  // Called by the BluetoothLowEnergyDeviceMac instance when the
  // characteristics value has been written.
  void DidWriteValue(CBCharacteristic* characteristic, NSError* error);
  // Called by the BluetoothLowEnergyDeviceMac instance when the notify session
  // has been started or failed.
  void DidUpdateNotificationState(CBCharacteristic* characteristic,
                                  NSError* error);
  // Returns true if the characteristics has been discovered.
  bool IsDiscoveryComplete();

  // Returns the mac adapter.
  BluetoothAdapterMac* GetMacAdapter() const;
  // Returns CBPeripheral.
  CBPeripheral* GetCBPeripheral() const;
  // Returns CBService.
  CBService* GetService() const;
  // Returns a remote characteristic based on the CBCharacteristic.
  BluetoothRemoteGattCharacteristicMac* GetBluetoothRemoteGattCharacteristicMac(
      CBCharacteristic* characteristic) const;

  // bluetooth_device_mac_ owns instances of this class.
  BluetoothLowEnergyDeviceMac* bluetooth_device_mac_;
  // A service from CBPeripheral.services.
  base::scoped_nsobject<CBService> service_;
  // Map of characteristics, keyed by characteristic identifier.
  std::unordered_map<std::string,
                     std::unique_ptr<BluetoothRemoteGattCharacteristicMac>>
      gatt_characteristic_macs_;
  bool is_primary_;
  // Service identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;
  // Is true if the characteristics has been discovered.
  bool is_discovery_complete_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_
