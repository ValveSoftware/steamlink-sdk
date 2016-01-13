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

#include "config.h"
#include "bindings/v8/IDBBindingUtilities.h"

#include "bindings/core/v8/V8DOMStringList.h"
#include "bindings/modules/v8/V8IDBCursor.h"
#include "bindings/modules/v8/V8IDBCursorWithValue.h"
#include "bindings/modules/v8/V8IDBDatabase.h"
#include "bindings/modules/v8/V8IDBIndex.h"
#include "bindings/modules/v8/V8IDBKeyRange.h"
#include "bindings/modules/v8/V8IDBObjectStore.h"
#include "bindings/modules/v8/V8IDBRequest.h"
#include "bindings/modules/v8/V8IDBTransaction.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBTracing.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"

namespace WebCore {

static v8::Handle<v8::Value> deserializeIDBValueBuffer(v8::Isolate*, SharedBuffer*, const Vector<blink::WebBlobInfo>*);

static v8::Handle<v8::Value> toV8(const IDBKeyPath& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (value.type()) {
    case IDBKeyPath::NullType:
        return v8::Null(isolate);
    case IDBKeyPath::StringType:
        return v8String(isolate, value.string());
    case IDBKeyPath::ArrayType:
        RefPtrWillBeRawPtr<DOMStringList> keyPaths = DOMStringList::create();
        for (Vector<String>::const_iterator it = value.array().begin(); it != value.array().end(); ++it)
            keyPaths->append(*it);
        return toV8(keyPaths.release(), creationContext, isolate);
    }
    ASSERT_NOT_REACHED();
    return v8::Undefined(isolate);
}

static v8::Handle<v8::Value> toV8(const IDBKey* key, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!key) {
        // This should be undefined, not null.
        // Spec: http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBKeyRange
        return v8Undefined();
    }

    switch (key->type()) {
    case IDBKey::InvalidType:
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
        return v8Undefined();
    case IDBKey::NumberType:
        return v8::Number::New(isolate, key->number());
    case IDBKey::StringType:
        return v8String(isolate, key->string());
    case IDBKey::BinaryType:
        return toV8(Uint8Array::create(reinterpret_cast<const unsigned char*>(key->binary()->data()), key->binary()->size()), creationContext, isolate);
    case IDBKey::DateType:
        return v8::Date::New(isolate, key->date());
    case IDBKey::ArrayType:
        {
            v8::Local<v8::Array> array = v8::Array::New(isolate, key->array().size());
            for (size_t i = 0; i < key->array().size(); ++i)
                array->Set(i, toV8(key->array()[i].get(), creationContext, isolate));
            return array;
        }
    }

    ASSERT_NOT_REACHED();
    return v8Undefined();
}

static v8::Handle<v8::Value> toV8(const IDBAny* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Null(isolate);

    switch (impl->type()) {
    case IDBAny::UndefinedType:
        return v8::Undefined(isolate);
    case IDBAny::NullType:
        return v8::Null(isolate);
    case IDBAny::DOMStringListType:
        return toV8(impl->domStringList(), creationContext, isolate);
    case IDBAny::IDBCursorType: {
        // Ensure request wrapper is kept alive at least as long as the cursor wrapper,
        // so that event listeners are retained.
        v8::Handle<v8::Value> cursor = toV8(impl->idbCursor(), creationContext, isolate);
        v8::Handle<v8::Value> request = toV8(impl->idbCursor()->request(), creationContext, isolate);
        V8HiddenValue::setHiddenValue(isolate, cursor->ToObject(), V8HiddenValue::idbCursorRequest(isolate), request);
        return cursor;
    }
    case IDBAny::IDBCursorWithValueType: {
        // Ensure request wrapper is kept alive at least as long as the cursor wrapper,
        // so that event listeners are retained.
        v8::Handle<v8::Value> cursor = toV8(impl->idbCursorWithValue(), creationContext, isolate);
        v8::Handle<v8::Value> request = toV8(impl->idbCursorWithValue()->request(), creationContext, isolate);
        V8HiddenValue::setHiddenValue(isolate, cursor->ToObject(), V8HiddenValue::idbCursorRequest(isolate), request);
        return cursor;
    }
    case IDBAny::IDBDatabaseType:
        return toV8(impl->idbDatabase(), creationContext, isolate);
    case IDBAny::IDBIndexType:
        return toV8(impl->idbIndex(), creationContext, isolate);
    case IDBAny::IDBObjectStoreType:
        return toV8(impl->idbObjectStore(), creationContext, isolate);
    case IDBAny::IDBTransactionType:
        return toV8(impl->idbTransaction(), creationContext, isolate);
    case IDBAny::BufferType:
        return deserializeIDBValueBuffer(isolate, impl->buffer(), impl->blobInfo());
    case IDBAny::StringType:
        return v8String(isolate, impl->string());
    case IDBAny::IntegerType:
        return v8::Number::New(isolate, impl->integer());
    case IDBAny::KeyType:
        return toV8(impl->key(), creationContext, isolate);
    case IDBAny::KeyPathType:
        return toV8(impl->keyPath(), creationContext, isolate);
    case IDBAny::BufferKeyAndKeyPathType: {
        v8::Handle<v8::Value> value = deserializeIDBValueBuffer(isolate, impl->buffer(), impl->blobInfo());
        v8::Handle<v8::Value> key = toV8(impl->key(), creationContext, isolate);
        bool injected = injectV8KeyIntoV8Value(isolate, key, value, impl->keyPath());
        ASSERT_UNUSED(injected, injected);
        return value;
    }
    }

    ASSERT_NOT_REACHED();
    return v8::Undefined(isolate);
}

static const size_t maximumDepth = 2000;

static IDBKey* createIDBKeyFromValue(v8::Isolate* isolate, v8::Handle<v8::Value> value, Vector<v8::Handle<v8::Array> >& stack, bool allowExperimentalTypes = false)
{
    if (value->IsNumber() && !std::isnan(value->NumberValue()))
        return IDBKey::createNumber(value->NumberValue());
    if (value->IsString())
        return IDBKey::createString(toCoreString(value.As<v8::String>()));
    if (value->IsDate() && !std::isnan(value->NumberValue()))
        return IDBKey::createDate(value->NumberValue());
    if (value->IsUint8Array() && (allowExperimentalTypes || RuntimeEnabledFeatures::indexedDBExperimentalEnabled())) {
        // Per discussion in https://www.w3.org/Bugs/Public/show_bug.cgi?id=23332 the
        // input type is constrained to Uint8Array to match the output type.
        ArrayBufferView* view = WebCore::V8ArrayBufferView::toNative(value->ToObject());
        const char* start = static_cast<const char*>(view->baseAddress());
        size_t length = view->byteLength();
        return IDBKey::createBinary(SharedBuffer::create(start, length));
    }
    if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);

        if (stack.contains(array))
            return 0;
        if (stack.size() >= maximumDepth)
            return 0;
        stack.append(array);

        IDBKey::KeyArray subkeys;
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; ++i) {
            v8::Local<v8::Value> item = array->Get(v8::Int32::New(isolate, i));
            IDBKey* subkey = createIDBKeyFromValue(isolate, item, stack, allowExperimentalTypes);
            if (!subkey)
                subkeys.append(IDBKey::createInvalid());
            else
                subkeys.append(subkey);
        }

        stack.removeLast();
        return IDBKey::createArray(subkeys);
    }
    return 0;
}

static IDBKey* createIDBKeyFromValue(v8::Isolate* isolate, v8::Handle<v8::Value> value, bool allowExperimentalTypes = false)
{
    Vector<v8::Handle<v8::Array> > stack;
    if (IDBKey* key = createIDBKeyFromValue(isolate, value, stack, allowExperimentalTypes))
        return key;
    return IDBKey::createInvalid();
}

template<typename T>
static bool getValueFrom(T indexOrName, v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Value->ToObject();
    if (!object->Has(indexOrName))
        return false;
    v8Value = object->Get(indexOrName);
    return true;
}

template<typename T>
static bool setValue(v8::Handle<v8::Value>& v8Object, T indexOrName, const v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Object->ToObject();
    return object->Set(indexOrName, v8Value);
}

static bool get(v8::Isolate* isolate, v8::Handle<v8::Value>& object, const String& keyPathElement, v8::Handle<v8::Value>& result)
{
    if (object->IsString() && keyPathElement == "length") {
        int32_t length = v8::Handle<v8::String>::Cast(object)->Length();
        result = v8::Number::New(isolate, length);
        return true;
    }
    return object->IsObject() && getValueFrom(v8String(isolate, keyPathElement), result);
}

static bool canSet(v8::Handle<v8::Value>& object, const String& keyPathElement)
{
    return object->IsObject();
}

static bool set(v8::Isolate* isolate, v8::Handle<v8::Value>& object, const String& keyPathElement, const v8::Handle<v8::Value>& v8Value)
{
    return canSet(object, keyPathElement) && setValue(object, v8String(isolate, keyPathElement), v8Value);
}

static v8::Handle<v8::Value> getNthValueOnKeyPath(v8::Isolate* isolate, v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    v8::Handle<v8::Value> currentValue(rootValue);
    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        if (!get(isolate, parentValue, keyPathElements[i], currentValue))
            return v8Undefined();
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(v8::Isolate* isolate, v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    if (!rootValue->IsObject())
        return false;

    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(isolate, parentValue, keyPathElement, currentValue))
            return canSet(parentValue, keyPathElement);
    }
    return true;
}


static v8::Handle<v8::Value> ensureNthValueOnKeyPath(v8::Isolate* isolate, v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(isolate, parentValue, keyPathElement, currentValue)) {
            v8::Handle<v8::Object> object = v8::Object::New(isolate);
            if (!set(isolate, parentValue, keyPathElement, object))
                return v8Undefined();
            currentValue = object;
        }
    }

    return currentValue;
}

static IDBKey* createIDBKeyFromScriptValueAndKeyPathInternal(v8::Isolate* isolate, const ScriptValue& value, const String& keyPath, bool allowExperimentalTypes)
{
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);
    ASSERT(isolate->InContext());

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(value.v8Value());
    v8::Handle<v8::Value> v8Key(getNthValueOnKeyPath(isolate, v8Value, keyPathElements, keyPathElements.size()));
    if (v8Key.IsEmpty())
        return 0;
    return createIDBKeyFromValue(isolate, v8Key, allowExperimentalTypes);
}

static IDBKey* createIDBKeyFromScriptValueAndKeyPathInternal(v8::Isolate* isolate, const ScriptValue& value, const IDBKeyPath& keyPath, bool allowExperimentalTypes = false)
{
    ASSERT(!keyPath.isNull());
    v8::HandleScope handleScope(isolate);
    if (keyPath.type() == IDBKeyPath::ArrayType) {
        IDBKey::KeyArray result;
        const Vector<String>& array = keyPath.array();
        for (size_t i = 0; i < array.size(); ++i) {
            IDBKey* key = createIDBKeyFromScriptValueAndKeyPathInternal(isolate, value, array[i], allowExperimentalTypes);
            if (!key)
                return 0;
            result.append(key);
        }
        return IDBKey::createArray(result);
    }

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    return createIDBKeyFromScriptValueAndKeyPathInternal(isolate, value, keyPath.string(), allowExperimentalTypes);
}

IDBKey* createIDBKeyFromScriptValueAndKeyPath(v8::Isolate* isolate, const ScriptValue& value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("createIDBKeyFromScriptValueAndKeyPath");
    return createIDBKeyFromScriptValueAndKeyPathInternal(isolate, value, keyPath);
}

static v8::Handle<v8::Value> deserializeIDBValueBuffer(v8::Isolate* isolate, SharedBuffer* buffer, const Vector<blink::WebBlobInfo>* blobInfo)
{
    ASSERT(isolate->InContext());
    if (!buffer)
        return v8::Null(isolate);

    // FIXME: The extra copy here can be eliminated by allowing SerializedScriptValue to take a raw const char* or const uint8_t*.
    Vector<uint8_t> value;
    value.append(buffer->data(), buffer->size());
    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::createFromWireBytes(value);
    return serializedValue->deserialize(isolate, 0, blobInfo);
}

bool injectV8KeyIntoV8Value(v8::Isolate* isolate, v8::Handle<v8::Value> key, v8::Handle<v8::Value> value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("injectIDBV8KeyIntoV8Value");
    ASSERT(isolate->InContext());

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return false;

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> parent(ensureNthValueOnKeyPath(isolate, value, keyPathElements, keyPathElements.size() - 1));
    if (parent.IsEmpty())
        return false;

    if (!set(isolate, parent, keyPathElements.last(), key))
        return false;

    return true;
}

bool canInjectIDBKeyIntoScriptValue(v8::Isolate* isolate, const ScriptValue& scriptValue, const IDBKeyPath& keyPath)
{
    IDB_TRACE("canInjectIDBKeyIntoScriptValue");
    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return false;

    v8::Handle<v8::Value> v8Value(scriptValue.v8Value());
    return canInjectNthValueOnKeyPath(isolate, v8Value, keyPathElements, keyPathElements.size() - 1);
}

ScriptValue idbAnyToScriptValue(ScriptState* scriptState, IDBAny* any)
{
    v8::Isolate* isolate = scriptState->isolate();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(toV8(any, scriptState->context()->Global(), isolate));
    return ScriptValue(scriptState, v8Value);
}

ScriptValue idbKeyToScriptValue(ScriptState* scriptState, IDBKey* key)
{
    v8::Isolate* isolate = scriptState->isolate();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(toV8(key, scriptState->context()->Global(), isolate));
    return ScriptValue(scriptState, v8Value);
}

IDBKey* scriptValueToIDBKey(v8::Isolate* isolate, const ScriptValue& scriptValue)
{
    ASSERT(isolate->InContext());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(scriptValue.v8Value());
    return createIDBKeyFromValue(isolate, v8Value);
}

IDBKeyRange* scriptValueToIDBKeyRange(v8::Isolate* isolate, const ScriptValue& scriptValue)
{
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> value(scriptValue.v8Value());
    return V8IDBKeyRange::toNativeWithTypeCheck(isolate, value);
}

#ifndef NDEBUG
void assertPrimaryKeyValidOrInjectable(ScriptState* scriptState, PassRefPtr<SharedBuffer> buffer, const Vector<blink::WebBlobInfo>* blobInfo, IDBKey* key, const IDBKeyPath& keyPath)
{
    ScriptState::Scope scope(scriptState);
    v8::Isolate* isolate = scriptState->isolate();
    ScriptValue keyValue = idbKeyToScriptValue(scriptState, key);
    ScriptValue scriptValue(scriptState, deserializeIDBValueBuffer(isolate, buffer.get(), blobInfo));

    // This assertion is about already persisted data, so allow experimental types.
    const bool allowExperimentalTypes = true;
    IDBKey* expectedKey = createIDBKeyFromScriptValueAndKeyPathInternal(isolate, scriptValue, keyPath, allowExperimentalTypes);
    ASSERT(!expectedKey || expectedKey->isEqual(key));

    bool injected = injectV8KeyIntoV8Value(isolate, keyValue.v8Value(), scriptValue.v8Value(), keyPath);
    ASSERT_UNUSED(injected, injected);
}
#endif

} // namespace WebCore
