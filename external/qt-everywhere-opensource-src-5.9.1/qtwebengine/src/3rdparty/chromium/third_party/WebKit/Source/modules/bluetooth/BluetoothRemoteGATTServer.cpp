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
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

namespace {

const char kGATTServerDisconnected[] =
    "GATT Server disconnected while retrieving services.";
const char kGATTServerNotConnected[] =
    "GATT Server is disconnected. Cannot retrieve services.";
}

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(BluetoothDevice* device)
    : m_device(device), m_connected(false) {}

BluetoothRemoteGATTServer* BluetoothRemoteGATTServer::create(
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTServer(device);
}

void BluetoothRemoteGATTServer::AddToActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  auto result = m_activeAlgorithms.add(resolver);
  CHECK(result.isNewEntry);
}

bool BluetoothRemoteGATTServer::RemoveFromActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  if (!m_activeAlgorithms.contains(resolver)) {
    return false;
  }
  m_activeAlgorithms.remove(resolver);
  return true;
}

DEFINE_TRACE(BluetoothRemoteGATTServer) {
  visitor->trace(m_activeAlgorithms);
  visitor->trace(m_device);
}

class ConnectCallback : public WebBluetoothRemoteGATTServerConnectCallbacks {
 public:
  ConnectCallback(BluetoothDevice* device, ScriptPromiseResolver* resolver)
      : m_device(device), m_resolver(resolver) {}

  void onSuccess() override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;
    m_device->gatt()->setConnected(true);
    m_resolver->resolve(m_device->gatt());
  }

  void onError(
      int32_t
          error /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */)
      override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;
    m_resolver->reject(BluetoothError::take(m_resolver, error));
  }

 private:
  Persistent<BluetoothDevice> m_device;
  Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* scriptState) {
  WebBluetooth* webbluetooth =
      BluetoothSupplement::fromScriptState(scriptState);
  if (!webbluetooth)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  webbluetooth->connect(device()->id(), device(),
                        new ConnectCallback(device(), resolver));
  return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* scriptState) {
  if (!m_connected)
    return;
  device()->cleanupDisconnectedDeviceAndFireEvent();
  WebBluetooth* webbluetooth =
      BluetoothSupplement::fromScriptState(scriptState);
  webbluetooth->disconnect(device()->id());
}

// Class that allows us to resolve the promise with a single service or
// with a vector owning the services.
class GetPrimaryServicesCallback
    : public WebBluetoothGetPrimaryServicesCallbacks {
 public:
  GetPrimaryServicesCallback(
      BluetoothDevice* device,
      mojom::blink::WebBluetoothGATTQueryQuantity quantity,
      ScriptPromiseResolver* resolver)
      : m_device(device), m_quantity(quantity), m_resolver(resolver) {
    // We always check that the device is connected before constructing this
    // object.
    CHECK(m_device->gatt()->connected());
    m_device->gatt()->AddToActiveAlgorithms(m_resolver.get());
  }

  void onSuccess(
      const WebVector<WebBluetoothRemoteGATTService*>& webServices) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;

    // If the resolver is not in the set of ActiveAlgorithms then the frame
    // disconnected so we reject.
    if (!m_device->gatt()->RemoveFromActiveAlgorithms(m_resolver.get())) {
      m_resolver->reject(
          DOMException::create(NetworkError, kGATTServerDisconnected));
      return;
    }

    if (m_quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, webServices.size());
      m_resolver->resolve(m_device->getOrCreateBluetoothRemoteGATTService(
          wrapUnique(webServices[0])));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTService>> services;
    services.reserveInitialCapacity(webServices.size());
    for (WebBluetoothRemoteGATTService* webService : webServices) {
      services.append(m_device->getOrCreateBluetoothRemoteGATTService(
          wrapUnique(webService)));
    }
    m_resolver->resolve(services);
  }

  void onError(
      int32_t
          error /* Corresponds to WebBluetoothResult in web_bluetooth.mojom */)
      override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
      return;

    if (!m_device->gatt()->RemoveFromActiveAlgorithms(m_resolver.get())) {
      m_resolver->reject(
          DOMException::create(NetworkError, kGATTServerDisconnected));
      return;
    }

    m_resolver->reject(BluetoothError::take(m_resolver, error));
  }

 private:
  Persistent<BluetoothDevice> m_device;
  mojom::blink::WebBluetoothGATTQueryQuantity m_quantity;
  const Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(
    ScriptState* scriptState,
    const StringOrUnsignedLong& service,
    ExceptionState& exceptionState) {
  String serviceUUID = BluetoothUUID::getService(service, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* scriptState,
    const StringOrUnsignedLong& service,
    ExceptionState& exceptionState) {
  String serviceUUID = BluetoothUUID::getService(service, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* scriptState,
    ExceptionState&) {
  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServicesImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String servicesUUID) {
  if (!connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  WebBluetooth* webbluetooth =
      BluetoothSupplement::fromScriptState(scriptState);
  webbluetooth->getPrimaryServices(
      device()->id(), static_cast<int32_t>(quantity), servicesUUID,
      new GetPrimaryServicesCallback(device(), quantity, resolver));

  return promise;
}

}  // namespace blink
