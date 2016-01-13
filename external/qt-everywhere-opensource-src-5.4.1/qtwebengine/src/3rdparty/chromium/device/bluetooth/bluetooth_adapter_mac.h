// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_

#include <IOKit/IOReturn.h>

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_manager_mac.h"

@class IOBluetoothDevice;
@class NSArray;
@class NSDate;

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterMacTest;

class BluetoothAdapterMac : public BluetoothAdapter,
                            public BluetoothDiscoveryManagerMac::Observer {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscoverable() const OVERRIDE;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      int channel,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;
  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      int psm,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;

  // BluetoothDiscoveryManagerMac::Observer overrides
  virtual void DeviceFound(BluetoothDiscoveryManagerMac* manager,
                           IOBluetoothDevice* device) OVERRIDE;
  virtual void DiscoveryStopped(BluetoothDiscoveryManagerMac* manager,
                                bool unexpected) OVERRIDE;

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(IOBluetoothDevice* device);

 protected:
  // BluetoothAdapter:
  virtual void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) OVERRIDE;

 private:
  friend class BluetoothAdapterMacTest;

  BluetoothAdapterMac();
  virtual ~BluetoothAdapterMac();

  // BluetoothAdapter:
  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  void Init();
  void InitForTest(scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void PollAdapter();

  // Updates |devices_| to include the currently paired devices, as well as any
  // connected, but unpaired, devices. Notifies observers if any previously
  // paired or connected devices are no longer present.
  void UpdateDevices();

  std::string address_;
  std::string name_;
  bool powered_;

  int num_discovery_sessions_;

  // Discovery manager for Bluetooth Classic.
  scoped_ptr<BluetoothDiscoveryManagerMac> classic_discovery_manager_;

  // A list of discovered device addresses.
  // This list is used to check if the same device is discovered twice during
  // the discovery between consecutive inquiries.
  base::hash_set<std::string> discovered_devices_;

  // Timestamp for the recently accessed device.
  // Used to determine if |devices_| needs an update.
  base::scoped_nsobject<NSDate> recently_accessed_device_timestamp_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
