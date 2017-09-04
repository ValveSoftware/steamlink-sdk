/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "bindings/core/v8/Dictionary.h"

#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8StringResource.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

Dictionary& Dictionary::operator=(const Dictionary& optionsObject) {
  m_options = optionsObject.m_options;
  m_isolate = optionsObject.m_isolate;
  return *this;
}

bool Dictionary::isObject() const {
  return !isUndefinedOrNull() && m_options->IsObject();
}

bool Dictionary::isUndefinedOrNull() const {
  if (m_options.IsEmpty())
    return true;
  return blink::isUndefinedOrNull(m_options);
}

bool Dictionary::hasProperty(const StringView& key) const {
  v8::Local<v8::Object> object;
  if (!toObject(object))
    return false;

  DCHECK(m_isolate);
  DCHECK_EQ(m_isolate, v8::Isolate::GetCurrent());
  return v8CallBoolean(object->Has(v8Context(), v8String(m_isolate, key)));
}

bool Dictionary::get(const StringView& key, v8::Local<v8::Value>& value) const {
  if (!m_isolate)
    return false;
  return getInternal(v8String(m_isolate, key), value);
}

DictionaryIterator Dictionary::getIterator(
    ExecutionContext* executionContext) const {
  v8::Local<v8::Value> iteratorGetter;
  if (!getInternal(v8::Symbol::GetIterator(m_isolate), iteratorGetter) ||
      !iteratorGetter->IsFunction())
    return nullptr;
  v8::Local<v8::Value> iterator;
  if (!v8Call(V8ScriptRunner::callFunction(
                  v8::Local<v8::Function>::Cast(iteratorGetter),
                  executionContext, m_options, 0, nullptr, m_isolate),
              iterator))
    return nullptr;
  if (!iterator->IsObject())
    return nullptr;
  return DictionaryIterator(v8::Local<v8::Object>::Cast(iterator), m_isolate);
}

bool Dictionary::get(const StringView& key, Dictionary& value) const {
  v8::Local<v8::Value> v8Value;
  if (!get(key, v8Value))
    return false;

  if (v8Value->IsObject()) {
    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    value = Dictionary(m_isolate, v8Value);
  }

  return true;
}

bool Dictionary::getInternal(const v8::Local<v8::Value>& key,
                             v8::Local<v8::Value>& result) const {
  v8::Local<v8::Object> object;
  if (!toObject(object))
    return false;

  DCHECK(m_isolate);
  DCHECK_EQ(m_isolate, v8::Isolate::GetCurrent());
  if (!v8CallBoolean(object->Has(v8Context(), key)))
    return false;

  // Swallow a possible exception in v8::Object::Get().
  // TODO(bashi,yukishiino): Should rethrow the exception.
  v8::TryCatch tryCatch(isolate());
  return object->Get(v8Context(), key).ToLocal(&result);
}

static inline bool propertyKey(v8::Local<v8::Context> v8Context,
                               v8::Local<v8::Array> properties,
                               uint32_t index,
                               v8::Local<v8::String>& key) {
  v8::Local<v8::Value> property;
  if (!properties->Get(v8Context, index).ToLocal(&property))
    return false;
  return property->ToString(v8Context).ToLocal(&key);
}

bool Dictionary::getOwnPropertiesAsStringHashMap(
    HashMap<String, String>& hashMap) const {
  v8::Local<v8::Object> object;
  if (!toObject(object))
    return false;

  v8::Local<v8::Array> properties;
  if (!object->GetOwnPropertyNames(v8Context()).ToLocal(&properties))
    return false;
  // Swallow a possible exception in v8::Object::Get().
  // TODO(bashi,yukishiino): Should rethrow the exception.
  // Note that propertyKey() may throw an exception.
  v8::TryCatch tryCatch(isolate());
  for (uint32_t i = 0; i < properties->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!propertyKey(v8Context(), properties, i, key))
      continue;
    if (!v8CallBoolean(object->Has(v8Context(), key)))
      continue;

    v8::Local<v8::Value> value;
    if (!object->Get(v8Context(), key).ToLocal(&value)) {
      tryCatch.Reset();
      continue;
    }
    TOSTRING_DEFAULT(V8StringResource<>, stringKey, key, false);
    TOSTRING_DEFAULT(V8StringResource<>, stringValue, value, false);
    if (!static_cast<const String&>(stringKey).isEmpty())
      hashMap.set(stringKey, stringValue);
  }

  return true;
}

bool Dictionary::getPropertyNames(Vector<String>& names) const {
  v8::Local<v8::Object> object;
  if (!toObject(object))
    return false;

  v8::Local<v8::Array> properties;
  if (!object->GetPropertyNames(v8Context()).ToLocal(&properties))
    return false;
  for (uint32_t i = 0; i < properties->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!propertyKey(v8Context(), properties, i, key))
      continue;
    if (!v8CallBoolean(object->Has(v8Context(), key)))
      continue;
    TOSTRING_DEFAULT(V8StringResource<>, stringKey, key, false);
    names.append(stringKey);
  }

  return true;
}

bool Dictionary::toObject(v8::Local<v8::Object>& object) const {
  return !isUndefinedOrNull() &&
         m_options->ToObject(v8Context()).ToLocal(&object);
}

}  // namespace blink
