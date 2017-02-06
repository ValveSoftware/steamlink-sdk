// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

namespace {

class NotReached : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState)
    {
        NotReached* self = new NotReached(scriptState);
        return self->bindToV8Function();
    }

private:
    explicit NotReached(ScriptState* scriptState)
        : ScriptFunction(scriptState)
    {
    }

    ScriptValue call(ScriptValue) override;
};

ScriptValue NotReached::call(ScriptValue)
{
    EXPECT_TRUE(false) << "'Unreachable' code was reached";
    return ScriptValue();
}

class Iteration final : public GarbageCollectedFinalized<Iteration> {
public:
    Iteration()
        : m_isSet(false)
        , m_isDone(false)
        , m_isValid(true) {}

    void set(ScriptValue v)
    {
        ASSERT(!v.isEmpty());
        m_isSet = true;
        v8::TryCatch block(v.getScriptState()->isolate());
        v8::Local<v8::Value> value;
        v8::Local<v8::Value> item = v.v8Value();
        if (!item->IsObject() || !v8Call(v8UnpackIteratorResult(v.getScriptState(), item.As<v8::Object>(), &m_isDone), value)) {
            m_isValid = false;
            return;
        }
        m_value = toCoreString(value->ToString());
    }

    bool isSet() const { return m_isSet; }
    bool isDone() const { return m_isDone; }
    bool isValid() const { return m_isValid; }
    const String& value() const { return m_value; }

    DEFINE_INLINE_TRACE() {}

private:
    bool m_isSet;
    bool m_isDone;
    bool m_isValid;
    String m_value;
};

class Function : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, Iteration* iteration)
    {
        Function* self = new Function(scriptState, iteration);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_iteration);
        ScriptFunction::trace(visitor);
    }

private:
    Function(ScriptState* scriptState, Iteration* iteration)
        : ScriptFunction(scriptState)
        , m_iteration(iteration)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        m_iteration->set(value);
        return value;
    }

    Member<Iteration> m_iteration;
};

class TestUnderlyingSource final : public UnderlyingSourceBase {
public:
    explicit TestUnderlyingSource(ScriptState* scriptState)
        : UnderlyingSourceBase(scriptState)
    {
    }

    // Just expose the controller methods for easy testing
    void enqueue(ScriptValue value) { controller()->enqueue(value); }
    void close() { controller()->close(); }
    void error(ScriptValue value) { controller()->error(value); }
    double desiredSize() { return controller()->desiredSize(); }
};

class TryCatchScope {
public:
    explicit TryCatchScope(v8::Isolate* isolate)
        : m_isolate(isolate)
        , m_trycatch(isolate)
    {
    }

    ~TryCatchScope()
    {
        v8::MicrotasksScope::PerformCheckpoint(m_isolate);
        EXPECT_FALSE(m_trycatch.HasCaught());
    }

private:
    v8::Isolate* m_isolate;
    v8::TryCatch m_trycatch;
};

ScriptValue eval(V8TestingScope* scope, const char* s)
{
    v8::Local<v8::String> source;
    v8::Local<v8::Script> script;
    v8::MicrotasksScope microtasks(scope->isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (!v8Call(v8::String::NewFromUtf8(scope->isolate(), s, v8::NewStringType::kNormal), source)) {
        ADD_FAILURE();
        return ScriptValue();
    }
    if (!v8Call(v8::Script::Compile(scope->context(), source), script)) {
        ADD_FAILURE() << "Compilation fails";
        return ScriptValue();
    }
    return ScriptValue(scope->getScriptState(), script->Run(scope->context()));
}

ScriptValue evalWithPrintingError(V8TestingScope* scope, const char* s)
{
    v8::TryCatch block(scope->isolate());
    ScriptValue r = eval(scope, s);
    if (block.HasCaught()) {
        ADD_FAILURE() << toCoreString(block.Exception()->ToString(scope->isolate())).utf8().data();
        block.ReThrow();
    }
    return r;
}

TEST(ReadableStreamOperationsTest, IsReadableStream)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scope.getScriptState(), ScriptValue(scope.getScriptState(), v8::Undefined(scope.isolate()))));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scope.getScriptState(), ScriptValue::createNull(scope.getScriptState())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scope.getScriptState(), ScriptValue(scope.getScriptState(), v8::Object::New(scope.isolate()))));
    ScriptValue stream = evalWithPrintingError(&scope, "new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());
    EXPECT_TRUE(ReadableStreamOperations::isReadableStream(scope.getScriptState(), stream));
}

TEST(ReadableStreamOperationsTest, IsReadableStreamDefaultReaderInvalid)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), ScriptValue(scope.getScriptState(), v8::Undefined(scope.isolate()))));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), ScriptValue::createNull(scope.getScriptState())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), ScriptValue(scope.getScriptState(), v8::Object::New(scope.isolate()))));
    ScriptValue stream = evalWithPrintingError(&scope, "new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), stream));
}

TEST(ReadableStreamOperationsTest, GetReader)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue stream = evalWithPrintingError(&scope, "new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isLocked(scope.getScriptState(), stream));
    ScriptValue reader;
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(scope.getScriptState(), stream, es);
        ASSERT_FALSE(es.hadException());
    }
    EXPECT_TRUE(ReadableStreamOperations::isLocked(scope.getScriptState(), stream));
    ASSERT_FALSE(reader.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scope.getScriptState(), reader));
    EXPECT_TRUE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), reader));

    // Already locked!
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(scope.getScriptState(), stream, es);
        ASSERT_TRUE(es.hadException());
    }
    ASSERT_TRUE(reader.isEmpty());
}

TEST(ReadableStreamOperationsTest, IsDisturbed)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue stream = evalWithPrintingError(&scope, "stream = new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isDisturbed(scope.getScriptState(), stream));

    ASSERT_FALSE(evalWithPrintingError(&scope, "stream.cancel()").isEmpty());

    EXPECT_TRUE(ReadableStreamOperations::isDisturbed(scope.getScriptState(), stream));
}

TEST(ReadableStreamOperationsTest, Read)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue reader = evalWithPrintingError(&scope,
        "var controller;"
        "function start(c) { controller = c; }"
        "new ReadableStream({start}).getReader()");
    EXPECT_FALSE(reader.isEmpty());
    ASSERT_TRUE(ReadableStreamOperations::isReadableStreamDefaultReader(scope.getScriptState(), reader));

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader).then(
        Function::createFunction(scope.getScriptState(), it1),
        NotReached::createFunction(scope.getScriptState()));
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader).then(
        Function::createFunction(scope.getScriptState(), it2),
        NotReached::createFunction(scope.getScriptState()));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    EXPECT_FALSE(it1->isSet());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError(&scope, "controller.enqueue('hello')").isEmpty());
    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError(&scope, "controller.close()").isEmpty());
    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_TRUE(it2->isDone());
}

TEST(ReadableStreamOperationsTest, CreateReadableStreamWithCustomUnderlyingSourceAndStrategy)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    auto underlyingSource = new TestUnderlyingSource(scope.getScriptState());

    ScriptValue strategy = ReadableStreamOperations::createCountQueuingStrategy(scope.getScriptState(), 10);
    ASSERT_FALSE(strategy.isEmpty());

    ScriptValue stream = ReadableStreamOperations::createReadableStream(scope.getScriptState(), underlyingSource, strategy);
    ASSERT_FALSE(stream.isEmpty());

    EXPECT_EQ(10, underlyingSource->desiredSize());

    underlyingSource->enqueue(ScriptValue::from(scope.getScriptState(), "a"));
    EXPECT_EQ(9, underlyingSource->desiredSize());

    underlyingSource->enqueue(ScriptValue::from(scope.getScriptState(), "b"));
    EXPECT_EQ(8, underlyingSource->desiredSize());

    ScriptValue reader;
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(scope.getScriptState(), stream, es);
        ASSERT_FALSE(es.hadException());
    }
    ASSERT_FALSE(reader.isEmpty());

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    Iteration* it3 = new Iteration();
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader).then(Function::createFunction(scope.getScriptState(), it1), NotReached::createFunction(scope.getScriptState()));
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader).then(Function::createFunction(scope.getScriptState(), it2), NotReached::createFunction(scope.getScriptState()));
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader).then(Function::createFunction(scope.getScriptState(), it3), NotReached::createFunction(scope.getScriptState()));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_EQ(10, underlyingSource->desiredSize());

    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("a", it1->value());

    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_FALSE(it2->isDone());
    EXPECT_EQ("b", it2->value());

    EXPECT_FALSE(it3->isSet());

    underlyingSource->close();
    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(it3->isSet());
    EXPECT_TRUE(it3->isValid());
    EXPECT_TRUE(it3->isDone());
}

TEST(ReadableStreamOperationsTest, UnderlyingSourceShouldHavePendingActivityWhenLockedAndControllerIsActive)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    auto underlyingSource = new TestUnderlyingSource(scope.getScriptState());

    ScriptValue strategy = ReadableStreamOperations::createCountQueuingStrategy(scope.getScriptState(), 10);
    ASSERT_FALSE(strategy.isEmpty());

    ScriptValue stream = ReadableStreamOperations::createReadableStream(scope.getScriptState(), underlyingSource, strategy);
    ASSERT_FALSE(stream.isEmpty());

    v8::Local<v8::Object> global = scope.getScriptState()->context()->Global();
    ASSERT_TRUE(global->Set(scope.context(), v8String(scope.isolate(), "stream"), stream.v8Value()).IsJust());

    EXPECT_FALSE(underlyingSource->hasPendingActivity());
    evalWithPrintingError(&scope, "let reader = stream.getReader();");
    EXPECT_TRUE(underlyingSource->hasPendingActivity());
    evalWithPrintingError(&scope, "reader.releaseLock();");
    EXPECT_FALSE(underlyingSource->hasPendingActivity());
    evalWithPrintingError(&scope, "reader = stream.getReader();");
    EXPECT_TRUE(underlyingSource->hasPendingActivity());
    underlyingSource->enqueue(ScriptValue(scope.getScriptState(), v8::Undefined(scope.isolate())));
    underlyingSource->close();
    EXPECT_FALSE(underlyingSource->hasPendingActivity());
}

TEST(ReadableStreamOperationsTest, IsReadable)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue readable = evalWithPrintingError(&scope, "new ReadableStream()");
    ScriptValue closed = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_TRUE(ReadableStreamOperations::isReadable(scope.getScriptState(), readable));
    EXPECT_FALSE(ReadableStreamOperations::isReadable(scope.getScriptState(), closed));
    EXPECT_FALSE(ReadableStreamOperations::isReadable(scope.getScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, IsClosed)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue readable = evalWithPrintingError(&scope, "new ReadableStream()");
    ScriptValue closed = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isClosed(scope.getScriptState(), readable));
    EXPECT_TRUE(ReadableStreamOperations::isClosed(scope.getScriptState(), closed));
    EXPECT_FALSE(ReadableStreamOperations::isClosed(scope.getScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, IsErrored)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue readable = evalWithPrintingError(&scope, "new ReadableStream()");
    ScriptValue closed = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError(&scope, "new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isErrored(scope.getScriptState(), readable));
    EXPECT_FALSE(ReadableStreamOperations::isErrored(scope.getScriptState(), closed));
    EXPECT_TRUE(ReadableStreamOperations::isErrored(scope.getScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, Tee)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue original = evalWithPrintingError(&scope,
        "var controller;"
        "new ReadableStream({start: c => controller = c})");
    ASSERT_FALSE(original.isEmpty());
    ScriptValue new1, new2;
    ReadableStreamOperations::tee(scope.getScriptState(), original, &new1, &new2);

    NonThrowableExceptionState ec;
    ScriptValue reader1 = ReadableStreamOperations::getReader(scope.getScriptState(), new1, ec);
    ScriptValue reader2 = ReadableStreamOperations::getReader(scope.getScriptState(), new2, ec);

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader1).then(
        Function::createFunction(scope.getScriptState(), it1),
        NotReached::createFunction(scope.getScriptState()));
    ReadableStreamOperations::defaultReaderRead(scope.getScriptState(), reader2).then(
        Function::createFunction(scope.getScriptState(), it2),
        NotReached::createFunction(scope.getScriptState()));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    EXPECT_FALSE(it1->isSet());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError(&scope, "controller.enqueue('hello')").isEmpty());
    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_FALSE(it2->isDone());
    EXPECT_EQ("hello", it2->value());
}

} // namespace

} // namespace blink
