// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

const AtomicString& PresentationReceiver::interfaceName() const
{
    return EventTargetNames::PresentationReceiver;
}

ExecutionContext* PresentationReceiver::getExecutionContext() const
{
    return frame() ? frame()->document() : nullptr;
}

ScriptPromise PresentationReceiver::getConnection(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "PresentationReceiver::getConnection() is not implemented yet."));
}

ScriptPromise PresentationReceiver::getConnections(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "PresentationReceiver::getConnections() is not implemented yet."));
}

DEFINE_TRACE(PresentationReceiver)
{
    EventTargetWithInlineData::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
