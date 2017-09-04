// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/NavigationPreloadCallbacks.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/network/HTTPParsers.h"

namespace blink {

ScriptPromise NavigationPreloadManager::enable(ScriptState* scriptState) {
  return setEnabled(true, scriptState);
}

ScriptPromise NavigationPreloadManager::disable(ScriptState* scriptState) {
  return setEnabled(false, scriptState);
}

ScriptPromise NavigationPreloadManager::setHeaderValue(ScriptState* scriptState,
                                                       const String& value) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::from(m_registration->getExecutionContext());
  if (!client || !client->provider()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, "No provider."));
  }

  if (!isValidHTTPHeaderValue(value)) {
    return ScriptPromise::reject(
        scriptState, V8ThrowException::createTypeError(
                         scriptState->isolate(),
                         "The string provided to setHeaderValue ('" + value +
                             "') is not a valid HTTP header field value."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->setNavigationPreloadHeader(
      value, client->provider(),
      makeUnique<SetNavigationPreloadHeaderCallbacks>(resolver));
  return promise;
}

ScriptPromise NavigationPreloadManager::getState(ScriptState* scriptState) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::from(m_registration->getExecutionContext());
  if (!client || !client->provider()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, "No provider."));
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->getNavigationPreloadState(
      client->provider(),
      makeUnique<GetNavigationPreloadStateCallbacks>(resolver));
  return promise;
}

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {}

ScriptPromise NavigationPreloadManager::setEnabled(bool enable,
                                                   ScriptState* scriptState) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::from(m_registration->getExecutionContext());
  if (!client || !client->provider()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, "No provider."));
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->enableNavigationPreload(
      enable, client->provider(),
      makeUnique<EnableNavigationPreloadCallbacks>(resolver));
  return promise;
}

DEFINE_TRACE(NavigationPreloadManager) {
  visitor->trace(m_registration);
}

}  // namespace blink
