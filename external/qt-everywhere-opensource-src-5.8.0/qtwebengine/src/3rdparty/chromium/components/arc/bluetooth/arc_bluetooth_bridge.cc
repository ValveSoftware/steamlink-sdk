// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/arc_bluetooth_bridge.h"

#include <bluetooth/bluetooth.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>

#include <iomanip>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/bluetooth/bluetooth_type_converters.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAdvertisement;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothDiscoverySession;
using device::BluetoothGattConnection;
using device::BluetoothGattNotifySession;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;
using device::BluetoothLocalGattCharacteristic;
using device::BluetoothLocalGattDescriptor;
using device::BluetoothLocalGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;
using device::BluetoothTransport;
using device::BluetoothUUID;

namespace {
constexpr int32_t kMinBtleVersion = 1;
constexpr int32_t kMinBtleNotifyVersion = 2;
constexpr int32_t kMinGattServerVersion = 3;
constexpr int32_t kMinAddrChangeVersion = 4;
constexpr int32_t kMinSdpSupportVersion = 5;
constexpr uint32_t kGattReadPermission =
    BluetoothGattCharacteristic::Permission::PERMISSION_READ |
    BluetoothGattCharacteristic::Permission::PERMISSION_READ_ENCRYPTED |
    BluetoothGattCharacteristic::Permission::
        PERMISSION_READ_ENCRYPTED_AUTHENTICATED;
constexpr uint32_t kGattWritePermission =
    BluetoothGattCharacteristic::Permission::PERMISSION_WRITE |
    BluetoothGattCharacteristic::Permission::PERMISSION_WRITE_ENCRYPTED |
    BluetoothGattCharacteristic::Permission::
        PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED;
constexpr int32_t kInvalidGattAttributeHandle = -1;
// Bluetooth Specification Version 4.2 Vol 3 Part F Section 3.2.2
// An attribute handle of value 0xFFFF is known as the maximum attribute handle.
constexpr int32_t kMaxGattAttributeHandle = 0xFFFF;
// Bluetooth Specification Version 4.2 Vol 3 Part F Section 3.2.9
// The maximum length of an attribute value shall be 512 octets.
constexpr int kMaxGattAttributeLength = 512;
// Bluetooth SDP Service Class ID List Attribute identifier
constexpr uint16_t kServiceClassIDListAttributeID = 0x0001;

using GattStatusCallback =
    base::Callback<void(arc::mojom::BluetoothGattStatus)>;
using GattReadCallback =
    base::Callback<void(arc::mojom::BluetoothGattValuePtr)>;
using CreateSdpRecordCallback =
    base::Callback<void(arc::mojom::BluetoothCreateSdpRecordResultPtr)>;
using RemoveSdpRecordCallback =
    base::Callback<void(arc::mojom::BluetoothStatus)>;

// Example of identifier: /org/bluez/hci0/dev_E0_CF_65_8C_86_1A/service001a
// Convert the last 4 characters of |identifier| to an
// int, by interpreting them as hexadecimal digits.
int ConvertGattIdentifierToId(const std::string identifier) {
  return std::stoi(identifier.substr(identifier.size() - 4), nullptr, 16);
}

// Create GattDBElement and fill in common data for
// Gatt Service/Characteristic/Descriptor.
template <class RemoteGattAttribute>
arc::mojom::BluetoothGattDBElementPtr CreateGattDBElement(
    const arc::mojom::BluetoothGattDBAttributeType type,
    const RemoteGattAttribute* attribute) {
  arc::mojom::BluetoothGattDBElementPtr element =
      arc::mojom::BluetoothGattDBElement::New();
  element->type = type;
  element->uuid = arc::mojom::BluetoothUUID::From(attribute->GetUUID());
  element->id = element->attribute_handle = element->start_handle =
      element->end_handle =
          ConvertGattIdentifierToId(attribute->GetIdentifier());
  element->properties = 0;
  return element;
}

template <class RemoteGattAttribute>
RemoteGattAttribute* FindGattAttributeByUuid(
    const std::vector<RemoteGattAttribute*>& attributes,
    const BluetoothUUID& uuid) {
  auto it = std::find_if(
      attributes.begin(), attributes.end(),
      [uuid](RemoteGattAttribute* attr) { return attr->GetUUID() == uuid; });
  return it != attributes.end() ? *it : nullptr;
}

// Common success callback for GATT operations that only need to report
// GattStatus back to Android.
void OnGattOperationDone(const GattStatusCallback& callback) {
  callback.Run(arc::mojom::BluetoothGattStatus::GATT_SUCCESS);
}

// Common error callback for GATT operations that only need to report
// GattStatus back to Android.
void OnGattOperationError(const GattStatusCallback& callback,
                          BluetoothGattService::GattErrorCode error_code) {
  callback.Run(mojo::ConvertTo<arc::mojom::BluetoothGattStatus>(error_code));
}

// Common success callback for ReadGattCharacteristic and ReadGattDescriptor
void OnGattReadDone(const GattReadCallback& callback,
                    const std::vector<uint8_t>& result) {
  arc::mojom::BluetoothGattValuePtr gattValue =
      arc::mojom::BluetoothGattValue::New();
  gattValue->status = arc::mojom::BluetoothGattStatus::GATT_SUCCESS;
  gattValue->value = mojo::Array<uint8_t>::From(result);
  callback.Run(std::move(gattValue));
}

// Common error callback for ReadGattCharacteristic and ReadGattDescriptor
void OnGattReadError(const GattReadCallback& callback,
                     BluetoothGattService::GattErrorCode error_code) {
  arc::mojom::BluetoothGattValuePtr gattValue =
      arc::mojom::BluetoothGattValue::New();
  gattValue->status =
      mojo::ConvertTo<arc::mojom::BluetoothGattStatus>(error_code);
  gattValue->value = nullptr;
  callback.Run(std::move(gattValue));
}

// Callback function for mojom::BluetoothInstance::RequestGattRead
void OnGattServerRead(
    const BluetoothLocalGattService::Delegate::ValueCallback& success_callback,
    const BluetoothLocalGattService::Delegate::ErrorCallback& error_callback,
    arc::mojom::BluetoothGattStatus status,
    mojo::Array<uint8_t> value) {
  if (status == arc::mojom::BluetoothGattStatus::GATT_SUCCESS)
    success_callback.Run(value.To<std::vector<uint8_t>>());
  else
    error_callback.Run();
}

// Callback function for mojom::BluetoothInstance::RequestGattWrite
void OnGattServerWrite(
    const base::Closure& success_callback,
    const BluetoothLocalGattService::Delegate::ErrorCallback& error_callback,
    arc::mojom::BluetoothGattStatus status) {
  if (status == arc::mojom::BluetoothGattStatus::GATT_SUCCESS)
    success_callback.Run();
  else
    error_callback.Run();
}

bool IsGattOffsetValid(int offset) {
  return 0 <= offset && offset < kMaxGattAttributeLength;
}

// This is needed because Android only support UUID 16 bits in advertising data.
uint16_t GetUUID16(const BluetoothUUID& uuid) {
  // Convert xxxxyyyy-xxxx-xxxx-xxxx-xxxxxxxxxxxx to int16 yyyy
  return std::stoi(uuid.canonical_value().substr(4, 4), nullptr, 16);
}

mojo::Array<arc::mojom::BluetoothPropertyPtr> GetDiscoveryTimeoutProperty(
    uint32_t timeout) {
  arc::mojom::BluetoothPropertyPtr property =
      arc::mojom::BluetoothProperty::New();
  property->set_discovery_timeout(timeout);
  mojo::Array<arc::mojom::BluetoothPropertyPtr> properties;
  properties.push_back(std::move(property));
  return properties;
}

void OnCreateServiceRecordDone(const CreateSdpRecordCallback& callback,
                               uint32_t service_handle) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  result->status = arc::mojom::BluetoothStatus::SUCCESS;
  result->service_handle = service_handle;

  callback.Run(std::move(result));
}

void OnCreateServiceRecordError(
    const CreateSdpRecordCallback& callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY) {
    result->status = arc::mojom::BluetoothStatus::NOT_READY;
  } else {
    result->status = arc::mojom::BluetoothStatus::FAIL;
  }

  callback.Run(std::move(result));
}

void OnRemoveServiceRecordDone(const RemoveSdpRecordCallback& callback) {
  callback.Run(arc::mojom::BluetoothStatus::SUCCESS);
}

void OnRemoveServiceRecordError(
    const RemoveSdpRecordCallback& callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothStatus status;
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY)
    status = arc::mojom::BluetoothStatus::NOT_READY;
  else
    status = arc::mojom::BluetoothStatus::FAIL;

  callback.Run(status);
}

}  // namespace

namespace arc {

ArcBluetoothBridge::ArcBluetoothBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_factory_(this) {
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    VLOG(1) << "registering bluetooth adapter";
    BluetoothAdapterFactory::GetAdapter(base::Bind(
        &ArcBluetoothBridge::OnAdapterInitialized, weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "no bluetooth adapter available";
  }

  arc_bridge_service()->bluetooth()->AddObserver(this);
}

ArcBluetoothBridge::~ArcBluetoothBridge() {
  DCHECK(CalledOnValidThread());

  arc_bridge_service()->bluetooth()->RemoveObserver(this);

  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void ArcBluetoothBridge::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(CalledOnValidThread());

  // We can downcast here because we are always running on Chrome OS, and
  // so our adapter uses BlueZ.
  bluetooth_adapter_ =
      static_cast<bluez::BluetoothAdapterBlueZ*>(adapter.get());
  bluetooth_adapter_->AddObserver(this);
}

void ArcBluetoothBridge::OnInstanceReady() {
  mojom::BluetoothInstance* bluetooth_instance =
      arc_bridge_service()->bluetooth()->instance();
  if (!bluetooth_instance) {
    LOG(ERROR) << "OnBluetoothInstanceReady called, "
               << "but no bluetooth instance found";
    return;
  }
  bluetooth_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcBluetoothBridge::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Invoke EnableAdapter or DisableAdapter via ARC bridge
  // service.
}

void ArcBluetoothBridge::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!HasBluetoothInstance())
    return;

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device);

  arc_bridge_service()->bluetooth()->instance()->OnDeviceFound(
      std::move(properties));

  if (!CheckBluetoothInstanceVersion(kMinBtleVersion))
    return;

  if (!(device->GetType() & device::BLUETOOTH_TRANSPORT_LE))
    return;

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());
  int rssi = device->GetInquiryRSSI();
  mojo::Array<mojom::BluetoothAdvertisingDataPtr> adv_data =
      GetAdvertisingData(device);
  arc_bridge_service()->bluetooth()->instance()->OnLEDeviceFound(
      std::move(addr), rssi, std::move(adv_data));

  if (!device->IsConnected())
    return;

  addr = mojom::BluetoothAddress::From(device->GetAddress());
  OnGattConnectStateChanged(std::move(addr), true);
}

void ArcBluetoothBridge::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!(device->GetType() & device::BLUETOOTH_TRANSPORT_LE))
    return;

  auto it = gatt_connection_cache_.find(device->GetAddress());
  bool was_connected = it != gatt_connection_cache_.end();
  bool is_connected = device->IsConnected();

  if (is_connected == was_connected)
    return;

  if (is_connected)
    gatt_connection_cache_.insert(device->GetAddress());
  else  // was_connected
    gatt_connection_cache_.erase(it);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());
  OnGattConnectStateChanged(std::move(addr), is_connected);
}

void ArcBluetoothBridge::DeviceAddressChanged(BluetoothAdapter* adapter,
                                              BluetoothDevice* device,
                                              const std::string& old_address) {
  if (old_address == device->GetAddress())
    return;

  if (!(device->GetType() & device::BLUETOOTH_TRANSPORT_LE))
    return;

  auto it = gatt_connection_cache_.find(old_address);
  if (it == gatt_connection_cache_.end())
    return;

  gatt_connection_cache_.erase(it);
  gatt_connection_cache_.insert(device->GetAddress());

  if (!HasBluetoothInstance())
    return;

  if (!CheckBluetoothInstanceVersion(kMinAddrChangeVersion))
    return;

  mojom::BluetoothAddressPtr old_addr =
      mojom::BluetoothAddress::From(old_address);
  mojom::BluetoothAddressPtr new_addr =
      mojom::BluetoothAddress::From(device->GetAddress());
  arc_bridge_service()->bluetooth()->instance()->OnLEDeviceAddressChange(
      std::move(old_addr), std::move(new_addr));
}

void ArcBluetoothBridge::DevicePairedChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             bool new_paired_status) {
  DCHECK(adapter);
  DCHECK(device);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  if (new_paired_status) {
    // OnBondStateChanged must be called with BluetoothBondState::BONDING to
    // make sure the bond state machine on Android is ready to take the
    // pair-done event. Otherwise the pair-done event will be dropped as an
    // invalid change of paired status.
    OnPairing(addr->Clone());
    OnPairedDone(std::move(addr));
  } else {
    OnForgetDone(std::move(addr));
  }
}

void ArcBluetoothBridge::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  DCHECK(adapter);
  DCHECK(device);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());
  OnForgetDone(std::move(addr));

  auto it = gatt_connection_cache_.find(device->GetAddress());
  if (it == gatt_connection_cache_.end())
    return;

  addr = mojom::BluetoothAddress::From(device->GetAddress());
  gatt_connection_cache_.erase(it);
  OnGattConnectStateChanged(std::move(addr), false);
}

void ArcBluetoothBridge::GattServiceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device,
                                          BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServicesDiscovered(BluetoothAdapter* adapter,
                                                BluetoothDevice* device) {
  if (!HasBluetoothInstance())
    return;

  if (!CheckBluetoothInstanceVersion(kMinBtleVersion))
    return;

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  arc_bridge_service()->bluetooth()->instance()->OnSearchComplete(
      std::move(addr), mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattServiceChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (!HasBluetoothInstance())
    return;

  if (!CheckBluetoothInstanceVersion(kMinBtleNotifyVersion))
    return;

  BluetoothRemoteGattService* service = characteristic->GetService();
  BluetoothDevice* device = service->GetDevice();
  mojom::BluetoothAddressPtr address =
      mojom::BluetoothAddress::From(device->GetAddress());
  mojom::BluetoothGattServiceIDPtr service_id =
      mojom::BluetoothGattServiceID::New();
  service_id->is_primary = service->IsPrimary();
  service_id->id = mojom::BluetoothGattID::New();
  service_id->id->inst_id = ConvertGattIdentifierToId(service->GetIdentifier());
  service_id->id->uuid = mojom::BluetoothUUID::From(service->GetUUID());

  mojom::BluetoothGattIDPtr char_id = mojom::BluetoothGattID::New();
  char_id->inst_id = ConvertGattIdentifierToId(characteristic->GetIdentifier());
  char_id->uuid = mojom::BluetoothUUID::From(characteristic->GetUUID());

  arc_bridge_service()->bluetooth()->instance()->OnGattNotify(
      std::move(address), std::move(service_id), std::move(char_id),
      true /* is_notify */, mojo::Array<uint8_t>::From(value));
}

void ArcBluetoothBridge::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  // Placeholder for GATT client functionality
}

template <class LocalGattAttribute>
void ArcBluetoothBridge::OnGattAttributeReadRequest(
    const BluetoothDevice* device,
    const LocalGattAttribute* attribute,
    int offset,
    const ValueCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK(CalledOnValidThread());
  if (!HasBluetoothInstance() ||
      !CheckBluetoothInstanceVersion(kMinGattServerVersion) ||
      !IsGattOffsetValid(offset)) {
    error_callback.Run();
    return;
  }

  DCHECK(gatt_handle_.find(attribute->GetIdentifier()) != gatt_handle_.end());

  arc_bridge_service()->bluetooth()->instance()->RequestGattRead(
      mojom::BluetoothAddress::From(device->GetAddress()),
      gatt_handle_[attribute->GetIdentifier()], offset, false /* is_long */,
      base::Bind(&OnGattServerRead, success_callback, error_callback));
}

template <class LocalGattAttribute>
void ArcBluetoothBridge::OnGattAttributeWriteRequest(
    const BluetoothDevice* device,
    const LocalGattAttribute* attribute,
    const std::vector<uint8_t>& value,
    int offset,
    const base::Closure& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK(CalledOnValidThread());
  if (!HasBluetoothInstance() ||
      !CheckBluetoothInstanceVersion(kMinGattServerVersion) ||
      !IsGattOffsetValid(offset)) {
    error_callback.Run();
    return;
  }

  DCHECK(gatt_handle_.find(attribute->GetIdentifier()) != gatt_handle_.end());

  arc_bridge_service()->bluetooth()->instance()->RequestGattWrite(
      mojom::BluetoothAddress::From(device->GetAddress()),
      gatt_handle_[attribute->GetIdentifier()], offset,
      mojo::Array<uint8_t>::From(value),
      base::Bind(&OnGattServerWrite, success_callback, error_callback));
}

void ArcBluetoothBridge::OnCharacteristicReadRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic,
    int offset,
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  OnGattAttributeReadRequest(device, characteristic, offset, callback,
                             error_callback);
}

void ArcBluetoothBridge::OnCharacteristicWriteRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  OnGattAttributeWriteRequest(device, characteristic, value, offset, callback,
                              error_callback);
}

void ArcBluetoothBridge::OnDescriptorReadRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattDescriptor* descriptor,
    int offset,
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  OnGattAttributeReadRequest(device, descriptor, offset, callback,
                             error_callback);
}

void ArcBluetoothBridge::OnDescriptorWriteRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value,
    int offset,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  OnGattAttributeWriteRequest(device, descriptor, value, offset, callback,
                              error_callback);
}

void ArcBluetoothBridge::OnNotificationsStart(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic) {}

void ArcBluetoothBridge::OnNotificationsStop(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic) {}

void ArcBluetoothBridge::EnableAdapter(const EnableAdapterCallback& callback) {
  DCHECK(bluetooth_adapter_);
  if (!bluetooth_adapter_->IsPowered()) {
    bluetooth_adapter_->SetPowered(
        true, base::Bind(&ArcBluetoothBridge::OnPoweredOn,
                         weak_factory_.GetWeakPtr(), callback),
        base::Bind(&ArcBluetoothBridge::OnPoweredError,
                   weak_factory_.GetWeakPtr(), callback));
    return;
  }

  OnPoweredOn(callback);
}

void ArcBluetoothBridge::DisableAdapter(
    const DisableAdapterCallback& callback) {
  DCHECK(bluetooth_adapter_);
  bluetooth_adapter_->SetPowered(
      false, base::Bind(&ArcBluetoothBridge::OnPoweredOff,
                        weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ArcBluetoothBridge::OnPoweredError,
                 weak_factory_.GetWeakPtr(), callback));
}

void ArcBluetoothBridge::GetAdapterProperty(mojom::BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetAdapterProperties(type);

  arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
      mojom::BluetoothStatus::SUCCESS, std::move(properties));
}

void ArcBluetoothBridge::OnSetDiscoverable(bool discoverable,
                                           bool success,
                                           uint32_t timeout) {
  DCHECK(CalledOnValidThread());

  if (success && discoverable && timeout > 0) {
    discoverable_off_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(timeout),
        base::Bind(&ArcBluetoothBridge::SetDiscoverable,
                   weak_factory_.GetWeakPtr(), false, 0));
  }

  if (!HasBluetoothInstance())
    return;

  if (success) {
    arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
        mojom::BluetoothStatus::SUCCESS, GetDiscoveryTimeoutProperty(timeout));
  } else {
    arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
        mojom::BluetoothStatus::FAIL,
        mojo::Array<arc::mojom::BluetoothPropertyPtr>::New(0));
  }
}

// Set discoverable state to on / off.
// In case of turning on, start timer to turn it back off in |timeout| seconds.
void ArcBluetoothBridge::SetDiscoverable(bool discoverable, uint32_t timeout) {
  DCHECK(bluetooth_adapter_);
  DCHECK(CalledOnValidThread());
  DCHECK(!discoverable || timeout == 0);

  bool currently_discoverable = bluetooth_adapter_->IsDiscoverable();

  if (!discoverable && !currently_discoverable)
    return;

  if (discoverable && currently_discoverable) {
    if (base::TimeDelta::FromSeconds(timeout) >
        discoverable_off_timer_.GetCurrentDelay()) {
      // Restart discoverable_off_timer_ if new timeout is greater
      OnSetDiscoverable(true, true, timeout);
    } else if (HasBluetoothInstance()) {
      // Just send message to Android if new timeout is lower.
      arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
          mojom::BluetoothStatus::SUCCESS,
          GetDiscoveryTimeoutProperty(timeout));
    }
    return;
  }

  bluetooth_adapter_->SetDiscoverable(
      discoverable,
      base::Bind(&ArcBluetoothBridge::OnSetDiscoverable,
                 weak_factory_.GetWeakPtr(), discoverable, true, timeout),
      base::Bind(&ArcBluetoothBridge::OnSetDiscoverable,
                 weak_factory_.GetWeakPtr(), discoverable, false, timeout));
}

void ArcBluetoothBridge::SetAdapterProperty(
    mojom::BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  if (property->is_discovery_timeout()) {
    uint32_t discovery_timeout = property->get_discovery_timeout();
    if (discovery_timeout > 0) {
      SetDiscoverable(true, discovery_timeout);
    } else {
      arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
          mojom::BluetoothStatus::PARM_INVALID,
          mojo::Array<arc::mojom::BluetoothPropertyPtr>::New(0));
    }
  } else {
    // TODO(puthik) Implement other case.
    arc_bridge_service()->bluetooth()->instance()->OnAdapterProperties(
        mojom::BluetoothStatus::UNSUPPORTED,
        mojo::Array<arc::mojom::BluetoothPropertyPtr>::New(0));
  }
}

void ArcBluetoothBridge::GetRemoteDeviceProperty(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  std::string addr_str = remote_addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);

  mojo::Array<mojom::BluetoothPropertyPtr> properties =
      GetDeviceProperties(type, device);
  mojom::BluetoothStatus status = mojom::BluetoothStatus::SUCCESS;

  if (!device) {
    VLOG(1) << __func__ << ": device " << addr_str << " not available";
    status = mojom::BluetoothStatus::FAIL;
  }

  arc_bridge_service()->bluetooth()->instance()->OnRemoteDeviceProperties(
      status, std::move(remote_addr), std::move(properties));
}

void ArcBluetoothBridge::SetRemoteDeviceProperty(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  // TODO(smbarber): Implement SetRemoteDeviceProperty
  arc_bridge_service()->bluetooth()->instance()->OnRemoteDeviceProperties(
      mojom::BluetoothStatus::FAIL, std::move(remote_addr),
      mojo::Array<mojom::BluetoothPropertyPtr>::New(0));
}

void ArcBluetoothBridge::GetRemoteServiceRecord(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothUUIDPtr uuid) {
  // TODO(smbarber): Implement GetRemoteServiceRecord
}

void ArcBluetoothBridge::GetRemoteServices(
    mojom::BluetoothAddressPtr remote_addr) {
  // TODO(smbarber): Implement GetRemoteServices
}

void ArcBluetoothBridge::StartDiscovery() {
  DCHECK(bluetooth_adapter_);
  // TODO(smbarber): Add timeout
  if (discovery_session_) {
    LOG(ERROR) << "Discovery session already running; leaving alone";
    SendCachedDevicesFound();
    return;
  }

  bluetooth_adapter_->StartDiscoverySession(
      base::Bind(&ArcBluetoothBridge::OnDiscoveryStarted,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ArcBluetoothBridge::OnDiscoveryError,
                 weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::CancelDiscovery() {
  if (!discovery_session_) {
    return;
  }

  discovery_session_->Stop(base::Bind(&ArcBluetoothBridge::OnDiscoveryStopped,
                                      weak_factory_.GetWeakPtr()),
                           base::Bind(&ArcBluetoothBridge::OnDiscoveryError,
                                      weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::OnPoweredOn(
    const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  callback.Run(mojom::BluetoothAdapterState::ON);
  SendCachedPairedDevices();
}

void ArcBluetoothBridge::OnPoweredOff(
    const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  callback.Run(mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnPoweredError(
    const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const {
  LOG(WARNING) << "failed to change power state";

  callback.Run(bluetooth_adapter_->IsPowered()
                   ? mojom::BluetoothAdapterState::ON
                   : mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnDiscoveryStarted(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  if (!HasBluetoothInstance())
    return;

  discovery_session_ = std::move(session);

  arc_bridge_service()->bluetooth()->instance()->OnDiscoveryStateChanged(
      mojom::BluetoothDiscoveryState::STARTED);

  SendCachedDevicesFound();
}

void ArcBluetoothBridge::OnDiscoveryStopped() {
  if (!HasBluetoothInstance())
    return;

  discovery_session_.reset();

  arc_bridge_service()->bluetooth()->instance()->OnDiscoveryStateChanged(
      mojom::BluetoothDiscoveryState::STOPPED);
}

void ArcBluetoothBridge::CreateBond(mojom::BluetoothAddressPtr addr,
                                    int32_t transport) {
  std::string addr_str = addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
  if (!device || !device->IsPairable()) {
    VLOG(1) << __func__ << ": device " << addr_str
            << " is no longer valid or pairable";
    OnPairedError(std::move(addr), BluetoothDevice::ERROR_FAILED);
    return;
  }

  if (device->IsPaired()) {
    OnPairedDone(std::move(addr));
    return;
  }

  // Use the default pairing delegate which is the delegate registered and owned
  // by ash.
  BluetoothDevice::PairingDelegate* delegate =
      bluetooth_adapter_->DefaultPairingDelegate();

  if (!delegate) {
    OnPairedError(std::move(addr), BluetoothDevice::ERROR_FAILED);
    return;
  }

  // If pairing finished successfully, DevicePairedChanged will notify Android
  // on paired state change event, so DoNothing is passed as a success callback.
  device->Pair(delegate, base::Bind(&base::DoNothing),
               base::Bind(&ArcBluetoothBridge::OnPairedError,
                          weak_factory_.GetWeakPtr(), base::Passed(&addr)));
}

void ArcBluetoothBridge::RemoveBond(mojom::BluetoothAddressPtr addr) {
  // Forget the device if it is no longer valid or not even paired.
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device || !device->IsPaired()) {
    OnForgetDone(std::move(addr));
    return;
  }

  // If unpairing finished successfully, DevicePairedChanged will notify Android
  // on paired state change event, so DoNothing is passed as a success callback.
  device->Forget(base::Bind(&base::DoNothing),
                 base::Bind(&ArcBluetoothBridge::OnForgetError,
                            weak_factory_.GetWeakPtr(), base::Passed(&addr)));
}

void ArcBluetoothBridge::CancelBond(mojom::BluetoothAddressPtr addr) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    OnForgetDone(std::move(addr));
    return;
  }

  device->CancelPairing();
  OnForgetDone(std::move(addr));
}

void ArcBluetoothBridge::GetConnectionState(
    mojom::BluetoothAddressPtr addr,
    const GetConnectionStateCallback& callback) {
  if (!bluetooth_adapter_) {
    callback.Run(false);
    return;
  }

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    callback.Run(false);
    return;
  }

  callback.Run(device->IsConnected());
}

void ArcBluetoothBridge::StartLEScan() {
  DCHECK(bluetooth_adapter_);
  if (discovery_session_) {
    LOG(WARNING) << "Discovery session already running; leaving alone";
    SendCachedDevicesFound();
    return;
  }
  bluetooth_adapter_->StartDiscoverySessionWithFilter(
      base::WrapUnique(
          new BluetoothDiscoveryFilter(device::BLUETOOTH_TRANSPORT_LE)),
      base::Bind(&ArcBluetoothBridge::OnDiscoveryStarted,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ArcBluetoothBridge::OnDiscoveryError,
                 weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::StopLEScan() {
  CancelDiscovery();
}

void ArcBluetoothBridge::OnGattConnectStateChanged(
    mojom::BluetoothAddressPtr addr,
    bool connected) const {
  if (!HasBluetoothInstance())
    return;

  if (!CheckBluetoothInstanceVersion(kMinBtleVersion))
    return;

  DCHECK(addr);

  arc_bridge_service()->bluetooth()->instance()->OnLEConnectionStateChange(
      std::move(addr), connected);
}

void ArcBluetoothBridge::OnGattConnected(
    mojom::BluetoothAddressPtr addr,
    std::unique_ptr<BluetoothGattConnection> connection) {
  DCHECK(CalledOnValidThread());
  gatt_connections_[addr->To<std::string>()] = std::move(connection);
  OnGattConnectStateChanged(std::move(addr), true);
}

void ArcBluetoothBridge::OnGattConnectError(
    mojom::BluetoothAddressPtr addr,
    BluetoothDevice::ConnectErrorCode error_code) const {
  OnGattConnectStateChanged(std::move(addr), false);
}

void ArcBluetoothBridge::OnGattDisconnected(
    mojom::BluetoothAddressPtr addr) {
  DCHECK(CalledOnValidThread());
  auto it = gatt_connections_.find(addr->To<std::string>());
  if (it == gatt_connections_.end()) {
    LOG(WARNING) << "OnGattDisconnected called, "
                 << "but no gatt connection was found";
  } else {
    gatt_connections_.erase(it);
  }

  OnGattConnectStateChanged(std::move(addr), false);
}

void ArcBluetoothBridge::ConnectLEDevice(
    mojom::BluetoothAddressPtr remote_addr) {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  DCHECK(device);

  if (device->IsConnected()) {
    arc_bridge_service()->bluetooth()->instance()->OnLEConnectionStateChange(
        std::move(remote_addr), true);
    return;
  }

  // Also pass disconnect callback in error case since it would be disconnected
  // anyway.
  mojom::BluetoothAddressPtr remote_addr_clone = remote_addr.Clone();
  device->CreateGattConnection(
      base::Bind(&ArcBluetoothBridge::OnGattConnected,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr)),
      base::Bind(&ArcBluetoothBridge::OnGattConnectError,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr_clone)));
}

void ArcBluetoothBridge::DisconnectLEDevice(
    mojom::BluetoothAddressPtr remote_addr) {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  DCHECK(device);

  if (!device->IsConnected()) {
    arc_bridge_service()->bluetooth()->instance()->OnLEConnectionStateChange(
        std::move(remote_addr), false);
    return;
  }

  mojom::BluetoothAddressPtr remote_addr_clone = remote_addr.Clone();
  device->Disconnect(
      base::Bind(&ArcBluetoothBridge::OnGattDisconnected,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr)),
      base::Bind(&ArcBluetoothBridge::OnGattDisconnected,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr_clone)));
}

void ArcBluetoothBridge::SearchService(mojom::BluetoothAddressPtr remote_addr) {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  DCHECK(device);

  // Call the callback if discovery is completed
  if (device->IsGattServicesDiscoveryComplete()) {
    arc_bridge_service()->bluetooth()->instance()->OnSearchComplete(
        std::move(remote_addr), mojom::BluetoothGattStatus::GATT_SUCCESS);
    return;
  }

  // Discard result. Will call the callback when discovery is completed.
  device->GetGattServices();
}

void ArcBluetoothBridge::OnStartLEListenDone(
    const StartLEListenCallback& callback,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  DCHECK(CalledOnValidThread());
  advertisment_ = advertisement;
  callback.Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::OnStartLEListenError(
    const StartLEListenCallback& callback,
    BluetoothAdvertisement::ErrorCode error_code) {
  DCHECK(CalledOnValidThread());
  advertisment_ = nullptr;
  callback.Run(mojom::BluetoothGattStatus::GATT_FAILURE);
}

void ArcBluetoothBridge::StartLEListen(const StartLEListenCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::unique_ptr<BluetoothAdvertisement::Data> adv_data =
      base::WrapUnique(new BluetoothAdvertisement::Data(
          BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST));
  bluetooth_adapter_->RegisterAdvertisement(
      std::move(adv_data), base::Bind(&ArcBluetoothBridge::OnStartLEListenDone,
                                      weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ArcBluetoothBridge::OnStartLEListenError,
                 weak_factory_.GetWeakPtr(), callback));
}

void ArcBluetoothBridge::OnStopLEListenDone(
    const StopLEListenCallback& callback) {
  DCHECK(CalledOnValidThread());
  advertisment_ = nullptr;
  callback.Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::OnStopLEListenError(
    const StopLEListenCallback& callback,
    BluetoothAdvertisement::ErrorCode error_code) {
  DCHECK(CalledOnValidThread());
  advertisment_ = nullptr;
  callback.Run(mojom::BluetoothGattStatus::GATT_FAILURE);
}

void ArcBluetoothBridge::StopLEListen(const StopLEListenCallback& callback) {
  if (!advertisment_) {
    OnStopLEListenError(
        callback,
        BluetoothAdvertisement::ErrorCode::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }
  advertisment_->Unregister(base::Bind(&ArcBluetoothBridge::OnStopLEListenDone,
                                       weak_factory_.GetWeakPtr(), callback),
                            base::Bind(&ArcBluetoothBridge::OnStopLEListenError,
                                       weak_factory_.GetWeakPtr(), callback));
}

void ArcBluetoothBridge::GetGattDB(mojom::BluetoothAddressPtr remote_addr) {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  mojo::Array<mojom::BluetoothGattDBElementPtr> db;
  for (auto* service : device->GetGattServices()) {
    mojom::BluetoothGattDBElementPtr service_element = CreateGattDBElement(
        service->IsPrimary()
            ? mojom::BluetoothGattDBAttributeType::BTGATT_DB_PRIMARY_SERVICE
            : mojom::BluetoothGattDBAttributeType::BTGATT_DB_SECONDARY_SERVICE,
        service);

    const auto& characteristics = service->GetCharacteristics();
    if (characteristics.size() > 0) {
      const auto& descriptors = characteristics.back()->GetDescriptors();
      service_element->start_handle =
          ConvertGattIdentifierToId(characteristics.front()->GetIdentifier());
      service_element->end_handle = ConvertGattIdentifierToId(
          descriptors.size() > 0 ? descriptors.back()->GetIdentifier()
                                 : characteristics.back()->GetIdentifier());
    }
    db.push_back(std::move(service_element));

    for (auto* characteristic : characteristics) {
      mojom::BluetoothGattDBElementPtr characteristic_element =
          CreateGattDBElement(
              mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
              characteristic);
      characteristic_element->properties = characteristic->GetProperties();
      db.push_back(std::move(characteristic_element));

      for (auto* descriptor : characteristic->GetDescriptors()) {
        db.push_back(CreateGattDBElement(
            mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
            descriptor));
      }
    }
  }

  arc_bridge_service()->bluetooth()->instance()->OnGetGattDB(
      std::move(remote_addr), std::move(db));
}

BluetoothRemoteGattCharacteristic* ArcBluetoothBridge::FindGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id) const {
  DCHECK(remote_addr);
  DCHECK(service_id);
  DCHECK(char_id);

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  if (!device)
    return nullptr;

  BluetoothRemoteGattService* service = FindGattAttributeByUuid(
      device->GetGattServices(), service_id->id->uuid.To<BluetoothUUID>());
  if (!service)
    return nullptr;

  return FindGattAttributeByUuid(service->GetCharacteristics(),
                                 char_id->uuid.To<BluetoothUUID>());
}

BluetoothRemoteGattDescriptor* ArcBluetoothBridge::FindGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id) const {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (!characteristic)
    return nullptr;

  return FindGattAttributeByUuid(characteristic->GetDescriptors(),
                                 desc_id->uuid.To<BluetoothUUID>());
}

void ArcBluetoothBridge::ReadGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    const ReadGattCharacteristicCallback& callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  DCHECK(characteristic);
  DCHECK(characteristic->GetPermissions() & kGattReadPermission);

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&OnGattReadDone, callback),
      base::Bind(&OnGattReadError, callback));
}

void ArcBluetoothBridge::WriteGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattValuePtr value,
    const WriteGattCharacteristicCallback& callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  DCHECK(characteristic);
  DCHECK(characteristic->GetPermissions() & kGattWritePermission);

  characteristic->WriteRemoteCharacteristic(
      value->value.To<std::vector<uint8_t>>(),
      base::Bind(&OnGattOperationDone, callback),
      base::Bind(&OnGattOperationError, callback));
}

void ArcBluetoothBridge::ReadGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id,
    const ReadGattDescriptorCallback& callback) {
  BluetoothRemoteGattDescriptor* descriptor =
      FindGattDescriptor(std::move(remote_addr), std::move(service_id),
                         std::move(char_id), std::move(desc_id));
  DCHECK(descriptor);
  DCHECK(descriptor->GetPermissions() & kGattReadPermission);

  descriptor->ReadRemoteDescriptor(base::Bind(&OnGattReadDone, callback),
                                   base::Bind(&OnGattReadError, callback));
}

void ArcBluetoothBridge::WriteGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id,
    mojom::BluetoothGattValuePtr value,
    const WriteGattDescriptorCallback& callback) {
  BluetoothRemoteGattDescriptor* descriptor =
      FindGattDescriptor(std::move(remote_addr), std::move(service_id),
                         std::move(char_id), std::move(desc_id));
  DCHECK(descriptor);
  DCHECK(descriptor->GetPermissions() & kGattWritePermission);

  // To register / deregister GATT notification, we need to
  // 1) Write to CCC Descriptor to enable/disable the notification
  // 2) Ask BT hw to register / deregister the notification
  // The Chrome API groups both steps into one API, and does not support writing
  // directly to the CCC Descriptor. Therefore, until we fix
  // https://crbug.com/622832, we return successfully when we encounter this.
  // TODO(http://crbug.com/622832)
  if (descriptor->GetUUID() ==
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()) {
    OnGattOperationDone(callback);
    return;
  }

  descriptor->WriteRemoteDescriptor(
      value->value.To<std::vector<uint8_t>>(),
      base::Bind(&OnGattOperationDone, callback),
      base::Bind(&OnGattOperationError, callback));
}

void ArcBluetoothBridge::OnGattNotifyStartDone(
    const RegisterForGattNotificationCallback& callback,
    const std::string char_string_id,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  DCHECK(CalledOnValidThread());
  notification_session_[char_string_id] = std::move(notify_session);
  callback.Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::RegisterForGattNotification(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    const RegisterForGattNotificationCallback& callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));

  if (!characteristic) {
    LOG(WARNING) << __func__ << " Characteristic is not existed.";
    return;
  }

  characteristic->StartNotifySession(
      base::Bind(&ArcBluetoothBridge::OnGattNotifyStartDone,
                 weak_factory_.GetWeakPtr(), callback,
                 characteristic->GetIdentifier()),
      base::Bind(&OnGattOperationError, callback));
}

void ArcBluetoothBridge::DeregisterForGattNotification(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    const DeregisterForGattNotificationCallback& callback) {
  DCHECK(CalledOnValidThread());

  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));

  if (!characteristic) {
    LOG(WARNING) << __func__ << " Characteristic is not existed.";
    return;
  }

  std::string char_id_str = characteristic->GetIdentifier();
  std::unique_ptr<BluetoothGattNotifySession> notify =
      std::move(notification_session_[char_id_str]);
  notification_session_.erase(char_id_str);
  notify->Stop(base::Bind(&OnGattOperationDone, callback));
}

void ArcBluetoothBridge::ReadRemoteRssi(
    mojom::BluetoothAddressPtr remote_addr,
    const ReadRemoteRssiCallback& callback) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  int rssi = device->GetInquiryRSSI();
  callback.Run(rssi);
}

bool ArcBluetoothBridge::IsGattServerAttributeHandleAvailable(int need) {
  return gatt_server_attribute_next_handle_ + need <= kMaxGattAttributeHandle;
}

int32_t ArcBluetoothBridge::GetNextGattServerAttributeHandle() {
  return IsGattServerAttributeHandleAvailable(1)
             ? ++gatt_server_attribute_next_handle_
             : kInvalidGattAttributeHandle;
}

template <class LocalGattAttribute>
int32_t ArcBluetoothBridge::CreateGattAttributeHandle(
    LocalGattAttribute* attribute) {
  DCHECK(CalledOnValidThread());
  if (!attribute)
    return kInvalidGattAttributeHandle;
  int32_t handle = GetNextGattServerAttributeHandle();
  if (handle == kInvalidGattAttributeHandle)
    return kInvalidGattAttributeHandle;
  const std::string& identifier = attribute->GetIdentifier();
  gatt_identifier_[handle] = identifier;
  gatt_handle_[identifier] = handle;
  return handle;
}

void ArcBluetoothBridge::AddService(mojom::BluetoothGattServiceIDPtr service_id,
                                    int32_t num_handles,
                                    const AddServiceCallback& callback) {
  if (!IsGattServerAttributeHandleAvailable(num_handles)) {
    callback.Run(kInvalidGattAttributeHandle);
    return;
  }
  base::WeakPtr<BluetoothLocalGattService> service =
      BluetoothLocalGattService::Create(
          bluetooth_adapter_.get(), service_id->id->uuid.To<BluetoothUUID>(),
          service_id->is_primary, nullptr /* included_service */,
          this /* delegate */);
  callback.Run(CreateGattAttributeHandle(service.get()));
}

void ArcBluetoothBridge::AddCharacteristic(
    int32_t service_handle,
    mojom::BluetoothUUIDPtr uuid,
    int32_t properties,
    int32_t permissions,
    const AddCharacteristicCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  if (!IsGattServerAttributeHandleAvailable(1)) {
    callback.Run(kInvalidGattAttributeHandle);
    return;
  }
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic =
      BluetoothLocalGattCharacteristic::Create(
          uuid.To<BluetoothUUID>(), properties, permissions,
          bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]));
  int32_t characteristic_handle =
      CreateGattAttributeHandle(characteristic.get());
  last_characteristic_[service_handle] = characteristic_handle;
  callback.Run(characteristic_handle);
}

void ArcBluetoothBridge::AddDescriptor(int32_t service_handle,
                                       mojom::BluetoothUUIDPtr uuid,
                                       int32_t permissions,
                                       const AddDescriptorCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!IsGattServerAttributeHandleAvailable(1)) {
    callback.Run(kInvalidGattAttributeHandle);
    return;
  }
  // Chrome automatically adds a CCC Descriptor to a characteristic when needed.
  // We will generate a bogus handle for Android.
  if (uuid.To<BluetoothUUID>() ==
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()) {
    int32_t handle = GetNextGattServerAttributeHandle();
    callback.Run(handle);
    return;
  }

  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);
  // Since the Android API does not give information about which characteristic
  // is the parent of the new descriptor, we assume that it would be the last
  // characteristic that was added to the given service. This matches the
  // Android framework code at android/bluetooth/BluetoothGattServer.java#594.
  // Link: https://goo.gl/cJZl1u
  DCHECK(last_characteristic_.find(service_handle) !=
         last_characteristic_.end());
  int32_t last_characteristic_handle = last_characteristic_[service_handle];

  DCHECK(gatt_identifier_.find(last_characteristic_handle) !=
         gatt_identifier_.end());
  BluetoothLocalGattCharacteristic* characteristic =
      service->GetCharacteristic(gatt_identifier_[last_characteristic_handle]);
  DCHECK(characteristic);

  base::WeakPtr<BluetoothLocalGattDescriptor> descriptor =
      BluetoothLocalGattDescriptor::Create(uuid.To<BluetoothUUID>(),
                                           permissions, characteristic);
  callback.Run(CreateGattAttributeHandle(descriptor.get()));
}

void ArcBluetoothBridge::StartService(int32_t service_handle,
                                      const StartServiceCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);
  service->Register(base::Bind(&OnGattOperationDone, callback),
                    base::Bind(&OnGattOperationError, callback));
}

void ArcBluetoothBridge::StopService(int32_t service_handle,
                                     const StopServiceCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);
  service->Unregister(base::Bind(&OnGattOperationDone, callback),
                      base::Bind(&OnGattOperationError, callback));
}

void ArcBluetoothBridge::DeleteService(int32_t service_handle,
                                       const DeleteServiceCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);
  gatt_identifier_.erase(service_handle);
  gatt_handle_.erase(service->GetIdentifier());
  service->Delete();
  OnGattOperationDone(callback);
}

void ArcBluetoothBridge::SendIndication(
    int32_t attribute_handle,
    mojom::BluetoothAddressPtr address,
    bool confirm,
    mojo::Array<uint8_t> value,
    const SendIndicationCallback& callback) {}

void ArcBluetoothBridge::OpenBluetoothSocket(
    const OpenBluetoothSocketCallback& callback) {
  base::ScopedFD sock(socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM));
  if (!sock.is_valid()) {
    LOG(ERROR) << "Failed to open socket.";
    callback.Run(mojo::ScopedHandle());
    return;
  }
  mojo::edk::ScopedPlatformHandle platform_handle{
      mojo::edk::PlatformHandle(sock.release())};
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      std::move(platform_handle), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle());
    return;
  }
  mojo::ScopedHandle scoped_handle{mojo::Handle(wrapped_handle)};

  callback.Run(std::move(scoped_handle));
}

void ArcBluetoothBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                       mojom::BluetoothUUIDPtr target_uuid) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());

  bluez::BluetoothDeviceBlueZ* device_bluez =
      static_cast<bluez::BluetoothDeviceBlueZ*>(device);

  mojom::BluetoothAddressPtr remote_addr_clone = remote_addr.Clone();
  mojom::BluetoothUUIDPtr target_uuid_clone = target_uuid.Clone();

  device_bluez->GetServiceRecords(
      base::Bind(&ArcBluetoothBridge::OnGetServiceRecordsDone,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr),
                 base::Passed(&target_uuid)),
      base::Bind(&ArcBluetoothBridge::OnGetServiceRecordsError,
                 weak_factory_.GetWeakPtr(), base::Passed(&remote_addr_clone),
                 base::Passed(&target_uuid_clone)));
}

void ArcBluetoothBridge::CreateSdpRecord(
    mojom::BluetoothSdpRecordPtr record_mojo,
    const CreateSdpRecordCallback& callback) {
  auto record = record_mojo.To<bluez::BluetoothServiceRecordBlueZ>();

  // Check if ServiceClassIDList attribute (attribute ID 0x0001) is included
  // after type conversion, since it is mandatory for creating a service record.
  if (!record.IsAttributePresented(kServiceClassIDListAttributeID)) {
    mojom::BluetoothCreateSdpRecordResultPtr result =
        mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::FAIL;
    callback.Run(std::move(result));
    return;
  }

  bluetooth_adapter_->CreateServiceRecord(
      record, base::Bind(&OnCreateServiceRecordDone, callback),
      base::Bind(&OnCreateServiceRecordError, callback));
}

void ArcBluetoothBridge::RemoveSdpRecord(
    uint32_t service_handle,
    const RemoveSdpRecordCallback& callback) {
  bluetooth_adapter_->RemoveServiceRecord(
      service_handle, base::Bind(&OnRemoveServiceRecordDone, callback),
      base::Bind(&OnRemoveServiceRecordError, callback));
}

void ArcBluetoothBridge::OnDiscoveryError() {
  LOG(WARNING) << "failed to change discovery state";
}

void ArcBluetoothBridge::OnPairing(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth()->instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::BONDING);
}

void ArcBluetoothBridge::OnPairedDone(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth()->instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::BONDED);
}

void ArcBluetoothBridge::OnPairedError(
    mojom::BluetoothAddressPtr addr,
    BluetoothDevice::ConnectErrorCode error_code) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth()->instance()->OnBondStateChanged(
      mojom::BluetoothStatus::FAIL, std::move(addr),
      mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetDone(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth()->instance()->OnBondStateChanged(
      mojom::BluetoothStatus::SUCCESS, std::move(addr),
      mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetError(mojom::BluetoothAddressPtr addr) const {
  if (!HasBluetoothInstance())
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  mojom::BluetoothBondState bond_state = mojom::BluetoothBondState::NONE;
  if (device && device->IsPaired()) {
    bond_state = mojom::BluetoothBondState::BONDED;
  }
  arc_bridge_service()->bluetooth()->instance()->OnBondStateChanged(
      mojom::BluetoothStatus::FAIL, std::move(addr), bond_state);
}

mojo::Array<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetDeviceProperties(mojom::BluetoothPropertyType type,
                                        BluetoothDevice* device) const {
  mojo::Array<mojom::BluetoothPropertyPtr> properties;

  if (!device) {
    return properties;
  }

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    // TODO(615720): Use the upcoming GetName (was GetDeviceName).
    btp->set_bdname(
        mojo::String::From(base::UTF16ToUTF8(device->GetNameForDisplay())));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdaddr(mojom::BluetoothAddress::From(device->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    std::vector<BluetoothUUID> uuids = device->GetUUIDs();
    mojo::Array<mojom::BluetoothUUIDPtr> uuid_results =
        mojo::Array<mojom::BluetoothUUIDPtr>::New(0);

    for (auto& uuid : uuids) {
      uuid_results.push_back(mojom::BluetoothUUID::From(uuid));
    }

    btp->set_uuids(std::move(uuid_results));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_class(device->GetBluetoothClass());
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_type(device->GetType());
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_FRIENDLY_NAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    // TODO(615720): Use the upcoming GetName (was GetDeviceName).
    btp->set_remote_friendly_name(
        mojo::String::From(base::UTF16ToUTF8(device->GetNameForDisplay())));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_RSSI) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_remote_rssi(device->GetInquiryRSSI());
    properties.push_back(std::move(btp));
  }
  // TODO(smbarber): Add remote version info

  return properties;
}

mojo::Array<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetAdapterProperties(
    mojom::BluetoothPropertyType type) const {
  mojo::Array<mojom::BluetoothPropertyPtr> properties;

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    std::string name = bluetooth_adapter_->GetName();
    btp->set_bdname(mojo::String(name));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_bdaddr(
        mojom::BluetoothAddress::From(bluetooth_adapter_->GetAddress()));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    // TODO(smbarber): Fill in once GetUUIDs is available for the adapter.
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual adapter class
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_class(0);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    // TODO(smbarber): Populate with the actual adapter type
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_device_type(device::BLUETOOTH_TRANSPORT_DUAL);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_SCAN_MODE) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    mojom::BluetoothScanMode scan_mode = mojom::BluetoothScanMode::CONNECTABLE;

    if (bluetooth_adapter_->IsDiscoverable())
      scan_mode = mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE;

    btp->set_adapter_scan_mode(scan_mode);
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_BONDED_DEVICES) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();

    mojo::Array<mojom::BluetoothAddressPtr> bonded_devices =
        mojo::Array<mojom::BluetoothAddressPtr>::New(0);

    for (auto device : devices) {
      if (device->IsPaired())
        continue;

      mojom::BluetoothAddressPtr addr =
          mojom::BluetoothAddress::From(device->GetAddress());
      bonded_devices.push_back(std::move(addr));
    }

    btp->set_bonded_devices(std::move(bonded_devices));
    properties.push_back(std::move(btp));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_DISCOVERY_TIMEOUT) {
    mojom::BluetoothPropertyPtr btp = mojom::BluetoothProperty::New();
    btp->set_discovery_timeout(bluetooth_adapter_->GetDiscoverableTimeout());
    properties.push_back(std::move(btp));
  }

  return properties;
}

// Android support 5 types of Advertising Data.
// However Chrome didn't expose AdvertiseFlag and ManufacturerData.
// So we will only expose local_name, service_uuids and service_data.
// Note that we need to use UUID 16 bits because Android does not support
// UUID 128 bits.
// TODO(crbug.com/618442) Make Chrome expose missing data.
mojo::Array<mojom::BluetoothAdvertisingDataPtr>
ArcBluetoothBridge::GetAdvertisingData(BluetoothDevice* device) const {
  mojo::Array<mojom::BluetoothAdvertisingDataPtr> advertising_data;

  // LocalName
  mojom::BluetoothAdvertisingDataPtr local_name =
      mojom::BluetoothAdvertisingData::New();
  local_name->set_local_name(base::UTF16ToUTF8(device->GetNameForDisplay()));
  advertising_data.push_back(std::move(local_name));

  // ServiceUuid
  BluetoothDevice::UUIDList uuid_list = device->GetUUIDs();
  if (uuid_list.size() > 0) {
    mojom::BluetoothAdvertisingDataPtr service_uuids_16 =
        mojom::BluetoothAdvertisingData::New();
    mojo::Array<uint16_t> uuid16s;
    for (auto& uuid : uuid_list) {
      uuid16s.push_back(GetUUID16(uuid));
    }
    service_uuids_16->set_service_uuids_16(std::move(uuid16s));
    advertising_data.push_back(std::move(service_uuids_16));
  }

  // Service data
  for (auto& uuid : device->GetServiceDataUUIDs()) {
    base::BinaryValue* data = device->GetServiceData(uuid);
    if (data->GetSize() == 0)
      continue;
    std::string data_str;
    if (!data->GetAsString(&data_str))
      continue;

    mojom::BluetoothAdvertisingDataPtr service_data_element =
        mojom::BluetoothAdvertisingData::New();
    mojom::BluetoothServiceDataPtr service_data =
        mojom::BluetoothServiceData::New();

    service_data->uuid_16bit = GetUUID16(uuid);
    for (auto& c : data_str) {
      service_data->data.push_back(c);
    }
    service_data_element->set_service_data(std::move(service_data));
    advertising_data.push_back(std::move(service_data_element));
  }

  return advertising_data;
}

void ArcBluetoothBridge::SendCachedDevicesFound() const {
  // Send devices that have already been discovered, but aren't connected.
  if (!HasBluetoothInstance())
    return;

  BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
  for (auto device : devices) {
    if (device->IsPaired())
      continue;

    mojo::Array<mojom::BluetoothPropertyPtr> properties =
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device);

    arc_bridge_service()->bluetooth()->instance()->OnDeviceFound(
        std::move(properties));

    if (arc_bridge_service()->bluetooth()->version() >= kMinBtleVersion) {
      mojom::BluetoothAddressPtr addr =
          mojom::BluetoothAddress::From(device->GetAddress());
      int rssi = device->GetInquiryRSSI();
      mojo::Array<mojom::BluetoothAdvertisingDataPtr> adv_data =
          GetAdvertisingData(device);
      arc_bridge_service()->bluetooth()->instance()->OnLEDeviceFound(
          std::move(addr), rssi, std::move(adv_data));
    }
  }
}

bool ArcBluetoothBridge::HasBluetoothInstance() const {
  if (!arc_bridge_service()->bluetooth()->instance()) {
    LOG(WARNING) << "no Bluetooth instance available";
    return false;
  }

  return true;
}

void ArcBluetoothBridge::SendCachedPairedDevices() const {
  DCHECK(bluetooth_adapter_);
  if (!HasBluetoothInstance())
    return;

  BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
  for (auto device : devices) {
    if (!device->IsPaired())
      continue;

    mojo::Array<mojom::BluetoothPropertyPtr> properties =
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device);

    arc_bridge_service()->bluetooth()->instance()->OnDeviceFound(
        std::move(properties));

    mojom::BluetoothAddressPtr addr =
        mojom::BluetoothAddress::From(device->GetAddress());

    if (arc_bridge_service()->bluetooth()->version() >= kMinBtleVersion) {
      int rssi = device->GetInquiryRSSI();
      mojo::Array<mojom::BluetoothAdvertisingDataPtr> adv_data =
          GetAdvertisingData(device);
      arc_bridge_service()->bluetooth()->instance()->OnLEDeviceFound(
          addr->Clone(), rssi, std::move(adv_data));
    }

    // OnBondStateChanged must be called with mojom::BluetoothBondState::BONDING
    // to make sure the bond state machine on Android is ready to take the
    // pair-done event. Otherwise the pair-done event will be dropped as an
    // invalid change of paired status.
    OnPairing(addr->Clone());
    OnPairedDone(std::move(addr));
  }
}

void ArcBluetoothBridge::OnGetServiceRecordsDone(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothUUIDPtr target_uuid,
    const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez) {
  if (!CheckBluetoothInstanceVersion(kMinSdpSupportVersion))
    return;

  if (!HasBluetoothInstance())
    return;

  arc_bridge_service()->bluetooth()->instance()->OnGetSdpRecords(
      mojom::BluetoothStatus::SUCCESS, std::move(remote_addr),
      std::move(target_uuid),
      mojo::Array<mojom::BluetoothSdpRecordPtr>::From(records_bluez));
}

void ArcBluetoothBridge::OnGetServiceRecordsError(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothUUIDPtr target_uuid,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  if (!HasBluetoothInstance())
    return;

  if (!CheckBluetoothInstanceVersion(kMinSdpSupportVersion))
    return;

  mojom::BluetoothStatus status;

  switch (error_code) {
    case bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY:
      status = mojom::BluetoothStatus::NOT_READY;
      break;
    case bluez::BluetoothServiceRecordBlueZ::ErrorCode::
        ERROR_DEVICE_DISCONNECTED:
      status = mojom::BluetoothStatus::RMT_DEV_DOWN;
      break;
    default:
      status = mojom::BluetoothStatus::FAIL;
      break;
  }

  arc_bridge_service()->bluetooth()->instance()->OnGetSdpRecords(
      status, std::move(remote_addr), std::move(target_uuid),
      mojo::Array<mojom::BluetoothSdpRecordPtr>::New(0));
}

bool ArcBluetoothBridge::CheckBluetoothInstanceVersion(
    uint32_t version_need) const {
  uint32_t version = arc_bridge_service()->bluetooth()->version();
  if (version >= version_need)
    return true;
  LOG(WARNING) << "Bluetooth instance is too old (version " << version
               << ") need version " << version_need;
  return false;
}

bool ArcBluetoothBridge::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace arc
