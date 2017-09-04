/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8EventListenerHelper_h
#define V8EventListenerHelper_h

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8EventListener.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include <v8.h>

namespace blink {

enum ListenerLookupType {
  ListenerFindOnly,
  ListenerFindOrCreate,
};

// This is a container for V8EventListener objects that uses hidden properties
// of v8::Object to speed up lookups.
class V8EventListenerHelper {
  STATIC_ONLY(V8EventListenerHelper);

 public:
  static V8EventListener* existingEventListener(v8::Local<v8::Value> value,
                                                ScriptState* scriptState) {
    DCHECK(scriptState->isolate()->InContext());
    if (!value->IsObject())
      return nullptr;

    v8::Local<v8::String> listenerProperty =
        getHiddenProperty(false, scriptState->isolate());
    return findEventListener(v8::Local<v8::Object>::Cast(value),
                             listenerProperty, scriptState);
  }

  template <typename ListenerType>
  static V8EventListener* ensureEventListener(v8::Local<v8::Value>,
                                              bool isAttribute,
                                              ScriptState*);

  CORE_EXPORT static EventListener* getEventListener(ScriptState*,
                                                     v8::Local<v8::Value>,
                                                     bool isAttribute,
                                                     ListenerLookupType);

 private:
  static V8EventListener* findEventListener(
      v8::Local<v8::Object> object,
      v8::Local<v8::String> listenerProperty,
      ScriptState* scriptState) {
    v8::HandleScope scope(scriptState->isolate());
    DCHECK(scriptState->isolate()->InContext());
    v8::Local<v8::Value> listener =
        V8HiddenValue::getHiddenValue(scriptState, object, listenerProperty);
    if (listener.IsEmpty())
      return nullptr;
    return static_cast<V8EventListener*>(
        v8::External::Cast(*listener)->Value());
  }

  static inline v8::Local<v8::String> getHiddenProperty(bool isAttribute,
                                                        v8::Isolate* isolate) {
    return isAttribute
               ? v8AtomicString(isolate, "EventListenerList::attributeListener")
               : v8AtomicString(isolate, "EventListenerList::listener");
  }
};

template <typename ListenerType>
V8EventListener* V8EventListenerHelper::ensureEventListener(
    v8::Local<v8::Value> value,
    bool isAttribute,
    ScriptState* scriptState) {
  v8::Isolate* isolate = scriptState->isolate();
  DCHECK(isolate->InContext());
  if (!value->IsObject())
    return nullptr;

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  v8::Local<v8::String> listenerProperty =
      getHiddenProperty(isAttribute, isolate);

  V8EventListener* listener =
      findEventListener(object, listenerProperty, scriptState);
  if (listener)
    return listener;

  listener = ListenerType::create(object, isAttribute, scriptState);
  if (listener)
    V8HiddenValue::setHiddenValue(scriptState, object, listenerProperty,
                                  v8::External::New(isolate, listener));

  return listener;
}

}  // namespace blink

#endif  // V8EventListenerHelper_h
