// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserverController_h
#define ResizeObserverController_h

#include "platform/heap/Handle.h"

namespace blink {

class ResizeObserver;

// ResizeObserverController keeps track of all ResizeObservers
// in a single Document.
//
// The observation API is used to integrate ResizeObserver
// and the event loop. It delivers notification in a loop.
// In each iteration, only notifications deeper than the
// shallowest notification from previous iteration are delivered.
class ResizeObserverController final
    : public GarbageCollected<ResizeObserverController> {
 public:
  static const size_t kDepthBottom = 4096;

  ResizeObserverController();

  void addObserver(ResizeObserver&);

  // observation API
  // Returns depth of shallowest observed node, kDepthLimit if none.
  size_t gatherObservations(size_t deeperThan);
  // Returns true if gatherObservations has skipped observations
  // because they were too shallow.
  bool skippedObservations();
  void deliverObservations();
  void clearObservations();
  void observerChanged() { m_observersChanged = true; }

  DECLARE_TRACE();

  // For testing only.
  const HeapHashSet<WeakMember<ResizeObserver>>& observers() {
    return m_observers;
  }

 private:
  // Active observers
  HeapHashSet<WeakMember<ResizeObserver>> m_observers;
  // True if any observers were changed since last notification.
  bool m_observersChanged;
};

}  // namespace blink

#endif
