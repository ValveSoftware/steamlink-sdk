// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/GeneratedCodeHelper.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

void v8ConstructorAttributeGetter(
    v8::Local<v8::Name> propertyName,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> data = info.Data();
  DCHECK(data->IsExternal());
  V8PerContextData* perContextData =
      V8PerContextData::from(info.Holder()->CreationContext());
  if (!perContextData)
    return;
  v8SetReturnValue(
      info, perContextData->constructorForType(WrapperTypeInfo::unwrap(data)));
}

v8::Local<v8::Value> v8Deserialize(v8::Isolate* isolate,
                                   PassRefPtr<SerializedScriptValue> value) {
  if (value)
    return value->deserialize();
  return v8::Null(isolate);
}

}  // namespace blink
