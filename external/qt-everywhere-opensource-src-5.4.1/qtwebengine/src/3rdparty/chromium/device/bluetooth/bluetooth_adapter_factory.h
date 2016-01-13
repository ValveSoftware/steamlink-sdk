// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

// A factory class for building a Bluetooth adapter on platforms where Bluetooth
// is available.
class BluetoothAdapterFactory {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothAdapter> adapter)>
      AdapterCallback;

  // Returns true if the Bluetooth adapter is available for the current
  // platform.
  static bool IsBluetoothAdapterAvailable();

  // Returns the shared instance of the default adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  static void GetAdapter(const AdapterCallback& callback);

  // Sets the shared instance of the default adapter for testing purposes only,
  // no reference is retained after completion of the call, removing the last
  // reference will reset the factory.
  static void SetAdapterForTesting(scoped_refptr<BluetoothAdapter> adapter);

  // Returns true iff the implementation has a (non-NULL) shared instance of the
  // adapter. Exposed for testing.
  static bool HasSharedInstanceForTesting();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
