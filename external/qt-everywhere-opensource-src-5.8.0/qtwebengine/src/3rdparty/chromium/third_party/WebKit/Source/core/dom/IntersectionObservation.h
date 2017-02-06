// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObservation_h
#define IntersectionObservation_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IntersectionObserver;

class IntersectionObservation final : public GarbageCollected<IntersectionObservation> {
public:
    IntersectionObservation(IntersectionObserver&, Element&, bool shouldReportRootBounds);

    struct IntersectionGeometry {
        LayoutRect targetRect;
        LayoutRect intersectionRect;
        LayoutRect rootRect;
        bool doesIntersect;

        IntersectionGeometry() : doesIntersect(false) {}
    };

    IntersectionObserver& observer() const { return *m_observer; }
    Element* target() const { return m_target; }
    unsigned lastThresholdIndex() const { return m_lastThresholdIndex; }
    void setLastThresholdIndex(unsigned index) { m_lastThresholdIndex = index; }
    bool shouldReportRootBounds() const { return m_shouldReportRootBounds; }
    void computeIntersectionObservations(DOMHighResTimeStamp);
    void disconnect();
    void clearRootAndRemoveFromTarget();

    DECLARE_TRACE();

private:
    void applyRootMargin(LayoutRect&) const;
    void initializeGeometry(IntersectionGeometry&) const;
    void initializeTargetRect(LayoutRect&) const;
    void initializeRootRect(LayoutRect&) const;
    void clipToRoot(IntersectionGeometry&) const;
    void mapTargetRectToTargetFrameCoordinates(LayoutRect&) const;
    void mapRootRectToRootFrameCoordinates(LayoutRect&) const;
    void mapRootRectToTargetFrameCoordinates(LayoutRect&) const;
    bool computeGeometry(IntersectionGeometry&) const;

    Member<IntersectionObserver> m_observer;

    WeakMember<Element> m_target;

    unsigned m_shouldReportRootBounds : 1;
    unsigned m_lastThresholdIndex : 30;
};

} // namespace blink

#endif // IntersectionObservation_h
