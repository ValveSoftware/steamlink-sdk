/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/ContextLifecycleNotifier.h"

#include "core/dom/ActiveDOMObject.h"
#include "wtf/AutoReset.h"

namespace blink {

void ContextLifecycleNotifier::notifyResumingActiveDOMObjects() {
  AutoReset<IterationState> scope(&m_iterationState, AllowingNone);
  for (ContextLifecycleObserver* observer : m_observers) {
    if (observer->observerType() !=
        ContextLifecycleObserver::ActiveDOMObjectType)
      continue;
    ActiveDOMObject* activeDOMObject = static_cast<ActiveDOMObject*>(observer);
#if DCHECK_IS_ON()
    DCHECK_EQ(activeDOMObject->getExecutionContext(), context());
    DCHECK(activeDOMObject->suspendIfNeededCalled());
#endif
    activeDOMObject->resume();
  }
}

void ContextLifecycleNotifier::notifySuspendingActiveDOMObjects() {
  AutoReset<IterationState> scope(&m_iterationState, AllowingNone);
  for (ContextLifecycleObserver* observer : m_observers) {
    if (observer->observerType() !=
        ContextLifecycleObserver::ActiveDOMObjectType)
      continue;
    ActiveDOMObject* activeDOMObject = static_cast<ActiveDOMObject*>(observer);
#if DCHECK_IS_ON()
    DCHECK_EQ(activeDOMObject->getExecutionContext(), context());
    DCHECK(activeDOMObject->suspendIfNeededCalled());
#endif
    activeDOMObject->suspend();
  }
}

unsigned ContextLifecycleNotifier::activeDOMObjectCount() const {
  DCHECK(!isIteratingOverObservers());
  unsigned activeDOMObjects = 0;
  for (ContextLifecycleObserver* observer : m_observers) {
    if (observer->observerType() !=
        ContextLifecycleObserver::ActiveDOMObjectType)
      continue;
    activeDOMObjects++;
  }
  return activeDOMObjects;
}

#if DCHECK_IS_ON()
bool ContextLifecycleNotifier::contains(ActiveDOMObject* object) const {
  DCHECK(!isIteratingOverObservers());
  for (ContextLifecycleObserver* observer : m_observers) {
    if (observer->observerType() !=
        ContextLifecycleObserver::ActiveDOMObjectType)
      continue;
    ActiveDOMObject* activeDOMObject = static_cast<ActiveDOMObject*>(observer);
    if (activeDOMObject == object)
      return true;
  }
  return false;
}
#endif

}  // namespace blink
