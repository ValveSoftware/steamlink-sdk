// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationReceiver_h
#define PresentationReceiver_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace blink {

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation session.
class PresentationReceiver final
    : public EventTargetWithInlineData
    , DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(PresentationReceiver);
    DEFINE_WRAPPERTYPEINFO();
public:
    PresentationReceiver(LocalFrame*);
    ~PresentationReceiver() = default;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    ScriptPromise getConnection(ScriptState*);
    ScriptPromise getConnections(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable);

    DECLARE_VIRTUAL_TRACE();
};

} // namespace blink

#endif // PresentationReceiver_h
