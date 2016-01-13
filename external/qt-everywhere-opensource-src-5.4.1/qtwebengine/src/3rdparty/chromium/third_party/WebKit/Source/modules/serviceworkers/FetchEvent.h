// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "modules/EventModules.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/RespondWithObserver.h"

namespace WebCore {

class ExecutionContext;
class Request;
class RespondWithObserver;

// A fetch event is dispatched by the client to a service worker's script
// context. RespondWithObserver can be used to notify the client about the
// service worker's response.
class FetchEvent FINAL : public Event {
public:
    static PassRefPtrWillBeRawPtr<FetchEvent> create();
    static PassRefPtrWillBeRawPtr<FetchEvent> create(PassRefPtr<RespondWithObserver>, PassRefPtr<Request>);
    virtual ~FetchEvent() { }

    Request* request() const;

    void respondWith(ScriptState*, const ScriptValue&);

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

protected:
    FetchEvent();
    FetchEvent(PassRefPtr<RespondWithObserver>, PassRefPtr<Request>);

private:
    RefPtr<RespondWithObserver> m_observer;
    RefPtr<Request> m_request;
};

} // namespace WebCore

#endif // FetchEvent_h
