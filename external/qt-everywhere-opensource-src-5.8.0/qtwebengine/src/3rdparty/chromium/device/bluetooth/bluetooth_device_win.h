// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothServiceRecordWin;
class BluetoothSocketThread;

class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceWin : public BluetoothDevice {
 public:
  explicit BluetoothDeviceWin(
      BluetoothAdapterWin* adapter,
      const BluetoothTaskManagerWin::DeviceState& device_state,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<BluetoothSocketThread>& socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& net_log_source);
  ~BluetoothDeviceWin() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDList GetUUIDs() const override;
  int16_t GetInquiryRSSI() const override;
  int16_t GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;

  // Used by BluetoothProfileWin to retrieve the service record for the given
  // |uuid|.
  const BluetoothServiceRecordWin* GetServiceRecord(
      const device::BluetoothUUID& uuid) const;

  // Returns true if all fields and services of this instance are equal to the
  // fields and services stored in |device_state|.
  bool IsEqual(const BluetoothTaskManagerWin::DeviceState& device_state);

  // Updates this instance with all fields and properties stored in
  // |device_state|.
  void Update(const BluetoothTaskManagerWin::DeviceState& device_state);

 protected:
  // BluetoothDevice override
  std::string GetDeviceName() const override;
  void CreateGattConnectionImpl() override;
  void DisconnectGatt() override;

 private:
  friend class BluetoothAdapterWin;

  typedef ScopedVector<BluetoothServiceRecordWin> ServiceRecordList;

  // Used by BluetoothAdapterWin to update the visible state during
  // discovery.
  void SetVisible(bool visible);

  // Updates the services with services stored in |device_state|.
  void UpdateServices(const BluetoothTaskManagerWin::DeviceState& device_state);

  // Checks if GATT service with |uuid| and |attribute_handle| has already been
  // discovered.
  bool IsGattServiceDiscovered(BluetoothUUID& uuid, uint16_t attribute_handle);

  // Checks if |service| still exist on device according to newly discovered
  // |service_state|.
  bool DoesGattServiceExist(
      const ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>&
          service_state,
      BluetoothRemoteGattService* service);

  // Updates the GATT services with the services stored in |service_state|.
  void UpdateGattServices(
      const ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>&
          service_state);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThread> socket_thread_;
  net::NetLog* net_log_;
  net::NetLog::Source net_log_source_;

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32_t bluetooth_class_;

  // The name of the device, as supplied by the remote device.
  std::string name_;

  // The Bluetooth address of the device.
  std::string address_;

  // Tracked device state, updated by the adapter managing the lifecycle of
  // the device.
  bool paired_;
  bool connected_;

  // Used to send change notifications when a device disappears during
  // discovery.
  bool visible_;

  // The services (identified by UUIDs) that this device provides.
  UUIDList uuids_;

  // The service records retrieved from SDP.
  ServiceRecordList service_record_list_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
