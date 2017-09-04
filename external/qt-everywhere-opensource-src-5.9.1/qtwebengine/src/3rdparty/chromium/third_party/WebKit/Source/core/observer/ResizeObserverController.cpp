// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserverController.h"

#include "core/observer/ResizeObserver.h"

namespace blink {

ResizeObserverController::ResizeObserverController()
    : m_observersChanged(false) {}

void ResizeObserverController::addObserver(ResizeObserver& observer) {
  m_observers.add(&observer);
}

size_t ResizeObserverController::gatherObservations(size_t deeperThan) {
  size_t shallowest = ResizeObserverController::kDepthBottom;
  if (!m_observersChanged)
    return shallowest;
  for (auto& observer : m_observers) {
    size_t depth = observer->gatherObservations(deeperThan);
    if (depth < shallowest)
      shallowest = depth;
  }
  return shallowest;
}

bool ResizeObserverController::skippedObservations() {
  for (auto& observer : m_observers) {
    if (observer->skippedObservations())
      return true;
  }
  return false;
}

void ResizeObserverController::deliverObservations() {
  m_observersChanged = false;
  // Copy is needed because m_observers might get modified during
  // deliverObservations.
  HeapVector<Member<ResizeObserver>> observers;
  copyToVector(m_observers, observers);

  for (auto& observer : observers) {
    if (observer) {
      observer->deliverObservations();
      m_observersChanged =
          m_observersChanged || observer->hasElementSizeChanged();
    }
  }
}

void ResizeObserverController::clearObservations() {
  for (auto& observer : m_observers)
    observer->clearObservations();
}

DEFINE_TRACE(ResizeObserverController) {
  visitor->trace(m_observers);
}

}  // namespace blink
