// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDevice_h
#define BluetoothDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/BluetoothRemoteGATTServer.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "public/platform/modules/bluetooth/WebBluetoothDeviceInit.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class BluetoothRemoteGATTServer;
class ScriptPromise;
class ScriptPromiseResolver;

// BluetoothDevice represents a physical bluetooth device in the DOM. See IDL.
//
// Callbacks providing WebBluetoothDevice objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothDevice final
    : public EventTargetWithInlineData
    , public ActiveDOMObject
    , public WebBluetoothDevice {
    USING_PRE_FINALIZER(BluetoothDevice, dispose);
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(BluetoothDevice);
public:
    BluetoothDevice(ExecutionContext*, std::unique_ptr<WebBluetoothDeviceInit>);

    // Interface required by CallbackPromiseAdapter:
    using WebType = std::unique_ptr<WebBluetoothDeviceInit>;
    static BluetoothDevice* take(ScriptPromiseResolver*, std::unique_ptr<WebBluetoothDeviceInit>);

    // We should disconnect from the device in all of the following cases:
    // 1. When the object gets GarbageCollected e.g. it went out of scope.
    // dispose() is called in this case.
    // 2. When the parent document gets detached e.g. reloading a page.
    // stop() is called in this case.
    // TODO(ortuno): Users should be able to turn on notifications for
    // events on navigator.bluetooth and still remain connected even if the
    // BluetoothDevice object is garbage collected.

    // USING_PRE_FINALIZER interface.
    // Called before the object gets garbage collected.
    void dispose();

    // ActiveDOMObject interface.
    void stop() override;

    // If gatt is connected then disconnects and sets gatt.connected to false.
    // Returns true if gatt was disconnected.
    bool disconnectGATTIfConnected();

    // EventTarget methods:
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // WebBluetoothDevice interface:
    void dispatchGattServerDisconnected() override;

    // Interface required by Garbage Collection:
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    String id() { return m_webDevice->id; }
    String name() { return m_webDevice->name; }
    BluetoothRemoteGATTServer* gatt() { return m_gatt; }
    Vector<String> uuids();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(gattserverdisconnected);

private:
    std::unique_ptr<WebBluetoothDeviceInit> m_webDevice;
    Member<BluetoothRemoteGATTServer> m_gatt;
};

} // namespace blink

#endif // BluetoothDevice_h
