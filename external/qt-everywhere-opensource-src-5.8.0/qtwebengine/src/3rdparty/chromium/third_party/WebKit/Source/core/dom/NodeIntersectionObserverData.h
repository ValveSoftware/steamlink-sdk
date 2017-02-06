// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NodeIntersectionObserverData_h
#define NodeIntersectionObserverData_h

#include "platform/heap/Handle.h"

namespace blink {

class Node;
class IntersectionObservation;
class IntersectionObserver;

class NodeIntersectionObserverData : public GarbageCollected<NodeIntersectionObserverData> {
public:
    NodeIntersectionObserverData();

    IntersectionObservation* getObservationFor(IntersectionObserver&);
    void addObservation(IntersectionObservation&);
    void removeObservation(IntersectionObserver&);
    void activateValidIntersectionObservers(Node&);
    void deactivateAllIntersectionObservers(Node&);

    DECLARE_TRACE();

    DECLARE_TRACE_WRAPPERS();

private:
    // IntersectionObservers for which the Node owning this data is root.
    HeapHashSet<WeakMember<IntersectionObserver>> m_intersectionObservers;
    // IntersectionObservations for which the Node owning this data is target.
    HeapHashMap<Member<IntersectionObserver>, Member<IntersectionObservation>> m_intersectionObservations;
};

} // namespace blink

#endif // NodeIntersectionObserverData_h
