// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationTestHelper.h"

#include "bindings/core/v8/V8Binding.h"

namespace blink {

void setV8ObjectPropertyAsString(v8::Isolate* isolate,
                                 v8::Local<v8::Object> object,
                                 const StringView& name,
                                 const StringView& value) {
  object
      ->Set(isolate->GetCurrentContext(), v8String(isolate, name),
            v8String(isolate, value))
      .ToChecked();
}

void setV8ObjectPropertyAsNumber(v8::Isolate* isolate,
                                 v8::Local<v8::Object> object,
                                 const StringView& name,
                                 double value) {
  object
      ->Set(isolate->GetCurrentContext(), v8String(isolate, name),
            v8::Number::New(isolate, value))
      .ToChecked();
}

}  // namespace blink
