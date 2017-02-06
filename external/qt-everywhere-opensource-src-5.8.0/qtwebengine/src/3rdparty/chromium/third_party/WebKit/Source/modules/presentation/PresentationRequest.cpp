// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationError.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

// TODO(mlamouri): refactor in one common place.
WebPresentationClient* presentationClient(ExecutionContext* executionContext)
{
    DCHECK(executionContext);

    Document* document = toDocument(executionContext);
    if (!document->frame())
        return nullptr;
    PresentationController* controller = PresentationController::from(*document->frame());
    return controller ? controller->client() : nullptr;
}

Settings* settings(ExecutionContext* executionContext)
{
    DCHECK(executionContext);

    Document* document = toDocument(executionContext);
    return document->settings();
}

} // anonymous namespace

// static
PresentationRequest* PresentationRequest::create(ExecutionContext* executionContext, const String& url, ExceptionState& exceptionState)
{
    KURL parsedUrl = KURL(executionContext->url(), url);
    if (!parsedUrl.isValid() || parsedUrl.protocolIsAbout()) {
        exceptionState.throwTypeError("'" + url + "' can't be resolved to a valid URL.");
        return nullptr;
    }

    PresentationRequest* request = new PresentationRequest(executionContext, parsedUrl);
    request->suspendIfNeeded();
    return request;
}

const AtomicString& PresentationRequest::interfaceName() const
{
    return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void PresentationRequest::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
    if (eventType == EventTypeNames::connectionavailable)
        UseCounter::count(getExecutionContext(), UseCounter::PresentationRequestConnectionAvailableEventListener);
}

bool PresentationRequest::hasPendingActivity() const
{
    if (!getExecutionContext() || getExecutionContext()->activeDOMObjectsAreStopped())
        return false;

    // Prevents garbage collecting of this object when not hold by another
    // object but still has listeners registered.
    return hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    Settings* contextSettings = settings(getExecutionContext());
    bool isUserGestureRequired = !contextSettings || contextSettings->presentationRequiresUserGesture();

    if (isUserGestureRequired && !UserGestureIndicator::utilizeUserGesture()) {
        resolver->reject(DOMException::create(InvalidAccessError, "PresentationRequest::start() requires user gesture."));
        return promise;
    }

    if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation)) {
        resolver->reject(DOMException::create(SecurityError, "The document is sandboxed and lacks the 'allow-presentation' flag."));
        return promise;
    }

    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The PresentationRequest is no longer associated to a frame."));
        return promise;
    }
    client->startSession(m_url.getString(), new PresentationConnectionCallbacks(resolver, this));
    return promise;
}

ScriptPromise PresentationRequest::reconnect(ScriptState* scriptState, const String& id)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation)) {
        resolver->reject(DOMException::create(SecurityError, "The document is sandboxed and lacks the 'allow-presentation' flag."));
        return promise;
    }

    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The PresentationRequest is no longer associated to a frame."));
        return promise;
    }
    client->joinSession(m_url.getString(), id, new PresentationConnectionCallbacks(resolver, this));
    return promise;
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation)) {
        resolver->reject(DOMException::create(SecurityError, "The document is sandboxed and lacks the 'allow-presentation' flag."));
        return promise;
    }

    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The object is no longer associated to a frame."));
        return promise;
    }
    client->getAvailability(m_url.getString(), new PresentationAvailabilityCallbacks(resolver, m_url));
    return promise;
}

const KURL& PresentationRequest::url() const
{
    return m_url;
}

DEFINE_TRACE(PresentationRequest)
{
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext, const KURL& url)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , m_url(url)
{
}

} // namespace blink
