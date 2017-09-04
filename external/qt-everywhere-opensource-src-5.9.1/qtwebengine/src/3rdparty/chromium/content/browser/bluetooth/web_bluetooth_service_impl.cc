// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ID Not In Map Note:
// A service, characteristic, or descriptor ID not in the corresponding
// WebBluetoothServiceImpl map [service_id_to_device_address_,
// characteristic_id_to_service_id_, descriptor_to_characteristic_] implies a
// hostile renderer because a renderer obtains the corresponding ID from this
// class and it will be added to the map at that time.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/bluetooth/web_bluetooth_device_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

using device::BluetoothAdapterFactoryWrapper;
using device::BluetoothUUID;

namespace content {

namespace {

blink::mojom::WebBluetoothResult TranslateConnectErrorAndRecord(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNKNOWN);
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_ERROR;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::IN_PROGRESS);
      return blink::mojom::WebBluetoothResult::CONNECT_ALREADY_IN_PROGRESS;
    case device::BluetoothDevice::ERROR_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::FAILED);
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_FAILED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_FAILED;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_CANCELED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_CANCELED;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_REJECTED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_REJECTED;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_TIMEOUT);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_TIMEOUT;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNSUPPORTED_DEVICE);
      return blink::mojom::WebBluetoothResult::CONNECT_UNSUPPORTED_DEVICE;
    case device::BluetoothDevice::ERROR_ATTRIBUTE_LENGTH_INVALID:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::ATTRIBUTE_LENGTH_INVALID);
      return blink::mojom::WebBluetoothResult::CONNECT_ATTRIBUTE_LENGTH_INVALID;
    case device::BluetoothDevice::ERROR_CONNECTION_CONGESTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::CONNECTION_CONGESTED);
      return blink::mojom::WebBluetoothResult::CONNECT_CONNECTION_CONGESTED;
    case device::BluetoothDevice::ERROR_INSUFFICIENT_ENCRYPTION:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::INSUFFICIENT_ENCRYPTION);
      return blink::mojom::WebBluetoothResult::CONNECT_INSUFFICIENT_ENCRYPTION;
    case device::BluetoothDevice::ERROR_OFFSET_INVALID:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::OFFSET_INVALID);
      return blink::mojom::WebBluetoothResult::CONNECT_OFFSET_INVALID;
    case device::BluetoothDevice::ERROR_READ_NOT_PERMITTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::READ_NOT_PERMITTED);
      return blink::mojom::WebBluetoothResult::CONNECT_READ_NOT_PERMITTED;
    case device::BluetoothDevice::ERROR_REQUEST_NOT_SUPPORTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::REQUEST_NOT_SUPPORTED);
      return blink::mojom::WebBluetoothResult::CONNECT_REQUEST_NOT_SUPPORTED;
    case device::BluetoothDevice::ERROR_WRITE_NOT_PERMITTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::WRITE_NOT_PERMITTED);
      return blink::mojom::WebBluetoothResult::CONNECT_WRITE_NOT_PERMITTED;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      return blink::mojom::WebBluetoothResult::UNTRANSLATED_CONNECT_ERROR_CODE;
  }
  NOTREACHED();
  return blink::mojom::WebBluetoothResult::UNTRANSLATED_CONNECT_ERROR_CODE;
}

blink::mojom::WebBluetoothResult TranslateGATTErrorAndRecord(
    device::BluetoothRemoteGattService::GattErrorCode error_code,
    UMAGATTOperation operation) {
  switch (error_code) {
    case device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::UNKNOWN);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_ERROR;
    case device::BluetoothRemoteGattService::GATT_ERROR_FAILED:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::FAILED);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_FAILURE;
    case device::BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::IN_PROGRESS);
      return blink::mojom::WebBluetoothResult::GATT_OPERATION_IN_PROGRESS;
    case device::BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::INVALID_LENGTH);
      return blink::mojom::WebBluetoothResult::GATT_INVALID_ATTRIBUTE_LENGTH;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PERMITTED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PERMITTED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_AUTHORIZED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_AUTHORIZED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_PAIRED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PAIRED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PAIRED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_SUPPORTED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED;
  }
  NOTREACHED();
  return blink::mojom::WebBluetoothResult::GATT_UNTRANSLATED_ERROR_CODE;
}

// TODO(ortuno): This should really be a BluetoothDevice method.
// Replace when implemented. http://crbug.com/552022
std::vector<device::BluetoothRemoteGattCharacteristic*>
GetCharacteristicsByUUID(device::BluetoothRemoteGattService* service,
                         const BluetoothUUID& characteristic_uuid) {
  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics;
  VLOG(1) << "Looking for characteristic: "
          << characteristic_uuid.canonical_value();
  for (device::BluetoothRemoteGattCharacteristic* characteristic :
       service->GetCharacteristics()) {
    VLOG(1) << "Characteristic in cache: "
            << characteristic->GetUUID().canonical_value();
    if (characteristic->GetUUID() == characteristic_uuid) {
      characteristics.push_back(characteristic);
    }
  }
  return characteristics;
}

// TODO(ortuno): This should really be a BluetoothDevice method.
// Replace when implemented. http://crbug.com/552022
std::vector<device::BluetoothRemoteGattService*> GetPrimaryServicesByUUID(
    device::BluetoothDevice* device,
    const BluetoothUUID& service_uuid) {
  std::vector<device::BluetoothRemoteGattService*> services;
  VLOG(1) << "Looking for service: " << service_uuid.canonical_value();
  for (device::BluetoothRemoteGattService* service :
       device->GetGattServices()) {
    VLOG(1) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->GetUUID() == service_uuid && service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

// TODO(ortuno): This should really be a BluetoothDevice method.
// Replace when implemented. http://crbug.com/552022
std::vector<device::BluetoothRemoteGattService*> GetPrimaryServices(
    device::BluetoothDevice* device) {
  std::vector<device::BluetoothRemoteGattService*> services;
  VLOG(1) << "Looking for services.";
  for (device::BluetoothRemoteGattService* service :
       device->GetGattServices()) {
    VLOG(1) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

}  // namespace

WebBluetoothServiceImpl::WebBluetoothServiceImpl(
    RenderFrameHost* render_frame_host,
    blink::mojom::WebBluetoothServiceRequest request)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      connected_devices_(new FrameConnectedBluetoothDevices(render_frame_host)),
      render_frame_host_(render_frame_host),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents());
}

WebBluetoothServiceImpl::~WebBluetoothServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearState();
}

void WebBluetoothServiceImpl::SetClientConnectionErrorHandler(
    base::Closure closure) {
  binding_.set_connection_error_handler(closure);
}

bool WebBluetoothServiceImpl::IsDevicePaired(
    const std::string& device_address) {
  return allowed_devices_map_.GetDeviceId(GetOrigin(), device_address) !=
         nullptr;
}

void WebBluetoothServiceImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      navigation_handle->GetRenderFrameHost() == render_frame_host_ &&
      !navigation_handle->IsSamePage()) {
    ClearState();
  }
}

void WebBluetoothServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AdapterPoweredChanged(powered);
  }
}

void WebBluetoothServiceImpl::DeviceAdded(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }
}

void WebBluetoothServiceImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  if (!device->IsGattConnected()) {
    base::Optional<WebBluetoothDeviceId> device_id =
        connected_devices_->CloseConnectionToDeviceWithAddress(
            device->GetAddress());
    if (device_id && client_) {
      client_->GattServerDisconnected(device_id.value());
    }
  }
}

void WebBluetoothServiceImpl::GattServicesDiscovered(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string& device_address = device->GetAddress();
  VLOG(1) << "Services discovered for device: " << device_address;

  auto iter = pending_primary_services_requests_.find(device_address);
  if (iter == pending_primary_services_requests_.end()) {
    return;
  }
  std::vector<PrimaryServicesRequestCallback> requests =
      std::move(iter->second);
  pending_primary_services_requests_.erase(iter);

  for (const PrimaryServicesRequestCallback& request : requests) {
    request.Run(device);
  }

  // Sending get-service responses unexpectedly queued another request.
  DCHECK(
      !base::ContainsKey(pending_primary_services_requests_, device_address));
}

void WebBluetoothServiceImpl::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // Don't notify of characteristics that we haven't returned.
  if (!base::ContainsKey(characteristic_id_to_service_id_,
                         characteristic->GetIdentifier())) {
    return;
  }

  // On Chrome OS and Linux, GattCharacteristicValueChanged is called before the
  // success callback for ReadRemoteCharacteristic is called, which could result
  // in an event being fired before the readValue promise is resolved.
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&WebBluetoothServiceImpl::NotifyCharacteristicValueChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     characteristic->GetIdentifier(), value))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

void WebBluetoothServiceImpl::NotifyCharacteristicValueChanged(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value) {
  if (client_) {
    client_->RemoteCharacteristicValueChanged(characteristic_instance_id,
                                              value);
  }
}

void WebBluetoothServiceImpl::SetClient(
    blink::mojom::WebBluetoothServiceClientAssociatedPtrInfo client) {
  DCHECK(!client_.get());
  client_.Bind(std::move(client));
}

void WebBluetoothServiceImpl::RequestDevice(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const RequestDeviceCallback& callback) {
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::REQUEST_DEVICE);
  RecordRequestDeviceOptions(options);

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergyAvailable()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this, base::Bind(&WebBluetoothServiceImpl::RequestDeviceImpl,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::Passed(std::move(options)), callback));
      return;
    }
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    callback.Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE,
        nullptr /* device */);
    return;
  }
  RequestDeviceImpl(std::move(options), callback, GetAdapter());
}

void WebBluetoothServiceImpl::RemoteServerConnect(
    const WebBluetoothDeviceId& device_id,
    const RemoteServerConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::CONNECT_GATT);

  const CacheQueryResult query_result = QueryCacheForDevice(device_id);

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordConnectGATTOutcome(query_result.outcome);
    callback.Run(query_result.GetWebResult());
    return;
  }

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    VLOG(1) << "Already connected.";
    callback.Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  // It's possible for WebBluetoothServiceImpl to issue two successive
  // connection requests for which it would get two successive responses
  // and consequently try to insert two BluetoothGattConnections for the
  // same device. WebBluetoothServiceImpl should reject or queue connection
  // requests if there is a pending connection already, but the platform
  // abstraction doesn't currently support checking for pending connections.
  // TODO(ortuno): CHECK that this never happens once the platform
  // abstraction allows to check for pending connections.
  // http://crbug.com/583544
  const base::TimeTicks start_time = base::TimeTicks::Now();
  query_result.device->CreateGattConnection(
      base::Bind(&WebBluetoothServiceImpl::OnCreateGATTConnectionSuccess,
                 weak_ptr_factory_.GetWeakPtr(), device_id, start_time,
                 callback),
      base::Bind(&WebBluetoothServiceImpl::OnCreateGATTConnectionFailed,
                 weak_ptr_factory_.GetWeakPtr(), start_time, callback));
}

void WebBluetoothServiceImpl::RemoteServerDisconnect(
    const WebBluetoothDeviceId& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::REMOTE_GATT_SERVER_DISCONNECT);

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    VLOG(1) << "Disconnecting device: " << device_id.str();
    connected_devices_->CloseConnectionToDeviceWithId(device_id);
  }
}

void WebBluetoothServiceImpl::RemoteServerGetPrimaryServices(
    const WebBluetoothDeviceId& device_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& services_uuid,
    const RemoteServerGetPrimaryServicesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE
          ? UMAWebBluetoothFunction::GET_PRIMARY_SERVICE
          : UMAWebBluetoothFunction::GET_PRIMARY_SERVICES);
  RecordGetPrimaryServicesServices(quantity, services_uuid);

  if (!allowed_devices_map_.IsOriginAllowedToAccessAtLeastOneService(
          GetOrigin(), device_id)) {
    callback.Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE,
        base::nullopt /* service */);
    return;
  }

  if (services_uuid &&
      !allowed_devices_map_.IsOriginAllowedToAccessService(
          GetOrigin(), device_id, services_uuid.value())) {
    callback.Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_SERVICE,
        base::nullopt /* service */);
    return;
  }

  const CacheQueryResult query_result = QueryCacheForDevice(device_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetPrimaryServicesOutcome(quantity, query_result.outcome);
    callback.Run(query_result.GetWebResult(), base::nullopt /* service */);
    return;
  }

  const std::string& device_address = query_result.device->GetAddress();

  // We can't know if a service is present or not until GATT service discovery
  // is complete for the device.
  if (query_result.device->IsGattServicesDiscoveryComplete()) {
    RemoteServerGetPrimaryServicesImpl(device_id, quantity, services_uuid,
                                       callback, query_result.device);
    return;
  }

  VLOG(1) << "Services not yet discovered.";
  pending_primary_services_requests_[device_address].push_back(base::Bind(
      &WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl,
      base::Unretained(this), device_id, quantity, services_uuid, callback));
}

void WebBluetoothServiceImpl::RemoteServiceGetCharacteristics(
    const std::string& service_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& characteristics_uuid,
    const RemoteServiceGetCharacteristicsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordWebBluetoothFunctionCall(
      quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE
          ? UMAWebBluetoothFunction::SERVICE_GET_CHARACTERISTIC
          : UMAWebBluetoothFunction::SERVICE_GET_CHARACTERISTICS);
  RecordGetCharacteristicsCharacteristic(quantity, characteristics_uuid);

  if (characteristics_uuid &&
      BluetoothBlocklist::Get().IsExcluded(characteristics_uuid.value())) {
    RecordGetCharacteristicsOutcome(quantity,
                                    UMAGetCharacteristicOutcome::BLOCKLISTED);
    callback.Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_CHARACTERISTIC_UUID,
        base::nullopt /* characteristics */);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForService(service_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetCharacteristicsOutcome(quantity, query_result.outcome);
    callback.Run(query_result.GetWebResult(),
                 base::nullopt /* characteristics */);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      characteristics_uuid
          ? GetCharacteristicsByUUID(query_result.service,
                                     characteristics_uuid.value())
          : query_result.service->GetCharacteristics();

  std::vector<blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr>
      response_characteristics;
  for (device::BluetoothRemoteGattCharacteristic* characteristic :
       characteristics) {
    if (BluetoothBlocklist::Get().IsExcluded(characteristic->GetUUID())) {
      continue;
    }
    std::string characteristic_instance_id = characteristic->GetIdentifier();
    auto insert_result = characteristic_id_to_service_id_.insert(
        std::make_pair(characteristic_instance_id, service_instance_id));
    // If value is already in map, DCHECK it's valid.
    if (!insert_result.second)
      DCHECK(insert_result.first->second == service_instance_id);

    blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr characteristic_ptr =
        blink::mojom::WebBluetoothRemoteGATTCharacteristic::New();
    characteristic_ptr->instance_id = characteristic_instance_id;
    characteristic_ptr->uuid = characteristic->GetUUID().canonical_value();
    characteristic_ptr->properties =
        static_cast<uint32_t>(characteristic->GetProperties());
    response_characteristics.push_back(std::move(characteristic_ptr));

    if (quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
      break;
    }
  }

  if (!response_characteristics.empty()) {
    RecordGetCharacteristicsOutcome(quantity,
                                    UMAGetCharacteristicOutcome::SUCCESS);
    callback.Run(blink::mojom::WebBluetoothResult::SUCCESS,
                 std::move(response_characteristics));
    return;
  }

  RecordGetCharacteristicsOutcome(
      quantity, characteristics_uuid
                    ? UMAGetCharacteristicOutcome::NOT_FOUND
                    : UMAGetCharacteristicOutcome::NO_CHARACTERISTICS);
  callback.Run(characteristics_uuid
                   ? blink::mojom::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND
                   : blink::mojom::WebBluetoothResult::NO_CHARACTERISTICS_FOUND,
               base::nullopt /* characteristics */);
}

void WebBluetoothServiceImpl::RemoteCharacteristicReadValue(
    const std::string& characteristic_instance_id,
    const RemoteCharacteristicReadValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_READ_VALUE);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicReadValueOutcome(query_result.outcome);
    callback.Run(query_result.GetWebResult(), base::nullopt /* value */);
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromReads(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    callback.Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_READ,
                 base::nullopt /* value */);
    return;
  }

  query_result.characteristic->ReadRemoteCharacteristic(
      base::Bind(&WebBluetoothServiceImpl::OnReadValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnReadValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicWriteValue(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    const RemoteCharacteristicWriteValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_WRITE_VALUE);

  // We perform the length check on the renderer side. So if we
  // get a value with length > 512, we can assume it's a hostile
  // renderer and kill it.
  if (value.size() > 512) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicWriteValueOutcome(query_result.outcome);
    callback.Run(query_result.GetWebResult());
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromWrites(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    callback.Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_WRITE);
    return;
  }

  query_result.characteristic->WriteRemoteCharacteristic(
      value, base::Bind(&WebBluetoothServiceImpl::OnWriteValueSuccess,
                        weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnWriteValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStartNotifications(
    const std::string& characteristic_instance_id,
    const RemoteCharacteristicStartNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_START_NOTIFICATIONS);

  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (iter != characteristic_id_to_notify_session_.end() &&
      iter->second->IsActive()) {
    // If the frame has already started notifications and the notifications
    // are active we return SUCCESS.
    callback.Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordStartNotificationsOutcome(query_result.outcome);
    callback.Run(query_result.GetWebResult());
    return;
  }

  device::BluetoothRemoteGattCharacteristic::Properties notify_or_indicate =
      query_result.characteristic->GetProperties() &
      (device::BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
       device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE);
  if (!notify_or_indicate) {
    callback.Run(blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED);
    return;
  }

  query_result.characteristic->StartNotifySession(
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStopNotifications(
    const std::string& characteristic_instance_id,
    const RemoteCharacteristicStopNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_STOP_NOTIFICATIONS);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  auto notify_session_iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (notify_session_iter == characteristic_id_to_notify_session_.end()) {
    // If the frame hasn't subscribed to notifications before we just
    // run the callback.
    callback.Run();
    return;
  }
  notify_session_iter->second->Stop(base::Bind(
      &WebBluetoothServiceImpl::OnStopNotifySessionComplete,
      weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id, callback));
}

void WebBluetoothServiceImpl::RequestDeviceImpl(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const RequestDeviceCallback& callback,
    device::BluetoothAdapter* adapter) {
  // requestDevice() can only be called when processing a user-gesture and any
  // user gesture outside of a chooser should close the chooser. This does
  // not happen on all platforms so we don't DCHECK that the old one is closed.
  // We destroy the old chooser before constructing the new one to make sure
  // they can't conflict.
  device_chooser_controller_.reset();

  device_chooser_controller_.reset(
      new BluetoothDeviceChooserController(this, render_frame_host_, adapter));

  device_chooser_controller_->GetDevice(
      std::move(options),
      base::Bind(&WebBluetoothServiceImpl::OnGetDeviceSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnGetDeviceFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl(
    const WebBluetoothDeviceId& device_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& services_uuid,
    const RemoteServerGetPrimaryServicesCallback& callback,
    device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(device->IsGattServicesDiscoveryComplete());

  std::vector<device::BluetoothRemoteGattService*> services =
      services_uuid ? GetPrimaryServicesByUUID(device, services_uuid.value())
                    : GetPrimaryServices(device);

  std::vector<blink::mojom::WebBluetoothRemoteGATTServicePtr> response_services;
  for (device::BluetoothRemoteGattService* service : services) {
    if (!allowed_devices_map_.IsOriginAllowedToAccessService(
            GetOrigin(), device_id, service->GetUUID())) {
      continue;
    }
    std::string service_instance_id = service->GetIdentifier();
    const std::string& device_address = device->GetAddress();
    auto insert_result = service_id_to_device_address_.insert(
        make_pair(service_instance_id, device_address));
    // If value is already in map, DCHECK it's valid.
    if (!insert_result.second)
      DCHECK_EQ(insert_result.first->second, device_address);

    blink::mojom::WebBluetoothRemoteGATTServicePtr service_ptr =
        blink::mojom::WebBluetoothRemoteGATTService::New();
    service_ptr->instance_id = service_instance_id;
    service_ptr->uuid = service->GetUUID().canonical_value();
    response_services.push_back(std::move(service_ptr));

    if (quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
      break;
    }
  }

  if (!response_services.empty()) {
    VLOG(1) << "Services found in device.";
    RecordGetPrimaryServicesOutcome(quantity,
                                    UMAGetPrimaryServiceOutcome::SUCCESS);
    callback.Run(blink::mojom::WebBluetoothResult::SUCCESS,
                 std::move(response_services));
    return;
  }

  VLOG(1) << "Services not found in device.";
  RecordGetPrimaryServicesOutcome(
      quantity, services_uuid ? UMAGetPrimaryServiceOutcome::NOT_FOUND
                              : UMAGetPrimaryServiceOutcome::NO_SERVICES);
  callback.Run(services_uuid
                   ? blink::mojom::WebBluetoothResult::SERVICE_NOT_FOUND
                   : blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND,
               base::nullopt /* services */);
}

void WebBluetoothServiceImpl::OnGetDeviceSuccess(
    const RequestDeviceCallback& callback,
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const std::string& device_address) {
  device_chooser_controller_.reset();

  const device::BluetoothDevice* const device =
      GetAdapter()->GetDevice(device_address);
  if (device == nullptr) {
    VLOG(1) << "Device " << device_address << " no longer in adapter";
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::CHOSEN_DEVICE_VANISHED);
    callback.Run(blink::mojom::WebBluetoothResult::CHOSEN_DEVICE_VANISHED,
                 nullptr /* device */);
    return;
  }

  const WebBluetoothDeviceId device_id_for_origin =
      allowed_devices_map_.AddDevice(GetOrigin(), device_address, options);

  VLOG(1) << "Device: " << device->GetNameForDisplay();

  blink::mojom::WebBluetoothDevicePtr device_ptr =
      blink::mojom::WebBluetoothDevice::New();
  device_ptr->id = device_id_for_origin;
  device_ptr->name = device->GetName();

  RecordRequestDeviceOutcome(UMARequestDeviceOutcome::SUCCESS);
  callback.Run(blink::mojom::WebBluetoothResult::SUCCESS,
               std::move(device_ptr));
}

void WebBluetoothServiceImpl::OnGetDeviceFailed(
    const RequestDeviceCallback& callback,
    blink::mojom::WebBluetoothResult result) {
  // Errors are recorded by the *device_chooser_controller_.
  callback.Run(result, nullptr /* device */);
  device_chooser_controller_.reset();
}

void WebBluetoothServiceImpl::OnCreateGATTConnectionSuccess(
    const WebBluetoothDeviceId& device_id,
    base::TimeTicks start_time,
    const RemoteServerConnectCallback& callback,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordConnectGATTTimeSuccess(base::TimeTicks::Now() - start_time);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::SUCCESS);

  connected_devices_->Insert(device_id, std::move(connection));
  callback.Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnCreateGATTConnectionFailed(
    base::TimeTicks start_time,
    const RemoteServerConnectCallback& callback,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordConnectGATTTimeFailed(base::TimeTicks::Now() - start_time);
  callback.Run(TranslateConnectErrorAndRecord(error_code));
}

void WebBluetoothServiceImpl::OnReadValueSuccess(
    const RemoteCharacteristicReadValueCallback& callback,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  callback.Run(blink::mojom::WebBluetoothResult::SUCCESS, value);
}

void WebBluetoothServiceImpl::OnReadValueFailed(
    const RemoteCharacteristicReadValueCallback& callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
                   error_code, UMAGATTOperation::CHARACTERISTIC_READ),
               base::nullopt /* value */);
}

void WebBluetoothServiceImpl::OnWriteValueSuccess(
    const RemoteCharacteristicWriteValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  callback.Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnWriteValueFailed(
    const RemoteCharacteristicWriteValueCallback& callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::CHARACTERISTIC_WRITE));
}

void WebBluetoothServiceImpl::OnStartNotifySessionSuccess(
    const RemoteCharacteristicStartNotificationsCallback& callback,
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Copy Characteristic Instance ID before passing a unique pointer because
  // compilers may evaluate arguments in any order.
  std::string characteristic_instance_id =
      notify_session->GetCharacteristicIdentifier();
  // Saving the BluetoothGattNotifySession keeps notifications active.
  characteristic_id_to_notify_session_[characteristic_instance_id] =
      std::move(notify_session);
  callback.Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnStartNotifySessionFailed(
    const RemoteCharacteristicStartNotificationsCallback& callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::START_NOTIFICATIONS));
}

void WebBluetoothServiceImpl::OnStopNotifySessionComplete(
    const std::string& characteristic_instance_id,
    const RemoteCharacteristicStopNotificationsCallback& callback) {
  characteristic_id_to_notify_session_.erase(characteristic_instance_id);
  callback.Run();
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForDevice(
    const WebBluetoothDeviceId& device_id) {
  const std::string& device_address =
      allowed_devices_map_.GetDeviceAddress(GetOrigin(), device_id);
  if (device_address.empty()) {
    CrashRendererAndClosePipe(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result;
  result.device = GetAdapter()->GetDevice(device_address);

  // When a device can't be found in the BluetoothAdapter, that generally
  // indicates that it's gone out of range. We reject with a NetworkError in
  // that case.
  if (result.device == nullptr) {
    result.outcome = CacheQueryOutcome::NO_DEVICE;
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForService(
    const std::string& service_instance_id) {
  auto device_iter = service_id_to_device_address_.find(service_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (device_iter == service_id_to_device_address_.end()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_SERVICE_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  const WebBluetoothDeviceId* device_id =
      allowed_devices_map_.GetDeviceId(GetOrigin(), device_iter->second);
  // Kill the renderer if origin is not allowed to access the device.
  if (device_id == nullptr) {
    CrashRendererAndClosePipe(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForDevice(*device_id);
  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.service = result.device->GetGattService(service_instance_id);
  if (result.service == nullptr) {
    result.outcome = CacheQueryOutcome::NO_SERVICE;
  } else if (!allowed_devices_map_.IsOriginAllowedToAccessService(
                 GetOrigin(), *device_id, result.service->GetUUID())) {
    CrashRendererAndClosePipe(bad_message::BDH_SERVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForCharacteristic(
    const std::string& characteristic_instance_id) {
  auto characteristic_iter =
      characteristic_id_to_service_id_.find(characteristic_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (characteristic_iter == characteristic_id_to_service_id_.end()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForService(characteristic_iter->second);

  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.characteristic =
      result.service->GetCharacteristic(characteristic_instance_id);

  if (result.characteristic == nullptr) {
    result.outcome = CacheQueryOutcome::NO_CHARACTERISTIC;
  }

  return result;
}

RenderProcessHost* WebBluetoothServiceImpl::GetRenderProcessHost() {
  return render_frame_host_->GetProcess();
}

device::BluetoothAdapter* WebBluetoothServiceImpl::GetAdapter() {
  return BluetoothAdapterFactoryWrapper::Get().GetAdapter(this);
}

void WebBluetoothServiceImpl::CrashRendererAndClosePipe(
    bad_message::BadMessageReason reason) {
  bad_message::ReceivedBadMessage(GetRenderProcessHost(), reason);
  binding_.Close();
}

url::Origin WebBluetoothServiceImpl::GetOrigin() {
  return render_frame_host_->GetLastCommittedOrigin();
}

void WebBluetoothServiceImpl::ClearState() {
  characteristic_id_to_notify_session_.clear();
  pending_primary_services_requests_.clear();
  characteristic_id_to_service_id_.clear();
  service_id_to_device_address_.clear();
  connected_devices_.reset(
      new FrameConnectedBluetoothDevices(render_frame_host_));
  allowed_devices_map_ = BluetoothAllowedDevicesMap();
  device_chooser_controller_.reset();
  BluetoothAdapterFactoryWrapper::Get().ReleaseAdapter(this);
}

}  // namespace content
