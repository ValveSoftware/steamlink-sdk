// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "core/workers/DedicatedWorkerMessagingProxyProvider.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"

namespace blink {

Worker::Worker(ExecutionContext* context) : InProcessWorkerBase(context) {}

Worker* Worker::create(ExecutionContext* context,
                       const String& url,
                       ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  Document* document = toDocument(context);
  UseCounter::count(context, UseCounter::WorkerStart);
  if (!document->page()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The context provided is invalid.");
    return nullptr;
  }
  Worker* worker = new Worker(context);
  if (worker->initialize(context, url, exceptionState))
    return worker;
  return nullptr;
}

Worker::~Worker() {
  DCHECK(isMainThread());
}

const AtomicString& Worker::interfaceName() const {
  return EventTargetNames::Worker;
}

InProcessWorkerMessagingProxy* Worker::createInProcessWorkerMessagingProxy(
    ExecutionContext* context) {
  Document* document = toDocument(context);
  DedicatedWorkerMessagingProxyProvider* proxyProvider =
      DedicatedWorkerMessagingProxyProvider::from(*document->page());
  DCHECK(proxyProvider);
  return proxyProvider->createWorkerMessagingProxy(this);
}

}  // namespace blink
