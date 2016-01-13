// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_value_converter.h"

#include "base/float_util.h"
#include "base/values.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/renderer/java/gin_java_bridge_object.h"
#include "gin/array_buffer.h"

namespace content {

GinJavaBridgeValueConverter::GinJavaBridgeValueConverter()
    : converter_(V8ValueConverter::create()) {
  converter_->SetDateAllowed(false);
  converter_->SetRegExpAllowed(false);
  converter_->SetFunctionAllowed(true);
  converter_->SetStrategy(this);
}

GinJavaBridgeValueConverter::~GinJavaBridgeValueConverter() {
}

v8::Handle<v8::Value> GinJavaBridgeValueConverter::ToV8Value(
    const base::Value* value,
    v8::Handle<v8::Context> context) const {
  return converter_->ToV8Value(value, context);
}

scoped_ptr<base::Value> GinJavaBridgeValueConverter::FromV8Value(
    v8::Handle<v8::Value> value,
    v8::Handle<v8::Context> context) const {
  return make_scoped_ptr(converter_->FromV8Value(value, context));
}

bool GinJavaBridgeValueConverter::FromV8Object(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  GinJavaBridgeObject* unwrapped;
  if (!gin::ConvertFromV8(isolate, value, &unwrapped)) {
    return false;
  }
  *out =
      GinJavaBridgeValue::CreateObjectIDValue(unwrapped->object_id()).release();
  return true;
}

namespace {

class TypedArraySerializer {
 public:
  virtual ~TypedArraySerializer() {}
  static scoped_ptr<TypedArraySerializer> Create(
      v8::Handle<v8::TypedArray> typed_array);
  virtual void serializeTo(char* data,
                           size_t data_length,
                           base::ListValue* out) = 0;
 protected:
  TypedArraySerializer() {}
};

template <typename ElementType, typename ListType>
class TypedArraySerializerImpl : public TypedArraySerializer {
 public:
  static scoped_ptr<TypedArraySerializer> Create(
      v8::Handle<v8::TypedArray> typed_array) {
    scoped_ptr<TypedArraySerializerImpl<ElementType, ListType> > result(
        new TypedArraySerializerImpl<ElementType, ListType>(typed_array));
    return result.template PassAs<TypedArraySerializer>();
  }

  virtual void serializeTo(char* data,
                   size_t data_length,
                   base::ListValue* out) OVERRIDE {
    DCHECK_EQ(data_length, typed_array_->Length() * sizeof(ElementType));
    for (ElementType *element = reinterpret_cast<ElementType*>(data),
                     *end = element + typed_array_->Length();
         element != end;
         ++element) {
      const ListType list_value = *element;
      out->Append(new base::FundamentalValue(list_value));
    }
  }

 private:
  explicit TypedArraySerializerImpl(v8::Handle<v8::TypedArray> typed_array)
      : typed_array_(typed_array) {}

  v8::Handle<v8::TypedArray> typed_array_;

  DISALLOW_COPY_AND_ASSIGN(TypedArraySerializerImpl);
};

// static
scoped_ptr<TypedArraySerializer> TypedArraySerializer::Create(
    v8::Handle<v8::TypedArray> typed_array) {
  if (typed_array->IsInt8Array() ||
      typed_array->IsUint8Array() ||
      typed_array->IsUint8ClampedArray()) {
    return TypedArraySerializerImpl<char, int>::Create(typed_array).Pass();
  } else if (typed_array->IsInt16Array() || typed_array->IsUint16Array()) {
    return TypedArraySerializerImpl<int16_t, int>::Create(typed_array).Pass();
  } else if (typed_array->IsInt32Array() || typed_array->IsUint32Array()) {
    return TypedArraySerializerImpl<int32_t, int>::Create(typed_array).Pass();
  } else if (typed_array->IsFloat32Array()) {
    return TypedArraySerializerImpl<float, double>::Create(typed_array).Pass();
  } else if (typed_array->IsFloat64Array()) {
    return TypedArraySerializerImpl<double, double>::Create(typed_array).Pass();
  }
  NOTREACHED();
  return scoped_ptr<TypedArraySerializer>();
}

}  // namespace

bool GinJavaBridgeValueConverter::FromV8ArrayBuffer(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate) const {
  if (!value->IsTypedArray()) {
    *out = GinJavaBridgeValue::CreateUndefinedValue().release();
    return true;
  }

  char* data = NULL;
  size_t data_length = 0;
  gin::ArrayBufferView view;
  if (ConvertFromV8(isolate, value.As<v8::ArrayBufferView>(), &view)) {
    data = reinterpret_cast<char*>(view.bytes());
    data_length = view.num_bytes();
  }
  if (!data) {
    *out = GinJavaBridgeValue::CreateUndefinedValue().release();
    return true;
  }

  base::ListValue* result = new base::ListValue();
  *out = result;
  scoped_ptr<TypedArraySerializer> serializer(
      TypedArraySerializer::Create(value.As<v8::TypedArray>()));
  serializer->serializeTo(data, data_length, result);
  return true;
}

bool GinJavaBridgeValueConverter::FromV8Number(v8::Handle<v8::Number> value,
                                               base::Value** out) const {
  double double_value = value->Value();
  if (base::IsFinite(double_value))
    return false;
  *out = GinJavaBridgeValue::CreateNonFiniteValue(double_value).release();
  return true;
}

bool GinJavaBridgeValueConverter::FromV8Undefined(base::Value** out) const {
  *out = GinJavaBridgeValue::CreateUndefinedValue().release();
  return true;
}

}  // namespace content
