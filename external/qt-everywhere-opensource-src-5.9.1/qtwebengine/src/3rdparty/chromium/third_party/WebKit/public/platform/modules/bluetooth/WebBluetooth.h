// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetooth_h
#define WebBluetooth_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

#include <memory>

namespace blink {

class WebBluetoothDevice;
class WebBluetoothRemoteGATTCharacteristic;

struct WebBluetoothDeviceInit;
struct WebBluetoothRemoteGATTCharacteristicInit;
struct WebBluetoothRemoteGATTService;
struct WebRequestDeviceOptions;

// Success and failure callbacks for requestDevice.
using WebBluetoothRequestDeviceCallbacks = WebCallbacks<
    std::unique_ptr<WebBluetoothDeviceInit>,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for GattServer.connect().
using WebBluetoothRemoteGATTServerConnectCallbacks = WebCallbacks<
    void,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for getPrimaryService(s).
using WebBluetoothGetPrimaryServicesCallbacks = WebCallbacks<
    const WebVector<WebBluetoothRemoteGATTService*>&,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for getCharacteristic(s).
using WebBluetoothGetCharacteristicsCallbacks = WebCallbacks<
    const WebVector<WebBluetoothRemoteGATTCharacteristicInit*>&,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for readValue.
using WebBluetoothReadValueCallbacks = WebCallbacks<
    const WebVector<uint8_t>&,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for writeValue.
using WebBluetoothWriteValueCallbacks = WebCallbacks<
    const WebVector<uint8_t>&,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

// Success and failure callbacks for characteristic.startNotifications and
// characteristic.stopNotifications.
using WebBluetoothNotificationsCallbacks = WebCallbacks<
    void,
    int32_t /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */>;

class WebBluetooth {
 public:
  virtual ~WebBluetooth() {}

  // Bluetooth Methods:
  // See https://webbluetoothchrome.github.io/web-bluetooth/#device-discovery
  // WebBluetoothRequestDeviceCallbacks ownership transferred to the client.
  virtual void requestDevice(const WebRequestDeviceOptions&,
                             WebBluetoothRequestDeviceCallbacks*) {}

  // BluetoothDevice methods:

  // BluetoothRemoteGATTServer methods:
  // See
  // https://webbluetoothchrome.github.io/web-bluetooth/#idl-def-bluetoothgattremoteserver
  virtual void connect(const WebString& deviceId,
                       WebBluetoothDevice* device,
                       WebBluetoothRemoteGATTServerConnectCallbacks*) {}
  virtual void disconnect(const WebString& deviceId) = 0;
  virtual void getPrimaryServices(
      const WebString& deviceId,
      // Corresponds to WebBluetoothGATTQueryQuantity in web_bluetooth.mojom:
      int32_t quantity,
      const WebString& servicesUUID,
      WebBluetoothGetPrimaryServicesCallbacks*) = 0;

  // BluetoothRemoteGATTService methods:
  // See
  // https://webbluetoothchrome.github.io/web-bluetooth/#idl-def-bluetoothgattservice
  virtual void getCharacteristics(
      const WebString& serviceInstanceID,
      // Corresponds to WebBluetoothGATTQueryQuantity in web_bluetooth.mojom
      int32_t quantity,
      const WebString& characteristicsUUID,
      WebBluetoothGetCharacteristicsCallbacks*) = 0;

  // BluetoothRemoteGATTCharacteristic methods:
  // See
  // https://webbluetoothchrome.github.io/web-bluetooth/#bluetoothgattcharacteristic
  virtual void readValue(const WebString& characteristicInstanceID,
                         WebBluetoothReadValueCallbacks*) {}
  virtual void writeValue(const WebString& characteristicInstanceID,
                          const WebVector<uint8_t>& value,
                          WebBluetoothWriteValueCallbacks*) {}
  virtual void startNotifications(const WebString& characteristicInstanceID,
                                  WebBluetoothNotificationsCallbacks*) {}
  virtual void stopNotifications(const WebString& characteristicInstanceID,
                                 WebBluetoothNotificationsCallbacks*) {}

  // Called when addEventListener is called on a characteristic.
  virtual void registerCharacteristicObject(
      const WebString& characteristicInstanceID,
      WebBluetoothRemoteGATTCharacteristic*) = 0;
  virtual void characteristicObjectRemoved(
      const WebString& characteristicInstanceID,
      WebBluetoothRemoteGATTCharacteristic*) {}
};

}  // namespace blink

#endif  // WebBluetooth_h
