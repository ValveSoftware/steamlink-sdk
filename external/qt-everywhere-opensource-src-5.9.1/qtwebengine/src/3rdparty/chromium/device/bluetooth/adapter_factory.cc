// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/adapter_factory.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

AdapterFactory::AdapterFactory() : weak_ptr_factory_(this) {}
AdapterFactory::~AdapterFactory() {}

// static
void AdapterFactory::Create(mojom::AdapterFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<AdapterFactory>(),
                          std::move(request));
}

void AdapterFactory::GetAdapter(const GetAdapterCallback& callback) {
  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&AdapterFactory::OnGetAdapter,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    callback.Run(nullptr /* AdapterPtr */);
  }
}

void AdapterFactory::OnGetAdapter(
    const GetAdapterCallback& callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  mojom::AdapterPtr adapter_ptr;
  mojo::MakeStrongBinding(base::MakeUnique<Adapter>(adapter),
                          mojo::GetProxy(&adapter_ptr));
  callback.Run(std::move(adapter_ptr));
}

}  // namespace bluetooth
