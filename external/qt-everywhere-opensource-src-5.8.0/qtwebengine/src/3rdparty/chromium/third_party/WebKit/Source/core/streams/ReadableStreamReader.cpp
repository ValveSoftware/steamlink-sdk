// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamReader.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStream.h"

namespace blink {

ReadableStreamReader::ReadableStreamReader(ExecutionContext* executionContext, ReadableStream* stream)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , m_stream(stream)
    , m_closed(new ClosedPromise(executionContext, this, ClosedPromise::Closed))
{
    suspendIfNeeded();
    ASSERT(m_stream->isLockedTo(nullptr));
    m_stream->setReader(this);

    if (m_stream->stateInternal() == ReadableStream::Closed)
        m_closed->resolve(ToV8UndefinedGenerator());
    if (m_stream->stateInternal() == ReadableStream::Errored)
        m_closed->reject(m_stream->storedException());
}

ScriptPromise ReadableStreamReader::closed(ScriptState* scriptState)
{
    return m_closed->promise(scriptState->world());
}

bool ReadableStreamReader::isActive() const
{
    return m_stream->isLockedTo(this);
}

ScriptPromise ReadableStreamReader::cancel(ScriptState* scriptState)
{
    return cancel(scriptState, ScriptValue(scriptState, v8::Undefined(scriptState->isolate())));
}

ScriptPromise ReadableStreamReader::cancel(ScriptState* scriptState, ScriptValue reason)
{
    if (isActive())
        return m_stream->cancelInternal(scriptState, reason);

    return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "the reader is already released"));
}

ScriptPromise ReadableStreamReader::read(ScriptState* scriptState)
{
    if (!isActive())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "the reader is already released"));

    return m_stream->read(scriptState);
}

void ReadableStreamReader::releaseLock(ExceptionState& es)
{
    if (!isActive())
        return;
    if (m_stream->hasPendingReads()) {
        es.throwTypeError("The stream has pending read operations.");
        return;
    }

    releaseLock();
}

void ReadableStreamReader::releaseLock()
{
    if (!isActive())
        return;

    ASSERT(!m_stream->hasPendingReads());
    if (m_stream->stateInternal() != ReadableStream::Readable)
        m_closed->reset();
    // Note: It is generally a bad idea to store world-dependent values
    // (e.g. v8::Object) in a ScriptPromiseProperty, so we use DOMException
    // though the spec says the promise should be rejected with a TypeError.
    m_closed->reject(DOMException::create(AbortError, "the reader is already released"));

    // We call setReader(nullptr) after resolving / rejecting |m_closed|
    // because it affects hasPendingActivity.
    m_stream->setReader(nullptr);
    ASSERT(!isActive());
}

void ReadableStreamReader::close()
{
    ASSERT(isActive());
    m_closed->resolve(ToV8UndefinedGenerator());
}

void ReadableStreamReader::error()
{
    ASSERT(isActive());
    m_closed->reject(m_stream->storedException());
}

bool ReadableStreamReader::hasPendingActivity() const
{
    // We need to extend ReadableStreamReader's wrapper's life while it is
    // active in order to call resolve / reject on ScriptPromiseProperties.
    return isActive() && m_stream->stateInternal() == ReadableStream::Readable;
}

void ReadableStreamReader::stop()
{
    if (isActive()) {
        // Calling |error| will release the lock.
        m_stream->error(DOMException::create(AbortError, "The frame stops working."));
    }
    ActiveDOMObject::stop();
}

DEFINE_TRACE(ReadableStreamReader)
{
    visitor->trace(m_stream);
    visitor->trace(m_closed);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
