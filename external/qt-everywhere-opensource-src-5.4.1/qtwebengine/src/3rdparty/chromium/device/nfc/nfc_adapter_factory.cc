// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_adapter_factory.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"

#if defined(OS_CHROMEOS)
#include "device/nfc/nfc_adapter_chromeos.h"
#endif

namespace device {

namespace {

// Shared default adapter instance, we don't want to keep this class around
// if nobody is using it so use a WeakPtr and create the object when needed;
// since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<device::NfcAdapter> >::Leaky
    default_adapter = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
bool NfcAdapterFactory::IsNfcAvailable() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

// static
void NfcAdapterFactory::GetAdapter(const AdapterCallback&  callback) {
  if (!IsNfcAvailable()) {
    LOG(WARNING) << "NFC is not available on the current platform.";
    return;
  }
  if (!default_adapter.Get().get()) {
#if defined(OS_CHROMEOS)
    chromeos::NfcAdapterChromeOS* new_adapter =
        new chromeos::NfcAdapterChromeOS();
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
#endif
  }
  if (default_adapter.Get()->IsInitialized())
    callback.Run(scoped_refptr<NfcAdapter>(default_adapter.Get().get()));
}

}  // namespace device
