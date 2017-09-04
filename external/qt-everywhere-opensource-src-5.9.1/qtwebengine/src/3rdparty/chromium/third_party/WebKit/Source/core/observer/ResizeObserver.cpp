// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserver.h"

#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/observer/ResizeObservation.h"
#include "core/observer/ResizeObserverCallback.h"
#include "core/observer/ResizeObserverController.h"
#include "core/observer/ResizeObserverEntry.h"

namespace blink {

ResizeObserver* ResizeObserver::create(Document& document,
                                       ResizeObserverCallback* callback) {
  return new ResizeObserver(callback, document);
}

ResizeObserver::ResizeObserver(ResizeObserverCallback* callback,
                               Document& document)
    : m_callback(callback),
      m_skippedObservations(false),
      m_elementSizeChanged(false) {
  m_controller = &document.ensureResizeObserverController();
  m_controller->addObserver(*this);
}

void ResizeObserver::observe(Element* target) {
  auto& observerMap = target->ensureResizeObserverData();
  if (observerMap.contains(this))
    return;  // Already registered.

  auto observation = new ResizeObservation(target, this);
  m_observations.add(observation);
  observerMap.set(this, observation);

  if (FrameView* frameView = target->document().view())
    frameView->scheduleAnimation();
}

void ResizeObserver::unobserve(Element* target) {
  auto observerMap = target ? target->resizeObserverData() : nullptr;
  if (!observerMap)
    return;
  auto observation = observerMap->find(this);
  if (observation != observerMap->end()) {
    m_observations.remove((*observation).value);
    observerMap->remove(observation);
  }
}

void ResizeObserver::disconnect() {
  ObservationList observations;
  m_observations.swap(observations);

  for (auto& observation : observations) {
    Element* target = (*observation).target();
    if (target)
      target->ensureResizeObserverData().remove(this);
  }
  clearObservations();
}

size_t ResizeObserver::gatherObservations(size_t deeperThan) {
  DCHECK(m_activeObservations.isEmpty());

  size_t minObservedDepth = ResizeObserverController::kDepthBottom;
  if (!m_elementSizeChanged)
    return minObservedDepth;
  for (auto& observation : m_observations) {
    if (!observation->observationSizeOutOfSync())
      continue;
    auto depth = observation->targetDepth();
    if (depth > deeperThan) {
      m_activeObservations.append(*observation);
      minObservedDepth = std::min(minObservedDepth, depth);
    } else {
      m_skippedObservations = true;
    }
  }
  return minObservedDepth;
}

void ResizeObserver::deliverObservations() {
  // We can only clear this flag after all observations have been
  // broadcast.
  m_elementSizeChanged = m_skippedObservations;
  if (m_activeObservations.size() == 0)
    return;

  HeapVector<Member<ResizeObserverEntry>> entries;

  for (auto& observation : m_activeObservations) {
    LayoutPoint location = observation->computeTargetLocation();
    LayoutSize size = observation->computeTargetSize();
    observation->setObservationSize(size);
    auto entry = new ResizeObserverEntry(observation->target(),
                                         LayoutRect(location, size));
    entries.append(entry);
  }
  m_callback->handleEvent(entries, this);
  clearObservations();
}

void ResizeObserver::clearObservations() {
  m_activeObservations.clear();
  m_skippedObservations = false;
}

void ResizeObserver::elementSizeChanged() {
  m_elementSizeChanged = true;
  if (m_controller)
    m_controller->observerChanged();
}

DEFINE_TRACE(ResizeObserver) {
  visitor->trace(m_callback);
  visitor->trace(m_observations);
  visitor->trace(m_activeObservations);
  visitor->trace(m_controller);
}

}  // namespace blink
