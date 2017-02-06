// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothDevice_h
#define WebBluetoothDevice_h

namespace blink {

// An object through which the embedder can trigger events on a Document-bound
// BluetoothDevice object.
class WebBluetoothDevice {
public:
    virtual void dispatchGattServerDisconnected() = 0;
};

} // namespace blink

#endif // WebBluetoothDevice_h
