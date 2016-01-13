/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/v8/ScriptPromiseResolver.h"

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/V8Binding.h"

#include <gtest/gtest.h>
#include <v8.h>

namespace WebCore {

namespace {

class Function : public ScriptFunction {
public:
    static PassOwnPtr<Function> create(v8::Isolate* isolate, String* value)
    {
        return adoptPtr(new Function(isolate, value));
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        ASSERT(!value.isEmpty());
        *m_value = toCoreString(value.v8Value()->ToString());
        return value;
    }

private:
    Function(v8::Isolate* isolate, String* value) : ScriptFunction(isolate), m_value(value) { }

    String* m_value;
};

class ScriptPromiseResolverTest : public testing::Test {
public:
    ScriptPromiseResolverTest()
        : m_scope(v8::Isolate::GetCurrent())
    {
        m_resolver = ScriptPromiseResolver::create(m_scope.scriptState());
    }

    virtual ~ScriptPromiseResolverTest()
    {
        // Run all pending microtasks here.
        isolate()->RunMicrotasks();
    }

    v8::Isolate* isolate() { return m_scope.isolate(); }

protected:
    RefPtr<ScriptPromiseResolver> m_resolver;
    V8TestingScope m_scope;
};

TEST_F(ScriptPromiseResolverTest, initialState)
{
    ScriptPromise promise = m_resolver->promise();
    ASSERT_FALSE(promise.isEmpty());
    String onFulfilled, onRejected;
    promise.then(Function::create(isolate(), &onFulfilled), Function::create(isolate(), &onRejected));

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);

    isolate()->RunMicrotasks();

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);
}

TEST_F(ScriptPromiseResolverTest, resolve)
{
    ScriptPromise promise = m_resolver->promise();
    ASSERT_FALSE(promise.isEmpty());
    String onFulfilled, onRejected;
    promise.then(Function::create(isolate(), &onFulfilled), Function::create(isolate(), &onRejected));

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->resolve("hello");
    EXPECT_TRUE(m_resolver->promise().isEmpty());
    isolate()->RunMicrotasks();

    EXPECT_EQ("hello", onFulfilled);
    EXPECT_EQ(String(), onRejected);
}

TEST_F(ScriptPromiseResolverTest, reject)
{
    ScriptPromise promise = m_resolver->promise();
    ASSERT_FALSE(promise.isEmpty());
    String onFulfilled, onRejected;
    promise.then(Function::create(isolate(), &onFulfilled), Function::create(isolate(), &onRejected));

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->reject("hello");
    EXPECT_TRUE(m_resolver->promise().isEmpty());
    isolate()->RunMicrotasks();

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ("hello", onRejected);
}

TEST_F(ScriptPromiseResolverTest, resolveOverResolve)
{
    ScriptPromise promise = m_resolver->promise();
    ASSERT_FALSE(promise.isEmpty());
    String onFulfilled, onRejected;
    promise.then(Function::create(isolate(), &onFulfilled), Function::create(isolate(), &onRejected));

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->resolve("hello");
    EXPECT_TRUE(m_resolver->promise().isEmpty());
    isolate()->RunMicrotasks();

    EXPECT_EQ("hello", onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->resolve("world");
    isolate()->RunMicrotasks();

    EXPECT_EQ("hello", onFulfilled);
    EXPECT_EQ(String(), onRejected);
}

TEST_F(ScriptPromiseResolverTest, rejectOverResolve)
{
    ScriptPromise promise = m_resolver->promise();
    ASSERT_FALSE(promise.isEmpty());
    String onFulfilled, onRejected;
    promise.then(Function::create(isolate(), &onFulfilled), Function::create(isolate(), &onRejected));

    EXPECT_EQ(String(), onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->resolve("hello");
    EXPECT_TRUE(m_resolver->promise().isEmpty());
    isolate()->RunMicrotasks();

    EXPECT_EQ("hello", onFulfilled);
    EXPECT_EQ(String(), onRejected);

    m_resolver->reject("world");
    isolate()->RunMicrotasks();

    EXPECT_EQ("hello", onFulfilled);
    EXPECT_EQ(String(), onRejected);
}

} // namespace

} // namespace WebCore
