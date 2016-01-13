// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/float_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/renderer/java/gin_java_bridge_value_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace content {

class GinJavaBridgeValueConverterTest : public testing::Test {
 public:
  GinJavaBridgeValueConverterTest()
      : isolate_(v8::Isolate::GetCurrent()) {
  }

 protected:
  virtual void SetUp() {
    v8::HandleScope handle_scope(isolate_);
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_, NULL, global));
  }

  virtual void TearDown() {
    context_.Reset();
  }

  v8::Isolate* isolate_;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

TEST_F(GinJavaBridgeValueConverterTest, BasicValues) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  scoped_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  v8::Handle<v8::Primitive> v8_undefined(v8::Undefined(isolate_));
  scoped_ptr<base::Value> undefined(
      converter->FromV8Value(v8_undefined, context));
  ASSERT_TRUE(undefined.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(undefined.get()));
  scoped_ptr<const GinJavaBridgeValue> undefined_value(
      GinJavaBridgeValue::FromValue(undefined.get()));
  ASSERT_TRUE(undefined_value.get());
  EXPECT_TRUE(undefined_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED));

  v8::Handle<v8::Number> v8_infinity(
      v8::Number::New(isolate_, std::numeric_limits<double>::infinity()));
  scoped_ptr<base::Value> infinity(
      converter->FromV8Value(v8_infinity, context));
  ASSERT_TRUE(infinity.get());
  EXPECT_TRUE(
      GinJavaBridgeValue::ContainsGinJavaBridgeValue(infinity.get()));
  scoped_ptr<const GinJavaBridgeValue> infinity_value(
      GinJavaBridgeValue::FromValue(infinity.get()));
  ASSERT_TRUE(infinity_value.get());
  float native_float;
  EXPECT_TRUE(
      infinity_value->IsType(GinJavaBridgeValue::TYPE_NONFINITE));
  EXPECT_TRUE(infinity_value->GetAsNonFinite(&native_float));
  EXPECT_FALSE(base::IsFinite(native_float));
  EXPECT_FALSE(base::IsNaN(native_float));
}

TEST_F(GinJavaBridgeValueConverterTest, ArrayBuffer) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  scoped_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  v8::Handle<v8::ArrayBuffer> v8_array_buffer(
      v8::ArrayBuffer::New(isolate_, 0));
  scoped_ptr<base::Value> undefined(
      converter->FromV8Value(v8_array_buffer, context));
  ASSERT_TRUE(undefined.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(undefined.get()));
  scoped_ptr<const GinJavaBridgeValue> undefined_value(
      GinJavaBridgeValue::FromValue(undefined.get()));
  ASSERT_TRUE(undefined_value.get());
  EXPECT_TRUE(undefined_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED));
}

TEST_F(GinJavaBridgeValueConverterTest, TypedArrays) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  scoped_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  const char* source_template = "(function() {"
      "var array_buffer = new ArrayBuffer(%s);"
      "var array_view = new %s(array_buffer);"
      "array_view[0] = 42;"
      "return array_view;"
      "})();";
  const char* array_types[] = {
    "1", "Int8Array", "1", "Uint8Array", "1", "Uint8ClampedArray",
    "2", "Int16Array", "2", "Uint16Array",
    "4", "Int32Array", "4", "Uint32Array",
    "4", "Float32Array", "8", "Float64Array"
  };
  for (size_t i = 0; i < arraysize(array_types); i += 2) {
    const char* typed_array_type = array_types[i + 1];
    v8::Handle<v8::Script> script(v8::Script::Compile(v8::String::NewFromUtf8(
        isolate_,
        base::StringPrintf(
            source_template, array_types[i], typed_array_type).c_str())));
    v8::Handle<v8::Value> v8_typed_array = script->Run();
    scoped_ptr<base::Value> list_value(
        converter->FromV8Value(v8_typed_array, context));
    ASSERT_TRUE(list_value.get()) << typed_array_type;
    EXPECT_TRUE(list_value->IsType(base::Value::TYPE_LIST)) << typed_array_type;
    base::ListValue* list;
    ASSERT_TRUE(list_value->GetAsList(&list)) << typed_array_type;
    EXPECT_EQ(1u, list->GetSize()) << typed_array_type;
    double first_element;
    ASSERT_TRUE(list->GetDouble(0, &first_element)) << typed_array_type;
    EXPECT_EQ(42.0, first_element) << typed_array_type;
  }
}

}  // namespace content
