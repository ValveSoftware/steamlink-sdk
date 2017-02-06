// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WaitUntilObserver_h
#define WaitUntilObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/Timer.h"
#include "wtf/Forward.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;

// Created for each ExtendableEvent instance.
class MODULES_EXPORT WaitUntilObserver final : public GarbageCollectedFinalized<WaitUntilObserver>, public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(WaitUntilObserver);
public:
    enum EventType {
        Activate,
        Fetch,
        Install,
        Message,
        NotificationClick,
        NotificationClose,
        Push,
        Sync
    };

    static WaitUntilObserver* create(ExecutionContext*, EventType, int eventID);

    // Must be called before and after dispatching the event.
    void willDispatchEvent();
    void didDispatchEvent(bool errorOccurred);

    // Observes the promise and delays calling the continuation until
    // the given promise is resolved or rejected.
    void waitUntil(ScriptState*, ScriptPromise, ExceptionState&);

    // These methods can be called when the lifecycle of ExtendableEvent
    // observed by this WaitUntilObserver should be extended by other reason
    // than ExtendableEvent.waitUntil.
    void incrementPendingActivity();
    void decrementPendingActivity();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class InternalsServiceWorker;
    class ThenFunction;

    WaitUntilObserver(ExecutionContext*, EventType, int eventID);

    void reportError(const ScriptValue&);

    void consumeWindowInteraction(Timer<WaitUntilObserver>*);

    EventType m_type;
    int m_eventID;
    int m_pendingActivity;
    bool m_hasError;
    bool m_eventDispatched;
    Timer<WaitUntilObserver> m_consumeWindowInteractionTimer;
};

} // namespace blink

#endif // WaitUntilObserver_h
