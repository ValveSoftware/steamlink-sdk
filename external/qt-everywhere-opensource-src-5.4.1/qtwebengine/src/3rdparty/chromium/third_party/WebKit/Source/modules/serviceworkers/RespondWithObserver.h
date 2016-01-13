// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RespondWithObserver_h
#define RespondWithObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExecutionContext;
class Response;
class ScriptState;
class ScriptValue;

// This class observes the service worker's handling of a FetchEvent and
// notifies the client.
class RespondWithObserver FINAL : public ContextLifecycleObserver, public RefCounted<RespondWithObserver> {
public:
    static PassRefPtr<RespondWithObserver> create(ExecutionContext*, int eventID);
    ~RespondWithObserver();

    virtual void contextDestroyed() OVERRIDE;

    void didDispatchEvent();

    // Observes the promise and delays calling didHandleFetchEvent() until the
    // given promise is resolved or rejected.
    void respondWith(ScriptState*, const ScriptValue&);

    void responseWasRejected();
    void responseWasFulfilled(const ScriptValue&);

private:
    class ThenFunction;

    RespondWithObserver(ExecutionContext*, int eventID);

    // Sends a response back to the client. The null response means to fallback
    // to native.
    void sendResponse(PassRefPtr<Response>);

    int m_eventID;

    enum State { Initial, Pending, Done };
    State m_state;
};

} // namespace WebCore

#endif // RespondWithObserver_h
