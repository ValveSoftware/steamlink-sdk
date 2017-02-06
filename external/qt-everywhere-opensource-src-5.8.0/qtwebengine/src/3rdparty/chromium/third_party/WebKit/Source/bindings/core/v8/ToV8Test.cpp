// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ToV8.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <limits>

#define TEST_TOV8(expected, value) testToV8(&scope, expected, value, __FILE__, __LINE__)

namespace blink {

namespace {

template<typename T>
void testToV8(V8TestingScope* scope, const char* expected, T value, const char* path, int lineNumber)
{
    v8::Local<v8::Value> actual = toV8(value, scope->context()->Global(), scope->isolate());
    if (actual.IsEmpty()) {
        ADD_FAILURE_AT(path, lineNumber) << "toV8 returns an empty value.";
        return;
    }
    String actualString = toCoreString(actual->ToString(scope->context()).ToLocalChecked());
    if (String(expected) != actualString) {
        ADD_FAILURE_AT(path, lineNumber) << "toV8 returns an incorrect value.\n  Actual: " << actualString.utf8().data() << "\nExpected: " << expected;
        return;
    }
}

class GarbageCollectedHolder : public GarbageCollected<GarbageCollectedHolder> {
public:
    GarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    DEFINE_INLINE_TRACE() { visitor->trace(m_scriptWrappable); }

    // This should be public in order to access a Member<X> object.
    Member<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

class OffHeapGarbageCollectedHolder {
public:
    OffHeapGarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    // This should be public in order to access a Persistent<X> object.
    Persistent<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

TEST(ToV8Test, garbageCollectedScriptWrappable)
{
    V8TestingScope scope;
    GarbageCollectedScriptWrappable* object = new GarbageCollectedScriptWrappable("world");
    GarbageCollectedHolder holder(object);
    OffHeapGarbageCollectedHolder offHeapHolder(object);

    TEST_TOV8("world", object);
    TEST_TOV8("world", holder.m_scriptWrappable);
    TEST_TOV8("world", offHeapHolder.m_scriptWrappable);

    object = nullptr;
    holder.m_scriptWrappable = nullptr;
    offHeapHolder.m_scriptWrappable = nullptr;

    TEST_TOV8("null", object);
    TEST_TOV8("null", holder.m_scriptWrappable);
    TEST_TOV8("null", offHeapHolder.m_scriptWrappable);
}

TEST(ToV8Test, string)
{
    V8TestingScope scope;
    char arrayString[] = "arrayString";
    const char constArrayString[] = "constArrayString";
    TEST_TOV8("arrayString", arrayString);
    TEST_TOV8("constArrayString", constArrayString);
    TEST_TOV8("pointer", const_cast<char*>("pointer"));
    TEST_TOV8("constPointer", static_cast<const char*>("constPointer"));
    TEST_TOV8("coreString", String("coreString"));
    TEST_TOV8("atomicString", AtomicString("atomicString"));

    // Null strings are converted to empty strings.
    TEST_TOV8("", static_cast<char*>(nullptr));
    TEST_TOV8("", static_cast<const char*>(nullptr));
    TEST_TOV8("", String());
    TEST_TOV8("", AtomicString());
}

TEST(ToV8Test, numeric)
{
    V8TestingScope scope;
    TEST_TOV8("0", static_cast<int>(0));
    TEST_TOV8("1", static_cast<int>(1));
    TEST_TOV8("-1", static_cast<int>(-1));

    TEST_TOV8("-2", static_cast<long>(-2));
    TEST_TOV8("2", static_cast<unsigned>(2));
    TEST_TOV8("2", static_cast<unsigned long>(2));

    TEST_TOV8("-2147483648", std::numeric_limits<int32_t>::min());
    TEST_TOV8("2147483647", std::numeric_limits<int32_t>::max());
    TEST_TOV8("4294967295", std::numeric_limits<uint32_t>::max());
    // v8::Number can represent exact numbers in [-(2^53-1), 2^53-1].
    TEST_TOV8("-9007199254740991", -9007199254740991); // -(2^53-1)
    TEST_TOV8("9007199254740991", 9007199254740991); // 2^53-1

    TEST_TOV8("0.5", static_cast<double>(0.5));
    TEST_TOV8("-0.5", static_cast<float>(-0.5));
    TEST_TOV8("NaN", std::numeric_limits<double>::quiet_NaN());
    TEST_TOV8("Infinity", std::numeric_limits<double>::infinity());
    TEST_TOV8("-Infinity", -std::numeric_limits<double>::infinity());
}

TEST(ToV8Test, boolean)
{
    V8TestingScope scope;
    TEST_TOV8("true", true);
    TEST_TOV8("false", false);
}

TEST(ToV8Test, v8Value)
{
    V8TestingScope scope;
    v8::Local<v8::Value> localValue(v8::Number::New(scope.isolate(), 1234));
    v8::Local<v8::Value> handleValue(v8::Number::New(scope.isolate(), 5678));

    TEST_TOV8("1234", localValue);
    TEST_TOV8("5678", handleValue);
}

TEST(ToV8Test, undefinedType)
{
    V8TestingScope scope;
    TEST_TOV8("undefined", ToV8UndefinedGenerator());
}

TEST(ToV8Test, scriptValue)
{
    V8TestingScope scope;
    ScriptValue value(scope.getScriptState(), v8::Number::New(scope.isolate(), 1234));

    TEST_TOV8("1234", value);
}

TEST(ToV8Test, stringVectors)
{
    V8TestingScope scope;
    Vector<String> stringVector;
    stringVector.append("foo");
    stringVector.append("bar");
    TEST_TOV8("foo,bar", stringVector);

    Vector<AtomicString> atomicStringVector;
    atomicStringVector.append("quux");
    atomicStringVector.append("bar");
    TEST_TOV8("quux,bar", atomicStringVector);
}

TEST(ToV8Test, basicTypeVectors)
{
    V8TestingScope scope;
    Vector<int> intVector;
    intVector.append(42);
    intVector.append(23);
    TEST_TOV8("42,23", intVector);

    Vector<long> longVector;
    longVector.append(31773);
    longVector.append(404);
    TEST_TOV8("31773,404", longVector);

    Vector<unsigned> unsignedVector;
    unsignedVector.append(1);
    unsignedVector.append(2);
    TEST_TOV8("1,2", unsignedVector);

    Vector<unsigned long> unsignedLongVector;
    unsignedLongVector.append(1001);
    unsignedLongVector.append(2002);
    TEST_TOV8("1001,2002", unsignedLongVector);

    Vector<float> floatVector;
    floatVector.append(0.125);
    floatVector.append(1.);
    TEST_TOV8("0.125,1", floatVector);

    Vector<double> doubleVector;
    doubleVector.append(2.3);
    doubleVector.append(4.2);
    TEST_TOV8("2.3,4.2", doubleVector);

    Vector<bool> boolVector;
    boolVector.append(true);
    boolVector.append(true);
    boolVector.append(false);
    TEST_TOV8("true,true,false", boolVector);
}

TEST(ToV8Test, dictionaryVector)
{
    V8TestingScope scope;
    Vector<std::pair<String, int>> dictionary;
    dictionary.append(std::make_pair("one", 1));
    dictionary.append(std::make_pair("two", 2));
    TEST_TOV8("[object Object]", dictionary);
    v8::Local<v8::Context> context = scope.getScriptState()->context();
    v8::Local<v8::Object> result = toV8(dictionary, context->Global(), scope.isolate())->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> one = result->Get(context, v8String(scope.isolate(), "one")).ToLocalChecked();
    EXPECT_EQ(1, one->NumberValue(context).FromJust());
    v8::Local<v8::Value> two = result->Get(context, v8String(scope.isolate(), "two")).ToLocalChecked();
    EXPECT_EQ(2, two->NumberValue(context).FromJust());
}

TEST(ToV8Test, heapVector)
{
    V8TestingScope scope;
    HeapVector<Member<GarbageCollectedScriptWrappable>> v;
    v.append(new GarbageCollectedScriptWrappable("hoge"));
    v.append(new GarbageCollectedScriptWrappable("fuga"));
    v.append(nullptr);

    TEST_TOV8("hoge,fuga,", v);
}

TEST(ToV8Test, withScriptState)
{
    V8TestingScope scope;
    ScriptValue value(scope.getScriptState(), v8::Number::New(scope.isolate(), 1234.0));

    v8::Local<v8::Value> actual = toV8(value, scope.getScriptState());
    EXPECT_FALSE(actual.IsEmpty());

    double actualAsNumber = actual.As<v8::Number>()->Value();
    EXPECT_EQ(1234.0, actualAsNumber);
}

} // namespace

} // namespace blink
