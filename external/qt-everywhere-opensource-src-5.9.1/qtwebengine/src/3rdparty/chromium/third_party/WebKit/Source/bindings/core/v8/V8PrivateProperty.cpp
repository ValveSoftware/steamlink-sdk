// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PrivateProperty.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"

namespace blink {

v8::Local<v8::Value> V8PrivateProperty::Symbol::getFromMainWorld(
    ScriptState* scriptState,
    ScriptWrappable* scriptWrappable) {
  v8::Local<v8::Object> wrapper =
      scriptWrappable->mainWorldWrapper(scriptState->isolate());
  return wrapper.IsEmpty() ? v8::Local<v8::Value>()
                           : get(scriptState->context(), wrapper);
}

v8::Local<v8::Private> V8PrivateProperty::createV8Private(v8::Isolate* isolate,
                                                          const char* symbol,
                                                          size_t length) {
  v8::Local<v8::String> str =
      v8::String::NewFromOneByte(
          isolate, reinterpret_cast<const uint8_t*>(symbol),
          v8::NewStringType::kNormal, static_cast<int>(length))
          .ToLocalChecked();
  return v8::Private::New(isolate, str);
}

}  // namespace blink
