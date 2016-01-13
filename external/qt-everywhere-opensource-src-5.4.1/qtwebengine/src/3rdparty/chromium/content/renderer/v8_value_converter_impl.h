// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
#define CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/renderer/v8_value_converter.h"

namespace base {
class BinaryValue;
class DictionaryValue;
class ListValue;
class Value;
}

namespace content {

class CONTENT_EXPORT V8ValueConverterImpl : public V8ValueConverter {
 public:
  V8ValueConverterImpl();

  // V8ValueConverter implementation.
  virtual void SetDateAllowed(bool val) OVERRIDE;
  virtual void SetRegExpAllowed(bool val) OVERRIDE;
  virtual void SetFunctionAllowed(bool val) OVERRIDE;
  virtual void SetStripNullFromObjects(bool val) OVERRIDE;
  virtual void SetStrategy(Strategy* strategy) OVERRIDE;
  virtual v8::Handle<v8::Value> ToV8Value(
      const base::Value* value,
      v8::Handle<v8::Context> context) const OVERRIDE;
  virtual base::Value* FromV8Value(
      v8::Handle<v8::Value> value,
      v8::Handle<v8::Context> context) const OVERRIDE;

 private:
  friend class ScopedAvoidIdentityHashForTesting;

  class FromV8ValueState;

  v8::Local<v8::Value> ToV8ValueImpl(v8::Isolate* isolate,
                                     v8::Handle<v8::Object> creation_context,
                                     const base::Value* value) const;
  v8::Handle<v8::Value> ToV8Array(v8::Isolate* isolate,
                                  v8::Handle<v8::Object> creation_context,
                                  const base::ListValue* list) const;
  v8::Handle<v8::Value> ToV8Object(
      v8::Isolate* isolate,
      v8::Handle<v8::Object> creation_context,
      const base::DictionaryValue* dictionary) const;
  v8::Handle<v8::Value> ToArrayBuffer(v8::Isolate* isolate,
                                      v8::Handle<v8::Object> creation_context,
                                      const base::BinaryValue* value) const;

  base::Value* FromV8ValueImpl(FromV8ValueState* state,
                               v8::Handle<v8::Value> value,
                               v8::Isolate* isolate) const;
  base::Value* FromV8Array(v8::Handle<v8::Array> array,
                           FromV8ValueState* state,
                           v8::Isolate* isolate) const;

  // This will convert objects of type ArrayBuffer or any of the
  // ArrayBufferView subclasses.
  base::Value* FromV8ArrayBuffer(v8::Handle<v8::Object> val,
                                 v8::Isolate* isolate) const;

  base::Value* FromV8Object(v8::Handle<v8::Object> object,
                            FromV8ValueState* state,
                            v8::Isolate* isolate) const;

  // If true, we will convert Date JavaScript objects to doubles.
  bool date_allowed_;

  // If true, we will convert RegExp JavaScript objects to string.
  bool reg_exp_allowed_;

  // If true, we will convert Function JavaScript objects to dictionaries.
  bool function_allowed_;

  // If true, undefined and null values are ignored when converting v8 objects
  // into Values.
  bool strip_null_from_objects_;

  bool avoid_identity_hash_for_testing_;

  // Strategy object that changes the converter's behavior.
  Strategy* strategy_;

  DISALLOW_COPY_AND_ASSIGN(V8ValueConverterImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
