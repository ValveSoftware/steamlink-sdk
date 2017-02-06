// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothError_h
#define WebBluetoothError_h

#include "public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace blink {

// Errors that can occur during Web Bluetooth execution, which are transformed
// to a DOMException in Source/modules/bluetooth/BluetoothError.cpp.
//
// These errors all produce constant message strings. If a particular message
// needs a dynamic component, we should add a separate enum so type-checking the IPC
// ensures the dynamic component is passed.
using WebBluetoothError = mojom::WebBluetoothError;
} // namespace blink

#endif // WebBluetoothError_h
