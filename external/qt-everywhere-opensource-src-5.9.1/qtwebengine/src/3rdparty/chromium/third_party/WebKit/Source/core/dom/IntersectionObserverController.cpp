// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverController.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

IntersectionObserverController* IntersectionObserverController::create(
    Document* document) {
  IntersectionObserverController* result =
      new IntersectionObserverController(document);
  result->suspendIfNeeded();
  return result;
}

IntersectionObserverController::IntersectionObserverController(
    Document* document)
    : ActiveDOMObject(document),
      m_weakPtrFactory(this),
      m_callbackFiredWhileSuspended(false) {}

IntersectionObserverController::~IntersectionObserverController() {}

void IntersectionObserverController::scheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  m_pendingIntersectionObservers.add(&observer);
  if (!m_weakPtrFactory.hasWeakPtrs()) {
    // TODO(ojan): These tasks decide whether to throttle a subframe, so they
    // need to be unthrottled, but we should throttle all the other tasks
    // (e.g. ones coming from the web page).
    TaskRunnerHelper::get(TaskType::Unthrottled, getExecutionContext())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&IntersectionObserverController::
                                 deliverIntersectionObservations,
                             m_weakPtrFactory.createWeakPtr()));
  }
}

void IntersectionObserverController::resume() {
  // If the callback fired while DOM objects were suspended, notifications might
  // be late, so deliver them right away (rather than waiting to fire again).
  if (m_callbackFiredWhileSuspended) {
    m_callbackFiredWhileSuspended = false;
    deliverIntersectionObservations();
  }
}

void IntersectionObserverController::deliverIntersectionObservations() {
  ExecutionContext* context = getExecutionContext();
  if (!context) {
    m_pendingIntersectionObservers.clear();
    return;
  }
  if (context->activeDOMObjectsAreSuspended()) {
    m_callbackFiredWhileSuspended = true;
    return;
  }
  HeapHashSet<Member<IntersectionObserver>> observers;
  m_pendingIntersectionObservers.swap(observers);
  for (auto& observer : observers)
    observer->deliver();
}

void IntersectionObserverController::computeTrackedIntersectionObservations() {
  TRACE_EVENT0(
      "blink",
      "IntersectionObserverController::computeTrackedIntersectionObservations");
  for (auto& observer : m_trackedIntersectionObservers) {
    observer->computeIntersectionObservations();
    if (observer->hasEntries())
      scheduleIntersectionObserverForDelivery(*observer);
  }
}

void IntersectionObserverController::addTrackedObserver(
    IntersectionObserver& observer) {
  m_trackedIntersectionObservers.add(&observer);
}

void IntersectionObserverController::removeTrackedObserversForRoot(
    const Node& root) {
  HeapVector<Member<IntersectionObserver>> toRemove;
  for (auto& observer : m_trackedIntersectionObservers) {
    if (observer->rootNode() == &root)
      toRemove.append(observer);
  }
  m_trackedIntersectionObservers.removeAll(toRemove);
}

DEFINE_TRACE(IntersectionObserverController) {
  visitor->trace(m_trackedIntersectionObservers);
  visitor->trace(m_pendingIntersectionObservers);
  ActiveDOMObject::trace(visitor);
}

}  // namespace blink
