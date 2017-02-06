// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/InstallEvent.h"

#include "core/dom/ExceptionCode.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

InstallEvent* InstallEvent::create()
{
    return new InstallEvent();
}

InstallEvent* InstallEvent::create(const AtomicString& type, const ExtendableEventInit& eventInit)
{
    return new InstallEvent(type, eventInit);
}

InstallEvent* InstallEvent::create(const AtomicString& type, const ExtendableEventInit& eventInit, WaitUntilObserver* observer)
{
    return new InstallEvent(type, eventInit, observer);
}

InstallEvent::~InstallEvent()
{
}

void InstallEvent::registerForeignFetch(ExecutionContext* executionContext, const ForeignFetchOptions& options, ExceptionState& exceptionState)
{
    if (!isBeingDispatched()) {
        exceptionState.throwDOMException(InvalidStateError, "The event handler is already finished.");
        return;
    }

    if (!options.hasOrigins() || options.origins().isEmpty()) {
        exceptionState.throwTypeError("At least one origin is required");
        return;
    }
    const Vector<String>& originList = options.origins();

    // The origins parameter is either just a "*" to indicate all origins, or an
    // explicit list of origins as absolute URLs. Internally an empty list of
    // origins is used to represent the "*" case though.
    Vector<RefPtr<SecurityOrigin>> parsedOrigins;
    if (originList.size() != 1 || originList[0] != "*") {
        parsedOrigins.resize(originList.size());
        for (size_t i = 0; i < originList.size(); ++i) {
            parsedOrigins[i] = SecurityOrigin::createFromString(originList[i]);
            // Invalid URLs will result in a unique origin. And in general
            // unique origins should not be accepted.
            if (parsedOrigins[i]->isUnique()) {
                exceptionState.throwTypeError("Invalid origin URL: " + originList[i]);
                return;
            }
        }
    }

    ServiceWorkerGlobalScopeClient* client = ServiceWorkerGlobalScopeClient::from(executionContext);

    String scopePath = static_cast<KURL>(client->scope()).path();
    RefPtr<SecurityOrigin> origin = executionContext->getSecurityOrigin();

    if (!options.hasScopes() || options.scopes().isEmpty()) {
        exceptionState.throwTypeError("At least one scope is required");
        return;
    }
    const Vector<String>& subScopes = options.scopes();
    Vector<KURL> subScopeURLs(subScopes.size());
    for (size_t i = 0; i < subScopes.size(); ++i) {
        subScopeURLs[i] = executionContext->completeURL(subScopes[i]);
        if (!subScopeURLs[i].isValid()) {
            exceptionState.throwTypeError("Invalid subscope URL: " + subScopes[i]);
            return;
        }
        subScopeURLs[i].removeFragmentIdentifier();
        if (!origin->canRequest(subScopeURLs[i])) {
            exceptionState.throwTypeError("Subscope URL is not within scope: " + subScopes[i]);
            return;
        }
        String subScopePath = subScopeURLs[i].path();
        if (!subScopePath.startsWith(scopePath)) {
            exceptionState.throwTypeError("Subscope URL is not within scope: " + subScopes[i]);
            return;
        }
    }
    client->registerForeignFetchScopes(subScopeURLs, parsedOrigins);
}

const AtomicString& InstallEvent::interfaceName() const
{
    return EventNames::InstallEvent;
}

InstallEvent::InstallEvent()
{
}

InstallEvent::InstallEvent(const AtomicString& type, const ExtendableEventInit& initializer)
    : ExtendableEvent(type, initializer)
{
}

InstallEvent::InstallEvent(const AtomicString& type, const ExtendableEventInit& initializer, WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer)
{
}

} // namespace blink
