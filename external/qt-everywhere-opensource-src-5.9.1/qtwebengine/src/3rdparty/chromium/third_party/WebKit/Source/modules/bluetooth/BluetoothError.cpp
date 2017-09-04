// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"

namespace blink {

DOMException* BluetoothError::take(
    ScriptPromiseResolver*,
    int32_t
        webError /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */) {
  switch (static_cast<mojom::blink::WebBluetoothResult>(webError)) {
    case mojom::blink::WebBluetoothResult::SUCCESS:
      ASSERT_NOT_REACHED();
      return DOMException::create(UnknownError);
#define MAP_ERROR(enumeration, name, message)         \
  case mojom::blink::WebBluetoothResult::enumeration: \
    return DOMException::create(name, message)

      // InvalidModificationErrors:
      MAP_ERROR(GATT_INVALID_ATTRIBUTE_LENGTH, InvalidModificationError,
                "GATT Error: invalid attribute length.");

      // InvalidStateErrors:
      MAP_ERROR(SERVICE_NO_LONGER_EXISTS, InvalidStateError,
                "GATT Service no longer exists.");
      MAP_ERROR(CHARACTERISTIC_NO_LONGER_EXISTS, InvalidStateError,
                "GATT Characteristic no longer exists.");

      // NetworkErrors:
      MAP_ERROR(CONNECT_ALREADY_IN_PROGRESS, NetworkError,
                "Connection already in progress.");
      MAP_ERROR(CONNECT_ATTRIBUTE_LENGTH_INVALID, NetworkError,
                "Write operation exceeds the maximum length of the attribute.");
      MAP_ERROR(CONNECT_AUTH_CANCELED, NetworkError,
                "Authentication canceled.");
      MAP_ERROR(CONNECT_AUTH_FAILED, NetworkError, "Authentication failed.");
      MAP_ERROR(CONNECT_AUTH_REJECTED, NetworkError,
                "Authentication rejected.");
      MAP_ERROR(CONNECT_AUTH_TIMEOUT, NetworkError, "Authentication timeout.");
      MAP_ERROR(CONNECT_CONNECTION_CONGESTED, NetworkError,
                "Remote device connection is congested.");
      MAP_ERROR(CONNECT_INSUFFICIENT_ENCRYPTION, NetworkError,
                "Insufficient encryption for a given operation");
      MAP_ERROR(
          CONNECT_OFFSET_INVALID, NetworkError,
          "Read or write operation was requested with an invalid offset.");
      MAP_ERROR(CONNECT_READ_NOT_PERMITTED, NetworkError,
                "GATT read operation is not permitted.");
      MAP_ERROR(CONNECT_REQUEST_NOT_SUPPORTED, NetworkError,
                "The given request is not supported.");
      MAP_ERROR(CONNECT_UNKNOWN_ERROR, NetworkError,
                "Unknown error when connecting to the device.");
      MAP_ERROR(CONNECT_UNKNOWN_FAILURE, NetworkError,
                "Connection failed for unknown reason.");
      MAP_ERROR(CONNECT_UNSUPPORTED_DEVICE, NetworkError,
                "Unsupported device.");
      MAP_ERROR(CONNECT_WRITE_NOT_PERMITTED, NetworkError,
                "GATT write operation is not permitted.");
      MAP_ERROR(DEVICE_NO_LONGER_IN_RANGE, NetworkError,
                "Bluetooth Device is no longer in range.");
      MAP_ERROR(GATT_NOT_PAIRED, NetworkError, "GATT Error: Not paired.");
      MAP_ERROR(GATT_OPERATION_IN_PROGRESS, NetworkError,
                "GATT operation already in progress.");
      MAP_ERROR(UNTRANSLATED_CONNECT_ERROR_CODE, NetworkError,
                "Unknown ConnectErrorCode.");

      // NotFoundErrors:
      MAP_ERROR(WEB_BLUETOOTH_NOT_SUPPORTED, NotFoundError,
                "Web Bluetooth is not supported on this platform. For a list "
                "of supported platforms see: https://goo.gl/J6ASzs");
      MAP_ERROR(NO_BLUETOOTH_ADAPTER, NotFoundError,
                "Bluetooth adapter not available.");
      MAP_ERROR(CHOSEN_DEVICE_VANISHED, NotFoundError,
                "User selected a device that doesn't exist anymore.");
      MAP_ERROR(CHOOSER_CANCELLED, NotFoundError,
                "User cancelled the requestDevice() chooser.");
      MAP_ERROR(CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED, NotFoundError,
                "Web Bluetooth API globally disabled.");
      MAP_ERROR(CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED, NotFoundError,
                "User or their enterprise policy has disabled Web Bluetooth.");
      MAP_ERROR(
          CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN, NotFoundError,
          "User denied the browser permission to scan for Bluetooth devices.");
      MAP_ERROR(SERVICE_NOT_FOUND, NotFoundError,
                "No Services with specified UUID found in Device.");
      MAP_ERROR(NO_SERVICES_FOUND, NotFoundError,
                "No Services found in device.");
      MAP_ERROR(CHARACTERISTIC_NOT_FOUND, NotFoundError,
                "No Characteristics with specified UUID found in Service.");
      MAP_ERROR(NO_CHARACTERISTICS_FOUND, NotFoundError,
                "No Characteristics found in service.");
      MAP_ERROR(BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE, NotFoundError,
                "Bluetooth Low Energy not available.");

      // NotSupportedErrors:
      MAP_ERROR(GATT_UNKNOWN_ERROR, NotSupportedError, "GATT Error Unknown.");
      MAP_ERROR(GATT_UNKNOWN_FAILURE, NotSupportedError,
                "GATT operation failed for unknown reason.");
      MAP_ERROR(GATT_NOT_PERMITTED, NotSupportedError,
                "GATT operation not permitted.");
      MAP_ERROR(GATT_NOT_SUPPORTED, NotSupportedError,
                "GATT Error: Not supported.");
      MAP_ERROR(GATT_UNTRANSLATED_ERROR_CODE, NotSupportedError,
                "GATT Error: Unknown GattErrorCode.");

      // SecurityErrors:
      MAP_ERROR(GATT_NOT_AUTHORIZED, SecurityError,
                "GATT operation not authorized.");
      MAP_ERROR(BLOCKLISTED_CHARACTERISTIC_UUID, SecurityError,
                "getCharacteristic(s) called with blocklisted UUID. "
                "https://goo.gl/4NeimX");
      MAP_ERROR(BLOCKLISTED_READ, SecurityError,
                "readValue() called on blocklisted object marked "
                "exclude-reads. https://goo.gl/4NeimX");
      MAP_ERROR(BLOCKLISTED_WRITE, SecurityError,
                "writeValue() called on blocklisted object marked "
                "exclude-writes. https://goo.gl/4NeimX");
      MAP_ERROR(NOT_ALLOWED_TO_ACCESS_ANY_SERVICE, SecurityError,
                "Origin is not allowed to access any service. Tip: Add the "
                "service UUID to 'optionalServices' in requestDevice() "
                "options. https://goo.gl/HxfxSQ");
      MAP_ERROR(NOT_ALLOWED_TO_ACCESS_SERVICE, SecurityError,
                "Origin is not allowed to access the service. Tip: Add the "
                "service UUID to 'optionalServices' in requestDevice() "
                "options. https://goo.gl/HxfxSQ");
      MAP_ERROR(REQUEST_DEVICE_WITH_BLOCKLISTED_UUID, SecurityError,
                "requestDevice() called with a filter containing a blocklisted "
                "UUID. https://goo.gl/4NeimX");
      MAP_ERROR(REQUEST_DEVICE_FROM_CROSS_ORIGIN_IFRAME, SecurityError,
                "requestDevice() called from cross-origin iframe.");
      MAP_ERROR(REQUEST_DEVICE_WITHOUT_FRAME, SecurityError,
                "No window to show the requestDevice() dialog.");

#undef MAP_ERROR
  }

  ASSERT_NOT_REACHED();
  return DOMException::create(UnknownError);
}

}  // namespace blink
