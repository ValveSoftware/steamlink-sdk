// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/web_bluetooth_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "content/child/mojo/type_converters.h"
#include "content/child/thread_safe_sender.h"
#include "content/renderer/bluetooth/bluetooth_type_converters.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/array.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothDeviceInit.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristic.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristicInit.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTService.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

namespace content {

WebBluetoothImpl::WebBluetoothImpl(shell::InterfaceProvider* remote_interfaces)
    : remote_interfaces_(remote_interfaces), binding_(this) {}

WebBluetoothImpl::~WebBluetoothImpl() {
}

void WebBluetoothImpl::requestDevice(
    const blink::WebRequestDeviceOptions& options,
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  GetWebBluetoothService().RequestDevice(
      blink::mojom::WebBluetoothRequestDeviceOptions::From(options),
      base::Bind(&WebBluetoothImpl::OnRequestDeviceComplete,
                 base::Unretained(this),
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::connect(
    const blink::WebString& device_id,
    blink::WebBluetoothDevice* device,
    blink::WebBluetoothRemoteGATTServerConnectCallbacks* callbacks) {
  // TODO(crbug.com/495270): After the Bluetooth Tree is implemented, there will
  // only be one object per device. But for now we replace the previous object.
  connected_devices_[device_id.utf8()] = device;

  GetWebBluetoothService().RemoteServerConnect(
      mojo::String::From(device_id),
      base::Bind(&WebBluetoothImpl::OnConnectComplete, base::Unretained(this),
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::disconnect(const blink::WebString& device_id) {
  connected_devices_.erase(device_id.utf8());

  GetWebBluetoothService().RemoteServerDisconnect(
      mojo::String::From(device_id));
}

void WebBluetoothImpl::getPrimaryServices(
    const blink::WebString& device_id,

    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const blink::WebString& services_uuid,
    blink::WebBluetoothGetPrimaryServicesCallbacks* callbacks) {
  GetWebBluetoothService().RemoteServerGetPrimaryServices(
      mojo::String::From(device_id), quantity,
      services_uuid.isEmpty()
          ? base::nullopt
          : base::make_optional(device::BluetoothUUID(services_uuid.utf8())),
      base::Bind(&WebBluetoothImpl::OnGetPrimaryServicesComplete,
                 base::Unretained(this), device_id,
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::getCharacteristics(
    const blink::WebString& service_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const blink::WebString& characteristics_uuid,
    blink::WebBluetoothGetCharacteristicsCallbacks* callbacks) {
  GetWebBluetoothService().RemoteServiceGetCharacteristics(
      mojo::String::From(service_instance_id), quantity,
      characteristics_uuid.isEmpty()
          ? base::nullopt
          : base::make_optional(
                device::BluetoothUUID(characteristics_uuid.utf8())),
      base::Bind(&WebBluetoothImpl::OnGetCharacteristicsComplete,
                 base::Unretained(this), service_instance_id,
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::readValue(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  GetWebBluetoothService().RemoteCharacteristicReadValue(
      mojo::String::From(characteristic_instance_id),
      base::Bind(&WebBluetoothImpl::OnReadValueComplete, base::Unretained(this),
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::writeValue(
    const blink::WebString& characteristic_instance_id,
    const blink::WebVector<uint8_t>& value,
    blink::WebBluetoothWriteValueCallbacks* callbacks) {
  GetWebBluetoothService().RemoteCharacteristicWriteValue(
      mojo::String::From(characteristic_instance_id),
      mojo::Array<uint8_t>::From(value),
      base::Bind(&WebBluetoothImpl::OnWriteValueComplete,
                 base::Unretained(this), value,
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::startNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetWebBluetoothService().RemoteCharacteristicStartNotifications(
      mojo::String::From(characteristic_instance_id),
      base::Bind(&WebBluetoothImpl::OnStartNotificationsComplete,
                 base::Unretained(this),
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::stopNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetWebBluetoothService().RemoteCharacteristicStopNotifications(
      mojo::String::From(characteristic_instance_id),
      base::Bind(&WebBluetoothImpl::OnStopNotificationsComplete,
                 base::Unretained(this),
                 base::Passed(base::WrapUnique(callbacks))));
}

void WebBluetoothImpl::characteristicObjectRemoved(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic) {
  active_characteristics_.erase(characteristic_instance_id.utf8());
}

void WebBluetoothImpl::registerCharacteristicObject(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic) {
  // TODO(ortuno): After the Bluetooth Tree is implemented, there will
  // only be one object per characteristic. But for now we replace
  // the previous object.
  // https://crbug.com/495270
  active_characteristics_[characteristic_instance_id.utf8()] = characteristic;
}

void WebBluetoothImpl::RemoteCharacteristicValueChanged(
    const mojo::String& characteristic_instance_id,
    mojo::Array<uint8_t> value) {
  // We post a task so that the event is fired after any pending promises have
  // resolved.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebBluetoothImpl::DispatchCharacteristicValueChanged,
                 base::Unretained(this), characteristic_instance_id,
                 value.PassStorage()));
}

void WebBluetoothImpl::OnRequestDeviceComplete(
    std::unique_ptr<blink::WebBluetoothRequestDeviceCallbacks> callbacks,
    const blink::mojom::WebBluetoothError error,
    blink::mojom::WebBluetoothDevicePtr device) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    blink::WebVector<blink::WebString> uuids(device->uuids.size());
    for (size_t i = 0; i < device->uuids.size(); ++i)
      uuids[i] = blink::WebString::fromUTF8(device->uuids[i]);

    callbacks->onSuccess(base::WrapUnique(new blink::WebBluetoothDeviceInit(
        blink::WebString::fromUTF8(device->id),
        blink::WebString::fromUTF8(device->name), uuids)));
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::GattServerDisconnected(const mojo::String& device_id) {
  auto device_iter = connected_devices_.find(device_id);
  if (device_iter != connected_devices_.end()) {
    // Remove device from the map before calling dispatchGattServerDisconnected
    // to avoid removing a device the gattserverdisconnected event handler might
    // have re-connected.
    blink::WebBluetoothDevice* device = device_iter->second;
    connected_devices_.erase(device_iter);
    device->dispatchGattServerDisconnected();
  }
}

void WebBluetoothImpl::OnConnectComplete(
    std::unique_ptr<blink::WebBluetoothRemoteGATTServerConnectCallbacks>
        callbacks,
    blink::mojom::WebBluetoothError error) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    callbacks->onSuccess();
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnGetPrimaryServicesComplete(
    const blink::WebString& device_id,
    std::unique_ptr<blink::WebBluetoothGetPrimaryServicesCallbacks> callbacks,
    blink::mojom::WebBluetoothError error,
    mojo::Array<blink::mojom::WebBluetoothRemoteGATTServicePtr> services) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    // TODO(dcheng): This WebVector should use smart pointers.
    blink::WebVector<blink::WebBluetoothRemoteGATTService*> promise_services(
        services.size());

    for (size_t i = 0; i < services.size(); i++) {
      promise_services[i] = new blink::WebBluetoothRemoteGATTService(
          blink::WebString::fromUTF8(services[i]->instance_id),
          blink::WebString::fromUTF8(services[i]->uuid), true /* isPrimary */,
          device_id);
    }
    callbacks->onSuccess(promise_services);
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnGetCharacteristicsComplete(
    const blink::WebString& service_instance_id,
    std::unique_ptr<blink::WebBluetoothGetCharacteristicsCallbacks> callbacks,
    blink::mojom::WebBluetoothError error,
    mojo::Array<blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr>
        characteristics) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    // TODO(dcheng): This WebVector should use smart pointers.
    blink::WebVector<blink::WebBluetoothRemoteGATTCharacteristicInit*>
        promise_characteristics(characteristics.size());

    for (size_t i = 0; i < characteristics.size(); i++) {
      promise_characteristics[i] =
          new blink::WebBluetoothRemoteGATTCharacteristicInit(
              service_instance_id,
              blink::WebString::fromUTF8(characteristics[i]->instance_id),
              blink::WebString::fromUTF8(characteristics[i]->uuid),
              characteristics[i]->properties);
    }
    callbacks->onSuccess(promise_characteristics);
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnReadValueComplete(
    std::unique_ptr<blink::WebBluetoothReadValueCallbacks> callbacks,
    blink::mojom::WebBluetoothError error,
    mojo::Array<uint8_t> value) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    callbacks->onSuccess(value.PassStorage());
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnWriteValueComplete(
    const blink::WebVector<uint8_t>& value,
    std::unique_ptr<blink::WebBluetoothWriteValueCallbacks> callbacks,
    blink::mojom::WebBluetoothError error) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    callbacks->onSuccess(value);
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnStartNotificationsComplete(
    std::unique_ptr<blink::WebBluetoothNotificationsCallbacks> callbacks,
    blink::mojom::WebBluetoothError error) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    callbacks->onSuccess();
  } else {
    callbacks->onError(error);
  }
}

void WebBluetoothImpl::OnStopNotificationsComplete(
    std::unique_ptr<blink::WebBluetoothNotificationsCallbacks> callbacks) {
  callbacks->onSuccess();
}

void WebBluetoothImpl::DispatchCharacteristicValueChanged(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value) {
  auto active_iter = active_characteristics_.find(characteristic_instance_id);
  if (active_iter != active_characteristics_.end()) {
    active_iter->second->dispatchCharacteristicValueChanged(value);
  }
}

blink::mojom::WebBluetoothService& WebBluetoothImpl::GetWebBluetoothService() {
  if (!web_bluetooth_service_) {
    remote_interfaces_->GetInterface(mojo::GetProxy(&web_bluetooth_service_));
    // Create an associated interface ptr and pass it to the WebBluetoothService
    // so that it can send us events without us prompting.
    blink::mojom::WebBluetoothServiceClientAssociatedPtrInfo ptr_info;
    binding_.Bind(&ptr_info, web_bluetooth_service_.associated_group());
    web_bluetooth_service_->SetClient(std::move(ptr_info));
  }
  return *web_bluetooth_service_;
}

}  // namespace content
