// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStreamImpl.h"
#include "core/streams/ReadableStreamReader.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gmock/include/gmock/gmock-more-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;

namespace {

using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using StringStream = ReadableStreamImpl<ReadableStreamChunkTypeTraits<String>>;

class StringCapturingFunction : public ScriptFunction {
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

class MockUnderlyingSource : public GarbageCollectedFinalized<MockUnderlyingSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(MockUnderlyingSource);
public:
    ~MockUnderlyingSource() override { }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        UnderlyingSource::trace(visitor);
    }

    MOCK_METHOD0(pullSource, void());
    MOCK_METHOD2(cancelSource, ScriptPromise(ScriptState*, ScriptValue));
};

class PermissiveStrategy : public StringStream::Strategy {
public:
    bool shouldApplyBackpressure(size_t, ReadableStream*) override { return false; }
};

class MockStrategy : public StringStream::Strategy {
public:
    static ::testing::StrictMock<MockStrategy>* create() { return new ::testing::StrictMock<MockStrategy>; }

    MOCK_METHOD2(shouldApplyBackpressure, bool(size_t, ReadableStream*));
    MOCK_METHOD2(size, size_t(const String&, ReadableStream*));
};

class ThrowError {
public:
    explicit ThrowError(const String& message)
        : m_message(message) { }

    void operator()(ExceptionState* exceptionState)
    {
        exceptionState->throwTypeError(m_message);
    }

private:
    String m_message;
};

} // unnamed namespace

// ReadableStream::read and some related functionalities are tested in
// ReadableStreamReaderTest.
class ReadableStreamTest : public ::testing::Test {
public:
    ReadableStreamTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_underlyingSource(new ::testing::StrictMock<MockUnderlyingSource>)
    {
    }

    ~ReadableStreamTest() override
    {
    }

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return getScriptState()->isolate(); }

    v8::Local<v8::Function> createCaptor(String* value)
    {
        return StringCapturingFunction::createFunction(getScriptState(), value);
    }

    StringStream* construct(MockStrategy* strategy)
    {
        Checkpoint checkpoint;
        {
            InSequence s;
            EXPECT_CALL(checkpoint, Call(0));
            EXPECT_CALL(*strategy, shouldApplyBackpressure(0, _)).WillOnce(Return(true));
            EXPECT_CALL(checkpoint, Call(1));
        }
        StringStream* stream = new StringStream(m_underlyingSource, strategy);
        checkpoint.Call(0);
        stream->didSourceStart();
        checkpoint.Call(1);
        return stream;
    }
    StringStream* construct()
    {
        Checkpoint checkpoint;
        {
            InSequence s;
            EXPECT_CALL(checkpoint, Call(0));
            EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
            EXPECT_CALL(checkpoint, Call(1));
        }
        StringStream* stream = new StringStream(m_underlyingSource, new PermissiveStrategy);
        checkpoint.Call(0);
        stream->didSourceStart();
        checkpoint.Call(1);
        return stream;
    }

    std::unique_ptr<DummyPageHolder> m_page;
    Persistent<MockUnderlyingSource> m_underlyingSource;
};

TEST_F(ReadableStreamTest, Start)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    StringStream* stream = new StringStream(m_underlyingSource);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_FALSE(stream->isDisturbed());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Readable);

    checkpoint.Call(0);
    stream->didSourceStart();
    checkpoint.Call(1);

    EXPECT_TRUE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Readable);

    // We need to call |error| in order to make
    // ActiveDOMObject::hasPendingActivity return false.
    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, StartFail)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = new StringStream(m_underlyingSource);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Readable);

    stream->error(DOMException::create(NotFoundError));

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Errored);
}

TEST_F(ReadableStreamTest, ErrorAndEnqueue)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
}

TEST_F(ReadableStreamTest, CloseAndEnqueue)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();

    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
}

TEST_F(ReadableStreamTest, CloseWhenErrored)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    stream->error(DOMException::create(NotFoundError, "error"));
    stream->close();

    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
}

TEST_F(ReadableStreamTest, ReadQueue)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    Deque<std::pair<String, size_t>> queue;

    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());

    checkpoint.Call(0);
    EXPECT_FALSE(stream->isDisturbed());
    stream->readInternal(queue);
    EXPECT_TRUE(stream->isDisturbed());
    checkpoint.Call(1);
    ASSERT_EQ(2u, queue.size());

    EXPECT_EQ(std::make_pair(String("hello"), static_cast<size_t>(5)), queue[0]);
    EXPECT_EQ(std::make_pair(String("bye"), static_cast<size_t>(3)), queue[1]);

    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());
}

TEST_F(ReadableStreamTest, CloseWhenReadable)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();

    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    stream->close();
    EXPECT_FALSE(stream->enqueue("should be ignored"));

    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    stream->read(getScriptState());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    stream->read(getScriptState());

    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());
}

TEST_F(ReadableStreamTest, CancelWhenClosed)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    EXPECT_FALSE(stream->isDisturbed());
    ScriptPromise promise = stream->cancel(getScriptState(), ScriptValue());
    EXPECT_TRUE(stream->isDisturbed());
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenErrored)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    EXPECT_FALSE(stream->isDisturbed());
    ScriptPromise promise = stream->cancel(getScriptState(), ScriptValue());
    EXPECT_TRUE(stream->isDisturbed());
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: error", onRejected);
}

TEST_F(ReadableStreamTest, CancelWhenReadable)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    String onCancelFulfilled, onCancelRejected;
    ScriptValue reason(getScriptState(), v8String(getScriptState()->isolate(), "reason"));
    ScriptPromise promise = ScriptPromise::cast(getScriptState(), v8String(getScriptState()->isolate(), "hello"));

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, cancelSource(getScriptState(), reason)).WillOnce(ReturnPointee(&promise));
    }

    stream->enqueue("hello");
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    EXPECT_FALSE(stream->isDisturbed());
    ScriptPromise cancelResult = stream->cancel(getScriptState(), reason);
    EXPECT_TRUE(stream->isDisturbed());
    cancelResult.then(createCaptor(&onCancelFulfilled), createCaptor(&onCancelRejected));

    EXPECT_NE(promise, cancelResult);
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    EXPECT_TRUE(onCancelFulfilled.isNull());
    EXPECT_TRUE(onCancelRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onCancelFulfilled);
    EXPECT_TRUE(onCancelRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenLocked)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    String onFulfilled, onRejected;
    StringStream* stream = construct();
    ReadableStreamReader* reader = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);

    EXPECT_TRUE(reader->isActive());
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    EXPECT_FALSE(stream->isDisturbed());
    stream->cancel(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate()))).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_FALSE(stream->isDisturbed());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("TypeError: this stream is locked to a ReadableStreamReader", onRejected);
    EXPECT_TRUE(reader->isActive());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
}

TEST_F(ReadableStreamTest, ReadableArrayBufferStreamCompileTest)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    // This test tests if ReadableStreamImpl<DOMArrayBuffer> can be
    // instantiated.
    new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>(m_underlyingSource);
}

TEST_F(ReadableStreamTest, ReadableArrayBufferViewStreamCompileTest)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    // This test tests if ReadableStreamImpl<DOMArrayBufferVIew> can be
    // instantiated.
    new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBufferView>>(m_underlyingSource);
}

TEST_F(ReadableStreamTest, BackpressureOnEnqueueing)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    auto strategy = MockStrategy::create();
    Checkpoint checkpoint;

    StringStream* stream = construct(strategy);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*strategy, size(String("hello"), stream)).WillOnce(Return(1));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(1, stream)).WillOnce(Return(false));
        EXPECT_CALL(checkpoint, Call(1));
        EXPECT_CALL(checkpoint, Call(2));
        EXPECT_CALL(*strategy, size(String("world"), stream)).WillOnce(Return(2));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(3, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(3));
    }
    checkpoint.Call(0);
    bool result = stream->enqueue("hello");
    checkpoint.Call(1);
    EXPECT_TRUE(result);

    checkpoint.Call(2);
    result = stream->enqueue("world");
    checkpoint.Call(3);
    EXPECT_FALSE(result);

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, BackpressureOnReading)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    auto strategy = MockStrategy::create();
    Checkpoint checkpoint;

    StringStream* stream = construct(strategy);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    {
        InSequence s;
        EXPECT_CALL(*strategy, size(String("hello"), stream)).WillOnce(Return(2));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(2, stream)).WillOnce(Return(false));
        EXPECT_CALL(*strategy, size(String("world"), stream)).WillOnce(Return(3));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(5, stream)).WillOnce(Return(false));

        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(3, stream)).WillOnce(Return(false));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
        // shouldApplyBackpressure and pullSource are not called because the
        // stream is pulling.
        EXPECT_CALL(checkpoint, Call(2));
        EXPECT_CALL(*strategy, size(String("foo"), stream)).WillOnce(Return(4));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(4, stream)).WillOnce(Return(true));
        EXPECT_CALL(*strategy, size(String("bar"), stream)).WillOnce(Return(5));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(9, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(3));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(5, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(4));
    }
    stream->enqueue("hello");
    stream->enqueue("world");

    checkpoint.Call(0);
    stream->read(getScriptState());
    checkpoint.Call(1);
    stream->read(getScriptState());
    checkpoint.Call(2);
    stream->enqueue("foo");
    stream->enqueue("bar");
    checkpoint.Call(3);
    stream->read(getScriptState());
    checkpoint.Call(4);

    stream->error(DOMException::create(AbortError, "done"));
}

// Note: Detailed tests are on ReadableStreamReaderTest.
TEST_F(ReadableStreamTest, ReadableStreamReader)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    ReadableStreamReader* reader = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);

    ASSERT_TRUE(reader);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(stream->isLockedTo(reader));

    ReadableStreamReader* another = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);
    ASSERT_EQ(nullptr, another);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(stream->isLockedTo(reader));
}

TEST_F(ReadableStreamTest, GetClosedReader)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    stream->close();
    ReadableStreamReader* reader = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);

    ASSERT_TRUE(reader);
    EXPECT_FALSE(exceptionState.hadException());

    String onFulfilled, onRejected;
    reader->closed(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, GetErroredReader)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    StringStream* stream = construct();
    stream->error(DOMException::create(SyntaxError, "some error"));
    ReadableStreamReader* reader = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);

    ASSERT_TRUE(reader);
    EXPECT_FALSE(exceptionState.hadException());

    String onFulfilled, onRejected;
    reader->closed(getScriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("SyntaxError: some error", onRejected);
}

TEST_F(ReadableStreamTest, StrictStrategy)
{
    ScriptState::Scope scope(getScriptState());
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "property", "interface", getScriptState()->context()->Global(), isolate());
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(checkpoint, Call(1));
        EXPECT_CALL(*m_underlyingSource, pullSource());
        EXPECT_CALL(checkpoint, Call(2));
        EXPECT_CALL(checkpoint, Call(3));
        EXPECT_CALL(*m_underlyingSource, pullSource());
        EXPECT_CALL(checkpoint, Call(4));
        EXPECT_CALL(checkpoint, Call(5));
        EXPECT_CALL(checkpoint, Call(6));
        EXPECT_CALL(checkpoint, Call(7));
        EXPECT_CALL(checkpoint, Call(8));
        EXPECT_CALL(checkpoint, Call(9));
        EXPECT_CALL(*m_underlyingSource, pullSource());
    }
    StringStream* stream = new StringStream(m_underlyingSource, new StringStream::StrictStrategy);
    ReadableStreamReader* reader = stream->getReader(getScriptState()->getExecutionContext(), exceptionState);

    checkpoint.Call(0);
    stream->didSourceStart();

    checkpoint.Call(1);
    EXPECT_FALSE(stream->isPulling());
    reader->read(getScriptState());
    EXPECT_TRUE(stream->isPulling());
    checkpoint.Call(2);
    stream->enqueue("hello");
    EXPECT_FALSE(stream->isPulling());
    checkpoint.Call(3);
    reader->read(getScriptState());
    EXPECT_TRUE(stream->isPulling());
    checkpoint.Call(4);
    reader->read(getScriptState());
    EXPECT_TRUE(stream->isPulling());
    checkpoint.Call(5);
    stream->enqueue("hello");
    EXPECT_FALSE(stream->isPulling());
    checkpoint.Call(6);
    stream->enqueue("hello");
    EXPECT_FALSE(stream->isPulling());
    checkpoint.Call(7);
    stream->enqueue("hello");
    EXPECT_FALSE(stream->isPulling());
    checkpoint.Call(8);
    reader->read(getScriptState());
    EXPECT_FALSE(stream->isPulling());
    checkpoint.Call(9);
    reader->read(getScriptState());
    EXPECT_TRUE(stream->isPulling());

    stream->error(DOMException::create(AbortError, "done"));
}

} // namespace blink
