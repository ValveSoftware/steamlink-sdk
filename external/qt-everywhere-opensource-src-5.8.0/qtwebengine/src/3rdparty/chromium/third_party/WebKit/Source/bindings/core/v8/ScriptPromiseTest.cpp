/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/ScriptPromise.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/NullExecutionContext.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

namespace {

typedef ScriptPromise::InternalResolver Resolver;

void callback(const v8::FunctionCallbackInfo<v8::Value>& info) { }

class Function : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, ScriptValue* output)
    {
        Function* self = new Function(scriptState, output);
        return self->bindToV8Function();
    }

private:
    Function(ScriptState* scriptState, ScriptValue* output)
        : ScriptFunction(scriptState)
        , m_output(output)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        ASSERT(!value.isEmpty());
        *m_output = value;
        return value;
    }

    ScriptValue* m_output;
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
        // FIXME: We put this statement here to clear an exception from the isolate.
        createClosure(callback, v8::Undefined(m_isolate), m_isolate);

        // Execute all pending microtasks
        v8::MicrotasksScope::PerformCheckpoint(m_isolate);
    }

    bool hasCaught() const { return m_trycatch.HasCaught(); }

private:
    v8::Isolate* m_isolate;
    v8::TryCatch m_trycatch;
};

String toString(v8::Local<v8::Context> context, const ScriptValue& value)
{
    return toCoreString(value.v8Value()->ToString(context).ToLocalChecked());
}

Vector<String> toStringArray(v8::Isolate* isolate, const ScriptValue& value)
{
    NonThrowableExceptionState exceptionState;
    return toImplArray<Vector<String>>(value.v8Value(), 0, isolate, exceptionState);
}


TEST(ScriptPromiseTest, constructFromNonPromise)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptPromise promise(scope.getScriptState(), v8::Undefined(scope.isolate()));
    ASSERT_TRUE(tryCatchScope.hasCaught());
    ASSERT_TRUE(promise.isEmpty());
}

TEST(ScriptPromiseTest, thenResolve)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    Resolver resolver(scope.getScriptState());
    ScriptPromise promise = resolver.promise();
    ScriptValue onFulfilled, onRejected;
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    resolver.resolve(v8String(scope.isolate(), "hello"));

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_EQ("hello", toString(scope.context(), onFulfilled));
    EXPECT_TRUE(onRejected.isEmpty());
}

TEST(ScriptPromiseTest, resolveThen)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    Resolver resolver(scope.getScriptState());
    ScriptPromise promise = resolver.promise();
    ScriptValue onFulfilled, onRejected;
    resolver.resolve(v8String(scope.isolate(), "hello"));
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_EQ("hello", toString(scope.context(), onFulfilled));
    EXPECT_TRUE(onRejected.isEmpty());
}

TEST(ScriptPromiseTest, thenReject)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    Resolver resolver(scope.getScriptState());
    ScriptPromise promise = resolver.promise();
    ScriptValue onFulfilled, onRejected;
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    resolver.reject(v8String(scope.isolate(), "hello"));

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_EQ("hello", toString(scope.context(), onRejected));
}

TEST(ScriptPromiseTest, rejectThen)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    Resolver resolver(scope.getScriptState());
    ScriptPromise promise = resolver.promise();
    ScriptValue onFulfilled, onRejected;
    resolver.reject(v8String(scope.isolate(), "hello"));
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_EQ("hello", toString(scope.context(), onRejected));
}

TEST(ScriptPromiseTest, castPromise)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptPromise promise = Resolver(scope.getScriptState()).promise();
    ScriptPromise newPromise = ScriptPromise::cast(scope.getScriptState(), promise.v8Value());

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_EQ(promise.v8Value(), newPromise.v8Value());
}

TEST(ScriptPromiseTest, castNonPromise)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled1, onFulfilled2, onRejected1, onRejected2;

    ScriptValue value = ScriptValue(scope.getScriptState(), v8String(scope.isolate(), "hello"));
    ScriptPromise promise1 = ScriptPromise::cast(scope.getScriptState(), ScriptValue(value));
    ScriptPromise promise2 = ScriptPromise::cast(scope.getScriptState(), ScriptValue(value));
    promise1.then(Function::createFunction(scope.getScriptState(), &onFulfilled1), Function::createFunction(scope.getScriptState(), &onRejected1));
    promise2.then(Function::createFunction(scope.getScriptState(), &onFulfilled2), Function::createFunction(scope.getScriptState(), &onRejected2));

    ASSERT_FALSE(promise1.isEmpty());
    ASSERT_FALSE(promise2.isEmpty());
    EXPECT_NE(promise1.v8Value(), promise2.v8Value());

    ASSERT_TRUE(promise1.v8Value()->IsPromise());
    ASSERT_TRUE(promise2.v8Value()->IsPromise());

    EXPECT_TRUE(onFulfilled1.isEmpty());
    EXPECT_TRUE(onFulfilled2.isEmpty());
    EXPECT_TRUE(onRejected1.isEmpty());
    EXPECT_TRUE(onRejected2.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_EQ("hello", toString(scope.context(), onFulfilled1));
    EXPECT_EQ("hello", toString(scope.context(), onFulfilled2));
    EXPECT_TRUE(onRejected1.isEmpty());
    EXPECT_TRUE(onRejected2.isEmpty());
}

TEST(ScriptPromiseTest, reject)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled, onRejected;

    ScriptValue value = ScriptValue(scope.getScriptState(), v8String(scope.isolate(), "hello"));
    ScriptPromise promise = ScriptPromise::reject(scope.getScriptState(), ScriptValue(value));
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    ASSERT_TRUE(promise.v8Value()->IsPromise());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_EQ("hello", toString(scope.context(), onRejected));
}

TEST(ScriptPromiseTest, rejectWithExceptionState)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled, onRejected;
    ScriptPromise promise = ScriptPromise::rejectWithDOMException(scope.getScriptState(), DOMException::create(SyntaxError, "some syntax error"));
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    ASSERT_FALSE(promise.isEmpty());
    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_EQ("SyntaxError: some syntax error", toString(scope.context(), onRejected));
}

TEST(ScriptPromiseTest, allWithEmptyPromises)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled, onRejected;

    ScriptPromise promise = ScriptPromise::all(scope.getScriptState(), Vector<ScriptPromise>());
    ASSERT_FALSE(promise.isEmpty());

    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_FALSE(onFulfilled.isEmpty());
    EXPECT_TRUE(toStringArray(scope.isolate(), onFulfilled).isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());
}

TEST(ScriptPromiseTest, allWithResolvedPromises)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled, onRejected;

    Vector<ScriptPromise> promises;
    promises.append(ScriptPromise::cast(scope.getScriptState(), v8String(scope.isolate(), "hello")));
    promises.append(ScriptPromise::cast(scope.getScriptState(), v8String(scope.isolate(), "world")));

    ScriptPromise promise = ScriptPromise::all(scope.getScriptState(), promises);
    ASSERT_FALSE(promise.isEmpty());
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_FALSE(onFulfilled.isEmpty());
    Vector<String> values = toStringArray(scope.isolate(), onFulfilled);
    EXPECT_EQ(2u, values.size());
    EXPECT_EQ("hello", values[0]);
    EXPECT_EQ("world", values[1]);
    EXPECT_TRUE(onRejected.isEmpty());
}

TEST(ScriptPromiseTest, allWithRejectedPromise)
{
    V8TestingScope scope;
    TryCatchScope tryCatchScope(scope.isolate());
    ScriptValue onFulfilled, onRejected;

    Vector<ScriptPromise> promises;
    promises.append(ScriptPromise::cast(scope.getScriptState(), v8String(scope.isolate(), "hello")));
    promises.append(ScriptPromise::reject(scope.getScriptState(), v8String(scope.isolate(), "world")));

    ScriptPromise promise = ScriptPromise::all(scope.getScriptState(), promises);
    ASSERT_FALSE(promise.isEmpty());
    promise.then(Function::createFunction(scope.getScriptState(), &onFulfilled), Function::createFunction(scope.getScriptState(), &onRejected));

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_TRUE(onRejected.isEmpty());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

    EXPECT_TRUE(onFulfilled.isEmpty());
    EXPECT_FALSE(onRejected.isEmpty());
    EXPECT_EQ("world", toString(scope.context(), onRejected));
}

} // namespace

} // namespace blink
