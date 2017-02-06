// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PlatformEventDispatcher.h"

#include "core/frame/PlatformEventController.h"
#include "wtf/TemporaryChange.h"

namespace blink {

PlatformEventDispatcher::PlatformEventDispatcher()
    : m_isDispatching(false)
    , m_isListening(false)
{
}

void PlatformEventDispatcher::addController(PlatformEventController* controller)
{
    ASSERT(controller);
    // TODO: If we can avoid to register a same controller twice, we can change
    // this 'if' to ASSERT.
    if (m_controllers.contains(controller))
        return;

    m_controllers.add(controller);

    if (!m_isListening) {
        startListening();
        m_isListening = true;
    }
}

void PlatformEventDispatcher::removeController(PlatformEventController* controller)
{
    ASSERT(m_controllers.contains(controller));

    m_controllers.remove(controller);
    if (!m_isDispatching && m_controllers.isEmpty()) {
        stopListening();
        m_isListening = false;
    }
}

void PlatformEventDispatcher::notifyControllers()
{
    if (m_controllers.isEmpty())
        return;

    {
        TemporaryChange<bool> changeIsDispatching(m_isDispatching, true);
        // HashSet m_controllers can be updated during an iteration, and it stops the iteration.
        // Thus we store it into a Vector to access all elements.
        HeapVector<Member<PlatformEventController>> snapshotVector;
        copyToVector(m_controllers, snapshotVector);
        for (PlatformEventController* controller : snapshotVector) {
            if (m_controllers.contains(controller))
                controller->didUpdateData();
        }
    }

    if (m_controllers.isEmpty()) {
        stopListening();
        m_isListening = false;
    }
}

DEFINE_TRACE(PlatformEventDispatcher)
{
    visitor->trace(m_controllers);
}

} // namespace blink
