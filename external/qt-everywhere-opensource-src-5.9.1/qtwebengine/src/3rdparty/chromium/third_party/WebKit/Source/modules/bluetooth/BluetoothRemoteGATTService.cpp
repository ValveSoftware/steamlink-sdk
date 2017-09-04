// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

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
#include <utility>

namespace blink {

namespace {

const char kGATTServerDisconnected[] =
    "GATT Server disconnected while retrieving characteristics.";
const char kGATTServerNotConnected[] =
    "GATT Server is disconnected. Cannot retrieve characteristics.";
const char kInvalidService[] =
    "Service is no longer valid. Remember to retrieve the service again after "
    "reconnecting.";

}  // namespace

BluetoothRemoteGATTService::BluetoothRemoteGATTService(
    std::unique_ptr<WebBluetoothRemoteGATTService> webService,
    BluetoothDevice* device)
    : m_webService(std::move(webService)), m_device(device) {
  DCHECK(m_webService);
}

DEFINE_TRACE(BluetoothRemoteGATTService) {
  visitor->trace(m_device);
}

// Class that allows us to resolve the promise with a single Characteristic or
// with a vector owning the characteristics.
class GetCharacteristicsCallback
    : public WebBluetoothGetCharacteristicsCallbacks {
 public:
  GetCharacteristicsCallback(
      BluetoothRemoteGATTService* service,
      mojom::blink::WebBluetoothGATTQueryQuantity quantity,
      ScriptPromiseResolver* resolver)
      : m_service(service), m_quantity(quantity), m_resolver(resolver) {
    // We always check that the device is connected before constructing this
    // object.
    CHECK(m_service->device()->gatt()->connected());
    m_service->device()->gatt()->AddToActiveAlgorithms(m_resolver.get());
  }

  void onSuccess(const WebVector<WebBluetoothRemoteGATTCharacteristicInit*>&
                     webCharacteristics) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;

    // If the resolver is not in the set of ActiveAlgorithms then the frame
    // disconnected so we reject.
    if (!m_service->device()->gatt()->RemoveFromActiveAlgorithms(
            m_resolver.get())) {
      m_resolver->reject(
          DOMException::create(NetworkError, kGATTServerDisconnected));
      return;
    }

    if (m_quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, webCharacteristics.size());
      m_resolver->resolve(
          m_service->device()->getOrCreateBluetoothRemoteGATTCharacteristic(
              m_resolver->getExecutionContext(),
              wrapUnique(webCharacteristics[0]), m_service));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTCharacteristic>> characteristics;
    characteristics.reserveInitialCapacity(webCharacteristics.size());
    for (WebBluetoothRemoteGATTCharacteristicInit* webCharacteristic :
         webCharacteristics) {
      characteristics.append(
          m_service->device()->getOrCreateBluetoothRemoteGATTCharacteristic(
              m_resolver->getExecutionContext(), wrapUnique(webCharacteristic),
              m_service));
    }
    m_resolver->resolve(characteristics);
  }

  void onError(
      int32_t
          error /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */)
      override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;

    // If the resolver is not in the set of ActiveAlgorithms then the frame
    // disconnected so we reject.
    if (!m_service->device()->gatt()->RemoveFromActiveAlgorithms(
            m_resolver.get())) {
      m_resolver->reject(
          DOMException::create(NetworkError, kGATTServerDisconnected));
      return;
    }

    m_resolver->reject(BluetoothError::take(m_resolver, error));
  }

 private:
  Persistent<BluetoothRemoteGATTService> m_service;
  mojom::blink::WebBluetoothGATTQueryQuantity m_quantity;
  const Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(
    ScriptState* scriptState,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exceptionState) {
  String characteristicUUID =
      BluetoothUUID::getCharacteristic(characteristic, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* scriptState,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exceptionState) {
  String characteristicUUID =
      BluetoothUUID::getCharacteristic(characteristic, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* scriptState,
    ExceptionState&) {
  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristicsImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String characteristicsUUID) {
  if (!device()->gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!device()->isValidService(m_webService->serviceInstanceID)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, kInvalidService));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  WebBluetooth* webbluetooth =
      BluetoothSupplement::fromScriptState(scriptState);
  webbluetooth->getCharacteristics(
      m_webService->serviceInstanceID, static_cast<int32_t>(quantity),
      characteristicsUUID,
      new GetCharacteristicsCallback(this, quantity, resolver));

  return promise;
}

}  // namespace blink
