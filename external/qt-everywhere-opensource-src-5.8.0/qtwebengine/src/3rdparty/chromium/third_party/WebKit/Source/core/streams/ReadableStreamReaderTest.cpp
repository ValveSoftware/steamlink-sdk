// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamReader.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStream.h"
#include "core/streams/ReadableStreamImpl.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

using StringStream = ReadableStreamImpl<ReadableStreamChunkTypeTraits<String>>;

namespace {

struct ReadResult {
    ReadResult() : isDone(false), isSet(false) { }

    String valueString;
    bool isDone;
    bool isSet;
};

class StringCapturingFunction final : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, String* value)
    {
        StringCapturingFunction* self = new StringCapturingFunction(scriptState, value);
        return self->bindToV8Function();
    }

private:
    StringCapturingFunction(ScriptState* scriptState, String* value)
        : ScriptFunction(scriptState)
        , m_value(value)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        ASSERT(!value.isEmpty());
        *m_value = toCoreString(value.v8Value()->ToString(getScriptState()->context()).ToLocalChecked());
        return value;
    }

    String* m_value;
};

class ReadResultCapturingFunction final : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, ReadResult* value)
    {
        ReadResultCapturingFunction* self = new ReadResultCapturingFunction(scriptState, value);
        return self->bindToV8Function();
    }

private:
    ReadResultCapturingFunction(ScriptState* scriptState, ReadResult* value)
        : ScriptFunction(scriptState)
        , m_result(value)
    {
    }

    ScriptValue call(ScriptValue result) override
    {
        ASSERT(!result.isEmpty());
        v8::Isolate* isolate = getScriptState()->isolate();
        if (!result.isObject()) {
            return result;
        }
        v8::Local<v8::Object> object = result.v8Value().As<v8::Object>();
        v8::Local<v8::String> doneString = v8String(isolate, "done");
        v8::Local<v8::String> valueString = v8String(isolate, "value");
        v8::Local<v8::Context> context = getScriptState()->context();
        v8::Maybe<bool> hasDone = object->Has(context, doneString);
        v8::Maybe<bool> hasValue = object->Has(context, valueString);

        if (hasDone.IsNothing() || !hasDone.FromJust() || hasValue.IsNothing() || !hasValue.FromJust()) {
            return result;
        }

        v8::Local<v8::Value> done;
        v8::Local<v8::Value> value;

        if (!object->Get(context, doneString).ToLocal(&done) || !object->Get(context, valueString).ToLocal(&value) || !done->IsBoolean()) {
            return result;
        }

        m_result->isSet = true;
        m_result->isDone = done.As<v8::Boolean>()->Value();
        m_result->valueString = toCoreString(value->ToString(getScriptState()->context()).ToLocalChecked());
        return result;
    }

    ReadResult* m_result;
};

class NoopUnderlyingSource final : public GarbageCollectedFinalized<NoopUnderlyingSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(NoopUnderlyingSource);
public:
    ~NoopUnderlyingSource() override { }

    void pullSource() override { }
    ScriptPromise cancelSource(ScriptState* scriptState, ScriptValue reason) { return ScriptPromise::cast(scriptState, reason); }
    DEFINE_INLINE_VIRTUAL_TRACE() { UnderlyingSource::trace(visitor); }
};

class PermissiveStrategy final : public StringStream::Strategy {
public:
    bool shouldApplyBackpressure(size_t, ReadableStream*) override { return false; }
};

class ReadableStreamReaderTest : public ::testing::Test {
public:
    ReadableStreamReaderTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_stream(new StringStream(new NoopUnderlyingSource, new PermissiveStrategy))
    {
        m_stream->didSourceStart();
    }

    ~ReadableStreamReaderTest() override
    {
        // We need to call |error| in order to make
        // ActiveDOMObject::hasPendingActivity return false.
        m_stream->error(DOMException::create(AbortError, "done"));
    }

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return getScriptState()->isolate(); }
    ExecutionContext* getExecutionContext() { return getScriptState()->getExecutionContext(); }

    v8::Local<v8::Function> createCaptor(String* value)
    {
        return StringCapturingFunction::createFunction(getScriptState(), value);
    }

    v8::Local<v8::Function> createResultCaptor(ReadResult* value)
    {
        return ReadResultCapturingFunction::createFunction(getScriptState(), value);
    }

    std::unique_ptr<DummyPageHolder> m_page;
    Persistent<StringStream> m_stream;
};

TEST_F(ReadableStreamReaderTest, Construct)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_TRUE(reader->isActive());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, Release)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    String onFulfilled, onRejected;
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_TRUE(reader->isActive());

    reader->closed(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    reader->releaseLock(exceptionState);
    EXPECT_FALSE(reader->isActive());
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("AbortError: the reader is already released", onRejected);

    ReadableStreamReader* another = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_TRUE(another->isActive());
    EXPECT_FALSE(reader->isActive());
    reader->releaseLock(exceptionState);
    EXPECT_TRUE(another->isActive());
    EXPECT_FALSE(reader->isActive());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, ReadAfterRelease)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_TRUE(reader->isActive());
    reader->releaseLock(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_FALSE(reader->isActive());

    ReadResult result;
    String onRejected;
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onRejected));

    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_FALSE(result.isSet);
    EXPECT_EQ("TypeError: the reader is already released", onRejected);
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, ReleaseShouldFailWhenCalledWhileReading)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_TRUE(reader->isActive());
    reader->read(getScriptState());

    reader->releaseLock(exceptionState);
    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();

    m_stream->enqueue("hello");
    reader->releaseLock(exceptionState);
    EXPECT_FALSE(reader->isActive());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, EnqueueThenRead)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    m_stream->enqueue("hello");
    m_stream->enqueue("world");
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    ReadResult result;
    String onRejected;
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onRejected));

    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result.isSet);
    EXPECT_FALSE(result.isDone);
    EXPECT_EQ("hello", result.valueString);
    EXPECT_TRUE(onRejected.isNull());

    ReadResult result2;
    String onRejected2;
    reader->read(getScriptState()).then(createResultCaptor(&result2), createCaptor(&onRejected2));

    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result2.isSet);
    EXPECT_FALSE(result2.isDone);
    EXPECT_EQ("world", result2.valueString);
    EXPECT_TRUE(onRejected2.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, ReadThenEnqueue)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    ReadResult result, result2;
    String onRejected, onRejected2;
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onRejected));
    reader->read(getScriptState()).then(createResultCaptor(&result2), createCaptor(&onRejected2));

    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    m_stream->enqueue("hello");
    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result.isSet);
    EXPECT_FALSE(result.isDone);
    EXPECT_EQ("hello", result.valueString);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    m_stream->enqueue("world");
    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result2.isSet);
    EXPECT_FALSE(result2.isDone);
    EXPECT_EQ("world", result2.valueString);
    EXPECT_TRUE(onRejected2.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, ClosedReader)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);

    m_stream->close();

    EXPECT_TRUE(reader->isActive());

    String onClosedFulfilled, onClosedRejected;
    ReadResult result;
    String onReadRejected;
    v8::MicrotasksScope::PerformCheckpoint(isolate());
    reader->closed(getScriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onReadRejected));
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onReadRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_TRUE(result.isSet);
    EXPECT_TRUE(result.isDone);
    EXPECT_EQ("undefined", result.valueString);
    EXPECT_TRUE(onReadRejected.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, ErroredReader)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);

    m_stream->error(DOMException::create(SyntaxError, "some error"));

    EXPECT_EQ(ReadableStream::Errored, m_stream->stateInternal());
    EXPECT_TRUE(reader->isActive());

    String onClosedFulfilled, onClosedRejected;
    String onReadFulfilled, onReadRejected;
    v8::MicrotasksScope::PerformCheckpoint(isolate());
    reader->closed(getScriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    reader->read(getScriptState()).then(createCaptor(&onReadFulfilled), createCaptor(&onReadRejected));
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_TRUE(onReadFulfilled.isNull());
    EXPECT_TRUE(onReadRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_EQ("SyntaxError: some error", onClosedRejected);
    EXPECT_TRUE(onReadFulfilled.isNull());
    EXPECT_EQ("SyntaxError: some error", onReadRejected);
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, PendingReadsShouldBeResolvedWhenClosed)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    ReadResult result, result2;
    String onRejected, onRejected2;
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onRejected));
    reader->read(getScriptState()).then(createResultCaptor(&result2), createCaptor(&onRejected2));

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    m_stream->close();
    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result.isSet);
    EXPECT_TRUE(result.isDone);
    EXPECT_EQ("undefined", result.valueString);
    EXPECT_TRUE(onRejected.isNull());

    EXPECT_TRUE(result2.isSet);
    EXPECT_TRUE(result2.isDone);
    EXPECT_EQ("undefined", result2.valueString);
    EXPECT_TRUE(onRejected2.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, PendingReadsShouldBeRejectedWhenErrored)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    String onFulfilled, onFulfilled2;
    String onRejected, onRejected2;
    reader->read(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    reader->read(getScriptState()).then(createCaptor(&onFulfilled2), createCaptor(&onRejected2));

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_TRUE(onFulfilled2.isNull());
    EXPECT_TRUE(onRejected2.isNull());

    m_stream->error(DOMException::create(SyntaxError, "some error"));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_TRUE(onFulfilled2.isNull());
    EXPECT_TRUE(onRejected2.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ(onRejected, "SyntaxError: some error");
    EXPECT_TRUE(onFulfilled2.isNull());
    EXPECT_EQ(onRejected2, "SyntaxError: some error");
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, PendingReadsShouldBeResolvedWhenCanceled)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    ReadResult result, result2;
    String onRejected, onRejected2;
    reader->read(getScriptState()).then(createResultCaptor(&result), createCaptor(&onRejected));
    reader->read(getScriptState()).then(createResultCaptor(&result2), createCaptor(&onRejected2));

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    reader->cancel(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate())));
    EXPECT_TRUE(reader->isActive());
    EXPECT_FALSE(result.isSet);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(result2.isSet);
    EXPECT_TRUE(onRejected2.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(result.isSet);
    EXPECT_TRUE(result.isDone);
    EXPECT_EQ("undefined", result.valueString);
    EXPECT_TRUE(onRejected.isNull());

    EXPECT_TRUE(result2.isSet);
    EXPECT_TRUE(result2.isDone);
    EXPECT_EQ("undefined", result2.valueString);
    EXPECT_TRUE(onRejected2.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, CancelShouldNotWorkWhenNotActive)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    reader->releaseLock(exceptionState);
    EXPECT_FALSE(reader->isActive());

    String onFulfilled, onRejected;
    reader->cancel(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate()))).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("TypeError: the reader is already released", onRejected);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, Cancel)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    String onClosedFulfilled, onClosedRejected;
    String onCancelFulfilled, onCancelRejected;
    reader->closed(getScriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    reader->cancel(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate()))).then(createCaptor(&onCancelFulfilled), createCaptor(&onCancelRejected));

    EXPECT_EQ(ReadableStream::Closed, m_stream->stateInternal());
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_TRUE(onCancelFulfilled.isNull());
    EXPECT_TRUE(onCancelRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_EQ("undefined", onCancelFulfilled);
    EXPECT_TRUE(onCancelRejected.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, Close)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    String onFulfilled, onRejected;
    reader->closed(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    m_stream->close();

    EXPECT_EQ(ReadableStream::Closed, m_stream->stateInternal());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ReadableStreamReaderTest, Error)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    ReadableStreamReader* reader = new ReadableStreamReader(getExecutionContext(), m_stream);
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());

    String onFulfilled, onRejected;
    reader->closed(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    m_stream->error(DOMException::create(SyntaxError, "some error"));

    EXPECT_EQ(ReadableStream::Errored, m_stream->stateInternal());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("SyntaxError: some error", onRejected);
    EXPECT_FALSE(exceptionState.hadException());
}

} // namespace

} // namespace blink
