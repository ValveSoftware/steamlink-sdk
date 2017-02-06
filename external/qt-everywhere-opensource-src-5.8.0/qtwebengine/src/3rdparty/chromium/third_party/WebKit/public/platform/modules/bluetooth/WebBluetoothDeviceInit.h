// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothDeviceInit_h
#define WebBluetoothDeviceInit_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// Information provided by the platform to initialize BluetoothDevice objects
// with attributes as specified in BluetoothDevice.idl.
struct WebBluetoothDeviceInit {
    WebBluetoothDeviceInit(const WebString& id,
        const WebString& name,
        const WebVector<WebString>& uuids)
        : id(id)
        , name(name)
        , uuids(uuids)
    {
    }

    // Members corresponding to BluetoothDevice attributes as specified in IDL.
    const WebString id;
    const WebString name;
    const WebVector<WebString> uuids;
};

} // namespace blink

#endif // WebBluetoothDeviceInit_h
