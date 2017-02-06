// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ValueCopier_h
#define V8ValueCopier_h

#include <v8.h>

namespace blink {

v8::MaybeLocal<v8::Value> copyValueFromDebuggerContext(v8::Isolate*, v8::Local<v8::Context> debuggerContext, v8::Local<v8::Context> toContext, v8::Local<v8::Value>);
v8::Maybe<bool> createDataProperty(v8::Local<v8::Context>, v8::Local<v8::Object>, v8::Local<v8::Name> key, v8::Local<v8::Value>);
v8::Maybe<bool> createDataProperty(v8::Local<v8::Context>, v8::Local<v8::Array>, int index, v8::Local<v8::Value>);

} // namespace blink

#endif // !defined(V8ValueCopier_h)
