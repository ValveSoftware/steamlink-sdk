// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObservation_h
#define ResizeObservation_h

#include "core/observer/ResizeObserverEntry.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class ResizeObserver;

// ResizeObservation represents an element that is being observed.
class CORE_EXPORT ResizeObservation final
    : public GarbageCollected<ResizeObservation> {
 public:
  ResizeObservation(Element* target, ResizeObserver*);

  Element* target() const { return m_target; }
  size_t targetDepth();
  // True if observationSize differs from target's current size.
  bool observationSizeOutOfSync();
  void setObservationSize(const LayoutSize&);
  void elementSizeChanged();

  LayoutSize computeTargetSize() const;
  LayoutPoint computeTargetLocation() const;

  DECLARE_TRACE();

 private:
  WeakMember<Element> m_target;
  Member<ResizeObserver> m_observer;
  // Target size sent in last observation notification.
  LayoutSize m_observationSize;
  bool m_elementSizeChanged;
};

}  // namespace blink

#endif  // ResizeObservation_h
