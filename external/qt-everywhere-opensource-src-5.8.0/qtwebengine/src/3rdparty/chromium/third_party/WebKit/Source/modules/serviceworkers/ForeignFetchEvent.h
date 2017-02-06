// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForeignFetchEvent_h
#define ForeignFetchEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ForeignFetchEventInit.h"
#include "modules/serviceworkers/ForeignFetchRespondWithObserver.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class Request;

// A foreignfetch event is dispatched by the client to a service worker's script
// context. ForeignFetchRespondWithObserver can be used to notify the client
// about the service worker's response.
class MODULES_EXPORT ForeignFetchEvent final : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ForeignFetchEvent* create();
    static ForeignFetchEvent* create(ScriptState*, const AtomicString& type, const ForeignFetchEventInit&);
    static ForeignFetchEvent* create(ScriptState*, const AtomicString& type, const ForeignFetchEventInit&, ForeignFetchRespondWithObserver*, WaitUntilObserver*);

    Request* request() const;
    String origin() const;

    void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

protected:
    ForeignFetchEvent();
    ForeignFetchEvent(ScriptState*, const AtomicString& type, const ForeignFetchEventInit&, ForeignFetchRespondWithObserver*, WaitUntilObserver*);

private:
    Member<ForeignFetchRespondWithObserver> m_observer;
    Member<Request> m_request;
    String m_origin;
};

} // namespace blink

#endif // ForeignFetchEvent_h
