// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMDataView.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/BluetoothCharacteristicProperties.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include <memory>

namespace blink {

namespace {

DOMDataView* ConvertWebVectorToDataView(const WebVector<uint8_t>& webVector)
{
    static_assert(sizeof(*webVector.data()) == 1, "uint8_t should be a single byte");
    DOMArrayBuffer* domBuffer = DOMArrayBuffer::create(webVector.data(), webVector.size());
    return DOMDataView::create(domBuffer, 0, webVector.size());
}

} // anonymous namespace

BluetoothRemoteGATTCharacteristic::BluetoothRemoteGATTCharacteristic(ExecutionContext* context, std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit> webCharacteristic, BluetoothRemoteGATTService* service)
    : ActiveDOMObject(context)
    , m_webCharacteristic(std::move(webCharacteristic))
    , m_service(service)
    , m_stopped(false)
{
    m_properties = BluetoothCharacteristicProperties::create(m_webCharacteristic->characteristicProperties);
    // See example in Source/platform/heap/ThreadState.h
    ThreadState::current()->registerPreFinalizer(this);
}

BluetoothRemoteGATTCharacteristic* BluetoothRemoteGATTCharacteristic::take(ScriptPromiseResolver* resolver, std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit> webCharacteristic, BluetoothRemoteGATTService* service)
{
    if (!webCharacteristic) {
        return nullptr;
    }
    BluetoothRemoteGATTCharacteristic* characteristic = new BluetoothRemoteGATTCharacteristic(resolver->getExecutionContext(), std::move(webCharacteristic), service);
    // See note in ActiveDOMObject about suspendIfNeeded.
    characteristic->suspendIfNeeded();
    return characteristic;
}

void BluetoothRemoteGATTCharacteristic::setValue(DOMDataView* domDataView)
{
    m_value = domDataView;
}

void BluetoothRemoteGATTCharacteristic::dispatchCharacteristicValueChanged(
    const WebVector<uint8_t>& value)
{
    this->setValue(ConvertWebVectorToDataView(value));
    dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
}

void BluetoothRemoteGATTCharacteristic::stop()
{
    notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::dispose()
{
    notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::notifyCharacteristicObjectRemoved()
{
    if (!m_stopped) {
        m_stopped = true;
        WebBluetooth* webbluetooth = BluetoothSupplement::fromExecutionContext(ActiveDOMObject::getExecutionContext());
        webbluetooth->characteristicObjectRemoved(m_webCharacteristic->characteristicInstanceID, this);
    }
}

const WTF::AtomicString& BluetoothRemoteGATTCharacteristic::interfaceName() const
{
    return EventTargetNames::BluetoothRemoteGATTCharacteristic;
}

ExecutionContext* BluetoothRemoteGATTCharacteristic::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void BluetoothRemoteGATTCharacteristic::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
    // We will also need to unregister a characteristic once all the event
    // listeners have been removed. See http://crbug.com/541390
    if (eventType == EventTypeNames::characteristicvaluechanged) {
        WebBluetooth* webbluetooth = BluetoothSupplement::fromExecutionContext(getExecutionContext());
        webbluetooth->registerCharacteristicObject(m_webCharacteristic->characteristicInstanceID, this);
    }
}

class ReadValueCallback : public WebBluetoothReadValueCallbacks {
public:
    ReadValueCallback(BluetoothRemoteGATTCharacteristic* characteristic, ScriptPromiseResolver* resolver) : m_webCharacteristic(characteristic), m_resolver(resolver) {}

    void onSuccess(const WebVector<uint8_t>& value) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;

        DOMDataView* domDataView = ConvertWebVectorToDataView(value);
        if (m_webCharacteristic)
            m_webCharacteristic->setValue(domDataView);

        m_resolver->resolve(domDataView);
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }

private:
    WeakPersistent<BluetoothRemoteGATTCharacteristic> m_webCharacteristic;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTCharacteristic::readValue(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->readValue(m_webCharacteristic->characteristicInstanceID, new ReadValueCallback(this, resolver));

    return promise;
}

class WriteValueCallback : public WebBluetoothWriteValueCallbacks {
public:
    WriteValueCallback(BluetoothRemoteGATTCharacteristic* characteristic, ScriptPromiseResolver* resolver) : m_webCharacteristic(characteristic), m_resolver(resolver) {}

    void onSuccess(const WebVector<uint8_t>& value) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;

        if (m_webCharacteristic) {
            m_webCharacteristic->setValue(ConvertWebVectorToDataView(value));
        }
        m_resolver->resolve();
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }

private:
    WeakPersistent<BluetoothRemoteGATTCharacteristic> m_webCharacteristic;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTCharacteristic::writeValue(ScriptState* scriptState, const DOMArrayPiece& value)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    // Partial implementation of writeValue algorithm:
    // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothgattcharacteristic-writevalue

    // If bytes is more than 512 bytes long (the maximum length of an attribute
    // value, per Long Attribute Values) return a promise rejected with an
    // InvalidModificationError and abort.
    if (value.byteLength() > 512)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidModificationError, "Value can't exceed 512 bytes."));

    // Let valueVector be a copy of the bytes held by value.
    WebVector<uint8_t> valueVector(value.bytes(), value.byteLength());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);

    ScriptPromise promise = resolver->promise();
    webbluetooth->writeValue(m_webCharacteristic->characteristicInstanceID, valueVector, new WriteValueCallback(this, resolver));

    return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::startNotifications(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->startNotifications(m_webCharacteristic->characteristicInstanceID, new CallbackPromiseAdapter<void, BluetoothError>(resolver));
    return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::stopNotifications(ScriptState* scriptState)
{
#if OS(MACOSX) || OS(ANDROID)
    // TODO(jlebel): Remove when stopNotifications is implemented.
    // TODO(scheib): Remove when stopNotifications is implemented.
    return ScriptPromise::rejectWithDOMException(scriptState,
        DOMException::create(NotSupportedError,
            "stopNotifications is not implemented yet. See https://goo.gl/J6ASzs"));
#endif // OS(MACOSX) || OS(ANDROID)

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->stopNotifications(m_webCharacteristic->characteristicInstanceID, new CallbackPromiseAdapter<void, BluetoothError>(resolver));
    return promise;
}

DEFINE_TRACE(BluetoothRemoteGATTCharacteristic)
{
    visitor->trace(m_service);
    visitor->trace(m_properties);
    visitor->trace(m_value);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
