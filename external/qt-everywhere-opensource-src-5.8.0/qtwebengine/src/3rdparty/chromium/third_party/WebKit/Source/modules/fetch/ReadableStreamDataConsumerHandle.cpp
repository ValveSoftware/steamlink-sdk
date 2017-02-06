// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/ReadableStreamDataConsumerHandle.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/DOMTypedArray.h"
#include "core/streams/ReadableStreamOperations.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/RefCounted.h"
#include <algorithm>
#include <string.h>
#include <v8.h>

namespace blink {

namespace {

bool isTerminating(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    if (!executionContext)
        return true;
    if (!executionContext->isWorkerGlobalScope())
        return false;
    return toWorkerGlobalScope(executionContext)->scriptController()->isExecutionTerminating();
}

} // namespace

using Result = WebDataConsumerHandle::Result;
using Flags = WebDataConsumerHandle::Flags;

// This context is not yet thread-safe.
class ReadableStreamDataConsumerHandle::ReadingContext final : public RefCounted<ReadingContext> {
    WTF_MAKE_NONCOPYABLE(ReadingContext);
public:
    class OnFulfilled final : public ScriptFunction {
    public:
        static v8::Local<v8::Function> createFunction(ScriptState* scriptState, PassRefPtr<ReadingContext> context)
        {
            return (new OnFulfilled(scriptState, context))->bindToV8Function();
        }

        ScriptValue call(ScriptValue v) override
        {
            RefPtr<ReadingContext> readingContext(m_readingContext);
            if (!readingContext)
                return v;
            bool done;
            v8::Local<v8::Value> item = v.v8Value();
            ASSERT(item->IsObject());
            if (isTerminating(v.getScriptState()))
                return ScriptValue();
            v8::MaybeLocal<v8::Value> maybeValue = v8UnpackIteratorResult(v.getScriptState(), item.As<v8::Object>(), &done);
            if (isTerminating(v.getScriptState()))
                return ScriptValue();
            v8::Local<v8::Value> value = v8CallOrCrash(maybeValue);
            if (done) {
                readingContext->onReadDone();
                return v;
            }
            if (!value->IsUint8Array()) {
                readingContext->onRejected();
                return ScriptValue();
            }
            readingContext->onRead(V8Uint8Array::toImpl(value.As<v8::Object>()));
            return v;
        }

    private:
        OnFulfilled(ScriptState* scriptState, PassRefPtr<ReadingContext> context)
            : ScriptFunction(scriptState), m_readingContext(context) {}

        RefPtr<ReadingContext> m_readingContext;
    };

    class OnRejected final : public ScriptFunction {
    public:
        static v8::Local<v8::Function> createFunction(ScriptState* scriptState, PassRefPtr<ReadingContext> context)
        {
            return (new OnRejected(scriptState, context))->bindToV8Function();
        }

        ScriptValue call(ScriptValue v) override
        {
            RefPtr<ReadingContext> readingContext(m_readingContext);
            if (!readingContext)
                return v;
            readingContext->onRejected();
            return v;
        }

    private:
        OnRejected(ScriptState* scriptState, PassRefPtr<ReadingContext> context)
            : ScriptFunction(scriptState), m_readingContext(context) {}

        RefPtr<ReadingContext> m_readingContext;
    };

    class ReaderImpl final : public FetchDataConsumerHandle::Reader {
    public:
        ReaderImpl(PassRefPtr<ReadingContext> context, Client* client)
            : m_readingContext(context)
        {
            m_readingContext->attachReader(client);
        }
        ~ReaderImpl() override
        {
            m_readingContext->detachReader();
        }

        Result beginRead(const void** buffer, Flags, size_t* available) override
        {
            return m_readingContext->beginRead(buffer, available);
        }

        Result endRead(size_t readSize) override
        {
            return m_readingContext->endRead(readSize);
        }

    private:
        RefPtr<ReadingContext> m_readingContext;
    };

    static PassRefPtr<ReadingContext> create(ScriptState* scriptState, ScriptValue streamReader)
    {
        return adoptRef(new ReadingContext(scriptState, streamReader));
    }

    void attachReader(WebDataConsumerHandle::Client* client)
    {
        m_client = client;
        notifyLater();
    }

    void detachReader()
    {
        m_client = nullptr;
    }

    Result beginRead(const void** buffer, size_t* available)
    {
        *buffer = nullptr;
        *available = 0;
        if (m_hasError)
            return WebDataConsumerHandle::UnexpectedError;
        if (m_isDone)
            return WebDataConsumerHandle::Done;

        if (m_pendingBuffer) {
            ASSERT(m_pendingOffset < m_pendingBuffer->length());
            *buffer = m_pendingBuffer->data() + m_pendingOffset;
            *available = m_pendingBuffer->length() - m_pendingOffset;
            return WebDataConsumerHandle::Ok;
        }
        if (!m_isReading) {
            m_isReading = true;
            ScriptState::Scope scope(m_scriptState.get());
            ScriptValue reader(m_scriptState.get(), m_reader.newLocal(m_scriptState->isolate()));
            if (reader.isEmpty()) {
                // The reader was collected.
                m_hasError = true;
                m_isReading = false;
                return WebDataConsumerHandle::UnexpectedError;
            }
            ReadableStreamOperations::defaultReaderRead(
                m_scriptState.get(), reader).then(
                    OnFulfilled::createFunction(m_scriptState.get(), this),
                    OnRejected::createFunction(m_scriptState.get(), this));
        }
        return WebDataConsumerHandle::ShouldWait;
    }

    Result endRead(size_t readSize)
    {
        ASSERT(m_pendingBuffer);
        ASSERT(m_pendingOffset + readSize <= m_pendingBuffer->length());
        m_pendingOffset += readSize;
        if (m_pendingOffset == m_pendingBuffer->length()) {
            m_pendingBuffer = nullptr;
            m_pendingOffset = 0;
        }
        return WebDataConsumerHandle::Ok;
    }

    void onRead(DOMUint8Array* buffer)
    {
        ASSERT(m_isReading);
        ASSERT(buffer);
        ASSERT(!m_pendingBuffer);
        ASSERT(!m_pendingOffset);
        m_isReading = false;
        if (buffer->length() > 0)
            m_pendingBuffer = buffer;
        notify();
    }

    void onReadDone()
    {
        ASSERT(m_isReading);
        ASSERT(!m_pendingBuffer);
        m_isReading = false;
        m_isDone = true;
        m_reader.clear();
        notify();
    }

    void onRejected()
    {
        ASSERT(m_isReading);
        ASSERT(!m_pendingBuffer);
        m_hasError = true;
        m_isReading = false;
        m_reader.clear();
        notify();
    }

    void notify()
    {
        if (!m_client)
            return;
        m_client->didGetReadable();
    }

    void notifyLater()
    {
        ASSERT(m_client);
        Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&ReadingContext::notify, PassRefPtr<ReadingContext>(this)));
    }

private:
    ReadingContext(ScriptState* scriptState, ScriptValue streamReader)
        : m_reader(scriptState->isolate(), streamReader.v8Value())
        , m_scriptState(scriptState)
        , m_client(nullptr)
        , m_pendingOffset(0)
        , m_isReading(false)
        , m_isDone(false)
        , m_hasError(false)
    {
        m_reader.setWeak(this, &ReadingContext::onCollected);
    }

    void onCollected()
    {
        m_reader.clear();
        if (m_isDone || m_hasError)
            return;
        m_hasError = true;
        if (m_client)
            notifyLater();
    }

    static void onCollected(const v8::WeakCallbackInfo<ReadableStreamDataConsumerHandle::ReadingContext>& data)
    {
        data.GetParameter()->onCollected();
    }

    // |m_reader| is a weak persistent. It should be kept alive by someone
    // outside of ReadableStreamDataConsumerHandle.
    // Holding a ScopedPersistent here is safe in terms of cross-world wrapper
    // leakage because we read only Uint8Array chunks from the reader.
    ScopedPersistent<v8::Value> m_reader;
    RefPtr<ScriptState> m_scriptState;
    WebDataConsumerHandle::Client* m_client;
    Persistent<DOMUint8Array> m_pendingBuffer;
    size_t m_pendingOffset;
    bool m_isReading;
    bool m_isDone;
    bool m_hasError;
};

ReadableStreamDataConsumerHandle::ReadableStreamDataConsumerHandle(ScriptState* scriptState, ScriptValue streamReader)
    : m_readingContext(ReadingContext::create(scriptState, streamReader))
{
}
ReadableStreamDataConsumerHandle::~ReadableStreamDataConsumerHandle() = default;

FetchDataConsumerHandle::Reader* ReadableStreamDataConsumerHandle::obtainReaderInternal(Client* client)
{
    return new ReadingContext::ReaderImpl(m_readingContext, client);
}

} // namespace blink
