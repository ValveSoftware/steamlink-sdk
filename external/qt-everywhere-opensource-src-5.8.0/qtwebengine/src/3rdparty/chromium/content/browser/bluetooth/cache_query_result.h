// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_CACHE_QUERY_RESULT_H_
#define CONTENT_BROWSER_BLUETOOTH_CACHE_QUERY_RESULT_H_

#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace device {
class BluetoothDevice;
class BluetoothRemoteGattService;
class BluetoothRemoteGattCharacteristic;
}

namespace content {

// Struct that holds the result of a cache query.
//
// TODO(ortuno): Move into WebBluetoothServiceImpl.
// https://crbug.com/508771
struct CacheQueryResult {
  CacheQueryResult() : outcome(CacheQueryOutcome::SUCCESS) {}

  explicit CacheQueryResult(CacheQueryOutcome outcome) : outcome(outcome) {}

  ~CacheQueryResult() {}

  blink::mojom::WebBluetoothError GetWebError() const {
    switch (outcome) {
      case CacheQueryOutcome::SUCCESS:
      case CacheQueryOutcome::BAD_RENDERER:
        NOTREACHED();
        return blink::mojom::WebBluetoothError::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::NO_DEVICE:
        return blink::mojom::WebBluetoothError::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::NO_SERVICE:
        return blink::mojom::WebBluetoothError::SERVICE_NO_LONGER_EXISTS;
      case CacheQueryOutcome::NO_CHARACTERISTIC:
        return blink::mojom::WebBluetoothError::CHARACTERISTIC_NO_LONGER_EXISTS;
    }
    NOTREACHED();
    return blink::mojom::WebBluetoothError::DEVICE_NO_LONGER_IN_RANGE;
  }

  device::BluetoothDevice* device = nullptr;
  device::BluetoothRemoteGattService* service = nullptr;
  device::BluetoothRemoteGattCharacteristic* characteristic = nullptr;
  CacheQueryOutcome outcome;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_CACHE_QUERY_RESULT_H_
