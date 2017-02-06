// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTService_h
#define BluetoothRemoteGATTService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrUnsignedLong.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothRemoteGATTService.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// Represents a GATT Service within a Bluetooth Peripheral, a collection of
// characteristics and relationships to other services that encapsulate the
// behavior of part of a device.
//
// Callbacks providing WebBluetoothRemoteGATTService objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothRemoteGATTService final
    : public GarbageCollectedFinalized<BluetoothRemoteGATTService>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    explicit BluetoothRemoteGATTService(std::unique_ptr<WebBluetoothRemoteGATTService>, BluetoothDevice*);

    // Interface required by CallbackPromiseAdapter:
    using WebType = std::unique_ptr<WebBluetoothRemoteGATTService>;
    static BluetoothRemoteGATTService* take(ScriptPromiseResolver*, std::unique_ptr<WebBluetoothRemoteGATTService>, BluetoothDevice*);

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    String uuid() { return m_webService->uuid; }
    bool isPrimary() { return m_webService->isPrimary; }
    BluetoothDevice* device() { return m_device; }
    ScriptPromise getCharacteristic(ScriptState*, const StringOrUnsignedLong& characteristic, ExceptionState&);
    ScriptPromise getCharacteristics(ScriptState*, const StringOrUnsignedLong& characteristic, ExceptionState&);
    ScriptPromise getCharacteristics(ScriptState*, ExceptionState&);

private:
    ScriptPromise getCharacteristicsImpl(ScriptState*, mojom::WebBluetoothGATTQueryQuantity, String characteristicUUID = String());

    std::unique_ptr<WebBluetoothRemoteGATTService> m_webService;
    Member<BluetoothDevice> m_device;
};

} // namespace blink

#endif // BluetoothRemoteGATTService_h
