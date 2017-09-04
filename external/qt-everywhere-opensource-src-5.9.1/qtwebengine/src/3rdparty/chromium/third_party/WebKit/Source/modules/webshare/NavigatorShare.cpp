// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webshare/NavigatorShare.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/webshare/ShareData.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

class NavigatorShare::ShareClientImpl final
    : public GarbageCollected<ShareClientImpl> {
 public:
  ShareClientImpl(NavigatorShare*, ScriptPromiseResolver*);

  void callback(const String& error);

  void onConnectionError();

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_navigator);
    visitor->trace(m_resolver);
  }

 private:
  WeakMember<NavigatorShare> m_navigator;
  Member<ScriptPromiseResolver> m_resolver;
};

NavigatorShare::ShareClientImpl::ShareClientImpl(
    NavigatorShare* navigator_share,
    ScriptPromiseResolver* resolver)
    : m_navigator(navigator_share), m_resolver(resolver) {}

void NavigatorShare::ShareClientImpl::callback(const String& error) {
  if (m_navigator)
    m_navigator->m_clients.remove(this);

  if (error.isNull()) {
    m_resolver->resolve();
  } else {
    // TODO(mgiuca): Work out which error type to use.
    m_resolver->reject(DOMException::create(AbortError, error));
  }
}

void NavigatorShare::ShareClientImpl::onConnectionError() {
  m_resolver->reject(
      DOMException::create(SecurityError, "WebShare is disabled."));
}

NavigatorShare::~NavigatorShare() = default;

NavigatorShare& NavigatorShare::from(Navigator& navigator) {
  NavigatorShare* supplement = static_cast<NavigatorShare*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorShare();
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

DEFINE_TRACE(NavigatorShare) {
  visitor->trace(m_clients);
  Supplement<Navigator>::trace(visitor);
}

NavigatorShare::NavigatorShare() {}

const char* NavigatorShare::supplementName() {
  return "NavigatorShare";
}

ScriptPromise NavigatorShare::share(ScriptState* scriptState,
                                    const ShareData& shareData) {
  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
    DOMException* error = DOMException::create(SecurityError, errorMessage);
    return ScriptPromise::rejectWithDOMException(scriptState, error);
  }
  if (!UserGestureIndicator::utilizeUserGesture()) {
    DOMException* error = DOMException::create(
        SecurityError,
        "Must be handling a user gesture to perform a share request.");
    return ScriptPromise::rejectWithDOMException(scriptState, error);
  }

  Document* doc = toDocument(scriptState->getExecutionContext());
  DCHECK(doc);
  if (!m_service) {
    LocalFrame* frame = doc->frame();
    DCHECK(frame);
    frame->interfaceProvider()->getInterface(mojo::GetProxy(&m_service));
    m_service.set_connection_error_handler(convertToBaseCallback(WTF::bind(
        &NavigatorShare::onConnectionError, wrapWeakPersistent(this))));
    DCHECK(m_service);
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ShareClientImpl* client = new ShareClientImpl(this, resolver);
  m_clients.add(client);
  ScriptPromise promise = resolver->promise();

  m_service->Share(shareData.hasTitle() ? shareData.title() : emptyString(),
                   shareData.hasText() ? shareData.text() : emptyString(),
                   doc->completeURL(shareData.url()),
                   convertToBaseCallback(WTF::bind(&ShareClientImpl::callback,
                                                   wrapPersistent(client))));

  return promise;
}

ScriptPromise NavigatorShare::share(ScriptState* scriptState,
                                    Navigator& navigator,
                                    const ShareData& shareData) {
  return from(navigator).share(scriptState, shareData);
}

void NavigatorShare::onConnectionError() {
  if (!Platform::current()) {
    // TODO(rockot): Clean this up once renderer shutdown sequence is fixed.
    return;
  }

  for (auto& client : m_clients) {
    client->onConnectionError();
  }
  m_clients.clear();
  m_service.reset();
}

}  // namespace blink
