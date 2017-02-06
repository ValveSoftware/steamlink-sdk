// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationRequest_h
#define PresentationRequest_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/weborigin/KURL.h"

namespace blink {

// Implements the PresentationRequest interface from the Presentation API from
// which websites can start or join presentation connections.
class PresentationRequest final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(PresentationRequest);
    DEFINE_WRAPPERTYPEINFO();
public:
    ~PresentationRequest() = default;

    static PresentationRequest* create(ExecutionContext*, const String& url, ExceptionState&);

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // ActiveScriptWrappable implementation.
    bool hasPendingActivity() const final;

    ScriptPromise start(ScriptState*);
    ScriptPromise reconnect(ScriptState*, const String& id);
    ScriptPromise getAvailability(ScriptState*);

    const KURL& url() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable);

    DECLARE_VIRTUAL_TRACE();

protected:
    // EventTarget implementation.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;

private:
    PresentationRequest(ExecutionContext*, const KURL&);

    KURL m_url;
};

} // namespace blink

#endif // PresentationRequest_h
