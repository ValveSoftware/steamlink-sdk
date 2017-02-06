// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTCharacteristic_h
#define BluetoothRemoteGATTCharacteristic_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMArrayPiece.h"
#include "core/dom/DOMDataView.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristic.h"
#include "public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristicInit.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class BluetoothCharacteristicProperties;
class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTCharacteristic represents a GATT Characteristic, which is a
// basic data element that provides further information about a peripheral's
// service.
//
// Callbacks providing WebBluetoothRemoteGATTCharacteristicInit objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothRemoteGATTCharacteristic final
    : public EventTargetWithInlineData
    , public ActiveDOMObject
    , public WebBluetoothRemoteGATTCharacteristic {
    USING_PRE_FINALIZER(BluetoothRemoteGATTCharacteristic, dispose);
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(BluetoothRemoteGATTCharacteristic);
public:
    explicit BluetoothRemoteGATTCharacteristic(ExecutionContext*, std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit>, BluetoothRemoteGATTService*);

    // Interface required by CallbackPromiseAdapter.
    using WebType = std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit>;
    static BluetoothRemoteGATTCharacteristic* take(ScriptPromiseResolver*, std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit>, BluetoothRemoteGATTService*);

    // Save value.
    void setValue(DOMDataView*);

    // WebBluetoothRemoteGATTCharacteristic interface:
    void dispatchCharacteristicValueChanged(const WebVector<uint8_t>&) override;

    // ActiveDOMObject interface.
    void stop() override;

    // USING_PRE_FINALIZER interface.
    // Called before the object gets garbage collected.
    void dispose();

    // Notify our embedder that we should stop any notifications.
    // The function only notifies the embedder once.
    void notifyCharacteristicObjectRemoved();

    // EventTarget methods:
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const;

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    BluetoothRemoteGATTService* service() { return m_service; }
    String uuid() { return m_webCharacteristic->uuid; }
    BluetoothCharacteristicProperties* properties() { return m_properties; }
    DOMDataView* value() const { return m_value; }
    ScriptPromise readValue(ScriptState*);
    ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);
    ScriptPromise startNotifications(ScriptState*);
    ScriptPromise stopNotifications(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(characteristicvaluechanged);

protected:
    // EventTarget overrides.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;

private:
    std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit> m_webCharacteristic;
    Member<BluetoothRemoteGATTService> m_service;
    bool m_stopped;
    Member<BluetoothCharacteristicProperties> m_properties;
    Member<DOMDataView> m_value;
};

} // namespace blink

#endif // BluetoothRemoteGATTCharacteristic_h
