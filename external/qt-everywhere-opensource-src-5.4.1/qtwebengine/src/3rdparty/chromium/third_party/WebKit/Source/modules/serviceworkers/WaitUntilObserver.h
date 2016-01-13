// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WaitUntilObserver_h
#define WaitUntilObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExecutionContext;
class ScriptState;
class ScriptValue;

// Created for each InstallPhaseEvent instance.
class WaitUntilObserver FINAL :
    public ContextLifecycleObserver,
    public RefCounted<WaitUntilObserver> {
public:
    enum EventType {
        Activate,
        Install
    };

    static PassRefPtr<WaitUntilObserver> create(ExecutionContext*, EventType, int eventID);

    ~WaitUntilObserver();

    // Must be called before and after dispatching the event.
    void willDispatchEvent();
    void didDispatchEvent();

    // Observes the promise and delays calling the continuation until
    // the given promise is resolved or rejected.
    void waitUntil(ScriptState*, const ScriptValue&);

private:
    class ThenFunction;

    WaitUntilObserver(ExecutionContext*, EventType, int eventID);

    void reportError(const ScriptValue&);

    void incrementPendingActivity();
    void decrementPendingActivity();

    EventType m_type;
    int m_eventID;
    int m_pendingActivity;
    bool m_hasError;
};

} // namespace WebCore

#endif // WaitUntilObserver_h
