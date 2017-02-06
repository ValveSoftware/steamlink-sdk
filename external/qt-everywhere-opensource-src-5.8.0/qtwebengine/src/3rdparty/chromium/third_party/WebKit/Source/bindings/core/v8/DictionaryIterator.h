// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryIterator_h
#define DictionaryIterator_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include <v8.h>

namespace blink {

class Dictionary;
class ExceptionState;
class ExecutionContext;

class CORE_EXPORT DictionaryIterator {
    STACK_ALLOCATED();
public:
    DictionaryIterator(std::nullptr_t)
        : m_isolate(nullptr)
        , m_done(true)
    { }

    DictionaryIterator(v8::Local<v8::Object> iterator, v8::Isolate*);

    bool isNull() const { return m_iterator.IsEmpty(); }

    // Returns true if the iterator is still not done.
    bool next(ExecutionContext*, ExceptionState&);

    v8::MaybeLocal<v8::Value> value() { return m_value; }
    bool valueAsDictionary(Dictionary& result, ExceptionState&);

private:
    v8::Isolate* m_isolate;
    v8::Local<v8::Object> m_iterator;
    v8::Local<v8::String> m_nextKey;
    v8::Local<v8::String> m_doneKey;
    v8::Local<v8::String> m_valueKey;
    bool m_done;
    v8::MaybeLocal<v8::Value> m_value;
};

} // namespace blink

#endif // DictionaryIterator_h
