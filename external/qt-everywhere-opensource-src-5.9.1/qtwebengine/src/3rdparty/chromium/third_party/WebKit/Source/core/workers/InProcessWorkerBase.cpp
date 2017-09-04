// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/InProcessWorkerBase.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/events/MessageEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include <memory>

namespace blink {

InProcessWorkerBase::InProcessWorkerBase(ExecutionContext* context)
    : AbstractWorker(context),
      ActiveScriptWrappable(this),
      m_contextProxy(nullptr) {}

InProcessWorkerBase::~InProcessWorkerBase() {
  DCHECK(isMainThread());
  if (!m_contextProxy)
    return;
  m_contextProxy->parentObjectDestroyed();
}

void InProcessWorkerBase::postMessage(ExecutionContext* context,
                                      PassRefPtr<SerializedScriptValue> message,
                                      const MessagePortArray& ports,
                                      ExceptionState& exceptionState) {
  DCHECK(m_contextProxy);
  // Disentangle the port in preparation for sending it to the remote context.
  std::unique_ptr<MessagePortChannelArray> channels =
      MessagePort::disentanglePorts(context, ports, exceptionState);
  if (exceptionState.hadException())
    return;
  m_contextProxy->postMessageToWorkerGlobalScope(std::move(message),
                                                 std::move(channels));
}

bool InProcessWorkerBase::initialize(ExecutionContext* context,
                                     const String& url,
                                     ExceptionState& exceptionState) {
  suspendIfNeeded();

  // TODO(mkwst): Revisit the context as
  // https://drafts.css-houdini.org/worklets/ evolves.
  KURL scriptURL =
      resolveURL(url, exceptionState, WebURLRequest::RequestContextScript);
  if (scriptURL.isEmpty())
    return false;

  m_scriptLoader = WorkerScriptLoader::create();
  m_scriptLoader->loadAsynchronously(
      *context, scriptURL, DenyCrossOriginRequests,
      context->securityContext().addressSpace(),
      WTF::bind(&InProcessWorkerBase::onResponse, wrapPersistent(this)),
      WTF::bind(&InProcessWorkerBase::onFinished, wrapPersistent(this)));

  m_contextProxy = createInProcessWorkerMessagingProxy(context);

  return true;
}

void InProcessWorkerBase::terminate() {
  if (m_contextProxy)
    m_contextProxy->terminateGlobalScope();
}

void InProcessWorkerBase::contextDestroyed() {
  if (m_scriptLoader)
    m_scriptLoader->cancel();
  terminate();
}

bool InProcessWorkerBase::hasPendingActivity() const {
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return (m_contextProxy && m_contextProxy->hasPendingActivity()) ||
         m_scriptLoader;
}

void InProcessWorkerBase::onResponse() {
  InspectorInstrumentation::didReceiveScriptResponse(
      getExecutionContext(), m_scriptLoader->identifier());
}

void InProcessWorkerBase::onFinished() {
  if (m_scriptLoader->canceled()) {
    // Do nothing.
  } else if (m_scriptLoader->failed()) {
    dispatchEvent(Event::createCancelable(EventTypeNames::error));
  } else {
    m_contextProxy->startWorkerGlobalScope(
        m_scriptLoader->url(), getExecutionContext()->userAgent(),
        m_scriptLoader->script(),
        m_scriptLoader->releaseContentSecurityPolicy(),
        m_scriptLoader->referrerPolicy());
    InspectorInstrumentation::scriptImported(getExecutionContext(),
                                             m_scriptLoader->identifier(),
                                             m_scriptLoader->script());
  }
  m_scriptLoader = nullptr;
}

DEFINE_TRACE(InProcessWorkerBase) {
  AbstractWorker::trace(visitor);
}

}  // namespace blink
