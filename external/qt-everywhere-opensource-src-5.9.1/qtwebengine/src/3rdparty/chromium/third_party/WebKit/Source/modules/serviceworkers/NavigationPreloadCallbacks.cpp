// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/NavigationPreloadState.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "public/platform/modules/serviceworker/WebNavigationPreloadState.h"

namespace blink {

EnableNavigationPreloadCallbacks::EnableNavigationPreloadCallbacks(
    ScriptPromiseResolver* resolver)
    : m_resolver(resolver) {
  DCHECK(m_resolver);
}

EnableNavigationPreloadCallbacks::~EnableNavigationPreloadCallbacks() {}

void EnableNavigationPreloadCallbacks::onSuccess() {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  m_resolver->resolve();
}

void EnableNavigationPreloadCallbacks::onError(
    const WebServiceWorkerError& error) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
}

GetNavigationPreloadStateCallbacks::GetNavigationPreloadStateCallbacks(
    ScriptPromiseResolver* resolver)
    : m_resolver(resolver) {
  DCHECK(m_resolver);
}

GetNavigationPreloadStateCallbacks::~GetNavigationPreloadStateCallbacks() {}

void GetNavigationPreloadStateCallbacks::onSuccess(
    const WebNavigationPreloadState& state) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  NavigationPreloadState dict;
  dict.setEnabled(state.enabled);
  dict.setHeaderValue(state.headerValue);
  m_resolver->resolve(dict);
}

void GetNavigationPreloadStateCallbacks::onError(
    const WebServiceWorkerError& error) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
}

SetNavigationPreloadHeaderCallbacks::SetNavigationPreloadHeaderCallbacks(
    ScriptPromiseResolver* resolver)
    : m_resolver(resolver) {
  DCHECK(m_resolver);
}

SetNavigationPreloadHeaderCallbacks::~SetNavigationPreloadHeaderCallbacks() {}

void SetNavigationPreloadHeaderCallbacks::onSuccess() {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  m_resolver->resolve();
}

void SetNavigationPreloadHeaderCallbacks::onError(
    const WebServiceWorkerError& error) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
}

}  // namespace blink
