// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_ADAPTER_FACTORY_H_
#define DEVICE_NFC_NFC_ADAPTER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/nfc/nfc_adapter.h"

namespace device {

// NfcAdapterFactory is a class that contains static methods, which
// instantiate either a specific NFC adapter, or the generic "default
// adapter" which may change depending on availability.
class NfcAdapterFactory {
 public:
  typedef base::Callback<void(scoped_refptr<NfcAdapter>)> AdapterCallback;

  // Returns true if NFC is available for the current platform.
  static bool IsNfcAvailable();

  // Returns the shared instance of the default adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  static void GetAdapter(const AdapterCallback& callback);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_ADAPTER_FACTORY_H_
