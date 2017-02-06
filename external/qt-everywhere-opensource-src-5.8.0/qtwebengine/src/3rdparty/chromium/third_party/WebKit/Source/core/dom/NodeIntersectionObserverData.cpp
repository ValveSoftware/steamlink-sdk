// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/NodeIntersectionObserverData.h"

#include "core/dom/Document.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserver.h"
#include "core/dom/IntersectionObserverController.h"

namespace blink {

NodeIntersectionObserverData::NodeIntersectionObserverData() { }

IntersectionObservation* NodeIntersectionObserverData::getObservationFor(IntersectionObserver& observer)
{
    auto i = m_intersectionObservations.find(&observer);
    if (i == m_intersectionObservations.end())
        return nullptr;
    return i->value;
}

void NodeIntersectionObserverData::addObservation(IntersectionObservation& observation)
{
    m_intersectionObservations.add(&observation.observer(), &observation);
}

void NodeIntersectionObserverData::removeObservation(IntersectionObserver& observer)
{
    m_intersectionObservations.remove(&observer);
}

void NodeIntersectionObserverData::activateValidIntersectionObservers(Node& node)
{
    IntersectionObserverController& controller = node.document().ensureIntersectionObserverController();
    for (auto& observer : m_intersectionObservers)
        controller.addTrackedObserver(*observer);
}

void NodeIntersectionObserverData::deactivateAllIntersectionObservers(Node& node)
{
    node.document().ensureIntersectionObserverController().removeTrackedObserversForRoot(node);
}

DEFINE_TRACE(NodeIntersectionObserverData)
{
    visitor->trace(m_intersectionObservers);
    visitor->trace(m_intersectionObservations);
}

DEFINE_TRACE_WRAPPERS(NodeIntersectionObserverData)
{
    for (auto& entry : m_intersectionObservations) {
        visitor->traceWrappers(entry.key);
    }
}

} // namespace blink
