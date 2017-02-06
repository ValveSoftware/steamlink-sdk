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
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/push_messaging/WebPushClient.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

const int kMaxApplicationServerKeyLength = 255;

WebPushProvider* pushProvider()
{
    WebPushProvider* webPushProvider = Platform::current()->pushProvider();
    DCHECK(webPushProvider);
    return webPushProvider;
}

String bufferSourceToString(const ArrayBufferOrArrayBufferView& applicationServerKey, ExceptionState& exceptionState)
{
    // Check the validity of the sender info. It must be a 65 byte unencrypted key,
    // which has the byte 0x04 as the first byte as a marker.
    unsigned char* input;
    int length;
    if (applicationServerKey.isArrayBuffer()) {
        input = static_cast<unsigned char*>(
            applicationServerKey.getAsArrayBuffer()->data());
        length = applicationServerKey.getAsArrayBuffer()->byteLength();
    } else if (applicationServerKey.isArrayBufferView()) {
        input = static_cast<unsigned char*>(
            applicationServerKey.getAsArrayBufferView()->buffer()->data());
        length = applicationServerKey.getAsArrayBufferView()->buffer()->byteLength();
    } else {
        NOTREACHED();
        return String();
    }

    // If the key is valid, just treat it as a string of bytes and pass it to
    // the push service.
    if (length <= kMaxApplicationServerKeyLength)
        return WebString::fromLatin1(input, length);

    exceptionState.throwDOMException(InvalidAccessError, "The provided applicationServerKey is not valid.");
    return String();
}

} // namespace

PushManager::PushManager(ServiceWorkerRegistration* registration)
    : m_registration(registration)
{
    DCHECK(registration);
}

WebPushSubscriptionOptions PushManager::toWebPushSubscriptionOptions(const PushSubscriptionOptions& options, ExceptionState& exceptionState)
{
    WebPushSubscriptionOptions webOptions;
    webOptions.userVisibleOnly = options.userVisibleOnly();
    if (options.hasApplicationServerKey()) {
        webOptions.applicationServerKey = bufferSourceToString(options.applicationServerKey(),
            exceptionState);
    }
    return webOptions;
}

ScriptPromise PushManager::subscribe(ScriptState* scriptState, const PushSubscriptionOptions& options, ExceptionState& exceptionState)
{
    if (!m_registration->active())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Subscription failed - no active Service Worker"));

    const WebPushSubscriptionOptions& webOptions = toWebPushSubscriptionOptions(options, exceptionState);
    if (exceptionState.hadException())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // The document context is the only reasonable context from which to ask the user for permission
    // to use the Push API. The embedder should persist the permission so that later calls in
    // different contexts can succeed.
    if (scriptState->getExecutionContext()->isDocument()) {
        Document* document = toDocument(scriptState->getExecutionContext());
        if (!document->domWindow() || !document->frame())
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Document is detached from window."));
        PushController::clientFrom(document->frame()).subscribe(m_registration->webRegistration(), webOptions, new PushSubscriptionCallbacks(resolver, m_registration));
    } else {
        pushProvider()->subscribe(m_registration->webRegistration(), webOptions, new PushSubscriptionCallbacks(resolver, m_registration));
    }

    return promise;
}

ScriptPromise PushManager::getSubscription(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    pushProvider()->getSubscription(m_registration->webRegistration(), new PushSubscriptionCallbacks(resolver, m_registration));
    return promise;
}

ScriptPromise PushManager::permissionState(ScriptState* scriptState, const PushSubscriptionOptions& options, ExceptionState& exceptionState)
{
    if (scriptState->getExecutionContext()->isDocument()) {
        Document* document = toDocument(scriptState->getExecutionContext());
        if (!document->domWindow() || !document->frame())
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Document is detached from window."));
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    pushProvider()->getPermissionStatus(m_registration->webRegistration(), toWebPushSubscriptionOptions(options, exceptionState), new PushPermissionStatusCallbacks(resolver));
    return promise;
}

DEFINE_TRACE(PushManager)
{
    visitor->trace(m_registration);
}

} // namespace blink
