// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
#define CONTENT_RENDERER_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetooth.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace blink {
class WebBluetoothRemoteGATTCharacteristic;
}

namespace shell {
class InterfaceProvider;
}

namespace content {

class BluetoothDispatcher;
class ThreadSafeSender;

// Implementation of blink::WebBluetooth. Passes calls through to the thread
// specific BluetoothDispatcher.
class CONTENT_EXPORT WebBluetoothImpl
    : NON_EXPORTED_BASE(public blink::mojom::WebBluetoothServiceClient),
      NON_EXPORTED_BASE(public blink::WebBluetooth) {
 public:
  WebBluetoothImpl(shell::InterfaceProvider* remote_interfaces);
  ~WebBluetoothImpl() override;

  // blink::WebBluetooth interface:
  void requestDevice(
      const blink::WebRequestDeviceOptions& options,
      blink::WebBluetoothRequestDeviceCallbacks* callbacks) override;
  void connect(
      const blink::WebString& device_id,
      blink::WebBluetoothDevice* device,
      blink::WebBluetoothRemoteGATTServerConnectCallbacks* callbacks) override;
  void disconnect(const blink::WebString& device_id) override;
  void getPrimaryServices(
      const blink::WebString& device_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const blink::WebString& services_uuid,
      blink::WebBluetoothGetPrimaryServicesCallbacks* callbacks) override;
  void getCharacteristics(
      const blink::WebString& service_instance_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const blink::WebString& characteristics_uuid,
      blink::WebBluetoothGetCharacteristicsCallbacks* callbacks) override;
  void readValue(const blink::WebString& characteristic_instance_id,
                 blink::WebBluetoothReadValueCallbacks* callbacks) override;
  void writeValue(const blink::WebString& characteristic_instance_id,
                  const blink::WebVector<uint8_t>& value,
                  blink::WebBluetoothWriteValueCallbacks*) override;
  void startNotifications(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothNotificationsCallbacks*) override;
  void stopNotifications(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothNotificationsCallbacks*) override;
  void characteristicObjectRemoved(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothRemoteGATTCharacteristic* characteristic) override;
  void registerCharacteristicObject(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothRemoteGATTCharacteristic* characteristic) override;

 private:
  struct GetCharacteristicsCallback;
  // WebBluetoothServiceClient methods:
  void RemoteCharacteristicValueChanged(
      const mojo::String& characteristic_instance_id,
      mojo::Array<uint8_t> value) override;
  void GattServerDisconnected(const mojo::String& device_id) override;

  // Callbacks for WebBluetoothService calls:
  void OnRequestDeviceComplete(
      std::unique_ptr<blink::WebBluetoothRequestDeviceCallbacks> callbacks,
      const blink::mojom::WebBluetoothError error,
      blink::mojom::WebBluetoothDevicePtr device);
  void OnConnectComplete(
      std::unique_ptr<blink::WebBluetoothRemoteGATTServerConnectCallbacks>
          callbacks,
      blink::mojom::WebBluetoothError error);
  void OnGetPrimaryServicesComplete(
      const blink::WebString& device_id,
      std::unique_ptr<blink::WebBluetoothGetPrimaryServicesCallbacks> callbacks,
      blink::mojom::WebBluetoothError error,
      mojo::Array<blink::mojom::WebBluetoothRemoteGATTServicePtr> services);
  void OnGetCharacteristicsComplete(
      const blink::WebString& service_instance_id,
      std::unique_ptr<blink::WebBluetoothGetCharacteristicsCallbacks> callbacks,
      blink::mojom::WebBluetoothError error,
      mojo::Array<blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr>
          characteristics);
  void OnReadValueComplete(
      std::unique_ptr<blink::WebBluetoothReadValueCallbacks> callbacks,
      blink::mojom::WebBluetoothError error,
      mojo::Array<uint8_t> value);
  void OnWriteValueComplete(
      const blink::WebVector<uint8_t>& value,
      std::unique_ptr<blink::WebBluetoothWriteValueCallbacks> callbacks,
      blink::mojom::WebBluetoothError error);
  void OnStartNotificationsComplete(
      std::unique_ptr<blink::WebBluetoothNotificationsCallbacks> callbacks,
      blink::mojom::WebBluetoothError error);
  void OnStopNotificationsComplete(
      std::unique_ptr<blink::WebBluetoothNotificationsCallbacks> callbacks);

  void DispatchCharacteristicValueChanged(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value);

  blink::mojom::WebBluetoothService& GetWebBluetoothService();
  shell::InterfaceProvider* const remote_interfaces_;
  blink::mojom::WebBluetoothServicePtr web_bluetooth_service_;

  // Map of characteristic_instance_ids to
  // WebBluetoothRemoteGATTCharacteristics. When characteristicObjectRemoved is
  // called the characteristic should be removed from the map.
  // Keeps track of what characteristics have listeners.
  std::unordered_map<std::string, blink::WebBluetoothRemoteGATTCharacteristic*>
      active_characteristics_;

  // Map of device_ids to WebBluetoothDevices. Added in connect() and removed in
  // disconnect(). This means a device may not actually be connected while in
  // this map, but that it will definitely be removed when the page navigates.
  std::unordered_map<std::string, blink::WebBluetoothDevice*>
      connected_devices_;

  // Binding associated with |web_bluetooth_service_|.
  mojo::AssociatedBinding<blink::mojom::WebBluetoothServiceClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
