/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/modules/v8/V8BindingForModules.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace blink;

namespace {

IDBKey* checkKeyFromValueAndKeyPathInternal(v8::Isolate* isolate, const ScriptValue& value, const String& keyPath)
{
    IDBKeyPath idbKeyPath(keyPath);
    EXPECT_TRUE(idbKeyPath.isValid());

    NonThrowableExceptionState exceptionState;
    return ScriptValue::to<IDBKey*>(isolate, value, exceptionState, idbKeyPath);
}

void checkKeyPathNullValue(v8::Isolate* isolate, const ScriptValue& value, const String& keyPath)
{
    ASSERT_FALSE(checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath));
}

bool injectKey(ScriptState* scriptState, IDBKey* key, ScriptValue& value, const String& keyPath)
{
    IDBKeyPath idbKeyPath(keyPath);
    EXPECT_TRUE(idbKeyPath.isValid());
    ScriptValue keyValue = ScriptValue::from(scriptState, key);
    return injectV8KeyIntoV8Value(scriptState->isolate(), keyValue.v8Value(), value.v8Value(), idbKeyPath);
}

void checkInjection(ScriptState* scriptState, IDBKey* key, ScriptValue& value, const String& keyPath)
{
    bool result = injectKey(scriptState, key, value, keyPath);
    ASSERT_TRUE(result);
    IDBKey* extractedKey = checkKeyFromValueAndKeyPathInternal(scriptState->isolate(), value, keyPath);
    EXPECT_TRUE(key->isEqual(extractedKey));
}

void checkInjectionIgnored(ScriptState* scriptState, IDBKey* key, ScriptValue& value, const String& keyPath)
{
    bool result = injectKey(scriptState, key, value, keyPath);
    ASSERT_TRUE(result);
    IDBKey* extractedKey = checkKeyFromValueAndKeyPathInternal(scriptState->isolate(), value, keyPath);
    EXPECT_FALSE(key->isEqual(extractedKey));
}

void checkInjectionDisallowed(ScriptState* scriptState, ScriptValue& value, const String& keyPath)
{
    const IDBKeyPath idbKeyPath(keyPath);
    ASSERT_TRUE(idbKeyPath.isValid());
    EXPECT_FALSE(canInjectIDBKeyIntoScriptValue(scriptState->isolate(), value, idbKeyPath));
}

void checkKeyPathStringValue(v8::Isolate* isolate, const ScriptValue& value, const String& keyPath, const String& expected)
{
    IDBKey* idbKey = checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath);
    ASSERT_TRUE(idbKey);
    ASSERT_EQ(IDBKey::StringType, idbKey->getType());
    ASSERT_TRUE(expected == idbKey->string());
}

void checkKeyPathNumberValue(v8::Isolate* isolate, const ScriptValue& value, const String& keyPath, int expected)
{
    IDBKey* idbKey = checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath);
    ASSERT_TRUE(idbKey);
    ASSERT_EQ(IDBKey::NumberType, idbKey->getType());
    ASSERT_TRUE(expected == idbKey->number());
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();

    // object = { foo: "zoo" }
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    ASSERT_TRUE(v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"), v8AtomicString(isolate, "zoo"))));

    ScriptValue scriptValue(scope.getScriptState(), object);

    checkKeyPathStringValue(isolate, scriptValue, "foo", "zoo");
    checkKeyPathNullValue(isolate, scriptValue, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();

    // object = { foo: 456 }
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    ASSERT_TRUE(v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"), v8::Number::New(isolate, 456))));

    ScriptValue scriptValue(scope.getScriptState(), object);

    checkKeyPathNumberValue(isolate, scriptValue, "foo", 456);
    checkKeyPathNullValue(isolate, scriptValue, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();

    // object = { foo: { bar: "zee" } }
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
    ASSERT_TRUE(v8CallBoolean(subProperty->Set(scope.context(), v8AtomicString(isolate, "bar"), v8AtomicString(isolate, "zee"))));
    ASSERT_TRUE(v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"), subProperty)));

    ScriptValue scriptValue(scope.getScriptState(), object);

    checkKeyPathStringValue(isolate, scriptValue, "foo.bar", "zee");
    checkKeyPathNullValue(isolate, scriptValue, "bar");
}

TEST(InjectIDBKeyTest, ImplicitValues)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();
    {
        v8::Local<v8::String> string = v8String(isolate, "string");
        ScriptValue value = ScriptValue(scope.getScriptState(), string);
        checkInjectionIgnored(scope.getScriptState(), IDBKey::createNumber(123), value, "length");
    }
    {
        v8::Local<v8::Array> array = v8::Array::New(isolate);
        ScriptValue value = ScriptValue(scope.getScriptState(), array);
        checkInjectionIgnored(scope.getScriptState(), IDBKey::createNumber(456), value, "length");
    }
}

TEST(InjectIDBKeyTest, TopLevelPropertyStringValue)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();

    // object = { foo: "zoo" }
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    ASSERT_TRUE(v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"), v8AtomicString(isolate, "zoo"))));

    ScriptValue scriptObject(scope.getScriptState(), object);
    checkInjection(scope.getScriptState(), IDBKey::createString("myNewKey"), scriptObject, "bar");
    checkInjection(scope.getScriptState(), IDBKey::createNumber(1234), scriptObject, "bar");

    checkInjectionDisallowed(scope.getScriptState(), scriptObject, "foo.bar");
}

TEST(InjectIDBKeyTest, SubProperty)
{
    V8TestingScope scope;
    v8::Isolate* isolate = scope.isolate();

    // object = { foo: { bar: "zee" } }
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
    ASSERT_TRUE(v8CallBoolean(subProperty->Set(scope.context(), v8AtomicString(isolate, "bar"), v8AtomicString(isolate, "zee"))));
    ASSERT_TRUE(v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"), subProperty)));

    ScriptValue scriptObject(scope.getScriptState(), object);
    checkInjection(scope.getScriptState(), IDBKey::createString("myNewKey"), scriptObject, "foo.baz");
    checkInjection(scope.getScriptState(), IDBKey::createNumber(789), scriptObject, "foo.baz");
    checkInjection(scope.getScriptState(), IDBKey::createDate(4567), scriptObject, "foo.baz");
    checkInjection(scope.getScriptState(), IDBKey::createDate(4567), scriptObject, "bar");
    checkInjection(scope.getScriptState(), IDBKey::createArray(IDBKey::KeyArray()), scriptObject, "foo.baz");
    checkInjection(scope.getScriptState(), IDBKey::createArray(IDBKey::KeyArray()), scriptObject, "bar");

    checkInjectionDisallowed(scope.getScriptState(), scriptObject, "foo.bar.baz");
    checkInjection(scope.getScriptState(), IDBKey::createString("zoo"), scriptObject, "foo.xyz.foo");
}

} // namespace
