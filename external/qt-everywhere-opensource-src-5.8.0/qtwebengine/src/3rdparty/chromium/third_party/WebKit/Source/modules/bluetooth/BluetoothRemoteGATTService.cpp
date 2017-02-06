// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

BluetoothRemoteGATTService::BluetoothRemoteGATTService(std::unique_ptr<WebBluetoothRemoteGATTService> webService, BluetoothDevice* device)
    : m_webService(std::move(webService))
    , m_device(device)
{
}

BluetoothRemoteGATTService* BluetoothRemoteGATTService::take(ScriptPromiseResolver*, std::unique_ptr<WebBluetoothRemoteGATTService> webService, BluetoothDevice* device)
{
    if (!webService) {
        return nullptr;
    }
    return new BluetoothRemoteGATTService(std::move(webService), device);
}

DEFINE_TRACE(BluetoothRemoteGATTService)
{
    visitor->trace(m_device);
}

// Class that allows us to resolve the promise with a single Characteristic or
// with a vector owning the characteristics.
class GetCharacteristicsCallback : public WebBluetoothGetCharacteristicsCallbacks {
public:
    GetCharacteristicsCallback(BluetoothRemoteGATTService* service, mojom::WebBluetoothGATTQueryQuantity quantity, ScriptPromiseResolver* resolver)
        : m_service(service)
        , m_quantity(quantity)
        , m_resolver(resolver) {}

    void onSuccess(const WebVector<WebBluetoothRemoteGATTCharacteristicInit*>& webCharacteristics) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;

        if (m_quantity == mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
            DCHECK_EQ(1u, webCharacteristics.size());
            m_resolver->resolve(BluetoothRemoteGATTCharacteristic::take(m_resolver, wrapUnique(webCharacteristics[0]), m_service));
            return;
        }

        HeapVector<Member<BluetoothRemoteGATTCharacteristic>> characteristics;
        characteristics.reserveInitialCapacity(webCharacteristics.size());
        for (WebBluetoothRemoteGATTCharacteristicInit* webCharacteristic : webCharacteristics) {
            characteristics.append(BluetoothRemoteGATTCharacteristic::take(m_resolver, wrapUnique(webCharacteristic), m_service));
        }
        m_resolver->resolve(characteristics);
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }
private:
    Persistent<BluetoothRemoteGATTService> m_service;
    mojom::WebBluetoothGATTQueryQuantity m_quantity;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(ScriptState* scriptState, const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::SINGLE, characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE, characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, ExceptionState&)
{
    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristicsImpl(ScriptState* scriptState, mojom::WebBluetoothGATTQueryQuantity quantity, String characteristicsUUID)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->getCharacteristics(m_webService->serviceInstanceID, quantity, characteristicsUUID, new GetCharacteristicsCallback(this, quantity, resolver));

    return promise;
}

} // namespace blink
