// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

class MyInterceptor : public Wrappable<MyInterceptor>,
                      public NamedPropertyInterceptor,
                      public IndexedPropertyInterceptor {
 public:
  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyInterceptor> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyInterceptor(isolate));
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  // gin::NamedPropertyInterceptor
  virtual v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                                const std::string& property)
      OVERRIDE {
    if (property == "value") {
      return ConvertToV8(isolate, value_);
    } else if (property == "func") {
      return CreateFunctionTemplate(isolate,
                                    base::Bind(&MyInterceptor::Call),
                                    HolderIsFirstArgument)->GetFunction();
    } else {
      return v8::Local<v8::Value>();
    }
  }
  virtual void SetNamedProperty(v8::Isolate* isolate,
                                const std::string& property,
                                v8::Local<v8::Value> value) OVERRIDE {
    if (property != "value")
      return;
    ConvertFromV8(isolate, value, &value_);
  }
  virtual std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) OVERRIDE {
    std::vector<std::string> result;
    result.push_back("func");
    result.push_back("value");
    return result;
  }

  // gin::IndexedPropertyInterceptor
  virtual v8::Local<v8::Value> GetIndexedProperty(v8::Isolate* isolate,
                                                  uint32_t index) OVERRIDE {
    if (index == 0)
      return ConvertToV8(isolate, value_);
    return v8::Local<v8::Value>();
  }
  virtual void SetIndexedProperty(v8::Isolate* isolate,
                                  uint32_t index,
                                  v8::Local<v8::Value> value) OVERRIDE {
    if (index != 0)
      return;
    ConvertFromV8(isolate, value, &value_);
  }
  virtual std::vector<uint32_t> EnumerateIndexedProperties(v8::Isolate* isolate)
      OVERRIDE {
    std::vector<uint32_t> result;
    result.push_back(0);
    return result;
  }

 private:
  explicit MyInterceptor(v8::Isolate* isolate)
      : NamedPropertyInterceptor(isolate, this),
        IndexedPropertyInterceptor(isolate, this),
        value_(0) {}
  virtual ~MyInterceptor() {}

  // gin::Wrappable
  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate)
      OVERRIDE {
    return Wrappable<MyInterceptor>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor()
        .AddIndexedPropertyInterceptor();
  }

  int Call(int value) {
    int tmp = value_;
    value_ = value;
    return tmp;
  }

  int value_;
};

WrapperInfo MyInterceptor::kWrapperInfo = {kEmbedderNativeGin};

class InterceptorTest : public V8Test {
 public:
  void RunInterceptorTest(const std::string& script_source) {
    v8::Isolate* isolate = instance_->isolate();
    v8::HandleScope handle_scope(isolate);

    gin::Handle<MyInterceptor> obj = MyInterceptor::Create(isolate);

    obj->set_value(42);
    EXPECT_EQ(42, obj->value());

    v8::Handle<v8::String> source = StringToV8(isolate, script_source);
    EXPECT_FALSE(source.IsEmpty());

    gin::TryCatch try_catch;
    v8::Handle<v8::Script> script = v8::Script::Compile(source);
    EXPECT_FALSE(script.IsEmpty());
    v8::Handle<v8::Value> val = script->Run();
    EXPECT_FALSE(val.IsEmpty());
    v8::Handle<v8::Function> func;
    EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
    v8::Handle<v8::Value> argv[] = {ConvertToV8(isolate, obj.get()), };
    func->Call(v8::Undefined(isolate), 1, argv);
    EXPECT_FALSE(try_catch.HasCaught());
    EXPECT_EQ("", try_catch.GetStackTrace());

    EXPECT_EQ(191, obj->value());
  }
};

TEST_F(InterceptorTest, NamedInterceptor) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
}

TEST_F(InterceptorTest, NamedInterceptorCall) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj.func(191) !== 42) throw 'FAIL';"
      "   })");
}

TEST_F(InterceptorTest, IndexedInterceptor) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj[0] !== 42) throw 'FAIL';"
      "   else obj[0] = 191; })");
}

}  // namespace gin
