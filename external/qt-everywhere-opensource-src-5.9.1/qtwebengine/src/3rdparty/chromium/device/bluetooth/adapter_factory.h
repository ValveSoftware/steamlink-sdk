// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_ADAPTER_FACTORY_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/interfaces/adapter.mojom.h"

namespace bluetooth {

// Implementation of Mojo AdapterFactory located in
// device/bluetooth/public/interfaces/adapter.mojom.
// It handles requests for Mojo Adapter instances
// and strongly binds the Adapter instance to the system's
// Bluetooth adapter.
class AdapterFactory : public mojom::AdapterFactory {
 public:
  AdapterFactory();
  ~AdapterFactory() override;

  // Creates an AdapterFactory with a strong Mojo binding to |request|.
  static void Create(mojom::AdapterFactoryRequest request);

  // mojom::AdapterFactory overrides:
  void GetAdapter(const GetAdapterCallback& callback) override;

 private:
  void OnGetAdapter(const GetAdapterCallback& callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  base::WeakPtrFactory<AdapterFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdapterFactory);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADAPTER_FACTORY_H_
