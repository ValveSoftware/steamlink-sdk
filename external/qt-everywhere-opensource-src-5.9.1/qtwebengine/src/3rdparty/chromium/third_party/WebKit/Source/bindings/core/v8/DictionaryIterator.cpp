// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DictionaryIterator.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8StringResource.h"
#include "wtf/text/WTFString.h"

namespace blink {

DictionaryIterator::DictionaryIterator(v8::Local<v8::Object> iterator,
                                       v8::Isolate* isolate)
    : m_isolate(isolate),
      m_iterator(iterator),
      m_nextKey(v8String(isolate, "next")),
      m_doneKey(v8String(isolate, "done")),
      m_valueKey(v8String(isolate, "value")),
      m_done(false) {
  DCHECK(!iterator.IsEmpty());
}

bool DictionaryIterator::next(ExecutionContext* executionContext,
                              ExceptionState& exceptionState) {
  DCHECK(!isNull());

  v8::TryCatch tryCatch(m_isolate);
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();

  v8::Local<v8::Value> next;
  if (!m_iterator->Get(context, m_nextKey).ToLocal(&next)) {
    CHECK(!tryCatch.Exception().IsEmpty());
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    m_done = true;
    return false;
  }
  if (!next->IsFunction()) {
    exceptionState.throwTypeError("Expected next() function on iterator.");
    m_done = true;
    return false;
  }

  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::callFunction(v8::Local<v8::Function>::Cast(next),
                                    executionContext, m_iterator, 0, nullptr,
                                    m_isolate)
           .ToLocal(&result)) {
    CHECK(!tryCatch.Exception().IsEmpty());
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    m_done = true;
    return false;
  }
  if (!result->IsObject()) {
    exceptionState.throwTypeError(
        "Expected iterator.next() to return an Object.");
    m_done = true;
    return false;
  }
  v8::Local<v8::Object> resultObject = v8::Local<v8::Object>::Cast(result);

  m_value = resultObject->Get(context, m_valueKey);
  if (m_value.IsEmpty()) {
    CHECK(!tryCatch.Exception().IsEmpty());
    exceptionState.rethrowV8Exception(tryCatch.Exception());
  }

  v8::Local<v8::Value> done;
  v8::Local<v8::Boolean> doneBoolean;
  if (!resultObject->Get(context, m_doneKey).ToLocal(&done) ||
      !done->ToBoolean(context).ToLocal(&doneBoolean)) {
    CHECK(!tryCatch.Exception().IsEmpty());
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    m_done = true;
    return false;
  }

  m_done = doneBoolean->Value();
  return !m_done;
}

bool DictionaryIterator::valueAsDictionary(Dictionary& result,
                                           ExceptionState& exceptionState) {
  DCHECK(!isNull());
  DCHECK(!m_done);

  v8::Local<v8::Value> value;
  if (!v8Call(m_value, value) || !value->IsObject())
    return false;

  result = Dictionary(value, m_isolate, exceptionState);
  return true;
}

bool DictionaryIterator::valueAsString(String& result) {
  DCHECK(!isNull());
  DCHECK(!m_done);

  v8::Local<v8::Value> value;
  if (!v8Call(m_value, value))
    return false;

  V8StringResource<> stringValue(value);
  if (!stringValue.prepare())
    return false;
  result = stringValue;
  return true;
}

}  // namespace blink
