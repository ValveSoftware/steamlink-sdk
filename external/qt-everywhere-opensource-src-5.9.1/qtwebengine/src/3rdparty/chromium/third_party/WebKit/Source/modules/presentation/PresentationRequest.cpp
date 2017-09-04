// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/MixedContentChecker.h"
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
WebPresentationClient* presentationClient(ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  if (!document->frame())
    return nullptr;
  PresentationController* controller =
      PresentationController::from(*document->frame());
  return controller ? controller->client() : nullptr;
}

Settings* settings(ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  return document->settings();
}

ScriptPromise rejectWithMixedContentException(ScriptState* scriptState,
                                              const String& url) {
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(SecurityError,
                           "Presentation of an insecure document [" + url +
                               "] is prohibited from a secure context."));
}

ScriptPromise rejectWithSandBoxException(ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(SecurityError,
                                        "The document is sandboxed and lacks "
                                        "the 'allow-presentation' flag."));
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::create(
    ExecutionContext* executionContext,
    const String& url,
    ExceptionState& exceptionState) {
  KURL parsedUrl = KURL(executionContext->url(), url);
  if (!parsedUrl.isValid() || parsedUrl.protocolIsAbout()) {
    exceptionState.throwTypeError("'" + url +
                                  "' can't be resolved to a valid URL.");
    return nullptr;
  }

  PresentationRequest* request =
      new PresentationRequest(executionContext, parsedUrl);
  request->suspendIfNeeded();
  return request;
}

const AtomicString& PresentationRequest::interfaceName() const {
  return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

void PresentationRequest::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  if (eventType == EventTypeNames::connectionavailable)
    UseCounter::count(
        getExecutionContext(),
        UseCounter::PresentationRequestConnectionAvailableEventListener);
}

bool PresentationRequest::hasPendingActivity() const {
  if (!getExecutionContext() ||
      getExecutionContext()->activeDOMObjectsAreStopped())
    return false;

  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  return hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState) {
  Settings* contextSettings = settings(getExecutionContext());
  bool isUserGestureRequired =
      !contextSettings || contextSettings->presentationRequiresUserGesture();

  if (isUserGestureRequired && !UserGestureIndicator::utilizeUserGesture())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidAccessError,
            "PresentationRequest::start() requires user gesture."));

  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  // TODO(crbug.com/627655): Accept multiple URLs per PresentationRequest.
  WebVector<WebURL> presentationUrls(static_cast<size_t>(1U));
  presentationUrls[0] = m_url;
  client->startSession(presentationUrls,
                       new PresentationConnectionCallbacks(resolver, this));
  return resolver->promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* scriptState,
                                             const String& id) {
  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  // TODO(crbug.com/627655): Accept multiple URLs per PresentationRequest.
  WebVector<WebURL> presentationUrls(static_cast<size_t>(1U));
  presentationUrls[0] = m_url;
  client->joinSession(presentationUrls, id,
                      new PresentationConnectionCallbacks(resolver, this));
  return resolver->promise();
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState) {
  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  client->getAvailability(
      m_url, new PresentationAvailabilityCallbacks(resolver, m_url));
  return resolver->promise();
}

const KURL& PresentationRequest::url() const {
  return m_url;
}

DEFINE_TRACE(PresentationRequest) {
  EventTargetWithInlineData::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext,
                                         const KURL& url)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(executionContext),
      m_url(url) {}

}  // namespace blink
