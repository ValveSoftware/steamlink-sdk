// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTServer_h
#define BluetoothRemoteGATTServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrUnsignedLong.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothError.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BluetoothDevice;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTServer provides a way to interact with a connected bluetooth peripheral.
class BluetoothRemoteGATTServer final
    : public GarbageCollected<BluetoothRemoteGATTServer>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    BluetoothRemoteGATTServer(BluetoothDevice*);

    static BluetoothRemoteGATTServer* create(BluetoothDevice*);

    void setConnected(bool connected) { m_connected = connected; }

    // Interface required by Garbage Collectoin:
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    BluetoothDevice* device() { return m_device; }
    bool connected() { return m_connected; }
    ScriptPromise connect(ScriptState*);
    void disconnect(ScriptState*);
    ScriptPromise getPrimaryService(ScriptState*, const StringOrUnsignedLong& service, ExceptionState&);
    ScriptPromise getPrimaryServices(ScriptState*, const StringOrUnsignedLong& service, ExceptionState&);
    ScriptPromise getPrimaryServices(ScriptState*, ExceptionState&);

private:
    ScriptPromise getPrimaryServicesImpl(ScriptState*, mojom::WebBluetoothGATTQueryQuantity, String serviceUUID = String());

    Member<BluetoothDevice> m_device;
    bool m_connected;
};

} // namespace blink

#endif // BluetoothDevice_h
