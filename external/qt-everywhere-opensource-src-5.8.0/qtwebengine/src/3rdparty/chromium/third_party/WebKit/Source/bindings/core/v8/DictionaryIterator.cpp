// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DictionaryIterator.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

DictionaryIterator::DictionaryIterator(v8::Local<v8::Object> iterator, v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_iterator(iterator)
    , m_nextKey(v8String(isolate, "next"))
    , m_doneKey(v8String(isolate, "done"))
    , m_valueKey(v8String(isolate, "value"))
    , m_done(false)
{
    ASSERT(!iterator.IsEmpty());
}

bool DictionaryIterator::next(ExecutionContext* executionContext, ExceptionState& exceptionState)
{
    ASSERT(!isNull());

    v8::Local<v8::Value> next;
    // TODO(alancutter): Support callable objects as well as functions.
    if (!v8Call(m_iterator->Get(m_isolate->GetCurrentContext(), m_nextKey), next) || !next->IsFunction()) {
        exceptionState.throwTypeError("Expected next() function on iterator.");
        m_done = true;
        return false;
    }

    v8::Local<v8::Value> result;
    if (!v8Call(ScriptController::callFunction(executionContext, v8::Local<v8::Function>::Cast(next), m_iterator, 0, nullptr, m_isolate), result) || !result->IsObject()) {
        exceptionState.throwTypeError("Expected iterator.next() to return an Object.");
        m_done = true;
        return false;
    }
    v8::Local<v8::Object> resultObject = v8::Local<v8::Object>::Cast(result);

    m_value = resultObject->Get(m_isolate->GetCurrentContext(), m_valueKey);

    v8::Local<v8::Value> done;
    if (v8Call(resultObject->Get(m_isolate->GetCurrentContext(), m_doneKey), done)) {
        v8::Local<v8::Boolean> doneBoolean;
        m_done = v8Call(done->ToBoolean(m_isolate->GetCurrentContext()), doneBoolean) ? doneBoolean->Value() : false;
    } else {
        m_done = false;
    }
    return !m_done;
}

bool DictionaryIterator::valueAsDictionary(Dictionary& result, ExceptionState& exceptionState)
{
    ASSERT(!isNull());
    ASSERT(!m_done);

    v8::Local<v8::Value> value;
    if (!v8Call(m_value, value) || !value->IsObject())
        return false;

    result = Dictionary(value, m_isolate, exceptionState);
    return true;
}

} // namespace blink

