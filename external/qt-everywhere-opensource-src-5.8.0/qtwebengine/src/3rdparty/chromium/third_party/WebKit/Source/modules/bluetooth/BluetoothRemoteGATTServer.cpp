// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTServer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(BluetoothDevice* device)
    : m_device(device)
    , m_connected(false)
{
}

BluetoothRemoteGATTServer* BluetoothRemoteGATTServer::create(BluetoothDevice* device)
{
    return new BluetoothRemoteGATTServer(device);
}

DEFINE_TRACE(BluetoothRemoteGATTServer)
{
    visitor->trace(m_device);
}

class ConnectCallback : public WebBluetoothRemoteGATTServerConnectCallbacks {
public:
    ConnectCallback(BluetoothDevice* device, ScriptPromiseResolver* resolver)
        : m_device(device)
        , m_resolver(resolver) {}

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->gatt()->setConnected(true);
        m_resolver->resolve(m_device->gatt());
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }
private:
    Persistent<BluetoothDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->connect(device()->id(), device(), new ConnectCallback(device(), resolver));
    return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* scriptState)
{
    if (!m_connected)
        return;
    m_connected = false;
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->disconnect(device()->id());
    device()->dispatchEvent(Event::createBubble(EventTypeNames::gattserverdisconnected));
}

// Class that allows us to resolve the promise with a single service or
// with a vector owning the services.
class GetPrimaryServicesCallback : public WebBluetoothGetPrimaryServicesCallbacks {
public:
    GetPrimaryServicesCallback(BluetoothDevice* device, mojom::WebBluetoothGATTQueryQuantity quantity, ScriptPromiseResolver* resolver)
        : m_device(device)
        , m_quantity(quantity)
        , m_resolver(resolver) {}

    void onSuccess(const WebVector<WebBluetoothRemoteGATTService*>& webServices) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;

        if (m_quantity == mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
            DCHECK_EQ(1u, webServices.size());
            m_resolver->resolve(BluetoothRemoteGATTService::take(m_resolver, wrapUnique(webServices[0]), m_device));
            return;
        }

        HeapVector<Member<BluetoothRemoteGATTService>> services;
        services.reserveInitialCapacity(webServices.size());
        for (WebBluetoothRemoteGATTService* webService : webServices) {
            services.append(BluetoothRemoteGATTService::take(m_resolver, wrapUnique(webService), m_device));
        }
        m_resolver->resolve(services);
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }
private:
    Persistent<BluetoothDevice> m_device;
    mojom::WebBluetoothGATTQueryQuantity m_quantity;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(ScriptState* scriptState, const StringOrUnsignedLong& service, ExceptionState& exceptionState)
{
    String serviceUUID = BluetoothUUID::getService(service, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getPrimaryServicesImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::SINGLE, serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(ScriptState* scriptState, const StringOrUnsignedLong& service, ExceptionState& exceptionState)
{
    String serviceUUID = BluetoothUUID::getService(service, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getPrimaryServicesImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE, serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(ScriptState* scriptState, ExceptionState&)
{
    return getPrimaryServicesImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServicesImpl(ScriptState* scriptState, mojom::WebBluetoothGATTQueryQuantity quantity, String servicesUUID)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->getPrimaryServices(device()->id(), quantity, servicesUUID, new GetPrimaryServicesCallback(device(), quantity, resolver));

    return promise;
}

} // namespace blink
