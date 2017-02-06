// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
#define COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/bluetooth.mojom.h"
#include "components/arc/instance_holder.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

class ArcBluetoothBridge
    : public ArcService,
      public InstanceHolder<mojom::BluetoothInstance>::Observer,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothAdapterFactory::AdapterCallback,
      public device::BluetoothLocalGattService::Delegate,
      public mojom::BluetoothHost {
 public:
  explicit ArcBluetoothBridge(ArcBridgeService* bridge_service);
  ~ArcBluetoothBridge() override;

  // Overridden from InstanceHolder<mojom::BluetoothInstance>::Observer:
  void OnInstanceReady() override;

  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from device::BluetoothAdadpter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;

  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;

  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void GattServiceAdded(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        device::BluetoothRemoteGattService* service) override;

  void GattServiceRemoved(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device,
                          device::BluetoothRemoteGattService* service) override;

  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;

  void GattServiceChanged(device::BluetoothAdapter* adapter,
                          device::BluetoothRemoteGattService* service) override;

  void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattCharacteristicRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattDescriptorAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattDescriptorRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  void GattDescriptorValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value) override;

  // Overridden from device::BluetoothLocalGattService::Delegate
  void OnCharacteristicReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      int offset,
      const ValueCallback& callback,
      const ErrorCallback& error_callback) override;

  void OnCharacteristicWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

  void OnDescriptorReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      int offset,
      const ValueCallback& callback,
      const ErrorCallback& error_callback) override;

  void OnDescriptorWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value,
      int offset,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

  void OnNotificationsStart(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  void OnNotificationsStop(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  // Bluetooth Mojo host interface
  void EnableAdapter(const EnableAdapterCallback& callback) override;
  void DisableAdapter(const DisableAdapterCallback& callback) override;

  void GetAdapterProperty(mojom::BluetoothPropertyType type) override;
  void SetAdapterProperty(mojom::BluetoothPropertyPtr property) override;

  void GetRemoteDeviceProperty(mojom::BluetoothAddressPtr remote_addr,
                               mojom::BluetoothPropertyType type) override;
  void SetRemoteDeviceProperty(mojom::BluetoothAddressPtr remote_addr,
                               mojom::BluetoothPropertyPtr property) override;
  void GetRemoteServiceRecord(mojom::BluetoothAddressPtr remote_addr,
                              mojom::BluetoothUUIDPtr uuid) override;

  void GetRemoteServices(mojom::BluetoothAddressPtr remote_addr) override;

  void StartDiscovery() override;
  void CancelDiscovery() override;

  void CreateBond(mojom::BluetoothAddressPtr addr, int32_t transport) override;
  void RemoveBond(mojom::BluetoothAddressPtr addr) override;
  void CancelBond(mojom::BluetoothAddressPtr addr) override;

  void GetConnectionState(mojom::BluetoothAddressPtr addr,
                          const GetConnectionStateCallback& callback) override;

  // Bluetooth Mojo host interface - Bluetooth Gatt Client functions
  void StartLEScan() override;
  void StopLEScan() override;
  void ConnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void DisconnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void StartLEListen(const StartLEListenCallback& callback) override;
  void StopLEListen(const StopLEListenCallback& callback) override;
  void SearchService(mojom::BluetoothAddressPtr remote_addr) override;

  void GetGattDB(mojom::BluetoothAddressPtr remote_addr) override;
  void ReadGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const ReadGattCharacteristicCallback& callback) override;
  void WriteGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattValuePtr value,
      const WriteGattCharacteristicCallback& callback) override;
  void ReadGattDescriptor(mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothGattServiceIDPtr service_id,
                          mojom::BluetoothGattIDPtr char_id,
                          mojom::BluetoothGattIDPtr desc_id,
                          const ReadGattDescriptorCallback& callback) override;
  void WriteGattDescriptor(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattIDPtr desc_id,
      mojom::BluetoothGattValuePtr value,
      const WriteGattDescriptorCallback& callback) override;
  void RegisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const RegisterForGattNotificationCallback& callback) override;
  void DeregisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const DeregisterForGattNotificationCallback& callback) override;
  void ReadRemoteRssi(mojom::BluetoothAddressPtr remote_addr,
                      const ReadRemoteRssiCallback& callback) override;

  // Bluetooth Mojo host interface - Bluetooth Gatt Server functions
  // Android counterpart link:
  // https://source.android.com/devices/halref/bt__gatt__server_8h.html
  // Create a new service. Chrome will create an integer service handle based on
  // that BlueZ identifier that will pass back to Android in the callback.
  // num_handles: number of handle for characteristic / descriptor that will be
  //              created in this service
  void AddService(mojom::BluetoothGattServiceIDPtr service_id,
                  int32_t num_handles,
                  const AddServiceCallback& callback) override;
  // Add a characteristic to a service and pass the characteristic handle back.
  void AddCharacteristic(int32_t service_handle,
                         mojom::BluetoothUUIDPtr uuid,
                         int32_t properties,
                         int32_t permissions,
                         const AddCharacteristicCallback& callback) override;
  // Add a descriptor to the last characteristic added to the given service
  // and pass the descriptor handle back.
  void AddDescriptor(int32_t service_handle,
                     mojom::BluetoothUUIDPtr uuid,
                     int32_t permissions,
                     const AddDescriptorCallback& callback) override;
  // Start a local service.
  void StartService(int32_t service_handle,
                    const StartServiceCallback& callback) override;
  // Stop a local service.
  void StopService(int32_t service_handle,
                   const StopServiceCallback& callback) override;
  // Delete a local service.
  void DeleteService(int32_t service_handle,
                     const DeleteServiceCallback& callback) override;
  // Send value indication to a remote device.
  void SendIndication(int32_t attribute_handle,
                      mojom::BluetoothAddressPtr address,
                      bool confirm,
                      mojo::Array<uint8_t> value,
                      const SendIndicationCallback& callback) override;

  void OpenBluetoothSocket(
      const OpenBluetoothSocketCallback& callback) override;

  // Bluetooth Mojo host interface - Bluetooth SDP functions
  void GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                     mojom::BluetoothUUIDPtr target_uuid) override;
  void CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                       const CreateSdpRecordCallback& callback) override;
  void RemoveSdpRecord(uint32_t service_handle,
                       const RemoveSdpRecordCallback& callback) override;

  // Chrome observer callbacks
  void OnPoweredOn(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredOff(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredError(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnDiscoveryStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoveryStopped();
  void OnDiscoveryError();
  void OnPairing(mojom::BluetoothAddressPtr addr) const;
  void OnPairedDone(mojom::BluetoothAddressPtr addr) const;
  void OnPairedError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnForgetDone(mojom::BluetoothAddressPtr addr) const;
  void OnForgetError(mojom::BluetoothAddressPtr addr) const;

  void OnGattConnectStateChanged(mojom::BluetoothAddressPtr addr,
                                 bool connected) const;
  void OnGattConnected(
      mojom::BluetoothAddressPtr addr,
      std::unique_ptr<device::BluetoothGattConnection> connection);
  void OnGattConnectError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnGattDisconnected(mojom::BluetoothAddressPtr addr);

  void OnStartLEListenDone(const StartLEListenCallback& callback,
                           scoped_refptr<device::BluetoothAdvertisement> adv);
  void OnStartLEListenError(
      const StartLEListenCallback& callback,
      device::BluetoothAdvertisement::ErrorCode error_code);

  void OnStopLEListenDone(const StopLEListenCallback& callback);
  void OnStopLEListenError(
      const StopLEListenCallback& callback,
      device::BluetoothAdvertisement::ErrorCode error_code);

  void OnGattNotifyStartDone(
      const RegisterForGattNotificationCallback& callback,
      const std::string char_string_id,
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);

 private:
  mojo::Array<mojom::BluetoothPropertyPtr> GetDeviceProperties(
      mojom::BluetoothPropertyType type,
      device::BluetoothDevice* device) const;
  mojo::Array<mojom::BluetoothPropertyPtr> GetAdapterProperties(
      mojom::BluetoothPropertyType type) const;
  mojo::Array<mojom::BluetoothAdvertisingDataPtr> GetAdvertisingData(
      device::BluetoothDevice* device) const;

  void SendCachedDevicesFound() const;
  bool HasBluetoothInstance() const;
  bool CheckBluetoothInstanceVersion(uint32_t version_need) const;

  device::BluetoothRemoteGattCharacteristic* FindGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id) const;

  device::BluetoothRemoteGattDescriptor* FindGattDescriptor(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattIDPtr desc_id) const;

  // Propagates the list of paired device to Android.
  void SendCachedPairedDevices() const;

  bool IsGattServerAttributeHandleAvailable(int need);
  int32_t GetNextGattServerAttributeHandle();
  template <class LocalGattAttribute>
  int32_t CreateGattAttributeHandle(LocalGattAttribute* attribute);

  // Common code for OnCharacteristicReadRequest and OnDescriptorReadRequest
  template <class LocalGattAttribute>
  void OnGattAttributeReadRequest(const device::BluetoothDevice* device,
                                  const LocalGattAttribute* attribute,
                                  int offset,
                                  const ValueCallback& success_callback,
                                  const ErrorCallback& error_callback);

  // Common code for OnCharacteristicWriteRequest and OnDescriptorWriteRequest
  template <class LocalGattAttribute>
  void OnGattAttributeWriteRequest(const device::BluetoothDevice* device,
                                   const LocalGattAttribute* attribute,
                                   const std::vector<uint8_t>& value,
                                   int offset,
                                   const base::Closure& success_callback,
                                   const ErrorCallback& error_callback);

  void OnSetDiscoverable(bool discoverable, bool success, uint32_t timeout);
  void SetDiscoverable(bool discoverable, uint32_t timeout);

  void OnGetServiceRecordsDone(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothUUIDPtr target_uuid,
      const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez);
  void OnGetServiceRecordsError(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothUUIDPtr target_uuid,
      bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code);

  bool CalledOnValidThread();

  mojo::Binding<mojom::BluetoothHost> binding_;

  scoped_refptr<bluez::BluetoothAdapterBlueZ> bluetooth_adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisment_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::unordered_map<std::string,
                     std::unique_ptr<device::BluetoothGattNotifySession>>
      notification_session_;
  // Map from Android int handle to Chrome (BlueZ) string identifier.
  std::unordered_map<int32_t, std::string> gatt_identifier_;
  // Map from Chrome (BlueZ) string identifier to android int handle.
  std::unordered_map<std::string, int32_t> gatt_handle_;
  // Store last GattCharacteristic added to each GattService for GattServer.
  std::unordered_map<int32_t, int32_t> last_characteristic_;
  // Monotonically increasing value to use as handle to give to Android side.
  int32_t gatt_server_attribute_next_handle_ = 0;
  // Keeps track of all devices which initiated a GATT connection to us.
  std::unordered_set<std::string> gatt_connection_cache_;
  // Map of device address to GATT connection objects for connections we
  // have made. We need to hang on to these as long as the connection is
  // active since their destructors will drop the connections otherwise.
  std::unordered_map<std::string,
                     std::unique_ptr<device::BluetoothGattConnection>>
      gatt_connections_;
  // Timer to turn adapter discoverable off.
  base::OneShotTimer discoverable_off_timer_;

  base::ThreadChecker thread_checker_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcBluetoothBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBluetoothBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
