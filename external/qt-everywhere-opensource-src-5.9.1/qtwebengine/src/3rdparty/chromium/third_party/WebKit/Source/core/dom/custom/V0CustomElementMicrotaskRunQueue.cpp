// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/V0CustomElementMicrotaskRunQueue.h"

#include "bindings/core/v8/Microtask.h"
#include "core/dom/custom/V0CustomElementAsyncImportMicrotaskQueue.h"
#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"
#include "core/html/imports/HTMLImportLoader.h"

namespace blink {

V0CustomElementMicrotaskRunQueue::V0CustomElementMicrotaskRunQueue()
    : m_syncQueue(V0CustomElementSyncMicrotaskQueue::create()),
      m_asyncQueue(V0CustomElementAsyncImportMicrotaskQueue::create()),
      m_dispatchIsPending(false) {}

void V0CustomElementMicrotaskRunQueue::enqueue(
    HTMLImportLoader* parentLoader,
    V0CustomElementMicrotaskStep* step,
    bool importIsSync) {
  if (importIsSync) {
    if (parentLoader)
      parentLoader->microtaskQueue()->enqueue(step);
    else
      m_syncQueue->enqueue(step);
  } else {
    m_asyncQueue->enqueue(step);
  }

  requestDispatchIfNeeded();
}

void V0CustomElementMicrotaskRunQueue::requestDispatchIfNeeded() {
  if (m_dispatchIsPending || isEmpty())
    return;
  Microtask::enqueueMicrotask(WTF::bind(
      &V0CustomElementMicrotaskRunQueue::dispatch, wrapWeakPersistent(this)));
  m_dispatchIsPending = true;
}

DEFINE_TRACE(V0CustomElementMicrotaskRunQueue) {
  visitor->trace(m_syncQueue);
  visitor->trace(m_asyncQueue);
}

void V0CustomElementMicrotaskRunQueue::dispatch() {
  m_dispatchIsPending = false;
  m_syncQueue->dispatch();
  if (m_syncQueue->isEmpty())
    m_asyncQueue->dispatch();
}

bool V0CustomElementMicrotaskRunQueue::isEmpty() const {
  return m_syncQueue->isEmpty() && m_asyncQueue->isEmpty();
}

}  // namespace blink
