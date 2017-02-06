// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

#include <memory>

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

namespace device {

BluetoothLowEnergyDiscoveryManagerMac::
    ~BluetoothLowEnergyDiscoveryManagerMac() {
}

bool BluetoothLowEnergyDiscoveryManagerMac::IsDiscovering() const {
  return discovering_;
}

void BluetoothLowEnergyDiscoveryManagerMac::StartDiscovery(
    BluetoothDevice::UUIDList services_uuids) {
  discovering_ = true;
  pending_ = true;
  services_uuids_ = services_uuids;
  TryStartDiscovery();
}

void BluetoothLowEnergyDiscoveryManagerMac::TryStartDiscovery() {
  if (!discovering_) {
    VLOG(1) << "TryStartDiscovery !discovering_";
    return;
  }

  if (!pending_) {
    VLOG(1) << "TryStartDiscovery !pending_";
    return;
  }

  if (!central_manager_) {
    VLOG(1) << "TryStartDiscovery !central_manager_";
    return;
  }

  if ([central_manager_ state] != CBCentralManagerStatePoweredOn) {
    VLOG(1) << "TryStartDiscovery != CBCentralManagerStatePoweredOn";
    return;
  }

  // Converts the services UUIDs to a CoreBluetooth data structure.
  NSMutableArray* services = nil;
  if (!services_uuids_.empty()) {
    services = [NSMutableArray array];
    for (auto& service_uuid : services_uuids_) {
      NSString* uuidString =
          base::SysUTF8ToNSString(service_uuid.canonical_value().c_str());
      CBUUID* uuid = [CBUUID UUIDWithString:uuidString];
      [services addObject:uuid];
    }
  };

  VLOG(1) << "TryStartDiscovery scanForPeripheralsWithServices";
  [central_manager_ scanForPeripheralsWithServices:services options:nil];
  pending_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::StopDiscovery() {
  VLOG(1) << "StopDiscovery";
  if (discovering_ && !pending_) {
    [central_manager_ stopScan];
  }
  discovering_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::SetCentralManager(
    CBCentralManager* central_manager) {
  central_manager_ = central_manager;
}

void BluetoothLowEnergyDiscoveryManagerMac::DiscoveredPeripheral(
    CBPeripheral* peripheral,
    NSDictionary* advertisementData,
    int rssi) {
  VLOG(1) << "DiscoveredPeripheral";
  observer_->LowEnergyDeviceUpdated(peripheral, advertisementData, rssi);
}

BluetoothLowEnergyDiscoveryManagerMac*
BluetoothLowEnergyDiscoveryManagerMac::Create(Observer* observer) {
  return new BluetoothLowEnergyDiscoveryManagerMac(observer);
}

BluetoothLowEnergyDiscoveryManagerMac::BluetoothLowEnergyDiscoveryManagerMac(
    Observer* observer)
    : observer_(observer) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  discovering_ = false;
}

}  // namespace device
