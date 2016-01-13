// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace device {

namespace {

// Shared default adapter instance.  We don't want to keep this class around
// if nobody is using it, so use a WeakPtr and create the object when needed.
// Since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors, we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<BluetoothAdapter> >::Leaky default_adapter =
    LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN)
typedef std::vector<BluetoothAdapterFactory::AdapterCallback>
    AdapterCallbackList;

// List of adapter callbacks to be called once the adapter is initialized.
// Since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a lazy instance for it.
base::LazyInstance<AdapterCallbackList> adapter_callbacks =
    LAZY_INSTANCE_INITIALIZER;

void RunAdapterCallbacks() {
  DCHECK(default_adapter.Get());
  scoped_refptr<BluetoothAdapter> adapter(default_adapter.Get().get());
  for (std::vector<BluetoothAdapterFactory::AdapterCallback>::const_iterator
           iter = adapter_callbacks.Get().begin();
       iter != adapter_callbacks.Get().end();
       ++iter) {
    iter->Run(adapter);
  }
  adapter_callbacks.Get().clear();
}
#endif  // defined(OS_WIN)

}  // namespace

// static
bool BluetoothAdapterFactory::IsBluetoothAdapterAvailable() {
  // SetAdapterForTesting() may be used to provide a test or mock adapter
  // instance even on platforms that would otherwise not support it.
  if (default_adapter.Get())
    return true;
#if defined(OS_CHROMEOS) || defined(OS_WIN)
  return true;
#elif defined(OS_MACOSX)
  return base::mac::IsOSLionOrLater();
#else
  return false;
#endif
}

// static
void BluetoothAdapterFactory::GetAdapter(const AdapterCallback& callback) {
  DCHECK(IsBluetoothAdapterAvailable());

#if defined(OS_WIN)
  if (!default_adapter.Get()) {
    default_adapter.Get() =
        BluetoothAdapter::CreateAdapter(base::Bind(&RunAdapterCallbacks));
    DCHECK(!default_adapter.Get()->IsInitialized());
  }

  if (!default_adapter.Get()->IsInitialized())
    adapter_callbacks.Get().push_back(callback);
#else  // !defined(OS_WIN)
  if (!default_adapter.Get()) {
    default_adapter.Get() =
        BluetoothAdapter::CreateAdapter(BluetoothAdapter::InitCallback());
  }

  DCHECK(default_adapter.Get()->IsInitialized());
#endif  // defined(OS_WIN)

  if (default_adapter.Get()->IsInitialized())
    callback.Run(scoped_refptr<BluetoothAdapter>(default_adapter.Get().get()));

}

// static
void BluetoothAdapterFactory::SetAdapterForTesting(
    scoped_refptr<BluetoothAdapter> adapter) {
  default_adapter.Get() = adapter->GetWeakPtrForTesting();
}

// static
bool BluetoothAdapterFactory::HasSharedInstanceForTesting() {
  return default_adapter.Get();
}

}  // namespace device
