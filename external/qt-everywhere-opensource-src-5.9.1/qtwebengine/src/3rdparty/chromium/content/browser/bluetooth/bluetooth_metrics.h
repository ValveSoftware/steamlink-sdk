// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_

#include <string>
#include <vector>

#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace base {
class TimeDelta;
}

namespace device {
class BluetoothUUID;
}

namespace content {

// General Metrics

// Enumeration of each Web Bluetooth API entry point.
enum class UMAWebBluetoothFunction {
  REQUEST_DEVICE = 0,
  CONNECT_GATT = 1,
  GET_PRIMARY_SERVICE = 2,
  SERVICE_GET_CHARACTERISTIC = 3,
  CHARACTERISTIC_READ_VALUE = 4,
  CHARACTERISTIC_WRITE_VALUE = 5,
  CHARACTERISTIC_START_NOTIFICATIONS = 6,
  CHARACTERISTIC_STOP_NOTIFICATIONS = 7,
  REMOTE_GATT_SERVER_DISCONNECT = 8,
  SERVICE_GET_CHARACTERISTICS = 9,
  GET_PRIMARY_SERVICES = 10,
  // NOTE: Add new actions immediately above this line. Make sure to update
  // the enum list in tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function for every call to the Web Bluetooth
// API.
void RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction function);

// Enumeration for outcomes of querying the bluetooth cache.
enum class CacheQueryOutcome {
  SUCCESS = 0,
  BAD_RENDERER = 1,
  NO_DEVICE = 2,
  NO_SERVICE = 3,
  NO_CHARACTERISTIC = 4,
};

// requestDevice() Metrics
enum class UMARequestDeviceOutcome {
  SUCCESS = 0,
  NO_BLUETOOTH_ADAPTER = 1,
  NO_RENDER_FRAME = 2,
  OBSOLETE_DISCOVERY_START_FAILED = 3,
  OBSOLETE_DISCOVERY_STOP_FAILED = 4,
  OBSOLETE_NO_MATCHING_DEVICES_FOUND = 5,
  BLUETOOTH_ADAPTER_NOT_PRESENT = 6,
  OBSOLETE_BLUETOOTH_ADAPTER_OFF = 7,
  CHOSEN_DEVICE_VANISHED = 8,
  BLUETOOTH_CHOOSER_CANCELLED = 9,
  BLUETOOTH_CHOOSER_DENIED_PERMISSION = 10,
  BLOCKLISTED_SERVICE_IN_FILTER = 11,
  BLUETOOTH_OVERVIEW_HELP_LINK_PRESSED = 12,
  ADAPTER_OFF_HELP_LINK_PRESSED = 13,
  NEED_LOCATION_HELP_LINK_PRESSED = 14,
  BLUETOOTH_CHOOSER_POLICY_DISABLED = 15,
  BLUETOOTH_GLOBALLY_DISABLED = 16,
  BLUETOOTH_CHOOSER_EVENT_HANDLER_INVALID = 17,
  BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE = 18,
  BLUETOOTH_CHOOSER_RESCAN = 19,
  // NOTE: Add new requestDevice() outcomes immediately above this line. Make
  // sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function before every
// Send(BluetoothMsg_RequestDeviceSuccess...) or
// Send(BluetoothMsg_RequestDeviceError...).
CONTENT_EXPORT void RecordRequestDeviceOutcome(UMARequestDeviceOutcome outcome);

// Records stats about the arguments used when calling requestDevice.
//  - The number of filters used.
//  - The size of each filter.
//  - UUID of the services used in filters.
//  - Number of optional services used.
//  - UUID of the optional services.
//  - Size of the union of all services.
void RecordRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

// GattServer.connect() Metrics

enum class UMAConnectGATTOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  UNKNOWN = 2,
  IN_PROGRESS = 3,
  FAILED = 4,
  AUTH_FAILED = 5,
  AUTH_CANCELED = 6,
  AUTH_REJECTED = 7,
  AUTH_TIMEOUT = 8,
  UNSUPPORTED_DEVICE = 9,
  ATTRIBUTE_LENGTH_INVALID = 10,
  CONNECTION_CONGESTED = 11,
  INSUFFICIENT_ENCRYPTION = 12,
  OFFSET_INVALID = 13,
  READ_NOT_PERMITTED = 14,
  REQUEST_NOT_SUPPORTED = 15,
  WRITE_NOT_PERMITTED = 16,
  // Note: Add new ConnectGATT outcomes immediately above this line. Make sure
  // to update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  COUNT
};

// There should be a call to this function before every
// Send(BluetoothMsg_ConnectGATTSuccess) and
// Send(BluetoothMsg_ConnectGATTError).
void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome);

// Records the outcome of the cache query for connectGATT. Should only be called
// if QueryCacheForDevice fails.
void RecordConnectGATTOutcome(CacheQueryOutcome outcome);

// Records how long it took for the connection to succeed.
void RecordConnectGATTTimeSuccess(const base::TimeDelta& duration);

// Records how long it took for the connection to fail.
void RecordConnectGATTTimeFailed(const base::TimeDelta& duration);

// getPrimaryService() and getPrimaryServices() Metrics

enum class UMAGetPrimaryServiceOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NOT_FOUND = 2,
  NO_SERVICES = 3,
  // Note: Add new GetPrimaryService outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function whenever
// RemoteServerGetPrimaryServicesCallback is run.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE for
// getPrimaryService.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE for
// getPrimaryServices.
void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetPrimaryServiceOutcome outcome);

// Records the outcome of the cache query for getPrimaryServices. Should only be
// called if QueryCacheForDevice fails.
void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// Records the UUID of the service used when calling getPrimaryService.
void RecordGetPrimaryServicesServices(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<device::BluetoothUUID>& service);

// getCharacteristic() and getCharacteristics() Metrics

enum class UMAGetCharacteristicOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NOT_FOUND = 3,
  BLOCKLISTED = 4,
  NO_CHARACTERISTICS = 5,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrisc/histogram/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function whenever
// RemoteServiceGetCharacteristicsCallback is run.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE for
// getCharacteristic.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE for
// getCharacteristics.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetCharacteristicOutcome outcome);

// Records the outcome of the cache query for getCharacteristics. Should only be
// called if QueryCacheForService fails.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// Records the UUID of the characteristic used when calling getCharacteristic.
void RecordGetCharacteristicsCharacteristic(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<device::BluetoothUUID>& characteristic);

// GATT Operations Metrics

// These are the possible outcomes when performing GATT operations i.e.
// characteristic.readValue/writeValue descriptor.readValue/writeValue.
enum UMAGATTOperationOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NO_CHARACTERISTIC = 3,
  NO_DESCRIPTOR = 4,
  UNKNOWN = 5,
  FAILED = 6,
  IN_PROGRESS = 7,
  INVALID_LENGTH = 8,
  NOT_PERMITTED = 9,
  NOT_AUTHORIZED = 10,
  NOT_PAIRED = 11,
  NOT_SUPPORTED = 12,
  BLOCKLISTED = 13,
  // Note: Add new GATT Outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

enum class UMAGATTOperation {
  CHARACTERISTIC_READ,
  CHARACTERISTIC_WRITE,
  START_NOTIFICATIONS,
  // Note: Add new GATT Operations immediately above this line.
  COUNT
};

// Records the outcome of a GATT operation.
// There should be a call to this function whenever the corresponding operation
// doesn't have a call to Record[Operation]Outcome.
void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome);

// Characteristic.readValue() Metrics
// There should be a call to this function for every call to
// Send(BluetoothMsg_ReadCharacteristicValueSuccess) and
// Send(BluetoothMsg_ReadCharacteristicValueError).
void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome error);

// Records the outcome of a cache query for readValue. Should only be called if
// QueryCacheForCharacteristic fails.
void RecordCharacteristicReadValueOutcome(CacheQueryOutcome outcome);

// Characteristic.writeValue() Metrics
// There should be a call to this function for every call to
// Send(BluetoothMsg_WriteCharacteristicValueSuccess) and
// Send(BluetoothMsg_WriteCharacteristicValueError).
void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome error);

// Records the outcome of a cache query for writeValue. Should only be called if
// QueryCacheForCharacteristic fails.
void RecordCharacteristicWriteValueOutcome(CacheQueryOutcome outcome);

// Characteristic.startNotifications() Metrics
// There should be a call to this function for every call to
// Send(BluetoothMsg_StartNotificationsSuccess) and
// Send(BluetoothMsg_StopNotificationsError).
void RecordStartNotificationsOutcome(UMAGATTOperationOutcome outcome);

// Records the outcome of a cache query for startNotifications. Should only be
// called if QueryCacheForCharacteristic fails.
void RecordStartNotificationsOutcome(CacheQueryOutcome outcome);

enum class UMARSSISignalStrengthLevel {
  LESS_THAN_OR_EQUAL_TO_MIN_RSSI,
  LEVEL_0,
  LEVEL_1,
  LEVEL_2,
  LEVEL_3,
  LEVEL_4,
  GREATER_THAN_OR_EQUAL_TO_MAX_RSSI,
  // Note: Add new RSSI signal strength level immediately above this line.
  COUNT
};

// Records the raw RSSI, and processed result displayed to users, when
// content::BluetoothDeviceChooserController::CalculateSignalStrengthLevel() is
// called.
void RecordRSSISignalStrength(int rssi);
void RecordRSSISignalStrengthLevel(UMARSSISignalStrengthLevel level);

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
