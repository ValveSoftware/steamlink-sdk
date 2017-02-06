// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_V8_HELPERS_H_
#define EXTENSIONS_RENDERER_V8_HELPERS_H_

#include <stdint.h>
#include <string.h>

#include "base/strings/string_number_conversions.h"
#include "v8/include/v8.h"

namespace extensions {
namespace v8_helpers {

// Helper functions for V8 APIs.

// Converts |str| to a V8 string. Returns true on success.
inline bool ToV8String(v8::Isolate* isolate,
                       const char* str,
                       v8::Local<v8::String>* out) {
  return v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal)
      .ToLocal(out);
}

inline bool ToV8String(v8::Isolate* isolate,
                       const std::string& str,
                       v8::Local<v8::String>* out) {
  return ToV8String(isolate, str.c_str(), out);
}

// Converts |str| to a V8 string.
// This crashes when strlen(str) > v8::String::kMaxLength.
inline v8::Local<v8::String> ToV8StringUnsafe(
    v8::Isolate* isolate,
    const char* str,
    v8::NewStringType string_type = v8::NewStringType::kNormal) {
  DCHECK(strlen(str) <= v8::String::kMaxLength);
  return v8::String::NewFromUtf8(isolate, str, string_type)
      .ToLocalChecked();
}

inline v8::Local<v8::String> ToV8StringUnsafe(
    v8::Isolate* isolate,
    const std::string& str,
    v8::NewStringType string_type = v8::NewStringType::kNormal) {
  return ToV8StringUnsafe(isolate, str.c_str(), string_type);
}

// Returns true if |maybe| is both a value, and that value is true.
inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();
}

// Returns true if |value| is empty or undefined.
inline bool IsEmptyOrUndefied(v8::Local<v8::Value> value) {
  return value.IsEmpty() || value->IsUndefined();
}

// SetProperty() family wraps V8::Object::DefineOwnProperty().
// Returns true on success.
// NOTE: Think about whether you want this or SetPrivateProperty() below.
// TODO(devlin): Sort through more of the callers of this and see if we can
// convert more to be private.
inline bool SetProperty(v8::Local<v8::Context> context,
                        v8::Local<v8::Object> object,
                        v8::Local<v8::String> key,
                        v8::Local<v8::Value> value) {
  return IsTrue(object->DefineOwnProperty(context, key, value));
}

// Wraps v8::Object::SetPrivate(). When possible, prefer this to SetProperty().
inline bool SetPrivateProperty(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object,
                               v8::Local<v8::String> key,
                               v8::Local<v8::Value> value) {
  return IsTrue(object->SetPrivate(
      context, v8::Private::ForApi(context->GetIsolate(), key), value));
}

inline bool SetPrivateProperty(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object,
                               const char* key,
                               v8::Local<v8::Value> value) {
  v8::Local<v8::String> v8_key;
  return ToV8String(context->GetIsolate(), key, &v8_key) &&
         IsTrue(object->SetPrivate(
             context, v8::Private::ForApi(context->GetIsolate(), v8_key),
             value));
}

// GetProperty() family calls V8::Object::Get() and extracts a value from
// returned MaybeLocal. Returns true on success.
// NOTE: Think about whether you want this or GetPrivateProperty() below.
template <typename Key>
inline bool GetProperty(v8::Local<v8::Context> context,
                        v8::Local<v8::Object> object,
                        Key key,
                        v8::Local<v8::Value>* out) {
  return object->Get(context, key).ToLocal(out);
}

inline bool GetProperty(v8::Local<v8::Context> context,
                        v8::Local<v8::Object> object,
                        const char* key,
                        v8::Local<v8::Value>* out) {
  v8::Local<v8::String> v8_key;
  if (!ToV8String(context->GetIsolate(), key, &v8_key))
    return false;
  return GetProperty(context, object, v8_key, out);
}

// Wraps v8::Object::GetPrivate(). When possible, prefer this to GetProperty().
inline bool GetPrivateProperty(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object,
                               v8::Local<v8::String> key,
                               v8::Local<v8::Value>* out) {
  return object
      ->GetPrivate(context, v8::Private::ForApi(context->GetIsolate(), key))
      .ToLocal(out);
}

inline bool GetPrivateProperty(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object,
                               const char* key,
                               v8::Local<v8::Value>* out) {
  v8::Local<v8::String> v8_key;
  return ToV8String(context->GetIsolate(), key, &v8_key) &&
         GetPrivateProperty(context, object, v8_key, out);
}

// GetPropertyUnsafe() family wraps v8::Object::Get(). They crash when an
// exception is thrown.
inline v8::Local<v8::Value> GetPropertyUnsafe(v8::Local<v8::Context> context,
                                              v8::Local<v8::Object> object,
                                              v8::Local<v8::Value> key) {
  return object->Get(context, key).ToLocalChecked();
}

inline v8::Local<v8::Value> GetPropertyUnsafe(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* key,
    v8::NewStringType string_type = v8::NewStringType::kNormal) {
  return object->Get(context,
                     ToV8StringUnsafe(context->GetIsolate(), key, string_type))
      .ToLocalChecked();
}

// Wraps v8::Function::Call(). Returns true on success.
inline bool CallFunction(v8::Local<v8::Context> context,
                         v8::Local<v8::Function> function,
                         v8::Local<v8::Value> recv,
                         int argc,
                         v8::Local<v8::Value> argv[],
                         v8::Local<v8::Value>* out) {
  v8::MicrotasksScope microtasks_scope(
      context->GetIsolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  return function->Call(context, recv, argc, argv).ToLocal(out);
}

}  // namespace v8_helpers
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_V8_HELPERS_H_
