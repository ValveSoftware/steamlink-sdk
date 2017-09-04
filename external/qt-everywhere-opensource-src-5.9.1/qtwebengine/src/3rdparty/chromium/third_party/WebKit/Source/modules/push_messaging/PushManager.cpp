// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/push_messaging/PushController.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushPermissionStatusCallbacks.h"
#include "modules/push_messaging/PushSubscription.h"
#include "modules/push_messaging/PushSubscriptionCallbacks.h"
#include "modules/push_messaging/PushSubscriptionOptions.h"
#include "modules/push_messaging/PushSubscriptionOptionsInit.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/push_messaging/WebPushClient.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

WebPushProvider* pushProvider() {
  WebPushProvider* webPushProvider = Platform::current()->pushProvider();
  DCHECK(webPushProvider);
  return webPushProvider;
}

}  // namespace

PushManager::PushManager(ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
}

ScriptPromise PushManager::subscribe(ScriptState* scriptState,
                                     const PushSubscriptionOptionsInit& options,
                                     ExceptionState& exceptionState) {
  if (!m_registration->active())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(AbortError,
                             "Subscription failed - no active Service Worker"));

  const WebPushSubscriptionOptions& webOptions =
      PushSubscriptionOptions::toWeb(options, exceptionState);
  if (exceptionState.hadException())
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // The document context is the only reasonable context from which to ask the
  // user for permission to use the Push API. The embedder should persist the
  // permission so that later calls in different contexts can succeed.
  if (scriptState->getExecutionContext()->isDocument()) {
    Document* document = toDocument(scriptState->getExecutionContext());
    if (!document->domWindow() || !document->frame())
      return ScriptPromise::rejectWithDOMException(
          scriptState,
          DOMException::create(InvalidStateError,
                               "Document is detached from window."));
    PushController::clientFrom(document->frame())
        .subscribe(m_registration->webRegistration(), webOptions,
                   new PushSubscriptionCallbacks(resolver, m_registration));
  } else {
    pushProvider()->subscribe(
        m_registration->webRegistration(), webOptions,
        new PushSubscriptionCallbacks(resolver, m_registration));
  }

  return promise;
}

ScriptPromise PushManager::getSubscription(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  pushProvider()->getSubscription(
      m_registration->webRegistration(),
      new PushSubscriptionCallbacks(resolver, m_registration));
  return promise;
}

ScriptPromise PushManager::permissionState(
    ScriptState* scriptState,
    const PushSubscriptionOptionsInit& options,
    ExceptionState& exceptionState) {
  if (scriptState->getExecutionContext()->isDocument()) {
    Document* document = toDocument(scriptState->getExecutionContext());
    if (!document->domWindow() || !document->frame())
      return ScriptPromise::rejectWithDOMException(
          scriptState,
          DOMException::create(InvalidStateError,
                               "Document is detached from window."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  pushProvider()->getPermissionStatus(
      m_registration->webRegistration(),
      PushSubscriptionOptions::toWeb(options, exceptionState),
      new PushPermissionStatusCallbacks(resolver));
  return promise;
}

DEFINE_TRACE(PushManager) {
  visitor->trace(m_registration);
}

}  // namespace blink
