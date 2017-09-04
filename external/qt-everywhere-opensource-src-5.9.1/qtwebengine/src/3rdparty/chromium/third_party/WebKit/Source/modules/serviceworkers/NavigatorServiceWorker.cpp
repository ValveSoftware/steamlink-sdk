// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

namespace blink {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator)
    : ContextLifecycleObserver(navigator.frame() ? navigator.frame()->document()
                                                 : nullptr) {}

NavigatorServiceWorker* NavigatorServiceWorker::from(Document& document) {
  if (!document.frame() || !document.frame()->domWindow())
    return nullptr;
  Navigator& navigator = *document.frame()->domWindow()->navigator();
  return &from(navigator);
}

NavigatorServiceWorker& NavigatorServiceWorker::from(Navigator& navigator) {
  NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
  if (!supplement) {
    supplement = new NavigatorServiceWorker(navigator);
    provideTo(navigator, supplementName(), supplement);
    if (navigator.frame() &&
        navigator.frame()
            ->securityContext()
            ->getSecurityOrigin()
            ->canAccessServiceWorkers()) {
      // Initialize ServiceWorkerContainer too.
      supplement->serviceWorker(navigator.frame(), ASSERT_NO_EXCEPTION);
    }
  }
  return *supplement;
}

NavigatorServiceWorker* NavigatorServiceWorker::toNavigatorServiceWorker(
    Navigator& navigator) {
  return static_cast<NavigatorServiceWorker*>(
      Supplement<Navigator>::from(navigator, supplementName()));
}

const char* NavigatorServiceWorker::supplementName() {
  return "NavigatorServiceWorker";
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ExecutionContext* executionContext,
    Navigator& navigator,
    ExceptionState& exceptionState) {
  DCHECK(!navigator.frame() ||
         executionContext->getSecurityOrigin()->canAccessCheckSuborigins(
             navigator.frame()->securityContext()->getSecurityOrigin()));
  return NavigatorServiceWorker::from(navigator).serviceWorker(
      navigator.frame(), exceptionState);
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    LocalFrame* frame,
    ExceptionState& exceptionState) {
  if (frame &&
      !frame->securityContext()
           ->getSecurityOrigin()
           ->canAccessServiceWorkers()) {
    if (frame->securityContext()->isSandboxed(SandboxOrigin))
      exceptionState.throwSecurityError(
          "Service worker is disabled because the context is sandboxed and "
          "lacks the 'allow-same-origin' flag.");
    else if (frame->securityContext()->getSecurityOrigin()->hasSuborigin())
      exceptionState.throwSecurityError(
          "Service worker is disabled because the context is in a suborigin.");
    else
      exceptionState.throwSecurityError(
          "Access to service workers is denied in this document origin.");
    return nullptr;
  }
  if (!m_serviceWorker && frame) {
    DCHECK(frame->domWindow());
    m_serviceWorker = ServiceWorkerContainer::create(
        frame->domWindow()->getExecutionContext());
  }
  return m_serviceWorker.get();
}

void NavigatorServiceWorker::contextDestroyed() {
  m_serviceWorker = nullptr;
}

DEFINE_TRACE(NavigatorServiceWorker) {
  visitor->trace(m_serviceWorker);
  Supplement<Navigator>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
