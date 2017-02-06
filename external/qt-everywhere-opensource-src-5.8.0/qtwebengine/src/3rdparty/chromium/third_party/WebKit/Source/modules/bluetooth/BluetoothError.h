// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothError_h
#define BluetoothError_h

#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/WebBluetoothError.h"
#include "wtf/Allocator.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

// BluetoothError is used with CallbackPromiseAdapter to receive
// WebBluetoothError responses. See CallbackPromiseAdapter class comments.
class BluetoothError {
    STATIC_ONLY(BluetoothError);
public:
    // Interface required by CallbackPromiseAdapter:
    using WebType = const WebBluetoothError&;
    static DOMException* take(ScriptPromiseResolver*, const WebBluetoothError&);
};

} // namespace blink

#endif // BluetoothError_h
