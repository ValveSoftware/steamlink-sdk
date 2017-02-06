// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8Binding.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

template<typename T> v8::Local<v8::Value> toV8(V8TestingScope* scope, T value)
{
    return blink::toV8(value, scope->context()->Global(), scope->isolate());
}

TEST(V8BindingTest, toImplArray)
{
    V8TestingScope scope;
    {
        v8::Local<v8::Array> v8StringArray = v8::Array::New(scope.isolate(), 2);
        EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(scope.context(), toV8(&scope, 0), toV8(&scope, "Hello, World!"))));
        EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(scope.context(), toV8(&scope, 1), toV8(&scope, "Hi, Mom!"))));

        NonThrowableExceptionState exceptionState;
        Vector<String> stringVector = toImplArray<Vector<String>>(v8StringArray, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVector.size());
        EXPECT_EQ("Hello, World!", stringVector[0]);
        EXPECT_EQ("Hi, Mom!", stringVector[1]);
    }
    {
        v8::Local<v8::Array> v8UnsignedArray = v8::Array::New(scope.isolate(), 3);
        EXPECT_TRUE(v8CallBoolean(v8UnsignedArray->Set(scope.context(), toV8(&scope, 0), toV8(&scope, 42))));
        EXPECT_TRUE(v8CallBoolean(v8UnsignedArray->Set(scope.context(), toV8(&scope, 1), toV8(&scope, 1729))));
        EXPECT_TRUE(v8CallBoolean(v8UnsignedArray->Set(scope.context(), toV8(&scope, 2), toV8(&scope, 31773))));

        NonThrowableExceptionState exceptionState;
        Vector<unsigned> unsignedVector = toImplArray<Vector<unsigned>>(v8UnsignedArray, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(3U, unsignedVector.size());
        EXPECT_EQ(42U, unsignedVector[0]);
        EXPECT_EQ(1729U, unsignedVector[1]);
        EXPECT_EQ(31773U, unsignedVector[2]);
    }
    {
        const double doublePi = 3.141592653589793238;
        const float floatPi = doublePi;
        v8::Local<v8::Array> v8RealArray = v8::Array::New(scope.isolate(), 1);
        EXPECT_TRUE(v8CallBoolean(v8RealArray->Set(scope.context(), toV8(&scope, 0), toV8(&scope, doublePi))));

        NonThrowableExceptionState exceptionState;
        Vector<double> doubleVector = toImplArray<Vector<double>>(v8RealArray, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(1U, doubleVector.size());
        EXPECT_EQ(doublePi, doubleVector[0]);

        Vector<float> floatVector = toImplArray<Vector<float>>(v8RealArray, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(1U, floatVector.size());
        EXPECT_EQ(floatPi, floatVector[0]);
    }
    {
        v8::Local<v8::Array> v8Array = v8::Array::New(scope.isolate(), 3);
        EXPECT_TRUE(v8CallBoolean(v8Array->Set(scope.context(), toV8(&scope, 0), toV8(&scope, "Vini, vidi, vici."))));
        EXPECT_TRUE(v8CallBoolean(v8Array->Set(scope.context(), toV8(&scope, 1), toV8(&scope, 65535))));
        EXPECT_TRUE(v8CallBoolean(v8Array->Set(scope.context(), toV8(&scope, 2), toV8(&scope, 0.125))));

        NonThrowableExceptionState exceptionState;
        Vector<v8::Local<v8::Value>> v8HandleVector = toImplArray<Vector<v8::Local<v8::Value>>>(v8Array, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(3U, v8HandleVector.size());
        EXPECT_EQ("Vini, vidi, vici.", toUSVString(scope.isolate(), v8HandleVector[0], exceptionState));
        EXPECT_EQ(65535U, toUInt32(scope.isolate(), v8HandleVector[1], NormalConversion, exceptionState));

        Vector<ScriptValue> scriptValueVector = toImplArray<Vector<ScriptValue>>(v8Array, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(3U, scriptValueVector.size());
        String reportOnZela;
        EXPECT_TRUE(scriptValueVector[0].toString(reportOnZela));
        EXPECT_EQ("Vini, vidi, vici.", reportOnZela);
        EXPECT_EQ(65535U, toUInt32(scope.isolate(), scriptValueVector[1].v8Value(), NormalConversion, exceptionState));
    }
    {
        v8::Local<v8::Array> v8StringArray1 = v8::Array::New(scope.isolate(), 2);
        EXPECT_TRUE(v8CallBoolean(v8StringArray1->Set(scope.context(), toV8(&scope, 0), toV8(&scope, "foo"))));
        EXPECT_TRUE(v8CallBoolean(v8StringArray1->Set(scope.context(), toV8(&scope, 1), toV8(&scope, "bar"))));
        v8::Local<v8::Array> v8StringArray2 = v8::Array::New(scope.isolate(), 3);
        EXPECT_TRUE(v8CallBoolean(v8StringArray2->Set(scope.context(), toV8(&scope, 0), toV8(&scope, "x"))));
        EXPECT_TRUE(v8CallBoolean(v8StringArray2->Set(scope.context(), toV8(&scope, 1), toV8(&scope, "y"))));
        EXPECT_TRUE(v8CallBoolean(v8StringArray2->Set(scope.context(), toV8(&scope, 2), toV8(&scope, "z"))));
        v8::Local<v8::Array> v8StringArrayArray = v8::Array::New(scope.isolate(), 2);
        EXPECT_TRUE(v8CallBoolean(v8StringArrayArray->Set(scope.context(), toV8(&scope, 0), v8StringArray1)));
        EXPECT_TRUE(v8CallBoolean(v8StringArrayArray->Set(scope.context(), toV8(&scope, 1), v8StringArray2)));

        NonThrowableExceptionState exceptionState;
        Vector<Vector<String>> stringVectorVector = toImplArray<Vector<Vector<String>>>(v8StringArrayArray, 0, scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVectorVector.size());
        EXPECT_EQ(2U, stringVectorVector[0].size());
        EXPECT_EQ("foo", stringVectorVector[0][0]);
        EXPECT_EQ("bar", stringVectorVector[0][1]);
        EXPECT_EQ(3U, stringVectorVector[1].size());
        EXPECT_EQ("x", stringVectorVector[1][0]);
        EXPECT_EQ("y", stringVectorVector[1][1]);
        EXPECT_EQ("z", stringVectorVector[1][2]);
    }
}

} // namespace

} // namespace blink
